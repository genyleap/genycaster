import test from 'node:test';
import assert from 'node:assert/strict';
import {generateKeyPairSync,sign} from 'node:crypto';
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
