import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createCloudHandler} from '../cloud_state.mjs';
import {createPgStore} from '../pg_store.mjs';
import {validateBuzzTarget} from '../genybuzz.mjs';

const issuer='https://identity.example.test';
const verifier=async(url,{headers})=>{
  if(headers?.authorization!=='Bearer verified')
    return new Response('{}',{status:401});
  if(url.endsWith('/account/profile'))
    return new Response(JSON.stringify({identity_id:'account:42'}));
  if(url.endsWith('/account/connections'))
    return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
  return new Response('{}',{status:404});
};
const owner={issuer,subject:'farcaster:42',fid:'42'};

test('GenyBuzz target is an account identity, never client-provided issuer or signer',()=>{
  assert.deepEqual(validateBuzzTarget({
    target_fid:'53',target_kind:'farcaster',context:'profile'
  },issuer),{issuer,subject:'farcaster:53',context:'profile'});
  assert.deepEqual(validateBuzzTarget({
    target_fid:'4294967296',target_kind:'hypersnap',context:'dm'
  },issuer),{issuer:'hypersnap:native-registry',
    subject:'hypersnap:4294967296',context:'dm'});
  for(const target of [
    {target_fid:'0',target_kind:'farcaster',context:'profile'},
    {target_fid:'01',target_kind:'farcaster',context:'profile'},
    {target_fid:'9007199254740992',target_kind:'farcaster',context:'profile'},
    {target_fid:'53',target_kind:'hypersnap',context:'profile'},
    {target_fid:'53',target_kind:'farcaster',context:'wallet'},
    {target_fid:'53',target_kind:'farcaster',context:'profile',
      issuer:'https://attacker.example'},
  ])assert.equal(validateBuzzTarget(target,issuer),null);
});

test('GenyBuzz API requires verified owner, explicit opt-in and bounded methods',async()=>{
  let revision=0,enabled=false,attempts=0,acknowledged='';
  const store={
    buzzReady:()=>true,
    getBuzzPreferences:async()=>({revision,enabled}),
    putBuzzPreferences:async(account,input)=>{
      assert.equal(account.subject,owner.subject);
      if(input.revision!==revision)return null;
      enabled=input.enabled;revision++;
      return {enabled,revision};
    },
    sendBuzz:async(account,target)=>{
      assert.equal(account.subject,owner.subject);
      assert.equal(target.subject,'farcaster:53');
      attempts++;
      return attempts===1?{status:'sent',id:'3ba02b4f-fb7f-4f62-abec-0f4b9ade3f19'}
        :{status:'rate_limited'};
    },
    listBuzzInbox:async()=>[{
      id:'3ba02b4f-fb7f-4f62-abec-0f4b9ade3f19',
      sender_fid:'53',sender_kind:'farcaster',context:'profile'
    }],
    ackBuzz:async(account,id)=>{
      assert.equal(account.subject,owner.subject);
      acknowledged=id;
    }
  };
  const handle=createCloudHandler({store,issuer,fetchImpl:verifier});
  const api=createServer((req,res)=>handle(req,res,new URL(req.url,'http://localhost').pathname));
  await new Promise(ok=>api.listen(0,'127.0.0.1',ok));
  const url='http://127.0.0.1:'+api.address().port+'/api/v1/cloud/';
  const call=(resource,method='GET',payload,auth='Bearer verified')=>fetch(url+resource,{
    method,headers:{authorization:auth,'content-type':'application/json'},
    ...(payload?{body:JSON.stringify(payload)}:{})
  });
  try{
    assert.equal((await call('buzz/inbox','GET',null,'Bearer wrong')).status,401);
    assert.deepEqual((await(await call('buzz/preferences')).json()).preferences,
      {enabled:false,revision:0});
    assert.equal((await call('buzz/preferences','POST')).status,405);
    assert.equal((await call('buzz/preferences','PUT',{enabled:'yes',revision:0})).status,400);
    assert.equal((await call('buzz/preferences','PUT',{enabled:true,revision:0,extra:1})).status,400);
    assert.equal((await call('buzz/preferences','PUT',{enabled:true,revision:0})).status,200);
    assert.equal((await call('buzz/preferences','PUT',{enabled:false,revision:0})).status,409);
    assert.equal((await call('buzz','POST',{
      target_fid:'42',target_kind:'farcaster',context:'profile'
    })).status,400);
    assert.equal((await call('buzz','POST',{
      target_fid:'53',target_kind:'farcaster',context:'wallet'
    })).status,400);
    assert.equal((await call('buzz','POST',{
      target_fid:'53',target_kind:'farcaster',context:'profile'
    })).status,201);
    const limited=await call('buzz','POST',{
      target_fid:'53',target_kind:'farcaster',context:'profile'
    });
    assert.equal(limited.status,429);
    assert.equal(limited.headers.get('retry-after'),'60');
    assert.equal((await call('buzz/inbox')).status,200);
    assert.equal((await call('buzz/ack','POST',{id:'invalid'})).status,400);
    assert.equal((await call('buzz/ack','POST',{
      id:'3ba02b4f-fb7f-4f62-abec-0f4b9ade3f19'
    })).status,200);
    assert.equal(acknowledged,'3ba02b4f-fb7f-4f62-abec-0f4b9ade3f19');
  }finally{api.close();}
});

test('GenyBuzz requires additive schema and serializes sender/recipient budgets',async()=>{
  const sql=[];
  const tx={
    async query(text,params){
      sql.push({text,params});
      if(text.includes('SELECT enabled FROM genybuzz_preferences'))
        return {rows:[{enabled:true}]};
      if(text.includes('SELECT issuer,subject,payload'))
        return {rows:[]};
      if(text.includes('AS pair_count'))
        return {rows:[{pair_count:0,sender_count:0,recipient_count:0}]};
      return {rows:[]};
    },
    release(){}
  };
  const pool={
    async query(text,params){
      sql.push({text,params});
      if(text.includes('SELECT version FROM schema_migrations'))
        return {rows:[{version:3},{version:14}]};
      if(text.includes('SELECT enabled,revision'))
        return {rows:[]};
      if(text.includes('RETURNING enabled,revision'))
        return {rows:[{enabled:true,revision:'1'}]};
      return {rows:[]};
    },
    async connect(){return tx;},
    async end(){}
  };
  const store=await createPgStore('postgres://unused',{poolFactory:()=>pool});
  try{
    assert.equal(store.buzzReady(),true);
    assert.deepEqual(await store.getBuzzPreferences(owner),{enabled:false,revision:0});
    assert.deepEqual(await store.putBuzzPreferences(owner,{enabled:true,revision:0}),
      {enabled:true,revision:1});
    const result=await store.sendBuzz(owner,{
      issuer,subject:'farcaster:53',context:'profile'});
    assert.equal(result.status,'sent');
    assert.match(result.id,/^[0-9a-f-]{36}$/);
    assert.equal(sql.filter(row=>row.text.includes('pg_advisory_xact_lock')).length,2);
    assert.ok(sql.some(row=>row.text.includes('recipient_count')));
    assert.ok(sql.some(row=>row.text.includes('muted_profiles')));
    assert.ok(sql.some(row=>row.text.includes('COMMIT')));
    await store.ackBuzz(owner,result.id);
    assert.ok(sql.at(-1).text.includes('recipient_issuer=$2'));
  }finally{await store.close();}
});
