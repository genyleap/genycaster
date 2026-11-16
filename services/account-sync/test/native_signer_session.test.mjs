import test from 'node:test';
import assert from 'node:assert/strict';
import {generateKeyPairSync,sign} from 'node:crypto';
import {createServer} from 'node:http';
import {createCloudHandler} from '../cloud_state.mjs';
import {createSignerSessionProof} from '../signer_session_proof.mjs';

const nativeFid='4294967296';
const farcasterFid='321969';
const {publicKey,privateKey}=generateKeyPairSync('ed25519');
const key=publicKey.export({format:'der',type:'spki'}).subarray(-32).toString('hex');
const now=Date.parse('2026-11-16T12:00:00Z');
function signer(originToTrust, signerScope=['MESSAGE_TYPE_CAST_ADD']) {
  const requests=[];
  const fetchImpl=async url=>{
    const actual=new URL(url);
    requests.push(actual);
    assert.equal(actual.origin,originToTrust);
    assert.equal(actual.pathname,'/v1/signersByFid');
    return new Response(JSON.stringify({signers:[{
      fid:actual.searchParams.get('fid'),key,keyType:1,
      source:'SIGNER_SOURCE_OFFCHAIN',scopes:signerScope,
      expiresAt:Math.floor(now/1000)-1609459200+600,
    }]}),{status:200,headers:{'content-type':'application/json'}});
  };
  return {fetchImpl,requests};
}
function proof(fid,fetchImpl){
  const verifier=createSignerSessionProof({
    issuer:'https://openproof.genyleap.com',snapchainOrigin:'https://snapchain.genyleap.com',
    hypersnapOrigin:'https://haatz.quilibrium.com',fetchImpl,now:()=>now,
  });
  const {nonce}=verifier.issue(fid);
  const bytes=Buffer.from('genycaster-account-session-v1\n'+fid+'\n'+nonce);
  return verifier.prove({fid,nonce,public_key:key,
    signature:sign(null,bytes,privateKey).toString('hex')});
}
test('Native HyperSnap account session proves active native Signer on HyperSnap only',async()=>{
  const registry=signer('https://haatz.quilibrium.com');
  const owner=await proof(nativeFid,registry.fetchImpl);
  assert.deepEqual(owner,{issuer:'hypersnap:native-registry',
    subject:'hypersnap:'+nativeFid,fid:nativeFid});
  assert.equal(registry.requests.length,1);
});
test('Farcaster account session remains bound to Snapchain',async()=>{
  const registry=signer('https://snapchain.genyleap.com');
  const owner=await proof(farcasterFid,registry.fetchImpl);
  assert.deepEqual(owner,{issuer:'https://openproof.genyleap.com',
    subject:'farcaster:'+farcasterFid,fid:farcasterFid});
  assert.equal(registry.requests.length,1);
});
test('Native account requires CastAdd scope and rejects wrong-key registration',async()=>{
  const registry=signer('https://haatz.quilibrium.com',['MESSAGE_TYPE_REACTION_ADD']);
  await assert.rejects(proof(nativeFid,registry.fetchImpl),/unregistered_signer/);
});

