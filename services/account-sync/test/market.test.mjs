import test from 'node:test';
import assert from 'node:assert/strict';
import {createMarketReader} from '../market.mjs';
import {createAccountApi} from '../server.mjs';

const json=(data)=>new Response(JSON.stringify(data),{
  status:200,headers:{'content-type':'application/json'}});
function cmcMock(path){
  if(path.includes('/cryptocurrency/quotes/latest'))return json({status:{error_code:0},data:[
    {id:1,symbol:'BTC',is_active:1,quote:[{symbol:'USD',price:75420.12,percent_change_24h:-0.4}]},
    {id:1027,symbol:'ETH',is_active:1,quote:[{symbol:'USD',price:2231.71,percent_change_24h:1.2}]},
    {id:5426,symbol:'SOL',is_active:1,quote:[{symbol:'USD',price:89.05,percent_change_24h:-1.3}]}
  ]});
  if(path.includes('cmc100-historical'))return json({status:{error_code:0},data:[
    {value:151.4,update_time:'2026-11-06T00:00:00Z'},
    {value:149.6,update_time:'2026-11-07T00:00:00Z'},
    {value:147.84,update_time:'2026-11-08T00:00:00Z'}
  ]});
  if(path.includes('global-metrics'))return json({status:{error_code:0},data:{
    btc_dominance:61,eth_dominance:10,quote:{USD:{
      total_market_cap:2_500_000_000_000,total_volume_24h:95_000_000_000}},
    last_updated:'2026-11-08T00:00:00Z'}});
  if(path.includes('fear-and-greed'))return json({status:{error_code:0},data:{
    value:33,value_classification:'Fear',update_time:'2026-11-08T00:00:00Z'}});
  if(path.includes('altcoin-season'))return json({status:{error_code:0},data:{
    altcoin_index:41,snapshot_time:'2026-11-08T00:00:00Z'}});
  if(path.includes('cmc100'))return json({status:{error_code:0},data:{
    value:151.2,value_24h_percentage_change:-0.4,last_update:'2026-11-08T00:00:00Z'}});
  throw Error('unexpected_url');
}
test('keyless public API delivers all four CMC metrics with no API key and shared cache',async()=>{
  let calls=0,clock=Date.now();
  const read=createMarketReader({key:'',now:()=>clock,
    fetchImpl:async(url,options)=>{
      calls++;
      assert.match(url,/^https:\/\/pro-api\.coinmarketcap\.com\/public-api\//);
      assert.deepEqual(options.headers,{Accept:'application/json'});
      return cmcMock(url);
    }});
  const [a,b]=await Promise.all([read(),read()]);
  assert.equal(calls,6,'parallel callers must be coalesced');
  assert.deepEqual(a,b);
  assert.equal(a.partial,false);
  assert.equal(a.plan_configured,false);
  assert.equal(a.access_mode,'public');
  assert.equal(a.metrics.global.btc_dominance,61);
  assert.equal(a.metrics.global.volume_24h_usd,95_000_000_000);
  assert.equal(a.metrics.sentiment.value,33);
  assert.equal(a.metrics.sentiment.source,'CoinMarketCap');
  assert.equal(a.metrics.season.value,41);
  assert.equal(a.metrics.cmc100.change_24h_percent,-0.4);
  assert.deepEqual(a.metrics.cmc100Trend.values,[151.4,149.6,147.84]);
  assert.deepEqual(a.metrics.prices.assets.map(x=>x.symbol),['BTC','ETH','SOL']);
  assert.equal(a.metrics.prices.assets[0].change_24h_percent,-0.4);
  await read();
  assert.equal(calls,6,'cached calls must not reach CMC again');
  clock+=11*60_000;
  await read();
  assert.equal(calls,8,'global metrics and token quotes TTL have expired');
});
test('authenticated API remains a server-side optional mode without leaking credentials',async()=>{
  let calls=0;
  const read=createMarketReader({key:'secret-not-for-clients',
    fetchImpl:async(url,options)=>{
      calls++;
      assert.match(url,/^https:\/\/pro-api\.coinmarketcap\.com\/(?!public-api\/)/);
      assert.equal(options.headers['X-CMC_PRO_API_KEY'],'secret-not-for-clients');
      return cmcMock(url);
    }});
  const a=await read();
  assert.equal(calls,6);
  assert.equal(a.partial,false);
  assert.equal(a.access_mode,'keyed');
  assert.equal(a.plan_configured,true);
  assert.equal(JSON.stringify(a).includes('secret-not-for-clients'),false);
});
test('Alternative.me sentiment is a transparent fallback only if keyless CMC sentiment fails',async()=>{
  let alternativeCalls=0,cmcCalls=0;
  const read=createMarketReader({key:'',fetchImpl:async(url,options)=>{
    if(url.startsWith('https://api.alternative.me/')){
      alternativeCalls++;
      return json({data:[{value:'27',value_classification:'Fear',
        timestamp:'1794114000'}]});
    }
    assert.ok(!('X-CMC_PRO_API_KEY' in options.headers));
    cmcCalls++;
    if(url.includes('/fear-and-greed/'))
      return new Response('rate limit',{status:429});
    return cmcMock(url);
  }});
  const result=await read();
  assert.equal(cmcCalls,6);
  assert.equal(alternativeCalls,1);
  assert.equal(result.metrics.sentiment.value,27);
  assert.equal(result.metrics.sentiment.source,'Alternative.me');
  assert.equal(result.metrics.global.btc_dominance,61);
  assert.equal(result.partial,false,'fallback must populate all available indicators');
  assert.equal(result.plan_configured,false);
  await read();
  assert.equal(cmcCalls,6,'public quota failures must be negatively cached');
  assert.equal(alternativeCalls,1,'alternative must respect TTL');
});
test('rate-limited keyless first fetch is negatively cached and does not hammer the shared IP',async()=>{
  let clock=Date.now(),calls=0;
  const read=createMarketReader({key:'',now:()=>clock,
    fetchImpl:async(url)=>{
      calls++;
      if(url.startsWith('https://api.alternative.me/')){
        return new Response('',{status:429});
      }
      return new Response('',{status:429});
    }});
  const first=await read();
  assert.equal(first.partial,true);
  assert.deepEqual(first.metrics,{});
  assert.equal(calls,7,'six CMC calls and one alternative fallback');
  await read();
  assert.equal(calls,7,'retry cooldown must prevent an immediate flood');
  clock+=60_001;
  await read();
  assert.equal(calls,14,'one new bounded attempt per metric after retry cooldown');
  await read();
  assert.equal(calls,14,'second failure doubles the cooldown');
});
test('a failed refresh retains the last valid CMC data as stale with no error leakage',async()=>{
  let clock=Date.now(),failure=false;
  const read=createMarketReader({key:'',now:()=>clock,
    fetchImpl:async(url)=>{
      if(failure)throw Error('PRIVATE_UPSTREAM_ERROR');
      return cmcMock(url);
    }});
  const first=await read();
  assert.equal(first.partial,false);
  clock+=3_700_000;
  failure=true;
  const old=await read();
  assert.equal(old.partial,true);
  assert.equal(old.metrics.global.market_cap_usd,first.metrics.global.market_cap_usd);
  assert.equal(old.metrics.global.stale,true);
  assert.equal(JSON.stringify(old).includes('PRIVATE_UPSTREAM_ERROR'),false);
});
test('optional price and historical requests do not invalidate complete core market data',async()=>{
  let optionalCalls=0;
  const read=createMarketReader({key:'',fetchImpl:async(url)=>{
    if(url.includes('/cryptocurrency/quotes/latest')
       || url.includes('/cmc100-historical')){
      optionalCalls++;
      return new Response('',{status:429});
    }
    return cmcMock(url);
  }});
  const result=await read();
  assert.equal(result.partial,false);
  assert.deepEqual(Object.keys(result.metrics).sort(),
    ['cmc100','global','season','sentiment']);
  assert.equal(optionalCalls,2);
  await read();
  assert.equal(optionalCalls,2,'optional requests are subject to retry cooldown');
});

test('public snapshot HTTP endpoint stays GET-only and needs no user account cookies',async()=>{
  let requests=0;
  const api=createAccountApi({catalogUrl:'https://example.test/catalog',
    marketReader:async()=>{
      requests++;
      return {metrics:{global:{btc_dominance:61,source:'CoinMarketCap'}},
        partial:true,plan_configured:false,access_mode:'public',
        updated_at:'2026-11-08T00:00:00Z'};
    }});
  await new Promise(resolve=>api.listen(0,'127.0.0.1',resolve));
  try{
    const url='http://127.0.0.1:'+api.address().port+'/api/v1/cloud/market';
    const response=await fetch(url);
    assert.equal(response.status,200);
    assert.match(response.headers.get('cache-control'),/max-age=60/);
    assert.equal((await response.json()).metrics.global.btc_dominance,61);
    assert.equal((await fetch(url,{method:'POST'})).status,405);
    assert.equal(requests,1);
  }finally{api.close();}
});
