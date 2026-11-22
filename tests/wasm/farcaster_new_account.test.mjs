import assert from 'node:assert/strict';
import test from 'node:test';
import {webcrypto} from 'node:crypto';
import {deriveFarcasterCustody} from '../../ui/wasm/farcaster_recovery.mjs';
import {verifyFarcasterTransfer} from '../../services/account-sync/neynar_id_registry.mjs';
import {createBrowserFarcasterRegistration} from '../../ui/wasm/farcaster_new_account.mjs';

const phrase=('abandon '.repeat(23)+'art').trim();
const browser={isSecureContext:true,crypto:globalThis.crypto||webcrypto,
  location:{origin:'https://genycaster.xyz'}};
const send=(status,json)=>new Response(JSON.stringify(json),{
  status,headers:{'content-type':'application/json'}});
const voucher='v'.repeat(85);
const a=async()=>{const value=await deriveFarcasterCustody(phrase);
  const addr=value.address;value.dispose();return addr;};

test('public Farcaster allocator is never assumed present; no phrase is collected when it is absent',async()=>{
  const client=createBrowserFarcasterRegistration({browser,
    fetchImpl:async()=>send(404,{error:'not_found'})});
  await assert.rejects(client.preflight(),/official Farcaster signup.*Existing account/);
});
test('rejects cross-site origins, invalid vouchers and missing explicit consent without allocation',async()=>{
  assert.throws(()=>createBrowserFarcasterRegistration({
    browser:{...browser,location:{origin:'https://elsewhere.example'}}}));
  let calls=0;
  const client=createBrowserFarcasterRegistration({browser,
    fetchImpl:async()=>{calls++;throw new Error('unexpected network request');}});
  await assert.rejects(client.register(phrase,{confirmIrreversible:true,voucher:'bad',username:'geny'}),/voucher/);
  await assert.rejects(client.register(phrase,{voucher,username:'geny'}),/consent/);
  assert.equal(calls,0);
});
test('valid paid voucher signs exact Optimism IdRegistry Transfer and confirms server-proved FID only',async()=>{
  const address=await a();
  const sequence=[];
  let lease='',submitted=false;
  const client=createBrowserFarcasterRegistration({browser,delay:async()=>{},
    fetchImpl:async(url,opts)=>{
      const path=new URL(url).pathname;
      sequence.push(path);
      const payload=JSON.parse(opts.body);
      if(path.endsWith('/reserve')) {
        lease=payload.idempotency_key;
        assert.match(lease,/^[0-9a-f-]{36}$/i);
        assert.equal(payload.custody_address,address);
        assert.equal(opts.headers['x-genycaster-paid-enrollment'],voucher);
        return send(200,{registration:{fid:'4221',reservation_id:lease,
          custody_address:address,status:'reserved',id_registry_nonce:'3'}});
      }
      if(path.endsWith('/submit')){
        submitted=true;
        assert.equal(payload.reservation_id,lease);
        assert.equal(payload.fname,'geny');
        assert.equal(payload.custody_address,address);
        assert.equal(verifyFarcasterTransfer({
          fid:'4221',to:address,nonce:'3',deadline:payload.deadline,
          signature:payload.signature}),true);
        return send(200,{registration:{fid:'4221',reservation_id:lease,
          custody_address:address,status:'submitted'}});
      }
      if(path.endsWith('/status')){
        assert.equal(submitted,true);
        return send(200,{registration:{fid:'4221',reservation_id:lease,
          custody_address:address,status:'confirmed'}});
      }
      throw new Error('unexpected route');
    }});
  const result=await client.register(phrase,{voucher,username:'geny',
    confirmIrreversible:true});
  assert.equal(result.fid,'4221');
  assert.deepEqual(sequence.map(x=>x.split('/').pop()),['reserve','submit','status']);
});
test('unverified server FID never creates a browser identity, and no second POST is attempted',async()=>{
  let submissions=0;const address=await a();
  const client=createBrowserFarcasterRegistration({browser,
    fetchImpl:async(url,opts)=>{
      const path=new URL(url).pathname;
      if(path.endsWith('/reserve'))return send(200,{registration:{
        reservation_id:JSON.parse(opts.body).idempotency_key,custody_address:address,
        status:'reserved',fid:'4221',id_registry_nonce:'3'}});
      if(path.endsWith('/submit')){
        submissions++;return send(200,{registration:{reservation_id:'different',
          custody_address:address,status:'submitted',fid:'4221'}});
      }
      throw new Error('unexpected route');
    }});
  await assert.rejects(client.register(phrase,{
    voucher,username:'geny',confirmIrreversible:true}),/identity mismatch/);
  assert.equal(submissions,1);
  await assert.rejects(client.register(phrase,{
    voucher,username:'geny',confirmIrreversible:true}),/already pending/);
  assert.equal(submissions,1);
});

test('same paid voucher and custody keep an identical durable FID reservation key after browser reload',async()=>{
  const ids=[];
  const fresh=()=>createBrowserFarcasterRegistration({
    browser,fetchImpl:async(url,opts)=>{
      ids.push(JSON.parse(opts.body).idempotency_key);
      return send(503,{error:'registration_unavailable'});
    }
  });
  for(let i=0;i<2;i++)
    await assert.rejects(fresh().register(phrase,{
      voucher,username:'geny',confirmIrreversible:true}),/registration_unavailable/);
  assert.equal(ids.length,2);
  assert.equal(ids[0],ids[1]);
});
