// Shared, read-only Market snapshot. Never send CMC credentials to a client.
// The authenticated Basic API is required for production CMC market metrics.
// A separately attributed Alternative.me Fear & Greed value is available
// when a CMC plan has not yet been configured.
const SOURCE='https://pro-api.coinmarketcap.com';
const ALTERNATIVE='https://api.alternative.me/fng/?limit=1';
const DEFINITIONS=Object.freeze({
  global:{path:'/v1/global-metrics/quotes/latest?convert=USD',ttl:600_000},
  sentiment:{path:'/v3/fear-and-greed/latest',ttl:3_600_000},
  season:{path:'/v1/altcoin-season-index/latest',ttl:3_600_000},
  cmc100:{path:'/v3/index/cmc100-latest',ttl:1_800_000}
});
function finite(v,upper=Number.MAX_SAFE_INTEGER){
  const n=Number(v);
  return v!==null && v!==undefined && v!=='' && Number.isFinite(n)
    && n>=0 && n<=upper ? n : null;
}
async function jsonLimited(response,max=96*1024){
  if(!response.ok)throw Error('upstream_status');
  if(Number(response.headers.get('content-length')||0)>max)throw Error('too_large');
  if(!response.body)throw Error('no_body');
  const reader=response.body.getReader();
  const chunks=[];let size=0;
  try{
    while(true){
      const {done,value}=await reader.read();
      if(done)break;
      size+=value.byteLength;
      if(size>max)throw Error('too_large');
      chunks.push(value);
    }
  }finally{await reader.cancel().catch(()=>{});}
  const data=Buffer.concat(chunks.map(chunk=>Buffer.from(chunk)));
  return JSON.parse(data.toString('utf8'));
}
function parse(name,root){
  if(root?.status && Number(root.status.error_code)!==0)
    throw Error('upstream_error');
  const d=root?.data;
  if(!d||typeof d!=='object')throw Error('invalid_data');
  if(name==='global'){
    const usd=d.quote?.USD;
    const btc=finite(d.btc_dominance,100);
    const eth=finite(d.eth_dominance,100);
    const cap=finite(usd?.total_market_cap);
    const volume=finite(usd?.total_volume_24h);
    if([btc,eth,cap,volume].some(x=>x===null))throw Error('invalid_global');
    return {btc_dominance:btc,eth_dominance:eth,
      market_cap_usd:cap,volume_24h_usd:volume,
      timestamp:String(d.last_updated||'')};
  }
  if(name==='sentiment'){
    const value=finite(d.value,100);
    if(value===null)throw Error('invalid_sentiment');
    return {value,label:String(d.value_classification||'').slice(0,24),
      timestamp:String(d.update_time||'')};
  }
  if(name==='season'){
    const value=finite(d.altcoin_index,100);
    if(value===null)throw Error('invalid_season');
    return {value,timestamp:String(d.snapshot_time||'')};
  }
  if(name==='cmc100'){
    const value=finite(d.value);
    const change=Number(d.value_24h_percentage_change);
    if(value===null||!Number.isFinite(change)||Math.abs(change)>100000)
      throw Error('invalid_cmc100');
    return {value,change_24h_percent:change,
      timestamp:String(d.last_update||'')};
  }
  throw Error('unsupported_metric');
}
export function createMarketReader({
  key=process.env.GENYCASTER_CMC_API_KEY||'',
  fetchImpl=globalThis.fetch,now=Date.now
}={}){
  // Per-field single-flight cache, retry cooldown and stale-while-error.
  const cache=new Map();
  const inflight=new Map();
  const planReady=typeof key==='string' && key.trim().length>0;
  const request=async(name)=>{
    const definition=DEFINITIONS[name];
    const cacheKey=planReady?'cmc:'+name:'alternative:sentiment';
    const existing=cache.get(cacheKey);
    const at=now();
    if(existing && at<existing.freshUntil)return {...existing,stale:false};
    if(existing && at<existing.retryAfter)return {...existing,stale:true};
    if(inflight.has(cacheKey))return inflight.get(cacheKey);
    const job=(async()=>{
      try{
        let value;
        let origin;
        if(!planReady && name==='sentiment'){
          const result=await fetchImpl(ALTERNATIVE,{headers:{accept:'application/json'},
            redirect:'error',signal:AbortSignal.timeout(8000)});
          const doc=await jsonLimited(result,8192);
          const entry=doc?.data?.[0];
          const n=finite(entry?.value,100);
          if(n===null)throw Error('invalid_alternative');
          value={value:n,label:String(entry.value_classification||'').slice(0,24),
            timestamp:new Date(Number(entry.timestamp)*1000).toISOString()};
          origin='Alternative.me';
        }else{
          const endpoint=SOURCE+definition.path;
          const result=await fetchImpl(endpoint,{headers:{
            'Accept':'application/json','X-CMC_PRO_API_KEY':key
          },redirect:'error',signal:AbortSignal.timeout(8000)});
          value=parse(name,await jsonLimited(result));
          origin='CoinMarketCap';
        }
        const data={value,source:origin,fetchedAt:new Date(at).toISOString(),
          freshUntil:at+(definition?.ttl||3_600_000),retryAfter:0};
        cache.set(cacheKey,data);
        return {...data,stale:false};
      }catch{
        if(existing){
          existing.retryAfter=at+Math.min(600_000,definition?.ttl||600_000);
          return {...existing,stale:true};
        }
        return null;
      }
    })().finally(()=>inflight.delete(cacheKey));
    inflight.set(cacheKey,job);
    return job;
  };
  return async()=>{
    const metrics={};
    const entries=await Promise.all(Object.keys(DEFINITIONS).map(async name=>{
      if(!planReady&&name!=='sentiment')return [name,null];
      return [name,await request(name)];
    }));
    for(const [key,value] of entries)
      if(value) metrics[key]={...value.value,source:value.source,
        fetched_at:value.fetchedAt,stale:value.stale};
    return {metrics,partial:Object.keys(metrics).length<4
      ||Object.values(metrics).some(v=>v.stale),
      plan_configured:planReady,
      updated_at:new Date(now()).toISOString()};
  };
}
