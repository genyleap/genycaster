// Independently enrolled app device credentials; no social signer registry lookup.
import {createPublicKey,randomBytes,verify} from 'node:crypto';
import {isCanonicalUint64Fid,FIRST_NATIVE_HYPERSNAP_FID} from './signer_session_proof.mjs';
import {verifyProWalletSignature} from './pro_wallet_proof.mjs';

const HEX32=/^[0-9a-f]{64}$/;
const HEX64=/^[0-9a-f]{128}$/;
const NONCE=/^[A-Za-z0-9_-]{43}$/;
const ADDRESS=/^0x[0-9a-f]{40}$/;
const DER=Buffer.from('302a300506032b6570032100','hex');
const TTL=90_000;
const LIMIT=1024;
export const CLOUD_DEVICE_PREFIX='genycaster-cloud-device-v2';
// A revocation receipt is deliberately replayable, not an authentication proof.
// The server retains a permanent key tombstone so replay cannot revoke a new key.
export const REVOCATION_NONCE='A'.repeat(43);
const denied=(status=401,message='invalid_device_proof')=>Object.assign(new Error(message),{status});

export function cloudDeviceMessage({purpose,network,fid,public_key,nonce}) {
  return [CLOUD_DEVICE_PREFIX,purpose,network,fid,public_key,nonce].join('\n');
}
export function custodyEnrollmentMessage(challenge,address) {
  return ['Genycaster — authorize private cloud device',
    'This authorizes private Genycaster data on this device, not social signing or a transaction.',
    'Origin: https://genycaster.xyz',cloudDeviceMessage(challenge),
    'Custody: '+address].join('\n');
}
function validPrincipal(network,fid) {
  return isCanonicalUint64Fid(fid) && (
    network==='farcaster' && Number.isSafeInteger(Number(fid)) ||
    network==='hypersnap' && BigInt(fid)>=FIRST_NATIVE_HYPERSNAP_FID);
}
function ownerFor(network,fid,issuer) {
  return {fid,issuer:network==='farcaster'?issuer:'hypersnap:native-registry',
    subject:network+':'+fid};
}
export function createCloudDeviceProof({store,issuer,hypersnapOrigin,fetchImpl=globalThis.fetch,now=Date.now}) {
  const pending=new Map();
  function prune(){
    for(const [nonce,row] of pending)if(row.expires<=now())pending.delete(nonce);
  }
  function issue(input) {
    const {purpose,network,fid,public_key}=input||{};
    if(!['enroll','restore'].includes(purpose)||!validPrincipal(network,fid)||
       typeof public_key!=='string'||!HEX32.test(public_key))throw denied(400,'invalid_device_challenge');
    prune();
    if(pending.size>=LIMIT)throw denied(429,'challenge_limit');
    const nonce=randomBytes(32).toString('base64url');
    const challenge={purpose,network,fid,public_key,nonce};
    pending.set(nonce,{challenge,expires:now()+TTL});
    return {...challenge,expires_in:90};
  }
  async function verifyNativeCustody(challenge,address,signature) {
    if(typeof address!=='string'||!ADDRESS.test(address)||
       !verifyProWalletSignature(custodyEnrollmentMessage(challenge,address),signature,address))
      throw denied();
    let response,body;
    try {
      const url=new URL(hypersnapOrigin+'/v1/nativeAccount');
      url.searchParams.set('custody',address);
      response=await fetchImpl(url,{redirect:'error',headers:{accept:'application/json'},
        signal:AbortSignal.timeout(6000)});
      if(!response.ok)throw Error('registry');
      const raw=await response.text();
      if(raw.length>32768)throw Error('registry');
      body=JSON.parse(raw);
    }catch {throw denied(503,'custody_registry_unavailable');}
    if(body?.fid!==challenge.fid||body?.custodyAddress?.toLowerCase()!==address||
       !body.message||typeof body.message!=='object'||Array.isArray(body.message)||
       !/^[1-9][0-9]*$/.test(String(body.blockNumber||'')))throw denied();
  }
  async function prove(input,authenticatedOwner=null) {
    const {nonce,signature}=input||{};
    if(typeof nonce!=='string'||!NONCE.test(nonce))throw denied();
    prune();
    const row=pending.get(nonce);
    pending.delete(nonce);
    if(!row||row.expires<=now()||typeof signature!=='string'||!HEX64.test(signature))throw denied();
    const challenge=row.challenge;
    if(['purpose','network','fid','public_key'].some(key=>input[key]!==challenge[key]))throw denied();
    const key=createPublicKey({key:Buffer.concat([DER,Buffer.from(challenge.public_key,'hex')]),
      format:'der',type:'spki'});
    if(!verify(null,Buffer.from(cloudDeviceMessage(challenge)),key,Buffer.from(signature,'hex')))throw denied();
    const owner=ownerFor(challenge.network,challenge.fid,issuer);
    if(challenge.purpose==='enroll') {
      if(challenge.network==='farcaster') {
        if(!authenticatedOwner||authenticatedOwner.cookieAuth||!authenticatedOwner.authorization||
           authenticatedOwner.issuer!==owner.issuer||authenticatedOwner.subject!==owner.subject)
          throw denied();
      }else await verifyNativeCustody(challenge,input.custody_address,input.custody_signature);
      await store.enrollCloudDevice(owner,challenge.public_key);
    }else if(!await store.touchCloudDevice(owner,challenge.public_key))throw denied(401,'device_not_enrolled');
    return {...owner,deviceKey:challenge.public_key,authSource:'device'};
  }
  async function revoke(input) {
    const {purpose,network,fid,public_key,nonce,signature}=input||{};
    if(purpose!=='revoke'||nonce!==REVOCATION_NONCE||!validPrincipal(network,fid)||
      typeof public_key!=='string'||!HEX32.test(public_key)||
      typeof signature!=='string'||!HEX64.test(signature))throw denied();
    const key=createPublicKey({key:Buffer.concat([DER,Buffer.from(public_key,'hex')]),
      format:'der',type:'spki'});
    if(!verify(null,Buffer.from(cloudDeviceMessage(input)),key,Buffer.from(signature,'hex')))throw denied();
    if(!await store.revokeCloudDevice(ownerFor(network,fid,issuer),public_key))
      throw denied(409,'device_enrollment_pending');
    return {revoked:true};
  }
  return {issue,prove,revoke};
}
