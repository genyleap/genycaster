import test from 'node:test';
import assert from 'node:assert/strict';
import {createAccountApi} from '../server.mjs';

const valid='Bearer allowed';
function networkFixture(resolver,assetsResolver=async fid=>({status:'verified',available:true,fid,wallets:[],addresses:[]})) {
 const calls=[];
 const fetchImpl=async (url,opts)=>{
   if(opts?.headers?.authorization!==valid)return new Response('{}',{status:401});
   if(url.endsWith('/account/profile'))return new Response(
     JSON.stringify({identity_id:'farcaster:42'}),{status:200});
   if(url.endsWith('/account/connections'))return new Response(
     JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}),{status:200});
   return new Response('{}',{status:404});
 };
 const store={
   bookmarkCollectionsReady:()=>true,
   async listBookmarkCollections(){return [];},
   async putBookmarkCollection(owner,change){calls.push(['write',owner.fid]);return {...change};},
   async listBookmarkMemberships(){return [];},
   async putBookmarkCollectionItem(owner,change){calls.push(['membership',owner.fid]);return change;}
 };
 const server=createAccountApi({catalogUrl:'https://example.com/providers',
   issuer:'https://example.com',fetchImpl,store,proResolver:resolver,assetsResolver});
 return {server,calls};
}
async function withServer(resolver,fn,assetsResolver) {
 const {server,calls}=networkFixture(resolver,assetsResolver);
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
 const request=(path,method='GET',body,headers={})=>
    fetch(base+path,{method,headers:{authorization:valid,
      'content-type':'application/json',...headers},
      ...(body?{body:JSON.stringify(body)}:{})});
 try {await fn(request,calls);}
 finally {await new Promise(resolve=>server.close(resolve));}
}
const collection={id:'11111111-2222-4333-8444-555555555555',
 action:'create',name:'Design'};
test('public status must never grant Pro without authenticated FID',async()=>{
 await withServer(async()=>({status:'verified',eligible:true}),async request=>{
   const denied=await request('entitlements','GET',undefined,{authorization:'Bearer invalid'});
   assert.equal(denied.status,401);
   const visible=await request('entitlements');
   assert.equal(visible.status,200);
   const status=(await visible.json()).entitlements;
   assert.equal(status.eligible,true);
   assert.equal((await request('entitlements','PUT',collection)).status,405);
 });
});
test('Basic users cannot mutate Pro bookmark collections',async()=>{
 const seen=[];
 await withServer(async fid=>{seen.push(fid);return {status:'verified',eligible:false};},
 async(request,calls)=>{
   assert.equal((await request('bookmark_collections','GET')).status,200);
   const denied=await request('bookmark_collections','PUT',collection);
   assert.equal(denied.status,403);
   assert.equal((await denied.json()).error,'geny_pro_required');
   assert.equal(calls.length,0);
   // A downgraded account must still be able to delete its own data.
   assert.equal((await request('bookmark_collections','PUT',
     {...collection,action:'delete',expected_revision:'1'})).status,200);
   assert.equal((await request('bookmark_collection_items','PUT',
     {collection_id:collection.id,cast_hash:'0x'+'ab'.repeat(20),remove:true})).status,200);
   assert.deepEqual(calls.map(c=>c[0]),['write','membership']);
   assert.deepEqual(seen,['42']);
 });
});
test('Pro authorization is bound to verified account and rechecked on each write',async()=>{
 let eligible=true;
 await withServer(async()=>({status:'verified',eligible}),async(request,calls)=>{
   assert.equal((await request('bookmark_collections','PUT',collection)).status,200);
   eligible=false;
   assert.equal((await request('bookmark_collections','PUT',collection)).status,403);
   assert.equal(calls.length,1);
 });
});
test('network verification failures fail closed with explicit status',async()=>{
 await withServer(async()=>({status:'unavailable',eligible:false}),
 async request=>{
   const denied=await request('bookmark_collections','PUT',collection);
   assert.equal(denied.status,403);
   assert.equal((await denied.json()).verification_status,'unavailable');
 });
});

test('My Assets requires signed account; no caller can supply a wallet/FID',async()=>{
 const seen=[];
 await withServer(async()=>({status:'verified',eligible:false}),async request=>{
   assert.equal((await request('geny/assets','GET',undefined,{authorization:'Bearer invalid'})).status,401);
   const response=await request('geny/assets?fid=999999');
   assert.equal(response.status,200);
   assert.equal((await response.json()).holding.fid,'42');
   assert.deepEqual(seen,['42']);
   assert.equal((await request('geny/assets','PUT',{wallet:'0xdead'})).status,405);
 },async fid=>{seen.push(fid);return {status:'verified',available:true,fid,wallets:[],addresses:[]};});
});
