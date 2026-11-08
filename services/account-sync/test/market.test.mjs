import test from 'node:test';
import assert from 'node:assert/strict';
import {createMarketReader} from '../market.mjs';
import {createAccountApi} from '../server.mjs';

const json=(data)=>new Response(JSON.stringify(data),{
  status:200,headers:{'content-type':'application/json'}});
function cmcMock(path){
  if(path.includes('global-metrics'))return json({status:{error_code:0},data:{
    btc_dominance:61,eth_dominance:10,quote:{USD:{
      total_market_cap:2_500_000_000_000,total_volume_24h:95_000_000_000}},last_updated:'2026-11-08T00:00:00Z'}});
  if(path.includes('fear-and-greed'))return json({status:{error_code:0},data:{
    value:33,value_classification:'Fear',update_time:'2026-11-08T00:00:00Z'}});
  if(path.includes('altcoin-season'))return json({status:{error_code:0},data:{
    altcoin_index:41,snapshot_time:'2026-11-08T00:00:00Z'}});
  if(path.includes('cmc100'))return json({status:{error_code:0},data:{
    value:151.2,value_24h_percentage_change:-0.4,last_update:'2026-11-08T00:00:00Z'}});
  throw Error('unexpected_url');
}
test('CMC authenticated metrics are aggregated once per shared TTL without credential leakage',async()=>{
  let calls=0,clock=Date.now();
  const read=createMarketReader({key:'secret-not-for-clients',now:()=>clock,
    fetchImpl:async(url,options)=>{calls++;
      assert.equal(options.headers['X-CMC_PRO_API_KEY'],'secret-not-for-clients');
      return cmcMock(url);
    }});
  const [a,b]=await Promise.all([read(),read()]);
  assert.equal(calls,4);
  assert.deepEqual(a,b);
  assert.equal(a.partial,false);
  assert.equal(a.metrics.global.btc_dominance,61);
  assert.equal(a.metrics.global.volume_24h_usd,95_000_000_000);
  assert.equal(a.metrics.sentiment.value,33);
  assert.equal(a.metrics.season.value,41);
  assert.equal(a.metrics.cmc100.change_24h_percent,-0.4);
  assert.equal(JSON.stringify(a).includes('secret-not-for-clients'),false);
  await read();assert.equal(calls,4);
  clock+=11*60_000;await read();assert.equal(calls,5);
});
test('without a commercial key, only explicitly attributed alternative sentiment is exposed',async()=>{
  let calls=0;
  const read=createMarketReader({key:'',fetchImpl:async(url)=>{
    assert.match(url,/^https:\/\/api\.alternative\.me\/fng/);
    calls++;return json({data:[{value:'27',value_classification:'Fear',
      timestamp:'1794114000'}]});
  }});
  const value=await read();
  assert.equal(calls,1);
  assert.equal(value.partial,true);
  assert.equal(value.plan_configured,false);
  assert.equal(value.metrics.sentiment.value,27);
  assert.equal(value.metrics.sentiment.source,'Alternative.me');
  assert.equal(value.metrics.global,undefined);
});
test('upstream failures do not leak error messages or replace last valid snapshot with zeros',async()=>{
  let clock=Date.now(),failure=false;
  const reader=createMarketReader({key:'testing',now:()=>clock,
    fetchImpl:async(url)=>{
      if(failure)throw Error('PRIVATE_UPSTREAM_ERROR');
      return cmcMock(url);
    }});
  const first=await reader();assert.equal(first.partial,false);
  clock+=3_700_000;failure=true;
  const old=await reader();
  assert.equal(old.partial,true);
  assert.equal(old.metrics.global.market_cap_usd,first.metrics.global.market_cap_usd);
  assert.equal(old.metrics.global.stale,true);
  assert.equal(JSON.stringify(old).includes('PRIVATE_UPSTREAM_ERROR'),false);
});
test('public market endpoint is GET-only, bounded, and requires no login cookies',async()=>{
  let requests=0;
  const api=createAccountApi({catalogUrl:'https://example.test/catalog',
    marketReader:async()=>{requests++;return {
      metrics:{sentiment:{value:42,source:'Alternative.me'}},
      partial:true,plan_configured:false,updated_at:'2026-11-08T00:00:00Z'};}});
  await new Promise(resolve=>api.listen(0,'127.0.0.1',resolve));
  try{
    const url='http://127.0.0.1:'+api.address().port+'/api/v1/cloud/market';
    const response=await fetch(url);
    assert.equal(response.status,200);
    assert.match(response.headers.get('cache-control'),/max-age=60/);
    assert.equal((await response.json()).metrics.sentiment.value,42);
    assert.equal((await fetch(url,{method:'POST'})).status,405);
    assert.equal(requests,1);
  }finally{api.close();}
});
