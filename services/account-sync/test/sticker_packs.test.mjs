import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createCloudHandler} from '../cloud_state.mjs';
import {createPgStore} from '../pg_store.mjs';
import {STICKER_PACKS,addedPremiumPack,canUseStickerPack,validStickerPackIds}
  from '../sticker_packs.mjs';

const issuer='https://identity.example.test';
const owner={issuer,subject:'farcaster:42',fid:'42'};
const verifier=async(url,{headers})=>{
 if(headers?.authorization!=='Bearer verified')return new Response('{}',{status:401});
 if(url.endsWith('/account/profile'))
   return Response.json({identity_id:'farcaster:42'});
 if(url.endsWith('/account/connections'))
   return Response.json({connections:[{provider:'farcaster',fid:'42'}]});
 return new Response('{}',{status:404});
};
async function withServer(store,resolvePro,action) {
 const handle=createCloudHandler({store,issuer,fetchImpl:verifier,resolvePro});
 const server=createServer((req,res)=>handle(req,res,new URL(req.url,'http://localhost').pathname));
 await new Promise(ok=>server.listen(0,'127.0.0.1',ok));
 const url='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/sticker_packs';
 const call=(method='GET',data,auth='Bearer verified')=>fetch(url,{
   method,headers:{authorization:auth,'content-type':'application/json'},
   ...(data===undefined?{}:{body:JSON.stringify(data)})
 });
 try {await action(call);} finally {server.close();}
}
test('pack IDs and premium metadata are strict server-owned constants',()=>{
 assert.deepEqual(Object.keys(STICKER_PACKS).sort(),['njal','twemoji-pro']);
 assert.equal(STICKER_PACKS.njal.sticker_count,68);
 assert.equal(STICKER_PACKS['twemoji-pro'].tier,'pro');
 assert.equal(validStickerPackIds(['njal','twemoji-pro']),true);
 for(const ids of [['njal','njal'],['not-real'],['njal',55],[1],new Array(20).fill('njal')])
   assert.equal(validStickerPackIds(ids),false);
 assert.equal(addedPremiumPack(['twemoji-pro'],[]),true);
 assert.equal(addedPremiumPack(['njal'],[]),false);
 assert.equal(addedPremiumPack(['njal','twemoji-pro'],['twemoji-pro']),false);
 assert.equal(canUseStickerPack('twemoji-pro',['twemoji-pro'],false),false);
 assert.equal(canUseStickerPack('twemoji-pro',['twemoji-pro'],true),true);
 assert.equal(canUseStickerPack('njal',['njal'],false),true);
 assert.equal(canUseStickerPack('njal',[],true),false);
});

test('verified account owns its library; Pro is server-enforced; revision conflicts fail',async()=>{
 let revision=0;
 let saved=[];
 let pro=false;
 let grants=0;
 const store={
  stickerPacksReady:()=>true,
  async getStickerPacks(account){
   assert.equal(account.subject,'farcaster:42');
   return {pack_ids:[...saved],revision};
  },
  async putStickerPacks(account,change){
   assert.equal(account.subject,'farcaster:42');
   if(change.revision!==revision)return null;
   saved=change.pack_ids.slice();
   revision++;
   return {pack_ids:[...saved],revision};
  }
 };
 await withServer(store,async()=>{grants++;return {status:'verified',eligible:pro};},
 async call=>{
  assert.equal((await call('GET',undefined,'Bearer wrong')).status,401);
  assert.deepEqual((await (await call('GET')).json()).library,{pack_ids:[],revision:0});
  assert.equal((await call('POST')).status,405);
  for(const change of [
   {pack_ids:['njal','njal'],revision:0},
   {pack_ids:['hacker'],revision:0},
   {pack_ids:['njal'],revision:0,eligible:true},
   {pack_ids:['njal'],revision:-1},
  ]) assert.equal((await call('PUT',change)).status,400);
  const free=await call('PUT',{pack_ids:['njal'],revision:0});
  assert.equal(free.status,200);
  assert.deepEqual((await free.json()).library,{pack_ids:['njal'],revision:1});
  assert.equal(grants,0,'installing free art does not need Pro');
  assert.equal((await call('PUT',{pack_ids:['twemoji-pro'],revision:1})).status,403);
  assert.deepEqual(saved,['njal'],'failed entitlement must not modify state');
  pro=true;
  const premium=await call('PUT',{pack_ids:['njal','twemoji-pro'],revision:1});
  assert.equal(premium.status,200);
  assert.equal((await premium.json()).library.revision,2);
  pro=false;
  assert.equal((await call('PUT',{pack_ids:['njal'],revision:2})).status,200,
    'removing Pro must remain possible after expiry');
  const conflict=await call('PUT',{pack_ids:[],revision:2});
  assert.equal(conflict.status,409);
  assert.deepEqual((await conflict.json()).library.pack_ids,['njal']);
 });
});

test('sticker library is unavailable when the account datastore is not ready',async()=>{
 await withServer({stickerPacksReady:()=>false},async()=>({status:'verified',eligible:true}),
 async call=>{
  assert.equal((await call('GET')).status,503);
  assert.equal((await call('PUT',{pack_ids:['njal'],revision:0})).status,503);
 });
});

test('PostgreSQL app-owned singleton uses scoped atomic CAS without new migration',async()=>{
 const queries=[];
 let row=null;
 const pool={
  async query(sql,args){
   queries.push({sql,args});
   if(sql.startsWith('SELECT version FROM schema_migrations'))
     return {rows:[{version:3}]};
   if(sql.includes('SELECT payload,revision::text') && sql.includes("sticker_packs"))
     return {rows:row?[row]:[]};
   if(sql.includes('RETURNING payload,revision::text') && sql.includes("sticker_packs")){
    if(row && args[4]!==Number(row.revision))return {rows:[]};
    if(!row && args[4]!==0)return {rows:[]};
    row={payload:JSON.parse(args[3]),revision:String(Number(row?.revision||0)+1)};
    return {rows:[row]};
   }
   return {rows:[]};
  },async end(){}
 };
 const store=await createPgStore('postgres://unused',{poolFactory:()=>pool});
 try {
  assert.equal(store.stickerPacksReady(),true);
  assert.deepEqual(await store.getStickerPacks(owner),{pack_ids:[],revision:0});
  assert.deepEqual(await store.putStickerPacks(owner,{pack_ids:['njal'],revision:0}),
   {pack_ids:['njal'],revision:1});
  assert.equal(await store.putStickerPacks(owner,{pack_ids:[],revision:0}),null);
  assert.deepEqual((await store.getStickerPacks(owner)).pack_ids,['njal']);
  const write=queries.find(q=>q.sql.includes('RETURNING payload,revision::text'));
  assert.match(write.sql,/ON CONFLICT/);
  assert.match(write.sql,/WHERE notifications\.revision=\$5::bigint/);
  assert.deepEqual(write.args.slice(0,2),[issuer,owner.subject]);
  await store.list('notifications',owner);
  assert.match(queries.at(-1).sql,/sticker_packs/);
  assert.match(write.sql,/kind='sticker_packs'/);
 }finally{await store.close();}
});
