// One-time, domain-separated account proof. The browser's non-extractable
// Farcaster Ed25519 signer remains on-device. The server independently checks
// signer registration in Snapchain before issuing a Genycaster account cookie.
import {createPublicKey,randomBytes,verify} from 'node:crypto';
import {serverDeployment,trustedOrigin,isActiveGaslessSigner} from '../deployment_config.mjs';

// Native HyperSnap uses uint64 FIDs. JavaScript Number silently rounds
// registry identities above 2^53; keep every principal as canonical decimal
// text and use BigInt for namespace/bounds checks. Numeric JSON registry FIDs
// beyond MAX_SAFE_INTEGER cannot attest identity (String() is lossy).
const FID=/^[1-9][0-9]{0,19}$/;
export const MAX_UINT64_FID=(1n<<64n)-1n;
export const FIRST_NATIVE_HYPERSNAP_FID=1n<<32n;
export function isCanonicalUint64Fid(fid){
  return typeof fid==='string' && FID.test(fid) && BigInt(fid)<=MAX_UINT64_FID;
}
const NONCE=/^[A-Za-z0-9_-]{43}$/;
const HEX32=/^[0-9a-f]{64}$/;
const HEX64=/^[0-9a-f]{128}$/;
const PAGE_LIMIT=10;
const PREFIX=Buffer.from('302a300506032b6570032100','hex');
const MAX_CHALLENGES=1024;
const TTL=90_000;
export const SIGNER_CHALLENGE_PREFIX='genycaster-account-session-v1';

function bad(status=401,error='invalid_account_proof') {
  const e=new Error(error);e.status=status;throw e;
}
function challengeBytes(fid,nonce) {
  return Buffer.from(SIGNER_CHALLENGE_PREFIX+'\n'+fid+'\n'+nonce,'utf8');
}
async function checkSigner(fid,key,fetchImpl,nowMs,snapchainOrigin) {
  let pageToken='';
  const seen=new Set();
  for(let page=0;page<PAGE_LIMIT;page++) {
    const url=new URL(snapchainOrigin+'/v1/signersByFid');
    url.searchParams.set('fid',fid);
    url.searchParams.set('pageSize','100');
    if(pageToken)url.searchParams.set('pageToken',pageToken);
    let response;
    try {
      response=await fetchImpl(url,{redirect:'error',
        headers:{accept:'application/json'},signal:AbortSignal.timeout(6000)});
    } catch {bad(503,'signer_registry_unavailable');}
    if(!response?.ok)bad(503,'signer_registry_unavailable');
    let body;
    try {
      if(Number(response.headers?.get('content-length')||0)>524288)
        bad(503,'signer_registry_unavailable');
      const text=await response.text();
      if(text.length>524288)bad(503,'signer_registry_unavailable');
      body=JSON.parse(text);
    }catch {bad(503,'signer_registry_unavailable');}
    if(!Array.isArray(body?.signers)||body.signers.length>100)
      bad(503,'signer_registry_unavailable');
    for(const signer of body.signers) {
      // Registry APIs may use JSON decimal strings or safe JSON numbers.
      // Never convert an *unsafe* JSON number back to a uint64 principal:
      // serialization may already have rounded a distinct native identity.
      const registryFid=signer?.fid;
      if(typeof registryFid!=='string' &&
          !(typeof registryFid==='number' && Number.isSafeInteger(registryFid)))
        continue;
      if(String(signer?.key||'').toLowerCase().replace(/^0x/,'')!==key ||
          String(registryFid)!==fid || Number(signer?.keyType)!==1)continue;
      if(signer.source==='SIGNER_SOURCE_ONCHAIN')return true;
      if(signer.source!=='SIGNER_SOURCE_OFFCHAIN')continue;
      if(!Array.isArray(signer.scopes) ||
         !signer.scopes.includes('MESSAGE_TYPE_CAST_ADD'))continue;
      if(!isActiveGaslessSigner(signer,Math.floor(nowMs/1000)))continue;
      return true;
    }
    if(!body.nextPageToken)return false;
    pageToken=String(body.nextPageToken);
    if(pageToken.length>512||seen.has(pageToken))
      bad(503,'signer_registry_unavailable');
    seen.add(pageToken);
  }
  return false;
}

export function createSignerSessionProof({
  fetchImpl=globalThis.fetch,now=Date.now,
  issuer=serverDeployment().openProofIssuer,
  snapchainOrigin=serverDeployment().snapchainOrigin,
  hypersnapOrigin=serverDeployment().hypersnapOrigin,
}={}) {
  const trustedIssuer=trustedOrigin(issuer,'GENYCASTER_OPENPROOF_ISSUER');
  const trustedSnapchain=trustedOrigin(snapchainOrigin,'GENYCASTER_SNAPCHAIN_URL');
  const trustedHyperSnap=trustedOrigin(hypersnapOrigin,'GENYCASTER_HYPERSNAP_ORIGIN');
  const pending=new Map();
  function prune() {
    const time=now();
    for(const [nonce,row] of pending)if(row.expires<=time)pending.delete(nonce);
  }
  function issue(fid) {
    if(!isCanonicalUint64Fid(fid))
      bad(400,'invalid_fid');
    prune();
    if(pending.size>=MAX_CHALLENGES)bad(429,'challenge_limit');
    const nonce=randomBytes(32).toString('base64url');
    pending.set(nonce,{fid,expires:now()+TTL});
    return {fid,nonce,expires_in:90};
  }
  async function prove({fid,nonce,public_key:publicKey,signature}={}) {
    if(!isCanonicalUint64Fid(fid) ||
       typeof nonce!=='string'||!NONCE.test(nonce) ||
       typeof publicKey!=='string'||!HEX32.test(publicKey) ||
       typeof signature!=='string'||!HEX64.test(signature))
      bad(401);
    prune();
    const row=pending.get(nonce);
    // Consume before any network I/O, even when signature is invalid.
    pending.delete(nonce);
    if(!row||row.fid!==fid||row.expires<=now())bad(401);
    const pub=createPublicKey({key:Buffer.concat([PREFIX,Buffer.from(publicKey,'hex')]),
      format:'der',type:'spki'});
    if(!verify(null,challengeBytes(fid,nonce),pub,Buffer.from(signature,'hex')))
      bad(401);
    // This namespace is allocated by HyperSnap's native registry, not
    // Optimism IdRegistry. A signed proof cannot silently cross the two.
    const native=BigInt(fid)>=FIRST_NATIVE_HYPERSNAP_FID;
    const registryOrigin=native?trustedHyperSnap:trustedSnapchain;
    if(!await checkSigner(fid,publicKey,fetchImpl,now(),registryOrigin))
      bad(401,'unregistered_signer');
    return {issuer:native?'hypersnap:native-registry':trustedIssuer,
      subject:(native?'hypersnap:':'farcaster:')+fid,fid};
  }
  return {issue,prove};
}
