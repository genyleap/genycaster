// Trusted, single-purpose account entitlement oracle for Smart Media Pipeline.
// The Farcaster FID comes from a verified signed Media Gateway upload; public
// callers must not be able to enumerate paid memberships.
import {createHmac, timingSafeEqual} from 'node:crypto';

const FID=/^[1-9][0-9]{0,18}$/;
const HEADER=/^[a-f0-9]{64}$/i;

export function mediaEntitlementSignature(secret,fid,timestamp) {
  return createHmac('sha256',secret)
    .update(['smart-media-tier-v1',fid,String(timestamp)].join('\n'))
    .digest('hex');
}

export function authorizeMediaEntitlement(request,{secret,now=Date.now}={}) {
  if(typeof secret!=='string'||secret.length<32)return null;
  const url=new URL(request.url,'http://localhost');
  if(url.searchParams.size!==1||url.searchParams.getAll('fid').length!==1)return null;
  const fid=url.searchParams.get('fid');
  const timestamp=String(request.headers['x-smart-media-time']||'');
  const signed=String(request.headers['x-smart-media-signature']||'');
  if(!FID.test(fid)||!/^\d{10,13}$/.test(timestamp)||!HEADER.test(signed))return null;
  const seconds=Number(timestamp);
  if(!Number.isSafeInteger(seconds)||Math.abs(Math.floor(now()/1000)-seconds)>30)return null;
  const expected=mediaEntitlementSignature(secret,fid,timestamp);
  if(!timingSafeEqual(Buffer.from(expected,'hex'),Buffer.from(signed,'hex')))return null;
  return fid;
}

export async function verifiedMediaPro(store,issuer,fid) {
  if(!store?.proBillingReady?.())return null;
  const owner={issuer,subject:'farcaster:'+fid};
  const subscription=await store.getProSubscription(owner);
  const expires=subscription?.expires_at instanceof Date
    ?subscription.expires_at.getTime():Date.parse(String(subscription?.expires_at||''));
  return Number.isFinite(expires)&&expires>Date.now();
}
