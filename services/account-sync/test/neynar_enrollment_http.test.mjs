import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {once} from 'node:events';
import {createPaidEnrollmentVerifier} from '../neynar_enrollment_voucher.mjs';
import {createNeynarRegistrationHttpHandler}
  from '../neynar_registration_http.mjs';

const wallet='0xf278cf59f82edcf871d630f28ecc8056f25c1cdb';
const now=1794562200000;
const invoice='23b837bb-3e30-4f43-a5c0-c47e0f113dfc';
test('paid enrollment is MAC-protected, custody-bound and time-limited',async()=>{
  const verifier=createPaidEnrollmentVerifier({
    secret:'a-secret-only-the-payment-backend-knows'.repeat(3),now:()=>now});
  const voucher=verifier.issue({
    invoiceId:invoice,custodyAddress:wallet,expiresAtMs:now+3600000
  });
  const approval=await verifier.authorize(voucher,wallet,'reserve');
  assert.equal(approval.approved,true);
  assert.equal(approval.subject,'paid-invoice:'+invoice);
  assert.equal(await verifier.authorize(voucher,'0x'+'0'.repeat(40)),null);
  assert.equal(await verifier.authorize(voucher.slice(0,-1)+'A',wallet),null);
  assert.equal(await verifier.authorize('anything',wallet),null);
  assert.throws(()=>verifier.issue({
    invoiceId:invoice,custodyAddress:wallet,expiresAtMs:now+90000000}),/expiry/);
  const expired=createPaidEnrollmentVerifier({
    secret:'a-secret-only-the-payment-backend-knows'.repeat(3),
    now:()=>now+3600001});
  assert.equal(await expired.authorize(voucher,wallet),null);
});
test('Neynar API rejects anonymous or cross-origin registration without touching relay',async()=>{
  let uses=0;
  const relay={
    reserve:async()=>{uses++;return {status:'reserved',fid:'123'};},
    submit:async()=>{uses++;return {status:'submitted'};},
    status:async()=>{uses++;return {status:'confirmed'};}
  };
  const handler=createNeynarRegistrationHttpHandler({
    relay,publicOrigin:'https://genycaster.app'
  });
  const server=createServer(async(req,res)=>{
    const path=new URL(req.url,'http://localhost').pathname;
    if(!(await handler(req,res,path))) {
      res.writeHead(404).end('{}');
    }
  });
  server.listen(0,'127.0.0.1');await once(server,'listening');
  try{
    const url='http://127.0.0.1:'+server.address().port
      +'/api/v1/registration/farcaster/reserve';
    const body=JSON.stringify({custody_address:wallet,
      idempotency_key:'23b837bb-3e30-4f43-a5c0-c47e0f113dfc'});
    let response=await fetch(url,{method:'POST',
      headers:{'content-type':'application/json'},body});
    assert.equal(response.status,401);
    assert.equal(uses,0);
    const token='abc'.repeat(100);
    response=await fetch(url,{method:'POST',headers:{
      'content-type':'application/json','x-genycaster-paid-enrollment':token,
      origin:'https://attacker.example'
    },body});
    assert.equal(response.status,403);
    assert.equal(uses,0);
    response=await fetch(url,{method:'POST',headers:{
      'content-type':'application/json','x-genycaster-paid-enrollment':token,
      origin:'https://genycaster.app'
    },body:JSON.stringify({...JSON.parse(body),private_key:'bad'})});
    assert.equal(response.status,400);
    assert.equal(uses,0);
    response=await fetch(url,{method:'POST',headers:{
      'content-type':'application/json','x-genycaster-paid-enrollment':token,
      origin:'https://genycaster.app'
    },body});
    assert.equal(response.status,200);
    assert.equal(uses,1);
    const result=await response.json();
    assert.equal(result.registration.fid,'123');
    assert.equal(result.registration.status,'reserved');
    assert.equal(response.headers.get('cache-control'),'no-store');
    response=await fetch(url,{method:'GET'});
    assert.equal(response.status,405);
  }finally{
    await new Promise(resolve=>server.close(resolve));
  }
});
test('public account API keeps Neynar spending disabled without secure server setup',async()=>{
  const {createAccountApi}=await import('../server.mjs');
  const server=createAccountApi({
    catalogUrl:'https://catalog.example.org/providers',
    publicOrigin:'https://genycaster.app'
  });
  server.listen(0,'127.0.0.1');await once(server,'listening');
  try{
    const response=await fetch('http://127.0.0.1:'+server.address().port
      +'/api/v1/registration/farcaster/reserve',{
      method:'POST',headers:{'content-type':'application/json'},
      body:JSON.stringify({custody_address:wallet})
    });
    assert.equal(response.status,503);
    assert.deepEqual(await response.json(),{error:'registration_not_configured'});
  }finally{
    await new Promise(resolve=>server.close(resolve));
  }
});
