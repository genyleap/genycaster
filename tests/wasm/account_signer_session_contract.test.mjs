import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {ensureBrowserAccountSession} from '../../ui/wasm/browser_account_session.mjs';

const file=(name)=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('previously registered Farcaster signer automatically starts account ownership proof',()=>{
  const pre=file('ui/wasm/protocol-signer-pre.js');
  const bridge=file('ui/app/browser/signer_bridge.hpp');
  const runtime=file('ui/app/runtime/bindings.cpp');
  const controller=file('ui/app/identity/connections.cpp');
  assert.match(pre,/status\.phase==="ready" && status\.ready/);
  assert.match(pre,/ensureBrowserAccountSession/);
  assert.match(bridge,/kind\s*==\s*QStringLiteral\("account-session"\)/);
  assert.match(runtime,/setAccountSessionObserver/);
  assert.match(controller,/acceptBrowserAccountSession/);
  assert.match(file('ui/CMakeLists.txt'),/browser_account_session\.mjs/);
  assert.match(file('ci/package_wasm_assets.py'),/browser_account_session\.mjs/);
  assert.match(file('ui/wasm/genycaster-sw.js'),/browser_account_session\.mjs/);
  assert.match(file('ui/wasm/secure_signer_vault.mjs'),
    /genycaster-account-session-v1/);
});

