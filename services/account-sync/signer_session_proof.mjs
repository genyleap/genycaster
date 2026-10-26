// One-time, domain-separated account proof. The browser's non-extractable
// Farcaster Ed25519 signer remains on-device. The server independently checks
// signer registration in Snapchain before issuing a Genycaster account cookie.
import {createPublicKey,randomBytes,verify} from 'node:crypto';

const FID=/^[1-9][0-9]{0,18}$/;
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
async function checkSigner(fid,key,fetchImpl,nowMs) {
  let pageToken='';
  const seen=new Set();
  for(let page=0;page<PAGE_LIMIT;page++) {
    const url=new URL('https://snapchain.genyleap.com/v1/signersByFid');
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
      if(String(signer?.key||'').toLowerCase().replace(/^0x/,'')!==key ||
          String(signer?.fid)!==fid || Number(signer?.keyType)!==1)continue;
      if(signer.source==='SIGNER_SOURCE_ONCHAIN')return true;
      if(signer.source!=='SIGNER_SOURCE_OFFCHAIN')continue;
      if(!Array.isArray(signer.scopes) ||
         !signer.scopes.includes('MESSAGE_TYPE_CAST_ADD'))continue;
      if(signer.expiresAt!=null &&
          (!Number.isFinite(Number(signer.expiresAt)) ||
            Number(signer.expiresAt)<=Math.floor(nowMs/1000)-1609459200))
        continue;
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

export function createSignerSessionProof({fetchImpl=globalThis.fetch,now=Date.now}={}) {
  const pending=new Map();
  function prune() {
    const time=now();
    for(const [nonce,row] of pending)if(row.expires<=time)pending.delete(nonce);
  }
  function issue(fid) {
    if(typeof fid!=='string'||!FID.test(fid)||!Number.isSafeInteger(Number(fid)))
      bad(400,'invalid_fid');
    prune();
    if(pending.size>=MAX_CHALLENGES)bad(429,'challenge_limit');
    const nonce=randomBytes(32).toString('base64url');
    pending.set(nonce,{fid,expires:now()+TTL});
    return {fid,nonce,expires_in:90};
  }
  async function prove({fid,nonce,public_key:publicKey,signature}={}) {
    if(typeof fid!=='string'||!FID.test(fid) ||
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
    if(!await checkSigner(fid,publicKey,fetchImpl,now()))bad(401,'unregistered_signer');
    return {issuer:'https://openproof.genyleap.com',subject:'farcaster:'+fid,fid};
  }
  return {issue,prove};
}
