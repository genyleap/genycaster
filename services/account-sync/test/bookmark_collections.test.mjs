import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createAccountApi} from '../server.mjs';
import {validateBookmarkCollectionChange,
        validateBookmarkCollectionItem} from '../cloud_state.mjs';

const id='11111111-2222-4333-8444-555555555555';
const hash='0x'+'ab'.repeat(20);
const calls=[];
const store={
  bookmarkCollectionsReady:()=>true,
  async listBookmarkCollections(owner){
    calls.push(['list',owner.subject]);
    return [{id,name:'Inspiration',revision:'1',item_count:1}];
  },
  async putBookmarkCollection(owner,change){
    calls.push(['update',owner.subject,change]);
    return change.action==='delete'?{id:change.id}:
      {id:change.id,name:change.name,revision:'1'};
  },
  async listBookmarkMemberships(owner){
    calls.push(['memberships',owner.subject]);
    return [{cast_hash:hash,collection_id:id}];
  },
  async listBookmarkCollectionItems(owner,collectionId){
    calls.push(['items',owner.subject,collectionId]);
    return [{cast_hash:hash}];
  },
  async putBookmarkCollectionItem(owner,change){
    calls.push(['item_write',owner.subject,change]);
    return {collection_id:change.collection_id,cast_hash:change.cast_hash,
            removed:change.remove};
  }
};
const fetchImpl=async(url,init)=>{
  const bearer=init?.headers?.authorization;
  const fid=bearer==='Bearer valid1'?'42':bearer==='Bearer valid2'?'43':null;
  if(fid===null) return new Response('{}',{status:401});
  return url.endsWith('/account/profile')
    ?new Response(JSON.stringify({identity_id:'farcaster:'+fid}),{status:200})
    :new Response(JSON.stringify({connections:[{provider:'farcaster',fid}]}),{status:200});
};
async function withServer(fn) {
  const server=createAccountApi({catalogUrl:'https://id.example.test/catalog',
    fetchImpl,store,issuer:'https://id.example.test',
    proResolver:async()=>({status:'verified',eligible:true})});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
  const request=(path,method='GET',data,bearer)=>
    fetch(base+path,{method,headers:{'content-type':'application/json',
      ...(bearer?{authorization:bearer}:{})},
      ...(data===undefined?{}:{body:JSON.stringify(data)})});
  try{await fn(request);}
  finally{await new Promise(resolve=>server.close(resolve));}
}

test('personal bookmark collection payloads are bounded and name-safe',()=>{
  assert.deepEqual(validateBookmarkCollectionChange({id,action:'create',name:'ایده‌های طراحی'}),
    {id,action:'create',name:'ایده‌های طراحی'});
  assert.deepEqual(validateBookmarkCollectionChange({id,action:'rename',
    name:'Photography',expected_revision:'3'}),
    {id,action:'rename',name:'Photography',expected_revision:'3'});
  for(const raw of [{id,action:'create',name:' '},
    {id,action:'create',name:' Bad'},{id,action:'create',name:'a'+String.fromCharCode(10)+'b'},
    {id,action:'rename',name:'X',expected_revision:'0'},
    {id:'bad',action:'create',name:'A'},
    {id,action:'delete',expected_revision:-1}])
    assert.equal(validateBookmarkCollectionChange(raw),null);
  assert.deepEqual(validateBookmarkCollectionItem({collection_id:id,
    cast_hash:hash.toUpperCase(),remove:false}),
    {collection_id:id,cast_hash:hash,remove:false});
  assert.equal(validateBookmarkCollectionItem({collection_id:id,
    cast_hash:hash,remove:'false'}),null);
});

test('bookmark collections and memberships are private to the verified owner',async()=>{
  calls.length=0;
  await withServer(async req=>{
    assert.equal((await req('bookmark_collections')).status,401);
    assert.equal((await req('bookmark_collection_items?collection_id='+id)).status,401);
    assert.equal((await req('bookmark_collections','GET',undefined,'Bearer forged')).status,401);
    const one='Bearer valid1',two='Bearer valid2';
    assert.equal((await req('bookmark_collections','GET',undefined,one)).status,200);
    const memberships=await req('bookmark_collection_items','GET',undefined,one);
    assert.equal(memberships.status,200);
    assert.deepEqual((await memberships.json()).items,[{cast_hash:hash,collection_id:id}]);
    assert.equal((await req('bookmark_collections','PUT',
      {id,action:'create',name:'Inspiration'},one)).status,200);
    assert.equal((await req('bookmark_collections','PUT',
      {id,action:'rename',name:'Ideas',expected_revision:'1'},two)).status,200);
    assert.equal((await req('bookmark_collection_items?collection_id='+id,
      'GET',undefined,one)).status,200);
    assert.equal((await req('bookmark_collection_items','PUT',
      {collection_id:id,cast_hash:hash,remove:false},one)).status,200);
    assert.equal((await req('bookmark_collection_items','PUT',
      {collection_id:id,cast_hash:'invalid',remove:false},one)).status,400);
    assert.equal((await req('bookmark_collection_items?collection_id=invalid',
      'GET',undefined,one)).status,400);
    assert.equal((await req('bookmark_collections','POST',{},one)).status,405);
    assert.deepEqual(calls.filter(c=>c[0]==='list').map(c=>c[1]),['farcaster:42']);
    assert.ok(calls.some(c=>c[0]==='update' && c[1]==='farcaster:43'));
    assert.ok(calls.some(c=>c[0]==='item_write' && c[1]==='farcaster:42'));
    assert.ok(calls.some(c=>c[0]==='memberships' && c[1]==='farcaster:42'));
  });
});

test('collection migration is owner-scoped, grants least-privilege and leaves Cast topics separate',()=>{
  const sql=readFileSync(new URL('../migrations/007_bookmark_collections.sql',
    import.meta.url),'utf8');
  assert.match(sql,/CREATE TABLE IF NOT EXISTS bookmark_collections/);
  assert.match(sql,/CREATE TABLE IF NOT EXISTS bookmark_collection_items/);
  assert.match(sql,/FOREIGN KEY \(issuer,subject,cast_hash\)/);
  assert.match(sql,/ON DELETE CASCADE/);
  assert.match(sql,/genycaster_prune_unsaved_collection_items/);
  assert.doesNotMatch(sql,/\bALTER TABLE cast_topics\b/);
});
