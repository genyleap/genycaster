/**
 * First-party Farcaster new-FID client.
 *
 * Real FID allocation is paid/sponsored and handled only by the Neynar relay.
 * Browser performs the exact custody EIP-712 transfer authorization and never
 * sends the phrase, raw key, provider API key, or an unsigned enrollment.
 * A previously ambiguous allocation is never automatically repeated.
 */
import {deriveFarcasterCustody} from './farcaster_recovery.mjs';
const REGISTRY='0x00000000fc6c5f01fc30151999387bb99a9f489b';
const error=message=>{throw new Error('Farcaster registration: '+message);};
const acceptedOrigin=uri=>{
  const url=new URL(uri);
  if(url.protocol!=='https:' || url.username ||url.password
      ||url.search||url.hash||url.pathname!=='/'||url.port&&url.port!=='443')
    error('registration requires a trusted HTTPS site origin');
  return url.origin;
};
const isId=v=>typeof v==='string'&&/^[1-9][0-9]{0,15}$/.test(v)
  &&BigInt(v)<=BigInt(Number.MAX_SAFE_INTEGER);
const isUint=v=>typeof v==='string'&&/^(0|[1-9][0-9]{0,19})$/.test(v)
  &&BigInt(v)<=0xffffffffffffffffn;
const isAddress=v=>typeof v==='string'&&/^0x[0-9a-f]{40}$/i.test(v);
const isUuid=v=>typeof v==='string'&&/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(v);
const isUsername=v=>typeof v==='string'&&/^[a-z0-9][a-z0-9-]{0,15}$/.test(v);
function validateVoucher(v){
  if(typeof v!=='string'||v.length<60||v.length>1100||/[\r\n]/.test(v))
    error('a valid operator-issued paid enrollment voucher is required');
  return v;
}
// The same paid voucher/custody pair MUST keep the same idempotency key
// across browser reloads. A new random UUID for each attempt could allocate
// a second funded Neynar FID after a lost response.
async function stableReservationId(browser,voucher,address) {
  const bytes=new TextEncoder().encode('genycaster-neynar-enrollment-v1\n'
    +address+'\n'+voucher);
  const digest=new Uint8Array(await browser.crypto.subtle.digest('SHA-256',bytes));
  bytes.fill(0);
  digest[6]=(digest[6]&0x0f)|0x40;
  digest[8]=(digest[8]&0x3f)|0x80;
  const raw=[...digest.slice(0,16)].map(v=>v.toString(16).padStart(2,'0')).join('');
  digest.fill(0);
  return raw.slice(0,8)+'-'+raw.slice(8,12)+'-'+raw.slice(12,16)
    +'-'+raw.slice(16,20)+'-'+raw.slice(20);
}
function transferTypedData(fid,address,nonce,deadline) {
  if(!isId(fid)||!isUint(nonce)||!isUint(deadline)||!isAddress(address))
    error('invalid verified custody transfer parameters');
  return {
    types:{
      EIP712Domain:[{name:'name',type:'string'},{name:'version',type:'string'},
        {name:'chainId',type:'uint256'},{name:'verifyingContract',type:'address'}],
      Transfer:[{name:'fid',type:'uint256'},{name:'to',type:'address'},
        {name:'nonce',type:'uint256'},{name:'deadline',type:'uint256'}],
    },
    domain:{name:'Farcaster IdRegistry',version:'1',chainId:10,
      verifyingContract:REGISTRY},
    primaryType:'Transfer',
    message:{fid,to:address.toLowerCase(),nonce,deadline},
  };
}
export function createBrowserFarcasterRegistration({
  browser=globalThis,fetchImpl=globalThis.fetch,base='https://genycaster.xyz',
  delay=ms=>new Promise(resolve=>setTimeout(resolve,ms))
}={}) {
  if(browser.isSecureContext!==true
      ||typeof browser.crypto?.subtle?.digest!=='function'
      ||typeof browser.crypto?.subtle?.sign!=='function')
    error('secure WebCrypto is required');
  if(typeof fetchImpl!=='function')error('network is unavailable');
  const origin=acceptedOrigin(base);
  if(browser.location && browser.location.origin!==origin)
    error('paid enrollment must use the canonical first-party site');
  let leaseId=null;
  async function post(action,body,voucher,signal){
    const response=await fetchImpl(origin+'/api/v1/registration/farcaster/'+action,{
      method:'POST',mode:'same-origin',redirect:'error',
      credentials:'omit',signal:signal??AbortSignal.timeout(35000),
      headers:{'content-type':'application/json',
        accept:'application/json','x-genycaster-paid-enrollment':voucher},
      body:JSON.stringify(body),
    });
    if(response.status===404)error('Farcaster paid enrollment is not enabled on this server');
    if(Number(response.headers?.get?.('content-length')||0)>16384)
      error('oversized registration response');
    const raw=await response.text();
    if(raw.length>16384)error('oversized registration response');
    let reply;
    try{reply=JSON.parse(raw);}catch{error('unrecognized registration service response');}
    if(!response.ok)
      error('enrollment server rejected '+action+' (HTTP '+response.status+': '
        +String(reply?.error||'unavailable').slice(0,80)+')');
    if(!reply?.registration||typeof reply.registration!=='object')
      error('invalid registration confirmation');
    return reply.registration;
  }
  async function preflight() {
    const response=await fetchImpl(origin+'/api/v1/registration/farcaster/reserve',{
      method:'POST',credentials:'omit',mode:'same-origin',redirect:'error',
      headers:{'content-type':'application/json'},body:'{}',
      signal:AbortSignal.timeout(12000)});
    await response.body?.cancel?.();
    if(response.status!==401 && response.status!==400)
      error(response.status===404
        ?'paid Farcaster registration is not installed on the public server'
        :'paid Farcaster enrollment service is not available (HTTP '+response.status+')');
    return true;
  }
  async function register(phrase,{voucher,username,signal,
    confirmIrreversible=false,onProgress=()=>{}}={}) {
    if(confirmIrreversible!==true)error('registration requires explicit consent');
    validateVoucher(voucher);
    if(!isUsername(username))error('choose a valid Farcaster username');
    if(leaseId)error('a funded FID reservation is already pending; inspect status before another attempt');
    const custody=await deriveFarcasterCustody(phrase);
    let irreversibleStarted=false;
    try{
      const address=custody.address.toLowerCase();
      leaseId=await stableReservationId(browser,voucher,address);
      if(!isUuid(leaseId))error('failed to derive registration idempotency key');
      onProgress(0,4,'reserve');
      const reserved=await post('reserve',{
        custody_address:address,idempotency_key:leaseId
      },voucher,signal);
      if(reserved.custody_address!==address||reserved.reservation_id!==leaseId
          ||!isId(reserved.fid))error('reservation identity mismatch');
      if(reserved.status!=='reserved')error('reservation is not active; inspect status before retrying');
      if(!isUint(reserved.id_registry_nonce))error('reservation nonce is unavailable');
      const unix=Math.floor(Date.now()/1000);
      const deadline=String(unix+300);
      const typed=transferTypedData(reserved.fid,address,reserved.id_registry_nonce,deadline);
      onProgress(1,4,'sign');
      const signature=await custody.signTypedData(typed);
      if(!/^0x[0-9a-f]{130}$/i.test(signature))
        error('custody transfer signature is malformed');
      if(signal?.aborted)error('registration cancelled before submission');
      irreversibleStarted=true;
      onProgress(2,4,'submit');
      const submitted=await post('submit',{
        custody_address:address,reservation_id:leaseId,
        deadline,signature,fname:username
      },voucher,signal);
      if(submitted.custody_address!==address
         ||submitted.reservation_id!==leaseId
         ||!isId(submitted.fid)||submitted.fid!==reserved.fid)
        error('submitted reservation identity mismatch');
      onProgress(3,4,'verify');
      for(let i=0;i<60;i++){
        if(signal?.aborted)
          error('registration submission may have succeeded; check status before retrying');
        const current=await post('status',{
          custody_address:address,reservation_id:leaseId
        },voucher,signal);
        if(current.reservation_id!==leaseId||current.custody_address!==address
           ||current.fid!==reserved.fid)
          error('registration status identity mismatch');
        if(current.status==='confirmed') {
          leaseId=null;onProgress(4,4,'confirmed');
          return {fid:reserved.fid,address};
        }
        if(['rejected','already_registered'].includes(current.status))
          error('network rejected the reserved Farcaster registration');
        if(i<59)await delay(2000);
      }
      error('FID submission remains unresolved; do not reserve another FID');
    }catch(cause){
      if(leaseId || irreversibleStarted)cause.outcomeUnknown=true;
      throw cause;
    }finally{custody.dispose();}
  }
  return Object.freeze({preflight,register});
}
