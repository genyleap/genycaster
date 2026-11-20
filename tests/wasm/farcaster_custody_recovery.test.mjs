import test from 'node:test';
import assert from 'node:assert/strict';
import {verifyFarcasterRecovery} from '../../ui/wasm/farcaster_custody_recovery.mjs';

const phrase='abandon '.repeat(23)+'art'; // public BIP39 test vector, NOT a real account
const address='0xf278cf59f82edcf871d630f28ecc8056f25c1cdb';
const fid='42';
function harness(mode='valid'){
  const calls=[];
  const fetchImpl=async(url,options)=>{
    calls.push({url,options});
    assert.equal(url,'https://mainnet.optimism.io');
    assert.equal(options.method,'POST');
    assert.equal(options.credentials,'omit');
    assert.ok(!options.body.includes(phrase));
    const req=JSON.parse(options.body);
    let result;
    if(req.method==='eth_chainId')result=mode==='wrong-chain'?'0x1':'0xa';
    else if(req.params[0].data.startsWith('0x'+
      '7036a3f8'))throw new Error('unexpected fixed selector');
    else if(req.params[0].data.endsWith(address.slice(2)))
      result='0x'+(mode==='unregistered'?'0':BigInt(fid).toString(16)).padStart(64,'0');
    else result='0x'+'0'.repeat(24)+(mode==='transferred'?'5'.repeat(40):address.slice(2));
    return new Response(JSON.stringify({jsonrpc:'2.0',result,id:1}),{status:200});
  };
  return {fetchImpl,calls};
}
test('custody recovery reads chain registry and never opens a signer vault or submits a write',async()=>{
  const h=harness();
  const seen=[];
  const result=await verifyFarcasterRecovery(phrase,{
    fetchImpl:h.fetchImpl,
    onVerifiedCustody:async(id,wallet)=>{
      seen.push({id,address:wallet.address});
    },
  });
  assert.equal(result,fid);
  assert.deepEqual(seen,[{id:fid,address}]);
  assert.equal(h.calls.length,3);
  for(const call of h.calls){
    assert.equal(JSON.parse(call.options.body).method==='eth_call'
      ||JSON.parse(call.options.body).method==='eth_chainId',true);
    assert.ok(!call.url.includes('submitMessage'));
  }
});
test('unregistered phrase returns distinct error, never a false login',async()=>{
  const h=harness('unregistered');
  await assert.rejects(verifyFarcasterRecovery(phrase,{fetchImpl:h.fetchImpl}),
    error=>error.code==='recovery-unregistered');
  assert.equal(h.calls.length,2);
});
test('transferred FID is refused even if idOf still claims a FID',async()=>{
  const h=harness('transferred');
  await assert.rejects(verifyFarcasterRecovery(phrase,{fetchImpl:h.fetchImpl}),
    error=>error.code==='recovery-ownership');
});
test('wrong chain must fail before looking up FIDs',async()=>{
  const h=harness('wrong-chain');
  await assert.rejects(verifyFarcasterRecovery(phrase,{fetchImpl:h.fetchImpl}),
    error=>error.code==='recovery-verification');
  assert.equal(h.calls.length,1);
});
test('Snapchain and optional OpenProof failures cannot block verified custody sign-in',async()=>{
  const h=harness();
  const result=await verifyFarcasterRecovery(phrase,{
    fetchImpl:h.fetchImpl,
    onVerifiedCustody:async()=>{throw new Error('service unavailable');}
  });
  assert.equal(result,fid);
});
test('user cancellation prevents committing a FID',async()=>{
  const controller=new AbortController();
  const h=harness();
  const fetchImpl=async(...args)=>{
    const result=await h.fetchImpl(...args);
    controller.abort();
    return result;
  };
  await assert.rejects(verifyFarcasterRecovery(phrase,{
    signal:controller.signal,fetchImpl,
  }),error=>error.code==='user-cancelled');
});
