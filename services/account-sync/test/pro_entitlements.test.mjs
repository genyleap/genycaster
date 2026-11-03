import test from 'node:test';
import assert from 'node:assert/strict';
import {createGenyProPolicy,verifiedFarcasterWallets,evaluateGenyPro} from '../pro_entitlements.mjs';
const first='0x1111111111111111111111111111111111111111';
const second='0x2222222222222222222222222222222222222222';
const asHex=n=>'0x'+n.toString(16).padStart(64,'0');
const walletRecord=(fid,wallet)=>({data:{fid,
  type:'MESSAGE_TYPE_VERIFICATION_ADD_ETH_ADDRESS',
  network:'FARCASTER_NETWORK_MAINNET',
  verificationAddAddressBody:{address:wallet,protocol:'PROTOCOL_ETHEREUM'}}});
function fixture(balances,options={}){
 const policy=createGenyProPolicy(options.env||{});
 const calls=[];
 const fetchImpl=async (url,request={})=>{
   calls.push({url,request});
   if(url.includes('/v1/verificationsByFid')){
     const requested=new URL(url).searchParams.get('fid');
     return new Response(JSON.stringify({messages:[
       walletRecord(Number(requested),first),walletRecord(Number(requested),second),
       walletRecord(999,'0x9999999999999999999999999999999999999999')]}),
       {status:200,headers:{'content-type':'application/json'}});
   }
   const rpc=JSON.parse(request.body);
   let result;
   if(rpc.method==='eth_chainId')result='0x2105';
   else if(rpc.params[0].data==='0x313ce567')result=asHex(18n);
   else{
     const address='0x'+rpc.params[0].data.slice(-40);
     if(balances[address.toLowerCase()]==='error')
       return new Response(JSON.stringify({jsonrpc:'2.0',error:{message:'unavailable'}}));
     result=asHex(BigInt(balances[address.toLowerCase()]||0)*10n**18n);
   }
   return new Response(JSON.stringify({jsonrpc:'2.0',id:1,result}),
      {status:200,headers:{'content-type':'application/json'}});
 };
 return {policy,fetchImpl,calls};
}
test('GENY Pro requires server-configured threshold in one verified address',async()=>{
 const f=fixture({[first]:12000,[second]:10000});
 const result=await evaluateGenyPro('42',f);
 assert.equal(result.tier,'basic');
 assert.equal(result.status,'verified');
 assert.equal(result.eligible,false);
 assert.equal(result.minimum_geny,'16000');
 assert.equal(result.limits.cast_bytes,1024);
 assert.equal(result.limits.image_embeds,1);
});
test('one qualifying wallet grants verified Pro without crossing protocol publishing limit',async()=>{
 const f=fixture({[first]:16000,[second]:0});
 const result=await evaluateGenyPro('42',f);
 assert.equal(result.tier,'pro');
 assert.equal(result.eligible,true);
 assert.equal(result.qualified_wallet,first);
 assert.equal(result.capabilities.custom_bookmark_collections,true);
 assert.equal(result.capabilities.advanced_code_editor,true);
 assert.equal(result.capabilities.extended_casts,false);
 assert.equal(result.capabilities.extra_images,false);
 assert.equal(result.capabilities.multiple_miniapps,false);
 assert.equal(result.limits.concurrent_miniapps,1);
 assert.equal(result.limits.cast_bytes,1024);
});
test('threshold is supplied by service config, not client or account metadata',async()=>{
 const f=fixture({[first]:24000,[second]:0},
   {env:{GENYCASTER_PRO_REQUIRED_GENY:'25000'}});
 assert.equal((await evaluateGenyPro('42',f)).tier,'basic');
 assert.equal((await verifiedFarcasterWallets('42',f)).length,2);
 assert.equal(f.calls[0].url.includes('fid=42'),true);
 assert.throws(()=>createGenyProPolicy({GENYCASTER_PRO_REQUIRED_GENY:'-1'}));
 assert.throws(()=>createGenyProPolicy({GENYCASTER_PRO_REQUIRED_GENY:'0'}));
});
test('chain and partial RPC failures never fabricate a qualifying membership',async()=>{
 const f=fixture({[first]:'error',[second]:10000});
 const result=await evaluateGenyPro('42',f);
 assert.equal(result.eligible,false);
 assert.equal(result.status,'unavailable');
 const allFailed=fixture({[first]:'error',[second]:'error'});
 assert.equal((await evaluateGenyPro('42',allFailed)).status,'unavailable');
});
test('no caller can invent wallets not returned by trusted verification source',async()=>{
 const f=fixture({[first]:0,[second]:0,
   '0x9999999999999999999999999999999999999999':1000000});
 const result=await evaluateGenyPro('42',f);
 assert.equal(result.tier,'basic');
 assert.equal(result.verified_wallet_count,2);
});
