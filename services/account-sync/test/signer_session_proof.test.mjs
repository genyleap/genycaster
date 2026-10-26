import assert from 'node:assert/strict';
import test from 'node:test';
import {generateKeyPairSync,sign} from 'node:crypto';
import {createAccountApi} from '../server.mjs';

const ORIGIN='https://caster.genyleap.com';
const issuer='https://openproof.genyleap.com';
const fid='321969';
const keys=generateKeyPairSync('ed25519');
const pub=keys.publicKey.export({format:'der',type:'spki'}).subarray(-32).toString('hex');
const ownerKey=o=>o.issuer+'/'+o.subject;
const nonceMessage=nonce=>Buffer.from('genycaster-account-session-v1\n'+fid+'\n'+nonce);
const signerRow=(overrides={})=>({fid,key:'0x'+pub,keyType:1,
  source:'SIGNER_SOURCE_OFFCHAIN',scopes:['MESSAGE_TYPE_CAST_ADD'],
  ...overrides});
function storeStub() {
  const sessions=new Map(),profiles=new Map(),drafts=new Map();
  return {
    sessions,profiles,drafts,
    async getDraft(owner,id){return drafts.get(ownerKey(owner)+'/'+id)||null;},
    async put(entity,owner,item) {
      if(entity!=='drafts')throw Error('unexpected item');
      const key=ownerKey(owner)+'/'+item.id;
      const prior=drafts.get(key);
      if(item.revision!==(prior?.revision||0))return null;
      const next={...item,revision:item.revision+1,deleted_at:null};
      drafts.set(key,next);return next;
    },
    async getSession(digest){return sessions.get(digest)||null;},
    async createSession(owner,digest,csrf){sessions.set(digest,{...owner,csrf_token:csrf});},
    async revokeSession(digest){sessions.delete(digest);},
    async getProfileExtras(owner){return profiles.get(ownerKey(owner))||null;},
    async putProfileExtras(owner,value){
      const key=ownerKey(owner),previous=profiles.get(key);
      if(value.revision!==(previous?.revision||0))return null;
      const next={...value,revision:value.revision+1};
      profiles.set(key,next);return next;
    }
  };
}
function registry(signer=signerRow()) {
  return async url=>{
    assert.equal(new URL(url).hostname,'snapchain.genyleap.com');
    assert.equal(new URL(url).searchParams.get('fid'),fid);
    return new Response(JSON.stringify({signers:signer?[signer]:[]}),{
      status:200,headers:{'content-type':'application/json'}});
  };
}
async function runCase(signers) {
  const store=storeStub();
  const server=createAccountApi({catalogUrl:issuer+'/catalog',issuer,store,
    fetchImpl:registry(signers)});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
  const req=(path,method='GET',body,headers={})=>fetch(base+path,{
    method,headers:{'content-type':'application/json',...headers},
    ...(body!==undefined?{body:JSON.stringify(body)}:{})
  });
  return {store,req,close:()=>new Promise(resolve=>server.close(resolve))};
}
const origin={'origin':ORIGIN,'sec-fetch-site':'same-origin'};
const cookieFrom=r=>(r.headers.get('set-cookie')||'').split(';')[0];

test('registered signer creates distinct sessions for two devices sharing one owner',async()=>{
  const ctx=await runCase(signerRow());
  try {
    async function device() {
      const challenge=await ctx.req('session/challenge','POST',{fid},origin);
      assert.equal(challenge.status,200);
      const {nonce}=await challenge.json();
      const signature=sign(null,nonceMessage(nonce),keys.privateKey).toString('hex');
      const verified=await ctx.req('session/proof','POST',{
        fid,nonce,public_key:pub,signature},origin);
      assert.equal(verified.status,200);
      const json=await verified.json();
      assert.equal(json.fid,fid);
      return {cookie:cookieFrom(verified),csrf:json.csrf_token,nonce,signature};
    }
    const phone=await device(),desktop=await device();
    assert.notEqual(phone.cookie,desktop.cookie);
    assert.equal(ctx.store.sessions.size,2);
    const profile={category:'Technology',profession:'Engineer',links:[],revision:0};
    const saved=await ctx.req('profile_extras','PUT',profile,{
      cookie:phone.cookie,'x-genycaster-csrf':phone.csrf});
    assert.equal(saved.status,200);
    const other=await ctx.req('profile_extras','GET',undefined,{cookie:desktop.cookie});
    assert.equal((await other.json()).item.profession,'Engineer');
    const draft={
      id:'00000000-0000-4000-8000-000000000002',
      kind:'cast',title:'',revision:0,
      content:{document:{blocks:[{text:'Hello, Qt!'}]}}
    };
    assert.equal((await ctx.req('drafts','PUT',draft,{
      cookie:phone.cookie,'x-genycaster-csrf':phone.csrf})).status,200);
    const acrossDevices=await ctx.req('drafts?id='+draft.id,'GET',undefined,{
      cookie:desktop.cookie});
    assert.equal(acrossDevices.status,200);
    const loaded=await acrossDevices.json();
    assert.match(JSON.stringify(loaded.items[0].content),/Hello, Qt!/);
    assert.equal((await ctx.req('session/proof','POST',{
      fid,nonce:phone.nonce,public_key:pub,signature:phone.signature},origin)).status,401);
    assert.equal((await ctx.req('profile_extras','PUT',profile,{
      cookie:phone.cookie,'x-genycaster-csrf':desktop.csrf})).status,401);
    assert.equal((await ctx.req('session','GET',undefined,{cookie:phone.cookie})).status,200);
  } finally {await ctx.close();}
});

test('challenge cannot be issued cross-origin or replayed',async()=>{
  const ctx=await runCase(signerRow());
  try {
    assert.equal((await ctx.req('session/challenge','POST',{fid},
      {'origin':'https://evil.example','sec-fetch-site':'cross-site'})).status,403);
    const response=await ctx.req('session/challenge','POST',{fid},origin);
    assert.equal(response.status,200);
    const {nonce}=await response.json();
    const signature=sign(null,nonceMessage(nonce),keys.privateKey).toString('hex');
    assert.equal((await ctx.req('session/proof','POST',{
      fid,nonce,public_key:pub,signature:'0'.repeat(128)},origin)).status,401);
    assert.equal((await ctx.req('session/proof','POST',{
      fid,nonce,public_key:pub,signature},origin)).status,401);
    assert.equal(ctx.store.sessions.size,0);
  }finally{await ctx.close();}
});

test('unregistered, expired or wrong-scoped signer cannot create sessions',async()=>{
  const variants=[
    null,signerRow({fid:'999'}),
    signerRow({scopes:['MESSAGE_TYPE_REACTION_ADD']}),
    signerRow({source:'SIGNER_SOURCE_OFFCHAIN',expiresAt:1}),
  ];
  for(const signer of variants){
    const ctx=await runCase(signer);
    try {
      const challenge=await ctx.req('session/challenge','POST',{fid},origin);
      const {nonce}=await challenge.json();
      const signature=sign(null,nonceMessage(nonce),keys.privateKey).toString('hex');
      assert.equal((await ctx.req('session/proof','POST',{
        fid,nonce,public_key:pub,signature},origin)).status,401);
      assert.equal(ctx.store.sessions.size,0);
    }finally{await ctx.close();}
  }
});
