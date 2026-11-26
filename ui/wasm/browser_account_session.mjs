// A background, same-origin Genycaster account sign-in using the already
// registered Farcaster signer. The Ed25519 CryptoKey never leaves IndexedDB.
// No manual account-data transfer and no OAuth bearer in browser storage.
import { deployment } from './deployment_config.mjs';
import {createBrowserSignerVault} from './secure_signer_vault.mjs';

const endpoint='/api/v1/cloud/session';
function validFid(value) {
  return typeof value==='string' && /^[1-9][0-9]{0,18}$/.test(value)
      && Number.isSafeInteger(Number(value));
}
// A numeric FID is not a global account identity: Farcaster and HyperSnap
// use independently verified issuers. This browser vault stores only a
// Farcaster signer; never adopt a native HyperSnap session with the same FID.
function isVerifiedFarcasterOwner(body,fid) {
  return body?.fid===fid && body?.issuer===deployment.openProofIssuer
      && body?.subject==='farcaster:'+fid
      && /^[A-Za-z0-9_-]{43}$/.test(body?.csrf_token||'');
}
async function jsonResponse(response) {
  const raw=await response.text();
  if(raw.length>2048)throw Error('Account response too large');
  const body=JSON.parse(raw);
  if(!response.ok)throw Error('Account HTTP '+response.status+': '+String(body?.error||'unavailable'));
  return body;
}
async function request(fetchImpl,path,options={}) {
  const response=await fetchImpl(path,{credentials:'same-origin',
    cache:'no-store',redirect:'error',...options});
  return jsonResponse(response);
}
export async function ensureBrowserAccountSession({
  fid,fetchImpl=globalThis.fetch,vault=createBrowserSignerVault(globalThis),
  signal,origin=globalThis.location?.origin,
}={}) {
  if(!validFid(fid))throw Error('Invalid account FID');
  // The account API, WASM shell and host-only HttpOnly cookie share the
  // canonical origin. Never restore credentials on the retired preview host.
  if(origin!==deployment.publicOrigin)
    throw Error('Account session requires an approved secure origin');
  const existing=await fetchImpl(endpoint,{method:'GET',
    credentials:'same-origin',cache:'no-store',redirect:'error',signal});
  if(existing.status===200) {
    const body=await jsonResponse(existing);
    if(isVerifiedFarcasterOwner(body,fid))
      return body;
    throw Error('Signed-in account differs from the existing browser session');
  }
  if(existing.status!==401)throw Error('Account session check HTTP '+existing.status);
  const key=await vault.load(fid);
  if(!key)throw Error('No registered browser signer for this account');
  const headers={'content-type':'application/json','accept':'application/json'};
  const challenge=await request(fetchImpl,endpoint+'/challenge',{
    method:'POST',headers,body:JSON.stringify({fid}),signal});
  if(challenge.fid!==fid||!/^[A-Za-z0-9_-]{43}$/.test(challenge.nonce||''))
    throw Error('Invalid account server challenge');
  const proof=await vault.signAccountChallenge(fid,challenge.nonce);
  if(proof.publicKeyHex!==key.publicKeyHex)
    throw Error('Browser signer changed while establishing account session');
  const granted=await request(fetchImpl,endpoint+'/proof',{
    method:'POST',headers,signal,body:JSON.stringify({
      fid,nonce:challenge.nonce,
      public_key:proof.publicKeyHex,signature:proof.signature,
    }),
  });
  if(!isVerifiedFarcasterOwner(granted,fid))
    throw Error('Account proof returned a different protocol-scoped owner');
  // A successful Set-Cookie is not sufficient in a browser: verify that the
  // browser accepted the HttpOnly cookie via a fresh same-origin request.
  const restored=await request(fetchImpl,endpoint,{method:'GET',signal});
  if(!isVerifiedFarcasterOwner(restored,fid))
    throw Error('Browser rejected the verified account session');
  return restored;
}


