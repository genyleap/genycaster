import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createCloudHandler} from '../cloud_state.mjs';
import {createPgStore} from '../pg_store.mjs';

const issuer='https://identity.example.test';
const owner={issuer,subject:'farcaster:42',fid:'42'};
const verifier=async(url,{headers})=>{
  if(headers?.authorization!=='Bearer verified')return new Response('{}',{status:401});
  if(url.endsWith('/account/profile'))
    return new Response(JSON.stringify({identity_id:'farcaster:42'}));
  if(url.endsWith('/account/connections'))
    return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
  return new Response('{}',{status:404});
};
test('Mute API authenticates account owner, validates FIDs and rejects conflicts',async()=>{
  let revision=0,muted_fids=[];
  const store={
    getMutedProfiles:async account=>{
      assert.equal(account.subject,owner.subject);
      return {muted_fids,revision};
    },
    putMutedProfiles:async(account,change)=>{
      assert.equal(account.subject,owner.subject);
      if(change.revision!==revision)return null;
      revision++;muted_fids=change.muted_fids;
      return {muted_fids,revision};
    },
  };
  const handle=createCloudHandler({store,issuer,fetchImpl:verifier});
  const server=createServer((req,res)=>handle(req,res,new URL(req.url,'http://localhost').pathname));
  await new Promise(ok=>server.listen(0,'127.0.0.1',ok));
  const url='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/muted_profiles';
  const call=(method='GET',payload,auth='Bearer verified')=>fetch(url,{
    method,headers:{authorization:auth,'content-type':'application/json'},
    ...(payload?{body:JSON.stringify(payload)}:{})
  });
  try {
    assert.equal((await call('GET',null,'Bearer wrong')).status,401);
    assert.equal((await call('GET')).status,200);
    assert.equal((await call('POST')).status,405);
    assert.equal((await call('PUT',{muted_fids:['42'],revision:0})).status,400,'cannot mute self');
    assert.equal((await call('PUT',{muted_fids:['53','53'],revision:0})).status,400,'duplicates rejected');
    assert.equal((await call('PUT',{muted_fids:['01'],revision:0})).status,400);
    assert.equal((await call('PUT',{muted_fids:['53'],revision:0,extra:1})).status,400);
    const put=await call('PUT',{muted_fids:['97','53'],revision:0});
    assert.equal(put.status,200);
    assert.deepEqual((await put.json()).preferences,{revision:1,muted_fids:['53','97']});
    assert.equal((await call('PUT',{muted_fids:[],revision:0})).status,409);
    assert.deepEqual((await(await call('GET')).json()).preferences,
      {muted_fids:['53','97'],revision:1});
    const unmute=await call('PUT',{muted_fids:['97'],revision:1});
    assert.equal(unmute.status,200);
    assert.deepEqual((await unmute.json()).preferences,
      {muted_fids:['97'],revision:2});
  }finally{server.close();}
});

test('database Mute preferences are isolated, revision-checked and hidden from normal notifications',async()=>{
  const queries=[];
  const pool={async query(sql,args){
    queries.push({sql,args});
    if(sql.startsWith('SELECT version FROM schema_migrations'))
      return {rows:[{version:3}]};
    if(sql.includes("SELECT payload,revision::text")&&sql.includes("muted_profiles"))
      return {rows:[{payload:{muted_fids:['22']},revision:'5'}]};
    if(sql.includes('RETURNING payload,revision::text'))
      return {rows:[{payload:{muted_fids:['22','53']},revision:'6'}]};
    return {rows:[],rowCount:0};
  },async end(){}};
  const store=await createPgStore('postgres://unused',{poolFactory:()=>pool});
  try{
    const before=await store.getMutedProfiles(owner);
    assert.deepEqual(before,{muted_fids:['22'],revision:5});
    assert.deepEqual(queries.at(-1).args.slice(0,2),[issuer,'farcaster:42']);
    const after=await store.putMutedProfiles(owner,{muted_fids:['22','53'],revision:5});
    assert.deepEqual(after,{muted_fids:['22','53'],revision:6});
    assert.match(queries.at(-1).sql,/notifications\.revision=\$5::bigint/);
    assert.match(queries.at(-1).sql,/ON CONFLICT/);
    await store.list('notifications',owner);
    assert.match(queries.at(-1).sql,/muted_profiles/);
  }finally{await store.close();}
});
