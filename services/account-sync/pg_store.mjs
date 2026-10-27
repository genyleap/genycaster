// The database role for this pool is restricted to genycaster_db.
// No JOIN to OpenProof tables, provider credentials, or protocol signer keys.
import {randomUUID} from 'node:crypto';
// Internal singleton row inside the existing per-account notifications table.
// A dedicated state endpoint owns it; ordinary notification APIs cannot write it.
const NOTIFICATION_SEEN_ROW='00000000-0000-4000-8000-000000000001';
const COLUMNS=Object.freeze({
  drafts:'id,kind,title,content,revision,deleted_at,updated_at',
  tasks:'id,kind,schedule_at,timezone,payload,state,revision,attempt_count,executed_at,deleted_at,updated_at',
  notifications:'id,source,kind,payload,occurred_at,read_at,deleted_at,revision,updated_at',
  connections:'provider,display_name,preferred_username,provider_profile_url,manual_profile_url,publicly_visible,last_confirmed_at,updated_at',
  bookmarks:"encode(cast_hash,'hex') AS cast_hash,is_deleted,changed_seq,changed_at",
});
function normalizeRow(entity, row) {
  if (!row) return null;
  if (entity==='bookmarks') return {...row,cast_hash:'0x'+row.cast_hash};
  if (entity==='drafts' || entity==='tasks' || entity==='notifications')
    return {...row,revision:Number(row.revision)};
  return row;
}

