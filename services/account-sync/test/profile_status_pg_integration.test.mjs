import test from 'node:test';
import assert from 'node:assert/strict';
import pg from 'pg';
import {createPgStore} from '../pg_store.mjs';

// Run against an isolated temporary table in a rolled-back transaction:
// never create, update or delete a production user's profile status.
test('real PostgreSQL profile-status CAS supports updates after revision 1', {
  skip: !process.env.GENYCASTER_DATABASE_URL
}, async () => {
  const client = new pg.Client({connectionString:process.env.GENYCASTER_DATABASE_URL});
  await client.connect();
  try {
    await client.query('BEGIN');
    await client.query(`CREATE TEMP TABLE notifications (
      issuer text NOT NULL, subject text NOT NULL, id uuid NOT NULL,
      source text NOT NULL, kind text NOT NULL, payload jsonb NOT NULL,
      occurred_at timestamptz NOT NULL DEFAULT now(),
      read_at timestamptz, deleted_at timestamptz,
      updated_at timestamptz NOT NULL DEFAULT now(),
      revision bigint NOT NULL DEFAULT 1,
      PRIMARY KEY(issuer,subject,id)
    ) ON COMMIT DROP`);
    const store=await createPgStore(process.env.GENYCASTER_DATABASE_URL,{
      poolFactory:()=>({query:(sql,args)=>client.query(sql,args),end:async()=>{}})
    });
    const owner={issuer:'https://openproof.genyleap.com',
      subject:'farcaster:42',fid:'42',network:'farcaster'};
    const other={...owner,subject:'farcaster:99',fid:'99'};
    const write=(mood,revision,visible=true,note='')=>
      store.putProfileStatus(owner,{mood,revision,visible,note});
    assert.equal(await store.getOwnProfileStatus(owner),null);
    assert.equal(await store.putProfileStatus(other,{mood:'busy',revision:7,
      note:'',visible:true}),null,'cannot create a missing row with stale revision');
    const first=await write('busy',0);
    assert.equal(first.revision,'1');
    assert.equal(first.mood,'busy');
    assert.equal(await write('idle',0),null,'stale rev0 does not overwrite');
    const second=await write('dnd',1);
    assert.equal(second.revision,'2','rev1 must take the ON CONFLICT update path');
    assert.equal(second.mood,'dnd');
    assert.equal(await write('away',1),null,'stale rev1 does not overwrite');
    const third=await write('none',2,false);
    assert.equal(third.revision,'3');
    assert.equal(await store.getPublicProfileStatus(owner),null);
    const fourth=await write('available',3);
    assert.equal(fourth.revision,'4');
    assert.equal((await store.getOwnProfileStatus(owner)).mood,'available');
    assert.equal((await store.getPublicProfileStatus(owner)).mood,'available');
    assert.equal(await store.getOwnProfileStatus(other),null);
    await store.close();
  } finally {
    await client.query('ROLLBACK');
    await client.end();
  }
});
