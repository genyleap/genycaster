import test from 'node:test';
import assert from 'node:assert/strict';
import {evaluateGenyAssets,createGenyProPolicy} from '../pro_entitlements.mjs';

const fid='42';
const first='0x1111111111111111111111111111111111111111';
const second='0x2222222222222222222222222222222222222222';
function fixture(balances) {
  const calls=[];
  const fetchImpl=async(url, options={})=>{
    calls.push({url,options});
    if(url.includes('verificationsByFid'))return Response.json({messages:[
      {data:{fid:42,type:'MESSAGE_TYPE_VERIFICATION_ADD_ETH_ADDRESS',
        network:'FARCASTER_NETWORK_MAINNET',verificationAddAddressBody:{address:first,protocol:'PROTOCOL_ETHEREUM'}}},
      {data:{fid:42,type:'MESSAGE_TYPE_VERIFICATION_ADD_ETH_ADDRESS',
        network:'FARCASTER_NETWORK_MAINNET',verificationAddAddressBody:{address:second,protocol:'PROTOCOL_ETHEREUM'}}},
      {data:{fid:404,type:'MESSAGE_TYPE_VERIFICATION_ADD_ETH_ADDRESS',
        network:'FARCASTER_NETWORK_MAINNET',verificationAddAddressBody:{
          address:'0x9999999999999999999999999999999999999999',protocol:'PROTOCOL_ETHEREUM'}}}
    ]});
    const rpc=JSON.parse(options.body);
    if(rpc.method==='eth_chainId')return Response.json({result:'0x2105'});
    const hex=rpc.params[0].data;
    if(hex==='0x313ce567')return Response.json({result:'0x12'});
    const address='0x'+hex.slice(-40);
    const value=balances[address.toLowerCase()];
    if(value==='error')return Response.json({error:{code:-1}});
    return Response.json({result:'0x'+(BigInt(value||0)*10n**18n).toString(16)});
  };
  return {policy:createGenyProPolicy({}),fetchImpl,calls};
}
test('browser balances only use account-verified wallets; sum is display-only',async()=>{
  const f=fixture({[first]:12000,[second]:9000});
  const result=await evaluateGenyAssets(fid,f);
  assert.equal(result.status,'verified');
  assert.equal(result.balance,21000);
  assert.equal(result.holder,true);
  assert.equal(result.available,true);
  assert.equal(result.partial,false);
  assert.deepEqual(result.wallets,[first,second]);
  assert.equal(result.addresses.length,2);
  assert.equal(result.source,'Base RPC');
  assert.equal(f.calls[0].url.includes('fid=42'),true);
  assert.equal(result.wallets.includes('0x9999999999999999999999999999999999999999'),false);
});
test('partial RPC failure is explicit, never presented as a verified zero',async()=>{
  const f=fixture({[first]:1500,[second]:'error'});
  const result=await evaluateGenyAssets(fid,f);
  assert.equal(result.status,'verified');
  assert.equal(result.balance,1500);
  assert.equal(result.partial,true);
  assert.equal(result.addresses[1].available,false);
});
test('all failed readings are unavailable, not a nonholder verdict',async()=>{
  const f=fixture({[first]:'error',[second]:'error'});
  const result=await evaluateGenyAssets(fid,f);
  assert.equal(result.status,'unavailable');
  assert.equal(result.available,false);
  assert.equal(result.partial,true);
});
