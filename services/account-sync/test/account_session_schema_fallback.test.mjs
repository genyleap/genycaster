import assert from 'node:assert/strict';
import test from 'node:test';
import {createPgStore} from '../pg_store.mjs';

function fakePool(versions) {
  const statements=[];
  let closed=false;
  return {
    statements,get closed(){return closed;},
    async query(sql,params=[]) {
      statements.push({sql,params});
      if(sql.includes('schema_migrations'))
        return {rows:versions.map(version=>({version})),rowCount:versions.length};
      if(sql.includes('FROM profile_extras'))return {rows:[],rowCount:0};
      if(sql.includes('INSERT INTO account_sessions'))return {rows:[],rowCount:1};
      if(sql.includes('FROM account_sessions'))return {rows:[
        {issuer:'https://openproof.genyleap.com',subject:'farcaster:123',
         csrf_token:'a'.repeat(43)}
      ],rowCount:1};
      if(sql.includes('DELETE FROM account_sessions'))return {rows:[],rowCount:1};
      throw Error('Unexpected SQL: '+sql);
    },
    async end(){closed=true;}
  };
}

test('v3 remains operational and private browser sessions fail closed',async()=>{
  const pool=fakePool([3]);
  const store=await createPgStore('postgres://not-used',{
    poolFactory:()=>pool
  });
  const owner={issuer:'https://openproof.genyleap.com',subject:'farcaster:123'};
  assert.equal(await store.getProfileExtras(owner),null);
  assert.equal(await store.getSession('a'.repeat(64)),null);
  await assert.rejects(store.createSession(owner,'a'.repeat(64),'a'.repeat(43)),
    /account_sessions_not_ready/);
  await store.revokeSession('a'.repeat(64));
  assert.equal(pool.statements.length,2);
  await store.close();assert.equal(pool.closed,true);
});

test('v4 enables owner-scoped sessions only after complete migration',async()=>{
  const pool=fakePool([3,4]);
  const store=await createPgStore('postgres://not-used',{
    poolFactory:()=>pool
  });
  const owner={issuer:'https://openproof.genyleap.com',subject:'farcaster:123'};
  const hash='b'.repeat(64),csrf='a'.repeat(43);
  await store.createSession(owner,hash,csrf);
  const session=await store.getSession(hash);
  assert.equal(session.subject,owner.subject);
  assert.equal(session.csrf_token,csrf);
  await store.revokeSession(hash);
  assert.equal(pool.statements.filter(x=>x.sql.includes('account_sessions')).length,3);
  await store.close();
});

test('missing v3 never silently serves partial account data',async()=>{
  const pool=fakePool([4]);
  await assert.rejects(
    createPgStore('postgres://not-used',{poolFactory:()=>pool}),
    /schema v3 is required/
  );
  assert.equal(pool.closed,true);
});
