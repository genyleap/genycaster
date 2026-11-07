import assert from 'node:assert/strict';
import test from 'node:test';
import {verifyRecoveredFarcasterIdentity} from '../../ui/wasm/openproof_recovery.mjs';
import {deriveFarcasterCustody} from '../../ui/wasm/farcaster_recovery.mjs';
import {Signature} from '../../ui/wasm/vendor/noble-secp256k1.mjs';
import {fromHex,hex} from '../../ui/wasm/farcaster_protocol.mjs';
import {keccak256} from '../../ui/wasm/browser_protocol_signer.mjs';

test('custody EIP-191 personal-sign uses a recoverable signature and never exports phrase',async()=>{
  const custody=await deriveFarcasterCustody('abandon '.repeat(23)+'art');
  try{
    const message='genyleap.com wants you to sign in with your Ethereum account: Farcaster Auth farcaster://fid/123';
    const sig=await custody.signMessage(message);
    assert.match(sig,/^0x[0-9a-f]{130}$/);
    assert.ok(['1b','1c'].includes(sig.slice(-2)));
    const bytes=fromHex(sig,65);
    const encoded=new TextEncoder().encode(message);
    const digest=keccak256(new Uint8Array([
      ...new TextEncoder().encode('\x19Ethereum Signed Message:\n'+encoded.length),
      ...encoded,
    ]));
    const signature=Signature.fromBytes(bytes.subarray(0,64))
      .addRecoveryBit(bytes[64]-27);
    assert.equal(signature.hasHighS(),false);
    const pub=signature.recoverPublicKey(digest).toBytes(false);
    assert.equal('0x'+hex(keccak256(pub.subarray(1)).slice(-20)),custody.address);
    const second=await custody.signMessage(message+' fresh nonce');
    assert.notEqual(sig,second);
    await assert.rejects(()=>custody.signMessage('short'),/invalid OpenProof challenge/);
  }finally{custody.dispose();}
});

test('recovery forwards only canonical FID, public custody address and signed one-time proof',async()=>{
  const origin='https://identity-gateway.example.test';
  const challenge='genyleap.com Farcaster Auth - farcaster://fid/123 -- nonce ABCDEFGHIJKL';
  const calls=[];
  const custody={address:'0x1111111111111111111111111111111111111111',
    signMessage:async text=>{assert.equal(text,challenge);return '0x'+'ab'.repeat(64)+'1b';}};
  const fetchImpl=async(url,options)=>{
    calls.push({url,options});
    assert.equal(options.credentials,'omit');
    assert.equal(options.redirect,'error');
    assert.doesNotMatch(options.body,/mnemonic|recovery_phrase|seed|privateKey/);
    const payload=url.endsWith('/start')
      ?{ticket:'A'.repeat(43),fid:'123',message:challenge}
      :{authenticated:true,issuer:'https://openproof.genyleap.com',
        fid:'123',subject:'central-account-123',
        account_ticket:'recovery.'+'A'.repeat(43),expires_in:2700};
    return new Response(JSON.stringify(payload),{
      status:200,headers:{'content-type':'application/json'}});
  };
  assert.deepEqual(await verifyRecoveredFarcasterIdentity({
    fid:'123',custody,gatewayOrigin:origin,fetchImpl,
  }),{issuer:'https://openproof.genyleap.com',subject:'central-account-123',fid:'123',
    accountTicket:'recovery.'+'A'.repeat(43),expiresSeconds:2700});
  assert.deepEqual(JSON.parse(calls[0].options.body),{
    fid:'123',address:custody.address});
  assert.deepEqual(JSON.parse(calls[1].options.body),{
    ticket:'A'.repeat(43),signature:'0x'+'ab'.repeat(64)+'1b'});
});

test('mismatched identity or invalid HTTPS gateway fails closed, offline error is recoverable',async()=>{
  const custody={address:'0x1111111111111111111111111111111111111111',
    signMessage:async()=> '0x'+'ab'.repeat(64)+'1b'};
  await assert.rejects(()=>verifyRecoveredFarcasterIdentity({
    fid:'123',custody,gatewayOrigin:'http://example.test',
  }),/HTTPS/);
  const badFetch=async()=>new Response(JSON.stringify({
    ticket:'A'.repeat(43),fid:'456',
    message:'farcaster://fid/456 only'.padEnd(45,'.')
  }),{status:200,headers:{'content-type':'application/json'}});
  await assert.rejects(()=>verifyRecoveredFarcasterIdentity({
    fid:'123',custody,gatewayOrigin:'https://safe.example.test',
    fetchImpl:badFetch,
  }),/Invalid OpenProof recovery challenge/);
  await assert.rejects(()=>verifyRecoveredFarcasterIdentity({
    fid:'123',custody,gatewayOrigin:'https://safe.example.test',
    fetchImpl:async()=>{throw Error('network down');},
  }),/network down/);
});

test('Setup session adopts only verified OpenProof issuer/subject with the same one-shot recovery event',async()=>{
  const {readFileSync}=await import('node:fs');
  const read=path=>readFileSync(new URL(path,import.meta.url),'utf8');
  const pre=read('../../ui/wasm/protocol-signer-pre.js');
  const qt=read('../../ui/app/browser/signer_bridge.hpp');
  const runtime=read('../../ui/app/runtime/bindings.cpp');
  const auth=read('../../ui/app/identity/connections.cpp');
  assert.match(pre,/openProofGatewayOrigin/);
  assert.match(pre,/identityIssuer:verifiedOpenProof\?\.issuer/);
  assert.match(pre,/accountTicket:verifiedOpenProof\?\.accountTicket/);
  assert.match(pre,/accountExpiresSeconds:verifiedOpenProof\?\.expiresSeconds/);
  assert.match(qt,/enteredRecovery && verified[\s\S]*?identitySubject/);
  assert.match(qt,/enteredRecovery && verified[\s\S]*?accountTicket/);
  assert.match(runtime,/openProofIssuer == connections\.identityIssuer\(\)/);
  assert.match(runtime,/bridge\.accountFid\(\) == ownerFid/);
  assert.match(runtime,/Qt::QueuedConnection/);
  assert.match(auth,/ticketFormat\.match\(QString::fromLatin1\(ticket\)\)/);
  assert.match(auth,/fid != m_viewerFid/);
  assert.doesNotMatch(pre,/JSON\.stringify\(\{[^}]*?mnemonic/);
});
