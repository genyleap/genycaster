// Signed backend-to-backend Pro limit check. Never accept a client-supplied
// tier, unverified FID, or claims embedded in an upload ticket.
import {createHmac} from 'node:crypto';

export function createMediaTierReader({
  origin='https://genycaster.xyz',secret='',
  fetchImpl=globalThis.fetch,now=Date.now,
}={}) {
  return async function isPro(fid) {
    if(typeof fid!=='string'||!/^[1-9][0-9]{0,18}$/.test(fid)
      ||!Number.isSafeInteger(Number(fid))||secret.length<32)return false;
    const timestamp=Math.floor(now()/1000);
    const signature=createHmac('sha256',secret)
      .update(['smart-media-tier-v1',fid,String(timestamp)].join('\n'))
      .digest('hex');
    try {
      const url=new URL('/api/v1/cloud/media/pro?fid='+fid,origin);
      if(url.protocol!=='https:'||url.hostname!=='genycaster.xyz')
        return false;
      const response=await fetchImpl(url,{
        method:'GET',redirect:'error',
        signal:AbortSignal.timeout(3500),
        headers:{
          'accept':'application/json',
          'x-smart-media-time':String(timestamp),
          'x-smart-media-signature':signature,
        },
      });
      if(response.status!==200)return false;
      const data=await response.json();
      return data?.eligible===true;
    }catch{return false;}
  };
}
