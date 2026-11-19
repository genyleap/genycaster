import assert from 'node:assert/strict';
import test from 'node:test';
import {createPgStore} from '../pg_store.mjs';

const farcaster={fid:'42',network:'farcaster',
  issuer:'https://identity.example.test',subject:'farcaster:42'};
const hypersnap={fid:'18446744073709551615',network:'hypersnap',
  issuer:'hypersnap:native-registry',
  subject:'hypersnap:18446744073709551615'};

function fakePool(versions, visible) {
  const statements=[];
  return {
    statements,
    async query(sql,params=[]) {
      statements.push({sql,params});
      if(sql.includes('schema_migrations'))
        return {rows:versions.map(version=>({version}))};
      if(sql.includes('FROM profile_extras') && sql.includes('publicly_visible=true'))
        return {rows:visible?[{category:'Technology',profession:'Developer',
          secondary_categories:['Science & technology'],roles:['Developer','Engineer'],
          links:[{url:'https://secret.example.test'}],revision:7}]:[]};
      if(sql.includes('INSERT INTO profile_extras'))
        return {rows:[{category:'Technology',profession:'Developer',
          publicly_visible:params.at(-1),links:[],revision:'1',
          secondary_categories:[],roles:['Developer']}]};
      if(sql.includes('FROM notifications') && sql.includes("kind='profile_status'"))
        return {rows:[{payload:{mood:'creating',note:'Building',visible:true},
          revision:'2',updated_at:'2026-11-19T00:00:00Z'}]};
      throw Error('unexpected SQL: '+sql);
    },
    async end() {}
  };
}

test('public category query exposes only explicitly opted-in curated fields',async()=>{
  for (const visible of [false,true]) {
    const pool=fakePool([3,8,9],visible);
    const store=await createPgStore('postgres://not-used',{poolFactory:()=>pool});
    const result=await store.getPublicProfileExtras(farcaster);
    assert.equal(result===null,!visible);
    if (result) {
      assert.deepEqual(result,{
        category:'Technology',profession:'Developer',
        secondary_categories:['Science & technology'],
        roles:['Developer','Engineer'],
        fid:'42',network:'farcaster'
      });
      assert.equal(Object.hasOwn(result,'links'),false);
      assert.equal(Object.hasOwn(result,'revision'),false);
    }
    const query=pool.statements.at(-1);
    assert.match(query.sql,/publicly_visible=true/);
    assert.doesNotMatch(query.sql,/SELECT[^]*links/);
    assert.deepEqual(query.params,[farcaster.issuer,farcaster.subject]);
    await store.close();
  }
});

test('protocol-scoped public status query keeps native uint64 FIDs as text',async()=>{
  const pool=fakePool([3],true);
  const store=await createPgStore('postgres://not-used',{poolFactory:()=>pool});
  const result=await store.getPublicProfileStatus(hypersnap);
  assert.equal(result.fid,hypersnap.fid);
  assert.equal(result.network,'hypersnap');
  const q=pool.statements.at(-1);
  assert.deepEqual(q.params.slice(0,2),[hypersnap.issuer,hypersnap.subject]);
  assert.ok(q.sql.includes("(payload->>'visible')::boolean=true"));
  await store.close();
});

test('public visibility binds correct SQL parameter in both schema generations',async()=>{
  for(const versions of [[3],[3,8],[3,9],[3,8,9]]) {
    const pool=fakePool(versions,false);
    const store=await createPgStore('postgres://not-used',{poolFactory:()=>pool});
    const result=await store.putProfileExtras(
      {issuer:farcaster.issuer,subject:farcaster.subject},
      {category:'Technology',profession:'Developer',links:[],
       secondary_categories:[],roles:['Developer'],revision:0,publicly_visible:true});
    assert.equal(result.publicly_visible,true);
    const q=pool.statements.at(-1);
    assert.ok(q.sql.includes('$'+q.params.length+'::boolean'));
    assert.equal(q.params.at(-1),true);
    await store.close();
  }
});