// Private device sessions are enrolled with a verified OpenProof grant
// or an independent native custody proof, never a social signer.
export async function ensureBrowserCloudDeviceSession({
  fid,network='farcaster',bearer,custody,
  fetchImpl=globalThis.fetch,environment=globalThis,signal,
  origin=globalThis.location?.origin,
  vault,
}={}) {
  if(origin!==deployment.publicOrigin)throw Error('Unapproved account origin');
  if(!['farcaster','hypersnap'].includes(network)||
     typeof fid!=='string'||!/^[1-9][0-9]{0,19}$/.test(fid)||
     (network==='farcaster'?!validFid(fid):BigInt(fid)<4294967296n||BigInt(fid)>18446744073709551615n))
    throw Error('Invalid protocol-scoped account');
  const enroll=network==='farcaster'?typeof bearer==='string'&&bearer.length>0:
    typeof custody?.signMessage==='function';
  const expectedIssuer=network==='farcaster'?deployment.openProofIssuer:'hypersnap:native-registry';
  const matches=body=>body?.fid===fid&&body.issuer===expectedIssuer&&
    body.subject===network+':'+fid&&/^[A-Za-z0-9_-]{43}$/.test(body.csrf_token||'');
  const options={credentials:'same-origin',cache:'no-store',redirect:'error',signal};
  const existing=await fetchImpl(endpoint,{...options,method:'GET'});
  if(existing.status===200){
    const body=await jsonResponse(existing);
    if(!matches(body))throw Error('Signed-in account differs from the existing browser session');
    if(body.auth_source==='device'&&!enroll)return body;
  }else if(existing.status!==401)throw Error('Account session unavailable');
  vault??=createBrowserSignerVault(environment,{scope:'cloud-device-'+network});
  const key=await (enroll?vault.ensure(fid):vault.load(fid));
  if(!key)return null; // old account has not yet enrolled; explicit migration required
  const expected={purpose:enroll?'enroll':'restore',network,fid,public_key:key.publicKeyHex};
  const headers={'content-type':'application/json',accept:'application/json'};
  const challengeReply=await fetchImpl(endpoint+'/device/challenge',{
    ...options,method:'POST',headers,body:JSON.stringify(expected)});
  if(challengeReply.status===404||challengeReply.status===503)return null;
  const challenge=await jsonResponse(challengeReply);
  if(Object.keys(expected).some(k=>challenge[k]!==expected[k])||
     !/^[A-Za-z0-9_-]{43}$/.test(challenge.nonce||''))throw Error('Invalid cloud device challenge');
  const proof=await vault.signCloudDeviceChallenge(fid,challenge);
  const body={...expected,nonce:challenge.nonce,signature:proof.signature};
  if(enroll&&network==='farcaster')headers.authorization='Bearer '+bearer;
  if(enroll&&network==='hypersnap'){
    const address=String(custody.address||'').toLowerCase();
    if(!/^0x[0-9a-f]{40}$/.test(address))throw Error('Invalid custody address');
    const canonical=['genycaster-cloud-device-v2',expected.purpose,network,fid,
      expected.public_key,challenge.nonce].join('\n');
    body.custody_address=address;
    body.custody_signature=await custody.signMessage([
      'Genycaster — authorize private cloud device',
      'This authorizes private Genycaster data on this device, not social signing or a transaction.',
      'Origin: https://genycaster.xyz',canonical,'Custody: '+address].join('\n'));
  }
  if(signal?.aborted)throw Error('Account session cancelled');
  const reply=await fetchImpl(endpoint+'/device/proof',{
    ...options,method:'POST',headers,body:JSON.stringify(body)});
  if(!enroll&&reply.status===401)return null;
  const granted=await jsonResponse(reply);
  if(!matches(granted)||granted.auth_source!=='device')throw Error('Cloud device principal mismatch');
  const confirmed=await request(fetchImpl,endpoint,{method:'GET',signal});
  if(!matches(confirmed)||confirmed.auth_source!=='device')throw Error('Cloud device cookie mismatch');
  return confirmed;
}


// During staged rollout a verified OpenProof grant can still mint a cookie
// directly on older account servers. Never downgrade this path to social proof.
export async function bootstrapBrowserOpenProofSession({fid,bearer,fetchImpl=globalThis.fetch,signal,
  origin=globalThis.location?.origin}={}) {
  if(origin!==deployment.publicOrigin||!validFid(fid)||typeof bearer!=='string'||!bearer)
    throw Error('Invalid OpenProof bootstrap');
  const principal=await request(fetchImpl,endpoint,{method:'POST',signal,
    headers:{authorization:'Bearer '+bearer,accept:'application/json'}});
  if(!isVerifiedFarcasterOwner(principal,fid))throw Error('OpenProof session principal mismatch');
  const confirmed=await request(fetchImpl,endpoint,{method:'GET',signal});
  if(!isVerifiedFarcasterOwner(confirmed,fid))throw Error('OpenProof cookie principal mismatch');
  return confirmed;
}
