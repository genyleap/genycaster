// Public, read-only cast impressions. The protocol hub does not provide
// views; these numbers are client-side Farcaster analytics, not protocol
// consensus. Match every upstream result by its exact 20-byte cast hash.
const HASH=/^0x[0-9a-f]{40}$/;
const TTL_MS=3*60*1000;
const MAX_RESPONSE_BYTES=2*1024*1024;

export function normalizeViewRequest(value) {
  if(!value || typeof value!=='object')return null;
  const hash=String(value.castId||'').trim().toLowerCase();
  const fid=Number(value.authorFid);
  if(!HASH.test(hash)||!Number.isSafeInteger(fid)||fid<=0)return null;
  const text=typeof value.text==='string'
    ? value.text.replace(/\s+/g,' ').trim().slice(0,160) : '';
  return {castId:hash,authorFid:fid,text};
}

function verifiedViewCount(row) {
  const value=row?.viewCount;
  return Number.isSafeInteger(value)&&value>=0?value:null;
}

async function getPublicJson(url,fetchImpl) {
  try {
    const response=await fetchImpl(url,{
      method:'GET',redirect:'error',
      headers:{accept:'application/json'},
      signal:AbortSignal.timeout(8_000),
    });
    if(!response.ok||!String(response.headers.get('content-type')||'')
       .toLowerCase().includes('application/json')){
      await response.body?.cancel();return null;
    }
    const declared=Number(response.headers.get('content-length')||0);
    if(!Number.isFinite(declared)||declared>MAX_RESPONSE_BYTES){
      await response.body?.cancel();return null;
    }
    if(!response.body)return null;
    const chunks=[];let length=0;
    const reader=response.body.getReader();
    try{
      for(;;){
        const {done,value}=await reader.read();
        if(done)break;
        length+=value.byteLength;
        if(length>MAX_RESPONSE_BYTES){await reader.cancel();return null;}
        chunks.push(Buffer.from(value));
      }
    }finally{reader.releaseLock();}
    return JSON.parse(Buffer.concat(chunks,length).toString('utf8'));
  }catch{return null;}
}

export function createCastViewService({fetchImpl=fetch,now=()=>Date.now()}={}) {
  const authorCache=new Map();
  const searchCache=new Map();

  async function authorViews(fid) {
    const key=String(fid),t=now();
    const found=authorCache.get(key);
    if(found&&found.expires>t)return found.promise;
    // One recent-cast request for every author, shared across all visible
    // posts from that account and all concurrent client batches.
    const promise=getPublicJson(
      'https://api.farcaster.xyz/v2/casts?fid='+encodeURIComponent(key)+'&limit=50',
      fetchImpl,
    ).then(data=>{
      const views=new Map();
      for(const cast of (Array.isArray(data?.result?.casts)?data.result.casts:[])){
        const hash=String(cast?.hash||'').toLowerCase();
        const count=verifiedViewCount(cast);
        if(HASH.test(hash)&&count!==null&&Number(cast?.author?.fid)===fid)
          views.set(hash,count);
      }
      return views;
    }).catch(()=>new Map());
    if(authorCache.size>=400)authorCache.clear();
    authorCache.set(key,{promise,expires:t+TTL_MS});
    return promise;
  }

  async function searchExact(row) {
    if(row.text.length<12)return null;
    const t=now();
    const cached=searchCache.get(row.castId);
    if(cached&&cached.expires>t)return cached.promise;
    // Only search a short, normalized prefix. Never trust text-match alone:
    // the authoritative identity is the full cast hash AND author FID.
    const query=row.text.slice(0,85);
    const promise=getPublicJson(
      'https://api.farcaster.xyz/v2/search-casts?q='
      +encodeURIComponent(query)+'&limit=10',fetchImpl,
    ).then(data=>{
      const matched=(Array.isArray(data?.result?.casts)?data.result.casts:[])
        .find(cast=>String(cast?.hash||'').toLowerCase()===row.castId
          && Number(cast?.author?.fid)===row.authorFid);
      return verifiedViewCount(matched);
    }).catch(()=>null);
    if(searchCache.size>=700)searchCache.clear();
    searchCache.set(row.castId,{promise,expires:t+TTL_MS});
    return promise;
  }

  async function resolve(payload) {
    if(!Array.isArray(payload)||payload.length===0||payload.length>16)
      return {status:400,error:'invalid_cast_view_batch'};
    const rows=[],seen=new Set();
    for(const raw of payload){
      const row=normalizeViewRequest(raw);
      if(!row)return {status:400,error:'invalid_cast_reference'};
      if(seen.has(row.castId))continue;
      seen.add(row.castId);rows.push(row);
    }
    const fids=[...new Set(rows.map(row=>row.authorFid))];
    const recent=new Map();
    // Cap concurrent upstream requests; a fast, popular feed may otherwise
    // stampede the public API and induce 429 responses for everyone.
    for(let index=0;index<fids.length;index+=3){
      const group=await Promise.all(fids.slice(index,index+3)
        .map(fid=>authorViews(fid)));
      group.forEach((views,offset)=>recent.set(fids[index+offset],views));
    }
    const counts=Object.create(null);
    const missing=[];
    for(const row of rows){
      const count=recent.get(row.authorFid)?.get(row.castId);
      if(count!==undefined)counts[row.castId]=count;
      else missing.push(row);
    }
    // Older replies frequently fall outside their author's 50 newest
    // casts. The previous slice(0,4) silently left up to 12 replies in every
    // viewport batch unqueried, even when Farcaster had their view counts.
    // Every requested cast now gets a chance, but cap concurrent public
    // search requests at three so heavily replied-to threads cannot stampede
    // the upstream service. Unknown values still stay absent, never zero.
    for(let index=0;index<missing.length;index+=3){
      const group=missing.slice(index,index+3);
      const searched=await Promise.all(group.map(row=>searchExact(row)));
      searched.forEach((count,offset)=>{
        if(count!==null)counts[group[offset].castId]=count;
      });
    }
    return {status:200,data:{counts}};
  }

  return {resolve};
}