test('manual Pro and Streak session retries use the registered WASM browser signer',()=>{
  const pre=file('ui/wasm/protocol-signer-pre.js');
  const controller=file('ui/app/identity/connections.cpp');
  const bridge=file('ui/app/browser/signer_bridge.hpp');
  assert.match(pre,/function retryAccountSession\(fid\)/);
  assert.match(pre,/owner !== fid \|\| !client/);
  assert.match(pre,/if \(accountSessionPromise && accountSessionOwner === fid\)/);
  assert.match(pre,/ensureBrowserAccountSession\(\{/);
  assert.match(pre,/Object\.freeze\(\{start,retryAccountSession,events\}\)/);
  assert.match(controller,/window\.__genycasterBrowserSigning/);
  assert.match(controller,/bridge\.retryAccountSession\(UTF8ToString\(\$0,\$1\)\)/);
  assert.match(controller,/if \(browserStarted\)/);
  assert.match(controller,/else\s+retryAccountSession\(\);\s*}\s*void OpenProofConnectionsController::fetchProPlan/);
  assert.match(bridge,/m_accountSessionObserver\(presentation->accountFid\(\),csrf\)/);
});

test('account-session browser client proves FID, then validates stored HttpOnly cookie',async()=>{
  const fid='321969',nonce='a'.repeat(43),csrf='b'.repeat(43);
  const principal={issuer:'https://openproof.genyleap.com',subject:'farcaster:'+fid};
  const calls=[];
  let cookie=false;
  const fetchImpl=async(path,options)=>{
    calls.push({path,options});
    if(path==='/api/v1/cloud/session') {
      return cookie?new Response(JSON.stringify({fid,...principal,csrf_token:csrf}),
        {status:200}):new Response(JSON.stringify({error:'authentication_required'}),
        {status:401});
    }
    if(path.endsWith('/challenge'))return new Response(JSON.stringify({fid,nonce}),
      {status:200});
    if(path.endsWith('/proof')) {
      const body=JSON.parse(options.body);
      assert.deepEqual(body,{fid,nonce,public_key:'c'.repeat(64),signature:'d'.repeat(128)});
      cookie=true;
      return new Response(JSON.stringify({fid,...principal,csrf_token:csrf}),{status:200});
    }
    throw Error('unexpected URL');
  };
  const vault={
    async load(identity){assert.equal(identity,fid);return {publicKeyHex:'c'.repeat(64)};},
    async signAccountChallenge(identity,challenge){
      assert.equal(identity,fid);assert.equal(challenge,nonce);
      return {publicKeyHex:'c'.repeat(64),signature:'d'.repeat(128)};
    },
  };
  const result=await ensureBrowserAccountSession({fid,fetchImpl,vault,
    origin:'https://genycaster.xyz'});
  assert.equal(result.csrf_token,csrf);
  assert.deepEqual(calls.map(x=>x.path),[
    '/api/v1/cloud/session','/api/v1/cloud/session/challenge',
    '/api/v1/cloud/session/proof','/api/v1/cloud/session']);
  for(const call of calls) {
    assert.equal(call.options.credentials,'same-origin');
    assert.equal(call.options.redirect,'error');
  }
  calls.length=0;
  await ensureBrowserAccountSession({fid,fetchImpl,vault,
    origin:'https://genycaster.xyz'});
  assert.deepEqual(calls.map(x=>x.path),['/api/v1/cloud/session']);
});

test('browser refuses an authenticated cookie from a different protocol or issuer',async()=>{
  const fid='321969',csrf='b'.repeat(43);
  const variants=[
    {fid,csrf_token:csrf,issuer:'hypersnap:native-registry',
      subject:'hypersnap:'+fid},
    {fid,csrf_token:csrf,issuer:'https://openproof.genyleap.com',
      subject:'hypersnap:'+fid},
    {fid,csrf_token:csrf,issuer:'https://untrusted.example',
      subject:'farcaster:'+fid},
    {fid,csrf_token:csrf,issuer:'https://openproof.genyleap.com',
      subject:'farcaster:999'},
    {fid,csrf_token:'bad',issuer:'https://openproof.genyleap.com',
      subject:'farcaster:'+fid}
  ];
  for(const principal of variants){
    let requests=0;
    await assert.rejects(ensureBrowserAccountSession({
      fid,origin:'https://genycaster.xyz',
      fetchImpl:async()=>{
        ++requests;
        return new Response(JSON.stringify(principal),{status:200});
      },
      vault:{load:async()=>{throw Error('must not touch signer for wrong owner');}}
    }),/differs/);
    assert.equal(requests,1,'wrong cookie owner cannot issue an account proof');
  }
});

test('browser rejects a proof receipt or restored cookie with a mismatched owner',async()=>{
  const fid='321969',nonce='a'.repeat(43),csrf='b'.repeat(43);
  const principal={fid,csrf_token:csrf,
    issuer:'https://openproof.genyleap.com',subject:'farcaster:'+fid};
  const wrong={...principal,issuer:'hypersnap:native-registry',
    subject:'hypersnap:'+fid};
  const vault={
    async load(){return {publicKeyHex:'c'.repeat(64)};},
    async signAccountChallenge(){return {
      publicKeyHex:'c'.repeat(64),signature:'d'.repeat(128)};}
  };
  for(const wrongStage of ['proof','restore']){
    let counter=0;
    const fetchImpl=async(path)=>{
      ++counter;
      if(path.endsWith('/challenge'))
        return Response.json({fid,nonce});
      if(path.endsWith('/proof'))
        return Response.json(wrongStage==='proof'?wrong:principal);
      return counter===1
        ?new Response('{}',{status:401})
        :Response.json(wrong);
    };
    await assert.rejects(ensureBrowserAccountSession({
      fid,origin:'https://genycaster.xyz',fetchImpl,vault
    }),wrongStage==='proof'?/different protocol-scoped owner/:/rejected the verified account session/);
    assert.equal(counter,wrongStage==='proof'?3:4);
  }
});

test('native cookie acceptance checks protocol-scoped issuer and subject, not only FID',()=>{
  const source=file('ui/app/identity/connections.cpp');
  const scoped=file('ui/app/identity/account_session_principal.hpp');
  assert.match(source,/#include "account_session_principal\.hpp"/);
  assert.match(scoped,/bool matchesAccountSessionPrincipal\(/);
  assert.match(scoped,/subject == QStringLiteral\("farcaster:"\) \+ canonicalFid/);
  assert.match(scoped,/subject == QStringLiteral\("hypersnap:"\) \+ canonicalFid/);
  assert.equal((source.match(/genycaster::identity::matchesAccountSessionPrincipal\(data, viewer, m_issuerUrl\)/g)||[]).length,2);
  assert.doesNotMatch(source,/data\.value\(QStringLiteral\("fid"\)\)\.toString\(\)!=QString::number\(viewer\)/);
});

test('missing signer, wrong origin and mismatched cookie cannot grant account data access',async()=>{
  await assert.rejects(ensureBrowserAccountSession({
    fid:'321969',origin:'https://evil.example',
    vault:{},fetchImpl:async()=>{throw Error('should not fetch');}
  }),/official secure origin/);
  const fetchImpl=async()=>new Response(JSON.stringify({
    fid:'999',csrf_token:'x'.repeat(43)
  }),{status:200});
  await assert.rejects(ensureBrowserAccountSession({
    fid:'321969',origin:'https://genycaster.xyz',
    vault:{},fetchImpl
  }),/differs/);
});
