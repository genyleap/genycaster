import test from 'node:test';
import assert from 'node:assert/strict';
import { createBrowserFarcasterBroker } from '../browser_farcaster_auth.mjs';

function fixture({ mismatch = false, linkedFid = '123', verificationStatus = 200 } = {}) {
  let nowMs = 1_000_000;
  let completed = false;
  const calls = [];
  const data = (payload, headers = {}) => new Response(JSON.stringify(payload), {
    status: headers.status ?? 200,
    headers: {
      'content-type': 'application/json',
      ...(headers.cookies ? { 'set-cookie': headers.cookies } : {}),
    },
  });
  // OpenProof supplies multiple Set-Cookie fields; stub getSetCookie in
  // Response.headers for compatibility with Node's fetch response contract.
  const fetchImpl = async (url, options) => {
    calls.push({ url: String(url), options });
    if (url === 'https://openproof.genyleap.com/auth/web3/start') {
      const response = data({
        nonce: 'nonce-123', domain: 'genyleap.com',
        uri: 'https://genyleap.com/account', expires_at: '2026-10-18T00:00:00Z',
      });
      response.headers.getSetCookie = () => [
        '__Host-openproof-web3-continuation=a1; Path=/',
        '__Host-openproof-web3-binding=b2; Path=/',
        '__Host-openproof-web3-transaction=c3; Path=/',
        '__Host-openproof-web3-challenge=d4; Path=/',
      ];
      return response;
    }
    if (url === 'https://relay.farcaster.xyz/v1/channel') {
      const args = JSON.parse(options.body);
      assert.equal(args.nonce, 'nonce-123');
      return data({
        channelToken: 'relay-secret-token',
        nonce: 'nonce-123',
        url: 'https://warpcast.com/~/siwf/approval',
      });
    }
    if (url === 'https://relay.farcaster.xyz/v1/channel/status') {
      assert.equal(options.headers.authorization, 'Bearer relay-secret-token');
      if (!completed) return data({ state: 'pending' }, { status: 202 });
      return data({
        state: 'completed', nonce: mismatch ? 'wrong-nonce' : 'nonce-123',
        fid: 123, message: 'signed SIWE challenge',
        signature: '0xproof', username: 'account',
      });
    }
    if (url === 'https://openproof.genyleap.com/auth/web3/complete') {
      assert.match(options.headers.cookie, /__Host-openproof-web3-binding=b2/);
      if (verificationStatus !== 200) {
        return data({ error: { code: 'AUTHENTICATION_FAILED' } }, { status: verificationStatus });
      }
      const response = data({ authenticated: true });
      response.headers.getSetCookie = () => [
        '__Host-openproof-session=verified-session; Path=/',
      ];
      return response;
    }
    if (url === 'https://openproof.genyleap.com/account/profile') {
      assert.match(options.headers.cookie, /__Host-openproof-session=verified-session/);
      return data({ identity_id: 'openproof-sub-123' });
    }
    if (url === 'https://openproof.genyleap.com/account/connections') {
      return data({ connections: [{ provider: 'farcaster', fid: linkedFid,
        preferred_username: 'account', display_name: 'Account' }] });
    }
    throw new Error('Unexpected upstream: ' + url);
  };
  const broker = createBrowserFarcasterBroker({ fetchImpl, now: () => nowMs });
  return { broker, calls, approve: () => { completed = true; },
    advance: (milliseconds = 2_000) => { nowMs += milliseconds; } };
}

test('direct Farcaster proof is accepted only after OpenProof verification and linked FID', async () => {
  const { broker, calls, approve, advance } = fixture();
  const started = await broker.start();
  assert.match(started.ticket, /^[A-Za-z0-9_-]{43}$/);
  assert.equal(started.approval_url, 'https://warpcast.com/~/siwf/approval');
  assert.equal(started.channelToken, undefined);
  assert.equal((await broker.poll(started.ticket)).pending, true);
  approve(); advance();
  const result = await broker.poll(started.ticket);
  assert.equal(result.authenticated, true);
  assert.equal(result.subject, 'openproof-sub-123');
  assert.equal(result.fid, '123');
  assert.equal(result.username, 'account');
  assert.equal(result.signature, undefined);
  assert.equal(result.cookie, undefined);
  assert.match(result.account_ticket, /^recovery\.[A-Za-z0-9_-]{43}$/);
  assert.equal(result.account_expires_in, 2700);
  assert.deepEqual(broker.authorizeRecoveryAccount('Bearer ' + result.account_ticket), {
    fid: '123', subject: 'openproof-sub-123',
    cookie: '__Host-openproof-web3-continuation=a1; __Host-openproof-web3-binding=b2; __Host-openproof-web3-transaction=c3; __Host-openproof-web3-challenge=d4; __Host-openproof-session=verified-session',
  });
  assert.equal(calls.length, 7);
  await assert.rejects(() => broker.poll(started.ticket), /auth_expired_or_unknown/);
  advance(45 * 60_000 + 1);
  assert.equal(broker.authorizeRecoveryAccount('Bearer ' + result.account_ticket), null,
    'channel-authenticated account access expires without affecting identity');
});

