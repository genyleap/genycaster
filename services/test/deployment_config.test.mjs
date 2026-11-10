import test from 'node:test';
import assert from 'node:assert/strict';
import {serverDeployment, trustedOrigin} from '../deployment_config.mjs';
import {createMediaGateway,loadConfig} from '../media-gateway/gateway.mjs';
import {defaultBrowserImageHosts,validImageUrl} from '../media-gateway/browser_image_proxy.mjs';
import {createBrowserFarcasterBroker} from '../media-gateway/browser_farcaster_auth.mjs';

test('a self-hosted domain changes the complete trusted browser allowlist', async () => {
  const deployment = serverDeployment({
    GENYCASTER_PUBLIC_ORIGIN:'https://app.example.org',
    GENYCASTER_OPENPROOF_ISSUER:'https://issuer.example.org',
    GENYCASTER_OPENPROOF_BROWSER_BASE:'https://example.org/openproof',
    GENYCASTER_BROWSER_GATEWAY_ORIGIN:'https://storage.example.org',
    GENYLEAP_MEDIA_PUBLIC_BASE_URL:'https://media.example.org',
    GENYCASTER_SNAPCHAIN_URL:'https://snap.example.org',
    GENYCASTER_HYPERSNAP_ORIGIN:'https://hyper.example.org',
  });
  assert.equal(deployment.publicOrigin,'https://app.example.org');
  assert.equal(deployment.openProofIssuer,'https://issuer.example.org');
  const config=loadConfig({
    GENYCASTER_PUBLIC_ORIGIN:deployment.publicOrigin,
    GENYCASTER_OPENPROOF_ISSUER:deployment.openProofIssuer,
    GENYCASTER_OPENPROOF_BROWSER_BASE:deployment.openProofBrowserBase,
    GENYCASTER_HYPERSNAP_ORIGIN:deployment.hypersnapOrigin,
    GENYCASTER_BROWSER_GATEWAY_ORIGIN:deployment.browserGateway,
    GENYLEAP_MEDIA_PUBLIC_BASE_URL:deployment.mediaPublicOrigin,
    GENYLEAP_MEDIA_SNAPCHAIN_BASE_URL:deployment.snapchainOrigin,
    GENYLEAP_MEDIA_TICKET_SECRET:'0123456789abcdef0123456789abcdef',
  });
  const server=createMediaGateway(config,{},{
    farcasterBroker:{},
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const address='http://127.0.0.1:'+server.address().port;
  try {
    const approved=await fetch(address+'/v1/browser/cast-views',{
      method:'OPTIONS',headers:{origin:'https://app.example.org'}});
    assert.equal(approved.status,204);
    assert.equal(approved.headers.get('access-control-allow-origin'),'https://app.example.org');
    const old=await fetch(address+'/v1/browser/cast-views',{
      method:'OPTIONS',headers:{origin:'https://caster.genyleap.com'}});
    assert.equal(old.status,403);
    assert.equal(old.headers.get('access-control-allow-origin'),null);
    const spoof=await fetch(address+'/v1/browser/cast-views',{
      method:'OPTIONS',headers:{origin:'https://app.example.org.evil.invalid'}});
    assert.equal(spoof.status,403);
  }finally {
    await new Promise(resolve=>server.close(resolve));
  }
});

test('host injection, insecure schemes, IP literals and credential URLs fail closed',()=>{
  const values=[
    'http://app.example.org','https://user:pass@app.example.org',
    'https://app.example.org:444','https://127.0.0.1',
    'https://app.example.org\\evil.invalid',
    'https://app.example.org/?next=https://evil.invalid',
    'https://app.example.org#fragment',
    'https://app.example.org\nx-malice: 1',
    'javascript:alert(1)', 'https://app.example.org.evil.invalid/path',
    'https://app.example.org";alert(1)',
  ];
  for(const value of values)assert.throws(
    ()=>trustedOrigin(value,'TEST_DEPLOYMENT'),{name:'Error'});
  assert.throws(()=>serverDeployment({
    GENYCASTER_PUBLIC_ORIGIN:'http://app.example.org',
  }),/HTTPS/);
});

test('custom media host permits only bounded managed asset paths',()=>{
  const hosts=[...defaultBrowserImageHosts,'media.example.org'];
  assert.equal(validImageUrl('https://media.example.org/m/v1/image.png',hosts,'media.example.org')?.hostname,
    'media.example.org');
  assert.equal(validImageUrl('https://media.example.org/private/keys',hosts,'media.example.org'),null);
  assert.equal(validImageUrl('https://evil.media.example.org/m/v1/image.png',hosts,'media.example.org'),null);
  assert.equal(validImageUrl('https://media.example.org/v1/browser/images?url=https://evil.invalid',hosts,'media.example.org'),null);
});

test('custom OpenProof browser authority rejects challenges for the former domain',async()=>{
  const response=()=>new Response(JSON.stringify({
    nonce:'abcdefgh12345',domain:'genyleap.com',
    uri:'https://genyleap.com/account',expires_at:'2026-11-11T00:00:00Z'
  }),{status:200,headers:{'content-type':'application/json'}});
  const broker=createBrowserFarcasterBroker({
    issuerOrigin:'https://identity.example.org',
    connectionBrowserBase:'https://example.org/openproof',
    fetchImpl:async()=>{
      const r=response();
      r.headers.getSetCookie=()=>[
        '__Host-openproof-a=aa','__Host-openproof-b=bb',
        '__Host-openproof-c=cc','__Host-openproof-d=dd'
      ];
      return r;
    }
  });
  await assert.rejects(()=>broker.start(),/auth_invalid_openproof_challenge/);
});
