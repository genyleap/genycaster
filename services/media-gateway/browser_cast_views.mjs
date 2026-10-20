// Public, read-only cast impressions. The protocol hub does not provide
// views; these numbers are client-side Farcaster analytics, not protocol
// consensus. Match every upstream result by its exact 20-byte cast hash.
const HASH=/^0x[0-9a-f]{40}$/;
const TTL_MS=3*60*1000;
const MAX_RESPONSE_BYTES=2*1024*1024;
const MAX_UPSTREAM_ACTIVE=3;
const MAX_UPSTREAM_QUEUED=32;
const OVERALL_BUDGET_MS=6_000;

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

async function getPublicJson(url,fetchImpl,signal) {
  try {
    const response=await fetchImpl(url,{
      method:'GET',redirect:'error',
      headers:{accept:'application/json'},
      signal:signal ? AbortSignal.any([signal,AbortSignal.timeout(8_000)])
        : AbortSignal.timeout(8_000),
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

export function createCastViewService({fetchImpl=fetch,now=()=>Date.now(),
  workBudgetMs=OVERALL_BUDGET_MS}={}) {
  const authorCache=new Map();
  const searchCache=new Map();
  let upstreamActive=0;
  const upstreamWaiting=[];

  // Admission belongs to the service, not each HTTP request. A busy batch
  // cannot spawn three more upstream requests for every concurrent client.
  function withUpstreamSlot(run,signal) {
    return new Promise(resolve=>{
      if(signal.aborted || (upstreamActive>=MAX_UPSTREAM_ACTIVE
            && upstreamWaiting.length>=MAX_UPSTREAM_QUEUED)) {
        resolve(null);return;
      }
      let settled=false;
      const finish=value=>{
        if(settled)return;
        settled=true;
        resolve(value);
      };
      const task={signal,launch:null};
      // Shared upstream work can time out while waiting behind a hung
      // transport. Release its queue admission immediately, not only after
      // an unrelated active request happens to finish and drains the queue.
      const cancelQueued=()=>{
        const index=upstreamWaiting.indexOf(task);
        if(index<0)return;
        upstreamWaiting.splice(index,1);
        signal.removeEventListener('abort',cancelQueued);
        finish(null);
      };
      task.launch=async()=>{
        signal.removeEventListener('abort',cancelQueued);
        if(signal.aborted){finish(null);drain();return;}
        ++upstreamActive;
        try{finish(await run());}
        catch{finish(null);}
        finally{--upstreamActive;drain();}
      };
      if(upstreamActive<MAX_UPSTREAM_ACTIVE)task.launch();
      else {
        upstreamWaiting.push(task);
        signal.addEventListener('abort',cancelQueued,{once:true});
        if(signal.aborted)cancelQueued();
      }
    });
  }

  function drain() {
    while(upstreamActive<MAX_UPSTREAM_ACTIVE && upstreamWaiting.length>0){
      const next=upstreamWaiting.shift();
      next.launch();
    }
  }

  function retain(cache,key,value,maximum) {
    cache.delete(key);
    cache.set(key,value);
    while(cache.size>maximum)cache.delete(cache.keys().next().value);
  }

  async function withinDeadline(promise,signal) {
    if(signal.aborted)return;
    let expired;
    const timeout=new Promise(resolve=>{
      expired=()=>resolve();
      signal.addEventListener('abort',expired,{once:true});
    });
    try{await Promise.race([promise,timeout]);}
    finally{signal.removeEventListener('abort',expired);}
  }

  async function authorViews(fid) {
    const key=String(fid),t=now();
    const found=authorCache.get(key);
    if(found&&found.expires>t)return found.promise;
    // One recent-cast request for every author, shared across all visible
    // posts from that account and all concurrent client batches.
    // Cache entries are shared across client batches. Their upstream lifetime
    // cannot belong to whichever caller happened to arrive first.
    const sharedSignal=AbortSignal.timeout(Math.max(25,Math.min(workBudgetMs,8_000)));
    const promise=withUpstreamSlot(() => getPublicJson(
      'https://api.farcaster.xyz/v2/casts?fid='+encodeURIComponent(key)+'&limit=50',
      fetchImpl,sharedSignal,
    ),sharedSignal).then(data=>{
      if(!data && authorCache.get(key)?.promise===promise)
        authorCache.get(key).expires=now()+5_000;
      const views=new Map();
      for(const cast of (Array.isArray(data?.result?.casts)?data.result.casts:[])){
        const hash=String(cast?.hash||'').toLowerCase();
        const count=verifiedViewCount(cast);
        if(HASH.test(hash)&&count!==null&&Number(cast?.author?.fid)===fid)
          views.set(hash,count);
      }
      return views;
    }).catch(()=>new Map());
    retain(authorCache,key,{promise,expires:t+TTL_MS},400);
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
    const sharedSignal=AbortSignal.timeout(Math.max(25,Math.min(workBudgetMs,8_000)));
    const promise=withUpstreamSlot(() => getPublicJson(
      'https://api.farcaster.xyz/v2/search-casts?q='
      +encodeURIComponent(query)+'&limit=10',fetchImpl,sharedSignal,
    ),sharedSignal).then(data=>{
      if(!data && searchCache.get(row.castId)?.promise===promise)
        searchCache.get(row.castId).expires=now()+5_000;
      const matched=(Array.isArray(data?.result?.casts)?data.result.casts:[])
        .find(cast=>String(cast?.hash||'').toLowerCase()===row.castId
          && Number(cast?.author?.fid)===row.authorFid);
      return verifiedViewCount(matched);
    }).catch(()=>null);
    retain(searchCache,row.castId,{promise,expires:t+TTL_MS},700);
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
    const deadline=AbortSignal.timeout(workBudgetMs);
    const fids=[...new Set(rows.map(row=>row.authorFid))];
    const recent=new Map();
    // Bound all upstream work by one operation deadline, rather than a fresh
    // timeout for every sequential group. Completed partial rows are usable.
    await withinDeadline(Promise.all(fids.map(async fid=>{
      const views=await authorViews(fid);
      if(views)recent.set(fid,views);
    })),deadline);
    const counts=Object.create(null);
    const missing=[];
    for(const row of rows){
      const count=recent.get(row.authorFid)?.get(row.castId);
      if(count!==undefined)counts[row.castId]=count;
      else missing.push(row);
    }
    // Search missing hashes only while budget remains. Slot admission is
    // shared across HTTP clients; unknowns stay absent rather than zero.
    if(!deadline.aborted)await withinDeadline(Promise.all(missing.map(async row=>{
      const count=await searchExact(row);
      if(count!==null && count!==undefined)counts[row.castId]=count;
    })),deadline);
    // Snapshot before returning: late completion after a partial response
    // must not mutate an object already handed to the HTTP serializer.
    return {status:200,data:{counts:{...counts}}};
  }

  return {resolve};
}