test('a wrong relay nonce fails closed before contacting OpenProof verification', async () => {
  const { broker, calls, approve } = fixture({ mismatch: true });
  const started = await broker.start();
  approve();
  await assert.rejects(() => broker.poll(started.ticket), /auth_invalid_farcaster_proof/);
  assert.equal(calls.filter(call => call.url.includes('web3/complete')).length, 0);
});

test('an OpenProof rejection is returned as unauthorized and consumes the relay attempt', async () => {
  const { broker, calls, approve } = fixture({ verificationStatus: 401 });
  const { ticket } = await broker.start();
  approve();
  await assert.rejects(() => broker.poll(ticket), {
    code: 'auth_openproof_not_verified', status: 401,
  });
  await assert.rejects(() => broker.poll(ticket), /auth_expired_or_unknown/);
  assert.equal(calls.filter(call => call.url.endsWith('/auth/web3/complete')).length, 1);
});

test('invalid and unknown tickets never contact an upstream service', async () => {
  const { broker, calls } = fixture();
  await assert.rejects(() => broker.poll('not-a-ticket'), /auth_invalid_ticket/);
  await assert.rejects(() => broker.poll('X'.repeat(43)), /auth_expired_or_unknown/);
  assert.equal(calls.length, 0);
});

test('Farcaster identity mismatch is rejected after the signed OpenProof ceremony', async () => {
  const { broker, approve } = fixture({ linkedFid: '999' });
  const started = await broker.start();
  approve();
  await assert.rejects(() => broker.poll(started.ticket), /auth_verified_identity_mismatch/);
});

test('pending proof cannot be polled at an unlimited rate', async () => {
  const { broker } = fixture();
  const started = await broker.start();
  assert.equal((await broker.poll(started.ticket)).pending, true);
  await assert.rejects(() => broker.poll(started.ticket), /auth_poll_too_fast/);
});

