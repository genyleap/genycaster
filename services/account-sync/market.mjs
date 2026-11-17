// Shared read-only market snapshot. The public CMC endpoints use the
// keyless /public-api prefix; an optional server-side key uses the authenticated
// API instead. Never send API credentials to a browser or QML client.
//
// Keyless access is IP-rate-limited, not unlimited production entitlement.
// Use shared TTL caches, single-flight requests and bounded retry backoff.
const SOURCE='https://pro-api.coinmarketcap.com';
const PUBLIC_SOURCE=SOURCE+'/public-api';
const ALTERNATIVE='https://api.alternative.me/fng/?limit=1';
const COINGECKO_GLOBAL='https://api.coingecko.com/api/v3/global';
const COINGECKO_PRICES='https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd&ids=bitcoin%2Cethereum%2Csolana&price_change_percentage=24h';
const CORE_METRICS=Object.freeze(['global','sentiment','season','cmc100']);
const DEFINITIONS=Object.freeze({
  global:{path:'/v1/global-metrics/quotes/latest?convert=USD',ttl:600_000},
  sentiment:{path:'/v3/fear-and-greed/latest',ttl:3_600_000},
  season:{path:'/v1/altcoin-season-index/latest',ttl:3_600_000},
  cmc100:{path:'/v3/index/cmc100-latest',ttl:1_800_000},
  // Optional presentation data: bounded, real CMC observations only.
  prices:{path:'/v3/cryptocurrency/quotes/latest?id=1,1027,5426&convert=USD',ttl:600_000},
  cmc100Trend:{path:'/v3/index/cmc100-historical?count=10&interval=daily',ttl:1_800_000}
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
  return JSON.parse(Buffer.concat(chunks.map(chunk=>Buffer.from(chunk))).toString('utf8'));
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
  if(name==='prices'){
    if(!Array.isArray(d))throw Error('invalid_prices');
    const ids=new Map([[1,'BTC'],[1027,'ETH'],[5426,'SOL']]);
    const byId=new Map();
    for(const coin of d){
      const symbol=ids.get(Number(coin?.id));
      if(!symbol || coin?.is_active===0)continue;
      const quote=Array.isArray(coin.quote)?coin.quote.find(q=>q.symbol==='USD'):null;
      const price=finite(quote?.price);
      const change=Number(quote?.percent_change_24h);
      if(price===null||!Number.isFinite(change)||Math.abs(change)>1000)continue;
      byId.set(Number(coin.id),{symbol,price_usd:price,change_24h_percent:change});
    }
    const assets=[...ids.keys()].map(id=>byId.get(id)).filter(Boolean);
    if(assets.length<2)throw Error('incomplete_prices');
    return {assets};
  }
  if(name==='cmc100Trend'){
    if(!Array.isArray(d))throw Error('invalid_history');
    const points=d.slice(-10).map(point=>({
      value:finite(point?.value),
      timestamp:String(point?.update_time||'')
    })).filter(point=>point.value!==null && Number.isFinite(Date.parse(point.timestamp)))
      .sort((a,b)=>Date.parse(a.timestamp)-Date.parse(b.timestamp));
    if(points.length<2)throw Error('insufficient_history');
    return {values:points.map(point=>point.value),
      from:points[0].timestamp,to:points[points.length-1].timestamp,
      days:points.length};
  }
  throw Error('unsupported_metric');
}
function alternativeSentiment(root){
  const d=root?.data?.[0];
  const value=finite(d?.value,100);
  if(value===null)throw Error('invalid_alternative');
  const timestamp=Number(d.timestamp);
  return {value,label:String(d.value_classification||'').slice(0,24),
    timestamp:Number.isFinite(timestamp) && timestamp>0 && timestamp<1e11
      ? new Date(timestamp*1000).toISOString() : ''};
}
// Only factual, directly published CoinGecko observations are fallbacks.
// Never derive CMC Altcoin Season or CMC100 from unrelated indicators.
function coingeckoGlobal(root){
  const d=root?.data;
  if(!d || typeof d!=='object')throw Error('invalid_coingecko_global');
  const cap=finite(d.total_market_cap?.usd);
  const volume=finite(d.total_volume?.usd);
  const btc=finite(d.market_cap_percentage?.btc,100);
  const eth=finite(d.market_cap_percentage?.eth,100);
  const updated=Number(d.updated_at);
  if([cap,volume,btc,eth].some(n=>n===null) || cap<=0 || volume<=0
     || !Number.isSafeInteger(updated) || updated<=0 || updated>1e11)
    throw Error('invalid_coingecko_global');
  return {btc_dominance:btc,eth_dominance:eth,
    market_cap_usd:cap,volume_24h_usd:volume,
    timestamp:new Date(updated*1000).toISOString()};
}
function coingeckoPrices(root){
  if(!Array.isArray(root)||root.length>50)throw Error('invalid_coingecko_prices');
  const requested=new Map([['bitcoin','BTC'],['ethereum','ETH'],['solana','SOL']]);
  const assets=new Map();
  for(const coin of root){
    const symbol=requested.get(coin?.id);
    if(!symbol || assets.has(symbol))continue;
    const price=finite(coin.current_price);
    const change=Number(coin.price_change_percentage_24h);
    if(price===null || price<=0 || !Number.isFinite(change)
       || Math.abs(change)>1000)continue;
    assets.set(symbol,{symbol,price_usd:price,change_24h_percent:change});
  }
  if(assets.size<2)throw Error('incomplete_coingecko_prices');
  return {assets:[...requested.values()].map(symbol=>assets.get(symbol)).filter(Boolean)};
}
export function createMarketReader({
  key=process.env.GENYCASTER_CMC_API_KEY||'',
  fetchImpl=globalThis.fetch,now=Date.now
}={}){
  // CMC quota is shared across all users of this one server instance.
  // A transient 429/failure is cached NEGATIVELY too, preventing retry floods
  // during the first fetch when no valid historical snapshot exists.
  const cache=new Map();
  const inflight=new Map();
  const failures=new Map();
  const hasKey=typeof key==='string' && key.trim().length>0;
  const mode=hasKey?'keyed':'public';
  const request=async(name,provider=mode)=>{
    if(provider==='alternative'&&name!=='sentiment')return null;
    if(provider==='coingecko' && name!=='global' && name!=='prices')return null;
    const definition=DEFINITIONS[name];
    const id=provider+':'+name;
    const existing=cache.get(id);
    const at=now();
    if(existing && at<existing.freshUntil)
      return {...existing,stale:false};
    if(at<(failures.get(id)?.retryAfter??0))
      return existing?{...existing,stale:true}:null;
    if(inflight.has(id))return inflight.get(id);
    const job=(async()=>{
      try{
        let value,origin;
        if(provider==='alternative'){
          const response=await fetchImpl(ALTERNATIVE,{
            headers:{Accept:'application/json'},redirect:'error',
            signal:AbortSignal.timeout(8000)});
          value=alternativeSentiment(await jsonLimited(response,8192));
          origin='Alternative.me';
        }else if(provider==='coingecko'){
          const endpoint=name==='global'?COINGECKO_GLOBAL:COINGECKO_PRICES;
          const response=await fetchImpl(endpoint,{
            headers:{Accept:'application/json'},redirect:'error',
            signal:AbortSignal.timeout(8000)});
          const root=await jsonLimited(response,name==='global'?16*1024:64*1024);
          value=name==='global'?coingeckoGlobal(root):coingeckoPrices(root);
          origin='CoinGecko';
        }else{
          const endpoint=(provider==='keyed'?SOURCE:PUBLIC_SOURCE)+definition.path;
          const headers=provider==='keyed'
            ? {Accept:'application/json','X-CMC_PRO_API_KEY':key}
            : {Accept:'application/json'};
          const response=await fetchImpl(endpoint,{
            headers,redirect:'error',signal:AbortSignal.timeout(8000)});
          value=parse(name,await jsonLimited(response,
            name==='cmc100Trend' ? 512*1024 : name==='prices' ? 192*1024 : 96*1024));
          origin='CoinMarketCap';
        }
        const data={value,source:origin,fetchedAt:new Date(at).toISOString(),
          freshUntil:at+definition.ttl};
        cache.set(id,data);
        failures.delete(id);
        return {...data,stale:false};
      }catch{
        const failuresSoFar=(failures.get(id)?.attempts||0)+1;
        // Exponential cooldown: 60s, 120s, 240s, 480s, capped at 15m.
        const cooldown=Math.min(900_000,60_000*Math.pow(2,Math.min(4,failuresSoFar-1)));
        failures.set(id,{attempts:failuresSoFar,retryAfter:at+cooldown});
        return existing?{...existing,stale:true}:null;
      }
    })().finally(()=>inflight.delete(id));
    inflight.set(id,job);
    return job;
  };
  return async()=>{
    const entries=await Promise.all(Object.keys(DEFINITIONS).map(async name=>{
      let result=await request(name);
      // The keyless CMC endpoint can return 429 to Node clients even when
      // interactive curl requests succeed. Use separately cached sources,
      // with honest provenance and shared server-side cooldowns.
      if(name==='sentiment'&&!result)result=await request(name,'alternative');
      if((name==='global'||name==='prices')&&!result)
        result=await request(name,'coingecko');
      return [name,result];
    }));
    const metrics={};
    for(const [name,result] of entries)
      if(result)metrics[name]={...result.value,source:result.source,
        fetched_at:result.fetchedAt,stale:result.stale};
    return {
      // Missing optional price quotes / trend never makes the core dashboard
      // misleadingly partial. They render independent unavailable states.
      metrics,partial:CORE_METRICS.some(name=>!metrics[name] || metrics[name].stale),
      plan_configured:hasKey,
      access_mode:mode,
      updated_at:new Date(now()).toISOString()
    };
  };
}
