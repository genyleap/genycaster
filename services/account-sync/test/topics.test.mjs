import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createAccountApi} from '../server.mjs';
import {initialTopics, validateCastTopicAssignment} from '../topics.mjs';
import {verifyCastTopicOwnership} from '../cloud_state.mjs';
const principal='farcaster:42';
const calls=[];
const store={
  async listTopics(){return initialTopics.map((slug,i)=>({id:String(i+1),slug,cast_count:i}));},
  async listTopicCasts(slug,limit,offset,authorFid){calls.push(['list',slug,limit,offset,authorFid]);return [{cast_hash:'0x'+'ab'.repeat(20),author_fid:'42'}];},
  async assignCastTopics(owner,hash,slugs){
    calls.push(['assign',owner.subject,owner.fid,hash,slugs]);
    return {cast_hash:hash,topics:slugs};
  }
};
const fetchImpl=async(url,init)=>{
  if(init?.headers?.authorization!=='Bearer good')
    return new Response('{}',{status:401});
  return url.endsWith('/account/profile')
    ? new Response(JSON.stringify({identity_id:principal}),{status:200})
    : new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}),{status:200});
};
async function withServer(run){
 const s=createAccountApi({catalogUrl:'https://id.example.test/catalog',fetchImpl,store,
  issuer:'https://id.example.test',topicVerifier:async()=>true});
 await new Promise(done=>s.listen(0,'127.0.0.1',done));
 const origin='http://127.0.0.1:'+s.address().port+'/api/v1/cloud/';
 try{await run((path,method='GET',data,auth)=>fetch(origin+path,{
  method,headers:{...(auth?{authorization:auth}:{}),'content-type':'application/json'},
  ...(data===undefined?{}:{body:JSON.stringify(data)})}));}
 finally{await new Promise(done=>s.close(done));}
}
test('topics and topic discovery require verified GenyCaster account identity',async()=>{
 await withServer(async req=>{
  assert.equal((await req('topics')).status,401);
  assert.equal((await req('topic_casts?topic=art')).status,401);
  assert.equal((await req('topics','GET',undefined,'Bearer forged')).status,401);
  assert.equal((await req('topic_casts?topic=art','GET',undefined,'Bearer forged')).status,401);
  const valid='Bearer good';
  const r=await req('topics','GET',undefined,valid);
  assert.equal(r.status,200);
  assert.deepEqual((await r.json()).items.slice(0,3).map(x=>x.slug),
     ['art','development','photography']);
  assert.equal((await req('topics','PUT',{},valid)).status,405);
  assert.equal((await req('topic_casts?topic=art&limit=10','GET',undefined,valid)).status,200);
  assert.equal((await req('topic_casts?topic=art&author_fid=42','GET',undefined,valid)).status,200);
  assert.ok(calls.some(c=>c[0]==='list' && c[1]==='art' && c[4]==='42'));
  for(const path of [
    'topic_casts?topic=art&author_fid=0',
    'topic_casts?topic=art&author_fid=abc',
    'topic_casts?topic=art&author_fid=42&author_fid=43',
    'topic_casts?topic=../admin',
    'topic_casts?topic=art&limit=1000'
  ]) assert.equal((await req(path,'GET',undefined,valid)).status,400);
  assert.equal((await req('topic_casts','PUT',{},valid)).status,405);
 });
});
test('topic associations only accept owner-authenticated canonical casts, unique enabled-like slugs and max three',async()=>{
 const valid={cast_hash:'0x'+'AA'.repeat(20),topics:['art','photography']};
 assert.deepEqual(validateCastTopicAssignment(valid),{
  cast_hash:'0x'+'aa'.repeat(20),topics:valid.topics});
 for(const bad of [
  {...valid,topics:[]},{...valid,topics:['art','art']},
  {...valid,topics:['art','development','photography','design']},
  {...valid,topics:['../admin']},{...valid,topics:['Art']},
  {...valid,cast_hash:'invalid'}
 ]) assert.equal(validateCastTopicAssignment(bad),null);
 calls.length=0;
 await withServer(async req=>{
  assert.equal((await req('cast_topics','PUT',valid)).status,401);
  assert.equal((await req('cast_topics','PUT',valid,'Bearer good')).status,200);
  assert.equal((await req('cast_topics','PUT',{...valid,topics:['art','art']},'Bearer good')).status,400);
  assert.ok(calls.some(c=>c[0]==='assign' && c[1]===principal && c[2]==='42'));
 });
});
test('cast ownership requires a matching, confirmed protocol CastAdd',async()=>{
 const hash='0x'+'ab'.repeat(20);
 const verified={message:{hash,data:{fid:42,castAddBody:{text:'Hello'}}}};
 const fake=(data,status=200,headers={})=>async(url)=>{
   assert.match(String(url),/\/v1\/castById\?fid=42&hash=0x/);
   return new Response(JSON.stringify(data),{status,headers});
 };
 assert.equal(await verifyCastTopicOwnership(hash,'42',
   {fetchImpl:fake(verified)}),true);
 assert.equal(await verifyCastTopicOwnership(hash,'43',
   {fetchImpl:async()=>new Response(JSON.stringify(verified))}),false);
 assert.equal(await verifyCastTopicOwnership(hash,'42',
   {fetchImpl:fake({message:{...verified.message,hash:'0x'+'cd'.repeat(20)}})}),false);
 assert.equal(await verifyCastTopicOwnership(hash,'42',
   {fetchImpl:fake({message:{hash,data:{fid:42,castRemoveBody:{}}}})}),false);
 assert.equal(await verifyCastTopicOwnership(hash,'42',
   {fetchImpl:fake(verified,404)}),false);
 // Do not trust Content-Length alone: streams without it must be bounded.
 assert.equal(await verifyCastTopicOwnership(hash,'42',{
   fetchImpl:async()=>new Response(' '.repeat(40000),{status:200})
 }),false);
});
test('migrations preserve separate dynamic tables and retain unique cast+topic link',()=>{
 const migration=readFileSync(new URL('../migrations/006_cast_topics.sql',import.meta.url),'utf8');
 assert.match(migration,/CREATE TABLE IF NOT EXISTS topics/);
 assert.match(migration,/CREATE TABLE IF NOT EXISTS cast_topics/);
 assert.match(migration,/PRIMARY KEY \(cast_hash, topic_id\)/);
 assert.doesNotMatch(migration, /\bparent_url\s+(?:text|varchar)|\bchannel_id\s+(?:text|varchar)/);
});