test('recovery signer proves the exact existing Farcaster FID to OpenProof without leaking keys',async()=>{
  const upstream=[];
  let timestamp=0;
  const message='genyleap.com wants you to sign in with your Ethereum account:\n0x1111111111111111111111111111111111111111\n\nFarcaster Auth\n\nURI: https://genyleap.com/account\nVersion: 1\nChain ID: 10\nNonce: abcdefgh12345\nResources:\n- farcaster://fid/123';
  const json=(obj)=>new Response(JSON.stringify(obj),{status:200,headers:{'content-type':'application/json'}});
  const fetchImpl=async(url,opts)=>{
    upstream.push({url,opts});
    if(url.endsWith('/auth/web3/start')){
      assert.deepEqual(JSON.parse(opts.body),{provider:'farcaster',fid:'123',
        address:'0x1111111111111111111111111111111111111111'});
      const r=json({provider:'farcaster',fid:'123',
        address:'0x1111111111111111111111111111111111111111',
        message,nonce:'abcdefgh12345',domain:'genyleap.com',
        statement:'Farcaster Auth',resource_prefix:'farcaster://fid/',
        expires_at:'2026-10-21T00:02:00Z'});
      r.headers.getSetCookie=()=>['__Host-openproof-a=aa; Path=/',
        '__Host-openproof-b=bb; Path=/','__Host-openproof-c=cc; Path=/',
        '__Host-openproof-d=dd; Path=/'];
      return r;
    }
    if(url.endsWith('/auth/web3/complete')){
      assert.deepEqual(JSON.parse(opts.body),{message,signature:'0x'+'ab'.repeat(64)+'1b'});
      assert.match(opts.headers.cookie,/__Host-openproof-a=aa/);
      const r=json({authenticated:true});
      r.headers.getSetCookie=()=>['__Host-openproof-session=trusted-session; Path=/'];
      return r;
    }
    assert.match(opts.headers.cookie,/__Host-openproof-session=trusted-session/);
    if(url.endsWith('/account/profile'))return json({identity_id:'canonical-123'});
    if(url.endsWith('/account/connections'))return json({
      connections:[{provider:'farcaster',fid:'123'}]});
    throw Error('unexpected '+url);
  };
  const broker=createBrowserFarcasterBroker({fetchImpl,now:()=>timestamp});
  const started=await broker.startDirect('123','0x1111111111111111111111111111111111111111');
  assert.equal(started.message,message);
  assert.equal(started.fid,'123');
  assert.equal(started.cookies,undefined);
  await assert.rejects(()=>broker.poll(started.ticket),/auth_expired_or_unknown/);
  const result=await broker.completeDirect(started.ticket,'0x'+'ab'.repeat(64)+'1b');
  assert.equal(result.authenticated,true);
  assert.equal(result.issuer,'https://openproof.genyleap.com');
  assert.equal(result.subject,'canonical-123');
  assert.equal(result.fid,'123');
  assert.match(result.account_ticket,/^recovery\.[A-Za-z0-9_-]{43}$/);
  assert.equal(result.expires_in,2700);
  assert.deepEqual(broker.authorizeRecoveryAccount('Bearer '+result.account_ticket),{
    fid:'123',subject:'canonical-123',
    cookie:'__Host-openproof-a=aa; __Host-openproof-b=bb; __Host-openproof-c=cc; __Host-openproof-d=dd; __Host-openproof-session=trusted-session',
  });
  assert.equal(upstream.length,4);
  timestamp+=45*60_000+1;
  assert.equal(broker.authorizeRecoveryAccount('Bearer '+result.account_ticket),null,
    'temporary account capability must expire even if the app does not sign out');
  await assert.rejects(
    ()=>broker.completeDirect(started.ticket,'0x'+'ab'.repeat(64)+'1b'),
    /auth_expired_or_unknown/);
  await assert.rejects(()=>broker.startDirect('123','not-an-address'),/auth_invalid_recovery_identity/);
  await assert.rejects(()=>broker.startDirect('000123','0x1111111111111111111111111111111111111111'),
    /auth_invalid_recovery_identity/);
});

test('recovery cannot claim an unrelated OpenProof FID or attacker supplied signed message',async()=>{
  const json=obj=>new Response(JSON.stringify(obj),{
    status:200,headers:{'content-type':'application/json'}});
  let steps=0;
  const broker=createBrowserFarcasterBroker({fetchImpl:async(url,opts)=>{
    steps++;
    if(url.endsWith('/auth/web3/start')){
      const r=json({provider:'farcaster',fid:'123',
        address:'0x1111111111111111111111111111111111111111',
        message:'genyleap.com wants you to sign in with your Ethereum account:\n0x1111111111111111111111111111111111111111\n\nFarcaster Auth\n\nURI: https://genyleap.com/account\nVersion: 1\nChain ID: 10\nNonce: abcdefgh12345\nResources:\n- farcaster://fid/123',
        nonce:'abcdefgh12345',domain:'genyleap.com',statement:'Farcaster Auth',
        resource_prefix:'farcaster://fid/',expires_at:'tomorrow'});
      r.headers.getSetCookie=()=>['__Host-openproof-a=aa','__Host-openproof-b=bb',
        '__Host-openproof-c=cc','__Host-openproof-d=dd'];
      return r;
    }
    if(url.endsWith('/auth/web3/complete')){
      const r=json({authenticated:true});
      r.headers.getSetCookie=()=>['__Host-openproof-session=session'];
      return r;
    }
    if(url.endsWith('/account/profile'))return json({identity_id:'one'});
    if(url.endsWith('/account/connections'))return json({
      connections:[{provider:'farcaster',fid:'999'}]});
    throw Error('unexpected');
  }});
  const started=await broker.startDirect('123','0x1111111111111111111111111111111111111111');
  await assert.rejects(()=>broker.completeDirect(started.ticket,'0x'+'ab'.repeat(64)+'1b'),
    /auth_verified_identity_mismatch/);
  assert.equal(steps,4);
  await assert.rejects(()=>broker.completeDirect(started.ticket,'0x'+'ab'.repeat(64)+'1b'),
    /auth_expired_or_unknown/);
});
