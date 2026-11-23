import test from 'node:test';
import assert from 'node:assert/strict';
import {mediaEntitlementSignature,authorizeMediaEntitlement,verifiedMediaPro}
  from '../media_entitlements.mjs';
import {createMediaTierReader} from '../../media-gateway/media_entitlements.mjs';
import {createCloudHandler} from '../cloud_state.mjs';

const secret='c'.repeat(64);
const now=Date.parse('2026-11-23T10:00:00Z');
const timestamp=String(Math.floor(now/1000));
function signed(fid='1234',signature=mediaEntitlementSignature(secret,fid,timestamp),time=timestamp) {
  return {url:'/api/v1/cloud/media/pro?fid='+fid,headers:{
    'x-smart-media-time':time,'x-smart-media-signature':signature,
  }};
}
test('trusted media tier check binds Farcaster FID, time and HMAC',()=>{
  assert.equal(authorizeMediaEntitlement(signed(),{secret,now:()=>now}),'1234');
  assert.equal(authorizeMediaEntitlement(signed('1234','0'.repeat(64)),{secret,now:()=>now}),null);
  assert.equal(authorizeMediaEntitlement(signed('1235',mediaEntitlementSignature(secret,'1234',timestamp)),{secret,now:()=>now}),null);
  assert.equal(authorizeMediaEntitlement(signed('0'),{secret,now:()=>now}),null);
  assert.equal(authorizeMediaEntitlement(signed('1234',undefined,String(Math.floor(now/1000)-61)),{secret,now:()=>now}),null);
  assert.equal(authorizeMediaEntitlement(signed(),{secret:'',now:()=>now}),null);
});
test('database subscription is the only Pro authorization, expires by server clock',async()=>{
  const store={
    proBillingReady:()=>true,
    getProSubscription:async principal=>{
      assert.deepEqual(principal,{issuer:'https://opp.genyleap.com',subject:'farcaster:1234'});
      return {expires_at:new Date(Date.now()+60000)};
    },
  };
  assert.equal(await verifiedMediaPro(store,'https://opp.genyleap.com','1234'),true);
  assert.equal(await verifiedMediaPro({...store,getProSubscription:async()=>({expires_at:new Date(Date.now()-60000)})},
    'https://opp.genyleap.com','1234'),false);
  assert.equal(await verifiedMediaPro({...store,proBillingReady:()=>false},
    'https://opp.genyleap.com','1234'),null);
});
test('cloud account endpoint never exposes Pro status without signed service proof',async()=>{
  const store={
    proBillingReady:()=>true,
    getProSubscription:async({issuer,subject})=>{
      assert.equal(issuer,'https://openproof.genyleap.com');
      assert.equal(subject,'farcaster:1234');
      return {expires_at:new Date(Date.now()+60000)};
    },
  };
  const cloud=createCloudHandler({
    store,issuer:'https://openproof.genyleap.com',
    mediaEntitlementSecret:secret,
  });
  async function send(request) {
    const result={status:0,body:null,writeHead(status){this.status=status;return this;},
      end(value){this.body=JSON.parse(value);}};
    await cloud({...request,method:'GET'},result,'/api/v1/cloud/media/pro');
    return result;
  }
  const currentStamp=String(Math.floor(Date.now()/1000));
  const good=await send(signed('1234',
    mediaEntitlementSignature(secret,'1234',currentStamp),currentStamp));
  assert.equal(good.status,200);
  assert.deepEqual(good.body,{eligible:true});
  const denied=await send(signed('1234','0'.repeat(64),currentStamp));
  assert.equal(denied.status,403);
  assert.equal(denied.body.error,'media_entitlement_forbidden');
});

test('media gateway Pro oracle fails closed and never leaks shared credentials',async()=>{
  let count=0;
  const reader=createMediaTierReader({secret,now:()=>now,fetchImpl:async(url,options)=>{
    count++;
    assert.equal(url.pathname,'/api/v1/cloud/media/pro');
    assert.equal(url.searchParams.get('fid'),'1234');
    assert.equal(options.headers['x-smart-media-signature'],
      mediaEntitlementSignature(secret,'1234',timestamp));
    assert.equal(options.method,'GET');
    return Response.json({eligible:true});
  }});
  assert.equal(await reader('1234'),true);
  assert.equal(count,1);
  assert.equal(await reader('0'),false);
  assert.equal(await createMediaTierReader({secret:'',fetchImpl:async()=>{throw Error('must not fetch')}})('1234'),false);
  assert.equal(await createMediaTierReader({secret,fetchImpl:async()=>{throw Error('offline')}})('1234'),false);
});
