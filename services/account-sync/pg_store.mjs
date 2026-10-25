// The database role for this pool is restricted to genycaster_db.
// No JOIN to OpenProof tables, provider credentials, or protocol signer keys.
import {randomUUID} from 'node:crypto';
const COLUMNS=Object.freeze({
  drafts:'id,kind,title,content,revision,deleted_at,updated_at',
  tasks:'id,kind,schedule_at,timezone,payload,state,revision,attempt_count,executed_at,deleted_at,updated_at',
  notifications:'id,source,kind,payload,occurred_at,read_at,deleted_at,revision,updated_at',
  connections:'provider,display_name,preferred_username,provider_profile_url,manual_profile_url,publicly_visible,last_confirmed_at,updated_at',
  bookmarks:"encode(cast_hash,'hex') AS cast_hash,is_deleted,changed_seq,changed_at",
});
export async function createPgStore(connectionString) {
  if (!connectionString) throw new Error('GENYCASTER_DATABASE_URL is required');
  const {Pool}=await import('pg');
  const pool=new Pool({connectionString, max:8, connectionTimeoutMillis:4000,
    idleTimeoutMillis:20000, statement_timeout:6000});
  // Fail closed without explicit schema provisioning. Never CREATE tables in a request.
  await pool.query('SELECT 1 FROM schema_migrations WHERE version=2');
  return {
    close:()=>pool.end(),
    async listUpdates() {
      const result=await pool.query('SELECT release_id,title,description,changes,published_at FROM app_updates WHERE visible=true ORDER BY published_at DESC LIMIT 100');
      return result.rows;
    },
    async list(entity,{issuer,subject}) {
      const columns=COLUMNS[entity];
      if (!columns) throw new Error('unsupported_table');
      const result=await pool.query(
        'SELECT '+columns+' FROM '+entity+' WHERE '+(
          entity==='bookmarks'?'openproof_issuer=$1 AND openproof_subject=$2':'issuer=$1 AND subject=$2'
        )+' ORDER BY '+(entity==='bookmarks'?'changed_at DESC':'updated_at DESC')+' LIMIT 500',
        [issuer,subject]);
      return result.rows.map(item=>entity==='bookmarks'?
        {...item,cast_hash:'0x'+item.cast_hash}:item);
    },
    async refreshConnections({issuer,subject},values) {
      const client=await pool.connect();
      try {
        await client.query('BEGIN');
        for (const value of values) {
          await client.query(`INSERT INTO connections
            (issuer,subject,provider,display_name,preferred_username,provider_profile_url,last_confirmed_at)
            VALUES ($1,$2,$3,NULLIF($4,''),NULLIF($5,''),$6,now())
            ON CONFLICT (issuer,subject,provider) DO UPDATE SET
              display_name=EXCLUDED.display_name,
              preferred_username=EXCLUDED.preferred_username,
              provider_profile_url=EXCLUDED.provider_profile_url,
              last_confirmed_at=now(),updated_at=now()`,
            [issuer,subject,value.provider,value.display_name,value.preferred_username,
             value.provider_profile_url]);
        }
        // Do not delete manually entered profiles if the provider is temporarily
        // unavailable or a reauthorization hasn't completed.
        await client.query('COMMIT');
      } catch(e){await client.query('ROLLBACK');throw e;}
      finally {client.release();}
    },
    async put(entity,{issuer,subject},item) {
      if (entity==='connections') {
        const result=await pool.query(`INSERT INTO connections
          (issuer,subject,provider,manual_profile_url,publicly_visible)
          VALUES($1,$2,$3,$4,$5)
          ON CONFLICT(issuer,subject,provider) DO UPDATE SET
            manual_profile_url=EXCLUDED.manual_profile_url,
            publicly_visible=EXCLUDED.publicly_visible,updated_at=now()
          RETURNING ${COLUMNS.connections}`,
          [issuer,subject,item.provider,item.manual_profile_url,item.publicly_visible]);
        return result.rows[0]||null;
      }
      if (entity==='bookmarks') {
        const client=await pool.connect();
        try {
          await client.query('BEGIN');
          const change=await client.query(`INSERT INTO bookmark_changes
            (openproof_issuer,openproof_subject,mutation_id,cast_hash,is_deleted)
            VALUES($1,$2,$3,decode($4,'hex'),$5) RETURNING sequence`,
            [issuer,subject,randomUUID(),item.cast_hash.slice(2),item.is_deleted]);
          const result=await client.query(`INSERT INTO bookmarks
            (openproof_issuer,openproof_subject,cast_hash,is_deleted,changed_seq)
            VALUES ($1,$2,decode($3,'hex'),$4,$5)
            ON CONFLICT(openproof_issuer,openproof_subject,cast_hash) DO UPDATE
              SET is_deleted=EXCLUDED.is_deleted,changed_seq=EXCLUDED.changed_seq,
                  changed_at=now()
            RETURNING encode(cast_hash,'hex') AS cast_hash,is_deleted,changed_seq,changed_at`,
            [issuer,subject,item.cast_hash.slice(2),item.is_deleted,change.rows[0].sequence]);
          await client.query('COMMIT');
          return {...result.rows[0],cast_hash:item.cast_hash};
        }catch(e){await client.query('ROLLBACK');throw e;}
        finally{client.release();}
      }
      const revision=Number.isSafeInteger(item.revision) && item.revision>=0?item.revision:0;
      if (entity==='drafts') {
        const result=await pool.query(`INSERT INTO drafts
          (issuer,subject,id,kind,title,content) VALUES ($1,$2,$3,$4,$5,$6::jsonb)
          ON CONFLICT(issuer,subject,id) DO UPDATE SET
            kind=EXCLUDED.kind,title=EXCLUDED.title,content=EXCLUDED.content,
            deleted_at=NULL,revision=drafts.revision+1,updated_at=now()
          WHERE drafts.revision=$7
          RETURNING ${COLUMNS.drafts}`,
          [issuer,subject,item.id,item.kind,item.title,JSON.stringify(item.content),revision]);
        return result.rows[0]||null;
      }
      if (entity==='tasks') {
        const result=await pool.query(`INSERT INTO tasks
          (issuer,subject,id,kind,schedule_at,timezone,payload,state,idempotency_key)
          VALUES($1,$2,$3,$4,$5,$6,$7::jsonb,$8,$9)
          ON CONFLICT(issuer,subject,id) DO UPDATE SET
            kind=EXCLUDED.kind,schedule_at=EXCLUDED.schedule_at,
            timezone=EXCLUDED.timezone,payload=EXCLUDED.payload,state=EXCLUDED.state,
            revision=tasks.revision+1,updated_at=now()
          WHERE tasks.revision=$10 AND tasks.state IN ('draft','scheduled','cancelled')
          RETURNING ${COLUMNS.tasks}`,
          [issuer,subject,item.id,item.kind,item.schedule_at,item.timezone,
           JSON.stringify(item.payload),item.state,item.idempotency_key,revision]);
        return result.rows[0]||null;
      }
      if (entity==='notifications') {
        const result=await pool.query(`INSERT INTO notifications
          (issuer,subject,id,source,kind,payload,occurred_at,read_at)
          VALUES($1,$2,$3,$4,$5,$6::jsonb,$7,$8)
          ON CONFLICT(issuer,subject,id) DO UPDATE SET
            read_at=EXCLUDED.read_at,revision=notifications.revision+1,updated_at=now()
          WHERE notifications.revision=$9
          RETURNING ${COLUMNS.notifications}`,
          [issuer,subject,item.id,item.source,item.kind,JSON.stringify(item.payload),
           item.occurred_at,item.read_at,revision]);
        return result.rows[0]||null;
      }
      throw new Error('unsupported_table');
    }
  };
}
