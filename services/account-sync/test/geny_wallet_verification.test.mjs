import test from 'node:test';
import assert from 'node:assert/strict';
import {createGenyAssetsPolicy,verifiedFarcasterWallets,evaluateGenyAssets}
  from '../geny_assets.mjs';

const first='0x'+'11'.repeat(20);
const second='0x'+'22'.repeat(20);
const foreign='0x'+'99'.repeat(20);
function add(fid,address,overrides={}) {
  return {data:{fid,type:'MESSAGE_TYPE_VERIFICATION_ADD_ETH_ADDRESS',
    network:'FARCASTER_NETWORK_MAINNET',
    verificationAddAddressBody:{address,protocol:'PROTOCOL_ETHEREUM'},
    ...overrides}};
}
function fixture(messages,options={}) {
  const policy=createGenyAssetsPolicy(options.env||{});
  const calls=[];
  const fetchImpl=async url=>{
    calls.push(String(url));
    return Response.json({messages,nextPageToken:options.nextPageToken||''});
  };
  return {policy,fetchImpl,calls};
}
test('asset policy contains no USD price or Pro membership threshold',()=>{
  const policy=createGenyAssetsPolicy();
  assert.equal(policy.contract,'0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b');
  assert.equal('minimumGeny' in policy,false);
  assert.equal('monthlyUsdCents' in policy,false);
  assert.throws(()=>createGenyAssetsPolicy({GENYCASTER_GENY_RPC_URL:'http://localhost'}));
});
test('verified Farcaster addresses cannot be spoofed by a foreign FID or field',async()=>{
  const f=fixture([add(42,first),add(42,second),add(42,first),
    add(13,foreign),{data:{fid:42,verificationAddAddressBody:{address:foreign}}},
    add(42,foreign,{network:'FARCASTER_NETWORK_TESTNET'})]);
  assert.deepEqual(await verifiedFarcasterWallets('42',f),[first,second]);
  assert.equal(f.calls.length,1);
  assert.equal(new URL(f.calls[0]).searchParams.get('fid'),'42');
  await assert.rejects(verifiedFarcasterWallets('not-a-fid',f),/invalid_fid/);
});
test('malformed verification payloads and paginated replay fail closed',async()=>{
  const f=fixture([], {nextPageToken:'replay'});
  await assert.rejects(verifiedFarcasterWallets('42',f),/provider_unavailable/);
  const broken=fixture([]);
  broken.fetchImpl=async()=>Response.json({messages:{}});
  await assert.rejects(verifiedFarcasterWallets('42',broken),/provider_unavailable/);
});
test('asset holdings never directly produce a Pro entitlement',async()=>{
  const f=fixture([]);
  const holding=await evaluateGenyAssets('42',f);
  assert.equal(holding.available,false);
  assert.equal('eligible' in holding,false);
  assert.equal('tier' in holding,false);
});
