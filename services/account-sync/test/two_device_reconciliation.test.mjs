import assert from 'node:assert/strict';
import test from 'node:test';
import {createAccountApi} from '../server.mjs';

const issuer='https://identity.example.test';
const normal='Bearer normal-owner';
const recovery='Bearer recovery.'+'R'.repeat(43);
const stranger='Bearer normal-stranger';
const bad='Bearer recovery.'+'Z'.repeat(43);
const hash='0x'+'ab'.repeat(20);
const mutation1='b3d992e2-a5a2-4753-9388-cc15363a4e65';
const mutation2='7d88be72-cc3d-47d4-92d9-21cf2868011a';

function ownerStore() {
  const changes=new Map();
  const mutations=new Map();
  const profiles=new Map();
  let sequence=0;
  const key=owner=>owner.issuer+':'+owner.subject;
  return {
    async listUpdates(){return [];},
    async listBookmarkChanges(owner,cursor){
      return (changes.get(key(owner))||[]).filter(e=>Number(e.sequence)>Number(cursor)).slice(0,200);
    },
    async getProfileExtras(owner){return profiles.get(key(owner))||null;},
    async putProfileExtras(owner,item){
      const k=key(owner), previous=profiles.get(k);
      if((previous?.revision||0)!==item.revision) return null;
      const saved={...item,revision:item.revision+1};
      profiles.set(k,saved);
      return saved;
    },
    async put(entity,owner,item){
      if(entity!=='bookmarks') throw Error('unexpected write '+entity);
      const k=key(owner);
      let ids=mutations.get(k);
      if(!ids){ids=new Map();mutations.set(k,ids);}
      if(ids.has(item.mutation_id)){
        const earlier=ids.get(item.mutation_id);
        return earlier.cast_hash===item.cast_hash && earlier.is_deleted===item.is_deleted
          ? earlier : null;
      }
      const saved={...item,sequence:String(++sequence)};
      ids.set(item.mutation_id,saved);
      changes.set(k,[...(changes.get(k)||[]),saved]);
      return saved;
    }
  };
}

function trustedVerifier(url,options){
  const authorization=options.headers.authorization;
  if(url.endsWith('/account/profile')){
    if(authorization===normal) return Promise.resolve(new Response(
      JSON.stringify({identity_id:'openproof-member-123'}),{status:200}));
    if(authorization===stranger) return Promise.resolve(new Response(
      JSON.stringify({identity_id:'openproof-member-999'}),{status:200}));
    return Promise.resolve(new Response('{}',{status:401}));
  }
  if(url.endsWith('/account/connections')){
    const fid=authorization===normal?'123':authorization===stranger?'999':null;
    return Promise.resolve(new Response(JSON.stringify({
      connections:fid?[{provider:'farcaster',fid}]:[]
    }),{status:fid?200:401}));
  }
  if(url==='https://storage-farcaster.genyleap.com/v1/browser/openproof/connections'){
    const verified=authorization===recovery;
    return Promise.resolve(new Response(JSON.stringify({
      connections:verified?[{provider:'farcaster',fid:'123'}]:[]
    }),{status:verified?200:401}));
  }
  throw Error('untrusted endpoint '+url);
}

test('two signed-in devices share owner state automatically, repeat writes are idempotent and other FIDs are isolated',async()=>{
  const service=createAccountApi({
    catalogUrl:issuer+'/auth/providers',issuer,store:ownerStore(),fetchImpl:trustedVerifier
  });
  await new Promise(resolve=>service.listen(0,'127.0.0.1',resolve));
  const origin='http://127.0.0.1:'+service.address().port+'/api/v1/cloud/';
  const request=(path,auth,method='GET',value)=>fetch(origin+path,{
    method,headers:{authorization:auth,'content-type':'application/json'},
    ...(value?{body:JSON.stringify(value)}:{})
  });
  try{
    const saved=await request('bookmarks',normal,'PUT',{cast_hash:hash,is_deleted:false,mutation_id:mutation1});
    assert.equal(saved.status,200);
    const deviceB=await request('bookmarks?since=0',recovery);
    assert.equal(deviceB.status,200);
    const initial=await deviceB.json();
    assert.equal(initial.items.length,1);
    assert.equal(initial.items[0].cast_hash,hash);
    const duplicate=await request('bookmarks',normal,'PUT',{cast_hash:hash,is_deleted:false,mutation_id:mutation1});
    assert.equal(duplicate.status,200);
    const foreign=await request('bookmarks?since=0',stranger);
    assert.equal(foreign.status,200);
    assert.equal((await foreign.json()).items.length,0);
    assert.equal((await request('bookmarks?since=0',bad)).status,401);
    const deleted=await request('bookmarks',recovery,'PUT',{cast_hash:hash,is_deleted:true,mutation_id:mutation2});
    assert.equal(deleted.status,200);
    const backOnA=await request('bookmarks?since='+initial.next_cursor,normal);
    assert.equal(backOnA.status,200);
    assert.equal((await backOnA.json()).items[0].is_deleted,true);

    const profile={category:'Technology',profession:'Developer',
      links:[{label:'Site',url:'https://genyleap.com'}],revision:0};
    assert.equal((await request('profile_extras',normal,'PUT',profile)).status,200);
    const mirror=await request('profile_extras',recovery);
    assert.equal(mirror.status,200);
    assert.equal((await mirror.json()).item.links[0].url,'https://genyleap.com');
    assert.equal((await request('profile_extras',recovery,'PUT',profile)).status,409);
    const unrelated=await request('profile_extras',stranger);
    assert.equal((await unrelated.json()).item,null);
  }finally{
    await new Promise(resolve=>service.close(resolve));
  }
});