export async function createPgStore(connectionString, {poolFactory}={}) {
  if (!connectionString) throw new Error('GENYCASTER_DATABASE_URL is required');
  const settings={connectionString, max:8, connectionTimeoutMillis:4000,
    idleTimeoutMillis:20000, statement_timeout:6000};
  const pool=poolFactory ? poolFactory(settings)
    : new (await import('pg')).Pool(settings);
  // Schema v3 is enough to keep the existing account API operational.
  // Do not require an un-applied v4 migration on a PM2 restart. The session
  // endpoints fail closed until the additive migration is applied and the
  // backend is restarted; the other account data remains available.
  const migrations=await pool.query(
    'SELECT version FROM schema_migrations WHERE version IN (3,4,5,6)');
  const versions=new Set(migrations.rows.map(row=>Number(row.version)));
  if (!versions.has(3)) {
    await pool.end();
    throw new Error('Genycaster database schema v3 is required');
  }
  const sessionsReady=versions.has(4);
  const bookmarkPreviewsReady=versions.has(5);
  const topicsReady=versions.has(6);
  return {
    close:()=>pool.end(),
    bookmarkPreviewsReady:()=>bookmarkPreviewsReady,
    topicsReady:()=>topicsReady,
    async listTopics() {
      if(!topicsReady) throw new Error('topics_not_ready');
      const result=await pool.query(`SELECT id::text,slug,sort_order,
           (SELECT count(*)::integer FROM cast_topics WHERE topic_id=topics.id) AS cast_count
         FROM topics WHERE is_active=true ORDER BY sort_order ASC,id ASC LIMIT 500`);
      return result.rows;
    },
    async assignCastTopics({fid},castHash,slugs) {
      if(!topicsReady) throw new Error('topics_not_ready');
      const client=await pool.connect();
      try {
        await client.query('BEGIN');
        // Lock this immutable cast identity across concurrent requests. Lock
        // does not depend on existing rows, so two devices cannot interleave
        // different sets of tags against one cast.
        await client.query('SELECT pg_advisory_xact_lock(hashtext($1))',[castHash]);
        const topics=await client.query(
          'SELECT id,slug FROM topics WHERE is_active=true AND slug=ANY($1::text[])',
          [slugs]);
        if(topics.rows.length!==slugs.length) {
          await client.query('ROLLBACK');return null;
        }
        const other=await client.query(
          'SELECT 1 FROM cast_topics WHERE cast_hash=decode(substr($1,3),\'hex\') AND author_fid<>$2 LIMIT 1',
          [castHash,fid]);
        if(other.rowCount) {await client.query('ROLLBACK');return null;}
        await client.query('DELETE FROM cast_topics WHERE cast_hash=decode(substr($1,3),\'hex\') AND author_fid=$2',
          [castHash,fid]);
        for(const row of topics.rows)
          await client.query(`INSERT INTO cast_topics(cast_hash,topic_id,author_fid)
             VALUES(decode(substr($1,3),'hex'),$2,$3)`,[castHash,row.id,fid]);
        await client.query('COMMIT');
        return {cast_hash:castHash,topics:slugs};
      }catch(e){await client.query('ROLLBACK');throw e;}
      finally{client.release();}
    },
    async listTopicCasts(slug,limit,offset) {
      if(!topicsReady) throw new Error('topics_not_ready');
      const result=await pool.query(`SELECT '0x'||encode(ct.cast_hash,'hex') AS cast_hash,
           ct.author_fid::text,ct.created_at
         FROM cast_topics ct INNER JOIN topics t ON t.id=ct.topic_id
         WHERE t.slug=$1 AND t.is_active=true
         ORDER BY ct.created_at DESC,ct.cast_hash DESC LIMIT $2 OFFSET $3`,
         [slug,limit,offset]);
      return result.rows;
    },
    async createSession({issuer,subject},tokenHash,csrfToken) {
      if(!sessionsReady)throw new Error('account_sessions_not_ready');
      await pool.query(`INSERT INTO account_sessions
          (issuer,subject,token_hash,csrf_token,expires_at)
          VALUES ($1,$2,$3,$4,now()+interval '30 days')`,
        [issuer,subject,tokenHash,csrfToken]);
    },
    async getSession(tokenHash) {
      if(!sessionsReady)return null;
      const result=await pool.query(`SELECT issuer,subject,csrf_token
          FROM account_sessions WHERE token_hash=$1 AND expires_at>now()`,
        [tokenHash]);
      return result.rows[0]||null;
    },
    async revokeSession(tokenHash) {
      if(!sessionsReady)return;
      await pool.query('DELETE FROM account_sessions WHERE token_hash=$1',
        [tokenHash]);
    },
    async getDraft({issuer,subject},id) {
      const result=await pool.query(`SELECT ${COLUMNS.drafts}
          FROM drafts WHERE issuer=$1 AND subject=$2 AND id=$3`,
        [issuer,subject,id]);
      return normalizeRow('drafts',result.rows[0]);
    },
    async getNotificationSeen({issuer,subject}) {
      const result=await pool.query(`SELECT
          (EXTRACT(EPOCH FROM read_at)*1000)::bigint::text AS seen_at_ms
          FROM notifications
          WHERE issuer=$1 AND subject=$2 AND id=$3 AND source='genycaster'
            AND kind='seen_watermark'`,
        [issuer,subject,NOTIFICATION_SEEN_ROW]);
      return result.rows[0]?.seen_at_ms||'0';
    },
    async putNotificationSeen({issuer,subject},milliseconds) {
      // Monotonic under simultaneous writes from two devices: older requests
      // cannot mark notifications unread again or overwrite a newer watermark.
      const result=await pool.query(`INSERT INTO notifications
          (issuer,subject,id,source,kind,payload,occurred_at,read_at)
          VALUES ($1,$2,$3,'genycaster','seen_watermark','{}'::jsonb,
              'epoch'::timestamptz,to_timestamp($4::double precision/1000))
          ON CONFLICT (issuer,subject,id) DO UPDATE SET
            source='genycaster',kind='seen_watermark',
            read_at=GREATEST(COALESCE(notifications.read_at,'epoch'::timestamptz),
                             EXCLUDED.read_at),
            revision=notifications.revision+1,updated_at=now()
          RETURNING (EXTRACT(EPOCH FROM read_at)*1000)::bigint::text AS seen_at_ms`,
        [issuer,subject,NOTIFICATION_SEEN_ROW,milliseconds]);
      return result.rows[0].seen_at_ms;
    },
    async getProfileExtras({issuer,subject}) {
      const result=await pool.query(`SELECT category,profession,links,publicly_visible,
          revision,updated_at FROM profile_extras WHERE issuer=$1 AND subject=$2`,
        [issuer,subject]);
      const record=result.rows[0];
      return record ? {...record,revision:Number(record.revision)} : null;
    },
    async putProfileExtras({issuer,subject},item) {
      // Version zero creates exactly once; existing rows require the expected
      // revision. Two devices cannot silently overwrite each other's changes.
      const result=await pool.query(`INSERT INTO profile_extras
          (issuer,subject,category,profession,links,publicly_visible,revision)
        SELECT $1,$2,$3,$4,$5::jsonb,false,1
        WHERE $6::bigint=0 OR EXISTS (
          SELECT 1 FROM profile_extras WHERE issuer=$1 AND subject=$2
            AND revision=$6
        )
        ON CONFLICT (issuer,subject) DO UPDATE SET
          category=EXCLUDED.category,profession=EXCLUDED.profession,
          links=EXCLUDED.links,publicly_visible=false,
          revision=profile_extras.revision+1,updated_at=now()
        WHERE profile_extras.revision=$6
        RETURNING category,profession,links,publicly_visible,revision,updated_at`,
        [issuer,subject,item.category,item.profession,JSON.stringify(item.links),item.revision]);
      const record=result.rows[0];
      return record ? {...record,revision:Number(record.revision)} : null;
    },
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
        )+(entity==='notifications'
          ? " AND NOT (source='genycaster' AND kind='seen_watermark')" : '')
          +' ORDER BY '+(entity==='bookmarks'?'changed_at DESC':'updated_at DESC')+' LIMIT 500',
        [issuer,subject]);
      return result.rows.map(row=>normalizeRow(entity,row));
    },
    async listBookmarkChanges({issuer,subject},cursor) {
      const extra=bookmarkPreviewsReady
        ? ',priority,preview' : '';
      const result=await pool.query(`SELECT sequence,
            '0x'||encode(cast_hash,'hex') AS cast_hash,is_deleted,changed_at`+
            extra+`
          FROM bookmark_changes
          WHERE openproof_issuer=$1 AND openproof_subject=$2
            AND sequence>$3::bigint
          ORDER BY sequence ASC LIMIT 200`,
        [issuer,subject,cursor]);
      return result.rows;
    },
    async listBookmarkSnapshot({issuer,subject}) {
      const extra=bookmarkPreviewsReady
        ? ',priority,preview' : '';
      const rows=await pool.query(`SELECT
        '0x'||encode(cast_hash,'hex') AS cast_hash,
        is_deleted,changed_seq,changed_at`+extra+`
        FROM bookmarks WHERE openproof_issuer=$1 AND openproof_subject=$2
        ORDER BY changed_seq DESC LIMIT 500`,[issuer,subject]);
      return rows.rows;
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
        // The upstream request already succeeded and this is the complete
        // verified provider set. Retire revoked links; preserve user-entered
        // URLs, but never continue labeling their provider as verified.
        await client.query(`UPDATE connections
             SET last_confirmed_at=NULL,updated_at=now()
             WHERE issuer=$1 AND subject=$2 AND last_confirmed_at IS NOT NULL
               AND NOT (provider = ANY($3::text[]))`,
          [issuer,subject,values.map(value=>value.provider)]);
        await client.query('COMMIT');
      } catch(e){await client.query('ROLLBACK');throw e;}
      finally {client.release();}
    },
    async importConnectionLabels({issuer,subject},values) {
      // The account holder may carry public-facing labels across devices.
      // This is NOT an OpenProof verification or an authorization source:
      // last_confirmed_at always stays NULL for newly imported rows.
      const client=await pool.connect();
      try {
        await client.query('BEGIN');
        for(const row of values) {
          await client.query(`INSERT INTO connections
            (issuer,subject,provider,display_name,preferred_username,
             provider_profile_url,manual_profile_url)
            VALUES($1,$2,$3,NULLIF($4,''),NULLIF($5,''),$6,NULLIF($7,''))
            ON CONFLICT (issuer,subject,provider) DO UPDATE SET
              display_name=CASE WHEN connections.last_confirmed_at IS NULL
                THEN COALESCE(EXCLUDED.display_name,connections.display_name)
                ELSE connections.display_name END,
              preferred_username=CASE WHEN connections.last_confirmed_at IS NULL
                THEN COALESCE(EXCLUDED.preferred_username,connections.preferred_username)
                ELSE connections.preferred_username END,
              provider_profile_url=CASE WHEN connections.last_confirmed_at IS NULL
                THEN COALESCE(EXCLUDED.provider_profile_url,connections.provider_profile_url)
                ELSE connections.provider_profile_url END,
              manual_profile_url=COALESCE(EXCLUDED.manual_profile_url,
                                          connections.manual_profile_url),
              updated_at=now()`,
            [issuer,subject,row.provider,row.display_name,
             row.preferred_username,row.provider_profile_url,
             row.manual_profile_url||'']);
        }
        await client.query('COMMIT');
      }catch(error){await client.query('ROLLBACK');throw error;}
      finally{client.release();}
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
          const mutationId=item.mutation_id||randomUUID();
          const change=bookmarkPreviewsReady
            ? await client.query(`INSERT INTO bookmark_changes
              (openproof_issuer,openproof_subject,mutation_id,cast_hash,
               is_deleted,priority,preview)
              VALUES($1,$2,$3,decode($4,'hex'),$5,$6,$7::jsonb)
              ON CONFLICT (openproof_issuer,openproof_subject,mutation_id) DO NOTHING
              RETURNING sequence`,
              [issuer,subject,mutationId,item.cast_hash.slice(2),item.is_deleted,
               item.priority,item.preview === null ? null : JSON.stringify(item.preview)])
            : await client.query(`INSERT INTO bookmark_changes
              (openproof_issuer,openproof_subject,mutation_id,cast_hash,is_deleted)
              VALUES($1,$2,$3,decode($4,'hex'),$5)
              ON CONFLICT (openproof_issuer,openproof_subject,mutation_id) DO NOTHING
              RETURNING sequence`,
              [issuer,subject,mutationId,item.cast_hash.slice(2),item.is_deleted]);
          if (change.rowCount === 0) {
            const previous=await client.query(`SELECT
                '0x'||encode(cast_hash,'hex') AS cast_hash,is_deleted
              FROM bookmark_changes
              WHERE openproof_issuer=$1 AND openproof_subject=$2 AND mutation_id=$3`,
              [issuer,subject,mutationId]);
            await client.query('COMMIT');
            const original=previous.rows[0];
            // A retried idempotency key must never rewrite a newer change,
            // nor may it be reused for a different bookmarked cast.
            if (!original || original.cast_hash!==item.cast_hash
                || original.is_deleted!==item.is_deleted) return null;
            const current=await pool.query(`SELECT
                '0x'||encode(cast_hash,'hex') AS cast_hash,
                is_deleted,changed_seq,changed_at
              FROM bookmarks WHERE openproof_issuer=$1 AND openproof_subject=$2
                AND cast_hash=decode($3,'hex')`,
              [issuer,subject,item.cast_hash.slice(2)]);
            return current.rows[0]||null;
          }
          const result=bookmarkPreviewsReady
            ? await client.query(`INSERT INTO bookmarks
              (openproof_issuer,openproof_subject,cast_hash,is_deleted,changed_seq,
               priority,preview)
              VALUES($1,$2,decode($3,'hex'),$4,$5,$6,$7::jsonb)
              ON CONFLICT(openproof_issuer,openproof_subject,cast_hash) DO UPDATE
                SET is_deleted=EXCLUDED.is_deleted,changed_seq=EXCLUDED.changed_seq,
                    priority=EXCLUDED.priority,
                    preview=COALESCE(EXCLUDED.preview,bookmarks.preview),
                    changed_at=now()
              RETURNING encode(cast_hash,'hex') AS cast_hash,
                is_deleted,changed_seq,changed_at,priority,preview`,
              [issuer,subject,item.cast_hash.slice(2),item.is_deleted,
               change.rows[0].sequence,item.priority,item.preview === null ? null : JSON.stringify(item.preview)])
            : await client.query(`INSERT INTO bookmarks
              (openproof_issuer,openproof_subject,cast_hash,is_deleted,changed_seq)
              VALUES ($1,$2,decode($3,'hex'),$4,$5)
              ON CONFLICT(openproof_issuer,openproof_subject,cast_hash) DO UPDATE
                SET is_deleted=EXCLUDED.is_deleted,changed_seq=EXCLUDED.changed_seq,
                    changed_at=now()
              RETURNING encode(cast_hash,'hex') AS cast_hash,is_deleted,changed_seq,changed_at`,
              [issuer,subject,item.cast_hash.slice(2),item.is_deleted,
               change.rows[0].sequence]);
          await client.query('COMMIT');
          return {...result.rows[0],cast_hash:item.cast_hash};
        }catch(e){await client.query('ROLLBACK');throw e;}
        finally{client.release();}
      }
      const revision=Number.isSafeInteger(item.revision) && item.revision>=0?item.revision:0;
      if (entity==='drafts') {
        // Deletions are versioned tombstones. A stale device cannot resurrect
        // a published or discarded draft without first observing its revision.
        const result=await pool.query(`INSERT INTO drafts
          (issuer,subject,id,kind,title,content,deleted_at)
          SELECT $1,$2,$3,$4,$5,$6::jsonb,
            CASE WHEN $8::boolean THEN now() ELSE NULL END
          WHERE $7::bigint=0 OR EXISTS (
            SELECT 1 FROM drafts WHERE issuer=$1 AND subject=$2
              AND id=$3 AND revision=$7
          )
          ON CONFLICT(issuer,subject,id) DO UPDATE SET
            kind=EXCLUDED.kind,title=EXCLUDED.title,content=EXCLUDED.content,
            deleted_at=EXCLUDED.deleted_at,
            revision=drafts.revision+1,updated_at=now()
          WHERE drafts.revision=$7
          RETURNING ${COLUMNS.drafts}`,
          [issuer,subject,item.id,item.kind,item.title,JSON.stringify(item.content),
           revision,item.is_deleted===true]);
        return normalizeRow('drafts',result.rows[0]);
      }
      if (entity==='tasks') {
        const result=await pool.query(`INSERT INTO tasks
          (issuer,subject,id,kind,schedule_at,timezone,payload,state,idempotency_key)
          SELECT $1,$2,$3,$4,$5,$6,$7::jsonb,$8,$9
          WHERE $10::bigint=0 OR EXISTS (
            SELECT 1 FROM tasks WHERE issuer=$1 AND subject=$2
              AND id=$3 AND revision=$10
          )
          ON CONFLICT(issuer,subject,id) DO UPDATE SET
            kind=EXCLUDED.kind,schedule_at=EXCLUDED.schedule_at,
            timezone=EXCLUDED.timezone,payload=EXCLUDED.payload,state=EXCLUDED.state,
            revision=tasks.revision+1,updated_at=now()
          WHERE tasks.revision=$10 AND tasks.state IN ('draft','scheduled','cancelled')
          RETURNING ${COLUMNS.tasks}`,
          [issuer,subject,item.id,item.kind,item.schedule_at,item.timezone,
           JSON.stringify(item.payload),item.state,item.idempotency_key,revision]);
        return normalizeRow('tasks',result.rows[0]);
      }
      if (entity==='notifications') {
        const result=await pool.query(`INSERT INTO notifications
          (issuer,subject,id,source,kind,payload,occurred_at,read_at)
          SELECT $1,$2,$3,$4,$5,$6::jsonb,$7,$8
          WHERE $9::bigint=0 OR EXISTS (
            SELECT 1 FROM notifications WHERE issuer=$1 AND subject=$2
              AND id=$3 AND revision=$9
          )
          ON CONFLICT(issuer,subject,id) DO UPDATE SET
            read_at=EXCLUDED.read_at,revision=notifications.revision+1,updated_at=now()
          WHERE notifications.revision=$9
          RETURNING ${COLUMNS.notifications}`,
          [issuer,subject,item.id,item.source,item.kind,JSON.stringify(item.payload),
           item.occurred_at,item.read_at,revision]);
        return normalizeRow('notifications',result.rows[0]);
      }
      throw new Error('unsupported_table');
    }
  };
}
