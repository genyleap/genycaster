// Neynar first-party paid registration coordinator.
// The public client never receives a Neynar API key or app-wallet identifier.
// This module is NOT an anonymous route: it requires an independently
// authorized enrollment/payment policy and an atomic, durable lease store.
import {randomUUID} from 'node:crypto';
import {exactAddress,exactFid,exactUint,verifyFarcasterTransfer}
  from './neynar_id_registry.mjs';

const UPSTREAM='https://api.neynar.com';
const MAX_RESPONSE=32768;
const MAX_RESERVATION_AGE_MS=600_000;
const FNAME=/^[a-z0-9][a-z0-9-]{0,15}$/;
const IDEMPOTENCY=/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
function reject(message) {throw new Error(message);}
async function parseNeynar(response) {
  if(Number(response.headers.get('content-length')||0)>MAX_RESPONSE)
    reject('neynar_response_too_large');
  if(!response.body)reject('neynar_empty_response');
  const chunks=[];
  let length=0;
  for await(const chunk of response.body) {
    length+=chunk.byteLength;
    if(length>MAX_RESPONSE)reject('neynar_response_too_large');
    chunks.push(Buffer.from(chunk));
  }
  const body=JSON.parse(Buffer.concat(chunks).toString('utf8'));
  if(!body||Array.isArray(body)||typeof body!=='object')
    reject('invalid_neynar_response');
  return body;
}
export function createNeynarRegistrationRelay({
  apiKey,walletId,registry,ledger,authorizeEnrollment,
  fetchImpl=globalThis.fetch,now=Date.now
}) {
  if(typeof apiKey!=='string'||!apiKey.trim()||/[\r\n]/.test(apiKey)
    ||typeof walletId!=='string'||!walletId.trim()||/[\r\n]/.test(walletId)
    ||!registry||typeof registry.nonce!=='function'
    ||typeof registry.idOf!=='function'
    ||typeof registry.confirmed!=='function'
    ||!ledger||typeof ledger.acquire!=='function'
    ||typeof ledger.get!=='function'
    ||typeof ledger.claimStart!=='function'
    ||typeof ledger.setReserved!=='function'
    ||typeof ledger.claimSubmit!=='function'
    ||typeof ledger.setOutcome!=='function'
    ||typeof authorizeEnrollment!=='function')
    throw new Error('Neynar relay requires funding authorization, registry and durable ledger');
  async function authorize(enrollment,custody,operation) {
    // This callback MUST verify the operator's payment entitlement or an
    // authenticated, one-time, non-reusable sponsorship voucher. An arbitrary
    // self-declared custody address or an existing read session is insufficient.
    const result=await authorizeEnrollment(enrollment,custody,operation);
    if(!result||result.approved!==true
        ||typeof result.subject!=='string'
        ||result.subject.length<10||result.subject.length>160)
      reject('enrollment_not_authorized');
    return result.subject;
  }
  async function request(path,{method='GET',body}={}) {
    const response=await fetchImpl(UPSTREAM+path,{
      method,redirect:'error',signal:AbortSignal.timeout(25_000),
      headers:{
        'x-api-key':apiKey,
        'x-wallet-id':walletId,
        accept:'application/json',
        ...(body?{'content-type':'application/json'}:{})
      },
      ...(body?{body:JSON.stringify(body)}:{})
    });
    return {code:response.status,data:await parseNeynar(response)};
  }
  const publicView=record=>Object.freeze({
    reservation_id:record.id,
    fid:record.fid||null,
    custody_address:record.custody,
    reserved_at:record.reservedAt||null,
    expires_at:record.reservedAt?record.reservedAt+MAX_RESERVATION_AGE_MS:null,
    status:record.status,
    transaction_hash:record.txHash||null
  });
  async function reserve({enrollment,custodyAddress,idempotencyKey}) {
    const custody=exactAddress(custodyAddress);
    if(!IDEMPOTENCY.test(idempotencyKey))reject('invalid_idempotency_key');
    const principal=await authorize(enrollment,custody,'reserve');
    // Atomic acquisition is required BEFORE an upstream request that spends
    // funds or leases a scarce FID. An existing pending lease is returned.
    const current=await ledger.acquire({
      id:idempotencyKey.toLowerCase(),principal,custody,createdAt:now()
    });
    if(!current||current.principal!==principal||current.custody!==custody)
      reject('reservation_conflict');
    if(current.status!=='reserving')return publicView(current);
    if(current.started) return publicView(current);
    if(!(await ledger.claimStart(current.id,principal)))
      return publicView(await ledger.get(current.id,principal));
    // Any failure leaves an "unknown" lease; never automatically replay a
    // potentially funded first-time Neynar allocation.
    try {
      if((await registry.idOf(custody))!=='0') {
        await ledger.setOutcome(current.id,principal,{status:'already_registered'});
        reject('custody_already_registered');
      }
      const {code,data}=await request('/v2/farcaster/user/fid');
      if(code!==200)reject('neynar_reservation_rejected');
      const fid=exactFid(data.fid);
      await ledger.setReserved(current.id,principal,{fid,reservedAt:now()});
      return publicView(await ledger.get(current.id,principal));
    }catch(error) {
      if(String(error.message)!=='custody_already_registered')
        await ledger.setOutcome(current.id,principal,{status:'reservation_unknown'});
      throw error;
    }
  }
  async function submit({enrollment,reservationId,custodyAddress,
                         deadline,signature,fname}) {
    if(!IDEMPOTENCY.test(reservationId))reject('invalid_reservation_id');
    const custody=exactAddress(custodyAddress);
    const principal=await authorize(enrollment,custody,'submit');
    const reservation=await ledger.get(reservationId.toLowerCase(),principal);
    if(!reservation||reservation.custody!==custody)
      reject('reservation_not_found');
    if(reservation.status!=='reserved')return publicView(reservation);
    if(typeof fname!=='string'||!FNAME.test(fname))
      reject('invalid_fname');
    const due=exactUint(deadline);
    const nowSecs=BigInt(Math.floor(now()/1000));
    if(BigInt(due)<=nowSecs+30n||BigInt(due)>nowSecs+3600n
      ||typeof reservation.reservedAt!=='number'
      ||reservation.reservedAt>now()
      ||now()-reservation.reservedAt>=MAX_RESERVATION_AGE_MS)
      reject('reservation_expired');
    if((await registry.idOf(custody))!=='0')
      reject('custody_already_registered');
    const nonce=await registry.nonce(custody);
    if(!verifyFarcasterTransfer({
      fid:reservation.fid,to:custody,nonce,deadline:due,signature
    }))reject('invalid_custody_signature');
    if(!(await ledger.claimSubmit(reservation.id,principal)))
      return publicView(await ledger.get(reservation.id,principal));
    try {
      // The sole irreversible mutation; exactly one write attempt for a
      // durable reservation, even with duplicated requests or server crashes.
      const {code,data}=await request('/v2/farcaster/user/',{
        method:'POST',
        body:{fid:Number(reservation.fid),deadline:Number(due),
          requested_user_custody_address:custody,
          signature,fname}
      });
      // NEVER treat Neynar 200 or success:true as chain finality.
      if(code===200 && data.success===true) {
        const tx=typeof data.transaction_hash==='string'
          && /^0x[0-9a-fA-F]{64}$/.test(data.transaction_hash)
          ? data.transaction_hash.toLowerCase() : null;
        await ledger.setOutcome(reservation.id,principal,
          {status:'submitted',txHash:tx});
      }else {
        await ledger.setOutcome(reservation.id,principal,
          {status:code>=400&&code<500 ? 'rejected' : 'submission_unknown'});
      }
    }catch {
      await ledger.setOutcome(reservation.id,principal,
        {status:'submission_unknown'});
    }
    return status({enrollment,reservationId:reservation.id,custodyAddress:custody});
  }
  async function status({enrollment,reservationId,custodyAddress}) {
    if(!IDEMPOTENCY.test(reservationId))reject('invalid_reservation_id');
    const custody=exactAddress(custodyAddress);
    const principal=await authorize(enrollment,custody,'status');
    const record=await ledger.get(reservationId.toLowerCase(),principal);
    if(!record||record.custody!==custody)reject('reservation_not_found');
    if(['submitted','submission_unknown','confirmed'].includes(record.status)) {
      try {
        const verified=await registry.confirmed(record.fid,custody);
        if(verified && record.status!=='confirmed')
          await ledger.setOutcome(record.id,principal,{status:'confirmed'});
      }catch {
        // An unavailable RPC MUST NOT turn a submission into a confirmed FID.
      }
    }
    return publicView(await ledger.get(record.id,principal));
  }
  return Object.freeze({reserve,submit,status});
}