test('Native HyperSnap account cookie restores verified Streak and rejects cross-account writes', async () => {
  const sessionTokens=new Map();
  const events=[];
  const origin='https://caster.genyleap.com';
  let lastOwner;
  const store={
    async createSession(owner,digest,csrf) {
      sessionTokens.set(digest,{...owner,csrf_token:csrf});
    },
    async getSession(digest) {return sessionTokens.get(digest)||null;},
    streakReady:()=>true,
    async listStreakEvents(account) {
      assert.equal(account.issuer,'hypersnap:native-registry');
      assert.equal(account.subject,'hypersnap:'+nativeFid);
      return [...events];
    },
    async recordStreakCast(account,hash,kind,day) {
      lastOwner=account;
      if(events.some(e=>e.hash===hash))return {recorded:false};
      events.push({hash,kind,day});return {recorded:true};
    },
  };
  const fetchImpl=async(url)=>{
    const parsed=new URL(url);
    assert.equal(parsed.origin,'https://haatz.quilibrium.com');
    assert.equal(parsed.pathname,'/v1/signersByFid');
    assert.equal(parsed.searchParams.get('fid'),nativeFid);
    return new Response(JSON.stringify({signers:[{
      fid:nativeFid,key,keyType:1,source:'SIGNER_SOURCE_OFFCHAIN',
      scopes:['MESSAGE_TYPE_CAST_ADD'],
      expiresAt:Math.floor(Date.now()/1000)-1609459200+600,
    }]}),{status:200});
  };
  const handler=createCloudHandler({
    store,issuer:'https://openproof.genyleap.com',
    publicOrigin:origin,
    snapchainOrigin:'https://snapchain.genyleap.com',
    hypersnapOrigin:'https://haatz.quilibrium.com',
    fetchImpl,discoverStreakCasts:async()=>[],
    verifyStreakCast:async(_hash,fid)=> {
      assert.equal(fid,nativeFid);
      return {kind:'cast',day:new Date().toISOString().slice(0,10)};
    },
  });
  const server=createServer((req,res)=>
    handler(req,res,new URL(req.url,'http://localhost').pathname));
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
  try {
    const post=async(path,data)=>fetch(base+path,{
      method:'POST',
      headers:{origin,'content-type':'application/json'},
      body:JSON.stringify(data),
    });
    const issued=await post('session/challenge',{fid:nativeFid});
    assert.equal(issued.status,200);
    const {nonce}=await issued.json();
    const bytes=Buffer.from('genycaster-account-session-v1\n'+nativeFid+'\n'+nonce);
    const confirmed=await post('session/proof',{
      fid:nativeFid,nonce,public_key:key,
      signature:sign(null,bytes,privateKey).toString('hex'),
    });
    assert.equal(confirmed.status,200);
    const cookie=confirmed.headers.get('set-cookie').split(';')[0];
    const challengeResult=await confirmed.json();
    assert.equal(challengeResult.fid,nativeFid);
    assert.equal(typeof challengeResult.csrf_token,'string');

    const session=await fetch(base+'session',{headers:{cookie}});
    assert.equal(session.status,200,'HyperSnap must survive session restore');
    assert.equal((await session.json()).fid,nativeFid);
    const first=await fetch(base+'streak',{headers:{cookie}});
    assert.equal(first.status,200,'HyperSnap must read its streak');
    await first.arrayBuffer();

    const cast_hash='0x'+'ab'.repeat(20);
    const rejected=await fetch(base+'streak/cast',{
      method:'PUT',headers:{cookie,'content-type':'application/json'},
      body:JSON.stringify({cast_hash}),
    });
    assert.equal(rejected.status,401,'cookie without CSRF must not write');
    await rejected.arrayBuffer();
    const recorded=await fetch(base+'streak/cast',{
      method:'PUT',headers:{cookie,
        'x-genycaster-csrf':challengeResult.csrf_token,
        'content-type':'application/json'},
      body:JSON.stringify({cast_hash}),
    });
    assert.equal(recorded.status,200);
    assert.equal((await recorded.json()).streak.activity.total_days,1);
    assert.equal(lastOwner.issuer,'hypersnap:native-registry');
    assert.equal(lastOwner.subject,'hypersnap:'+nativeFid);
    const restored=await fetch(base+'streak',{headers:{cookie}});
    assert.equal(restored.status,200);
    assert.equal((await restored.json()).streak.activity.total_days,1);
  }finally {
    // Undici retains pooled sockets across test files. Drain/cancel all
    // responses and close open keep-alive connections before server shutdown.
    server.closeAllConnections();
    await new Promise(resolve=>server.close(resolve));
  }
});
