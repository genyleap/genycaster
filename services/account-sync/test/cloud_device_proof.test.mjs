import test from 'node:test';
import assert from 'node:assert/strict';
import {generateKeyPairSync,sign} from 'node:crypto';
import {createCloudDeviceProof,cloudDeviceMessage,custodyEnrollmentMessage} from '../cloud_device_proof.mjs';
import {deriveFarcasterCustody} from '../../../ui/wasm/farcaster_recovery.mjs';

const issuer='https://openproof.genyleap.com';
const hypersnapOrigin='https://hypersnap.example';
const principal={fid:'42',issuer,subject:'farcaster:42',authorization:'Bearer verified'};
const keyPair=()=>generateKeyPairSync('ed25519');
const publicHex=k=>k.publicKey.export({format:'der',type:'spki'}).subarray(-32).toString('hex');
const signed=(challenge,key)=>({...challenge,
  signature:sign(null,Buffer.from(cloudDeviceMessage(challenge)),key.privateKey).toString('hex')});
function fixture(fetchImpl=()=>{throw Error('social registry must not be contacted');}){
  const devices=new Set();
  const id=(owner,key)=>owner.issuer+'|'+owner.subject+'|'+key;
  const store={
    async enrollCloudDevice(owner,key){devices.add(id(owner,key));},
    async touchCloudDevice(owner,key){return devices.has(id(owner,key));},
  };
  let time=1000000;
  const proof=createCloudDeviceProof({store,issuer,hypersnapOrigin,fetchImpl,now:()=>time});
  return {proof,devices,advance:ms=>time+=ms};
}
function challenge(proof,key,purpose='restore',network='farcaster',fid='42'){
  return proof.issue({purpose,network,fid,public_key:publicHex(key)});
}
test('only verified OpenProof grants can enroll, and a social key alone cannot restore',async()=>{
  const {proof,devices}=fixture(),key=keyPair();
  await assert.rejects(proof.prove(signed(challenge(proof,key),key)),/device_not_enrolled/);
  await assert.rejects(proof.prove(signed(challenge(proof,key,'enroll'),key),{
    ...principal,cookieAuth:true}),/invalid_device_proof/);
  await assert.rejects(proof.prove(signed(challenge(proof,key,'enroll'),key),{
    ...principal,subject:'farcaster:43'}),/invalid_device_proof/);
  assert.equal(devices.size,0);
  const enrolled=await proof.prove(signed(challenge(proof,key,'enroll'),key),principal);
  assert.equal(enrolled.deviceKey,publicHex(key));
  assert.equal(enrolled.authSource,'device');
  const restored=await proof.prove(signed(challenge(proof,key),key));
  assert.equal(restored.subject,principal.subject);
  devices.clear();
  await assert.rejects(proof.prove(signed(challenge(proof,key),key)),/device_not_enrolled/);
});
test('nonce replay, principal changes, bad signatures and expiry fail closed',async()=>{
  const {proof,advance}=fixture(),key=keyPair();
  const first=signed(challenge(proof,key,'enroll'),key);
  await proof.prove(first,principal);
  await assert.rejects(proof.prove(first,principal),/invalid_device_proof/);
  const changed=signed(challenge(proof,key),key);
  await assert.rejects(proof.prove({...changed,fid:'43'}),/invalid_device_proof/);
  await assert.rejects(proof.prove(changed),/invalid_device_proof/);
  await assert.rejects(proof.prove(signed(challenge(proof,key),keyPair())),/invalid_device_proof/);
  const expired=signed(challenge(proof,key),key);
  advance(90_001);
  await assert.rejects(proof.prove(expired),/invalid_device_proof/);
  assert.throws(()=>challenge(proof,key,'enroll','hypersnap','42'),/invalid_device_challenge/);
});
test('HyperSnap enrollment requires real custody signature and exact registry FID',async()=>{
  const phrase='abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about';
  const custody=await deriveFarcasterCustody(phrase);
  const fid='9007199254740993',key=keyPair();
  let registryCalls=0;
  try {
    const {proof}=fixture(async url=>{
      registryCalls++;
      assert.equal(url.origin,hypersnapOrigin);
      assert.equal(url.pathname,'/v1/nativeAccount');
      assert.equal(url.searchParams.get('custody'),custody.address);
      return Response.json({fid,custodyAddress:custody.address,blockNumber:'123',message:{}});
    });
    const c=challenge(proof,key,'enroll','hypersnap',fid);
    await assert.rejects(proof.prove({...signed(c,key),custody_address:custody.address,
      custody_signature:'0x'+'00'.repeat(65)}),/invalid_device_proof/);
    assert.equal(registryCalls,0);
    const valid=challenge(proof,key,'enroll','hypersnap',fid);
    const signature=await custody.signMessage(custodyEnrollmentMessage(valid,custody.address));
    const result=await proof.prove({...signed(valid,key),custody_address:custody.address,
      custody_signature:signature});
    assert.equal(result.subject,'hypersnap:'+fid);
    assert.equal(registryCalls,1);
    assert.equal((await proof.prove(signed(challenge(proof,key,'restore','hypersnap',fid),key))).fid,fid);
    assert.equal(registryCalls,1,'routine cloud restoration does not query the social registry');
  }finally{custody.dispose();}
});
