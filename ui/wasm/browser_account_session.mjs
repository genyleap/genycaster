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
