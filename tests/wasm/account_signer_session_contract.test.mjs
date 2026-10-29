import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {ensureBrowserAccountSession} from '../../ui/wasm/browser_account_session.mjs';

const file=(name)=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('previously registered Farcaster signer automatically starts account ownership proof',()=>{
  const pre=file('ui/wasm/protocol-signer-pre.js');
  const bridge=file('ui/mobile/browser_signer_bridge.hpp');
  const runtime=file('ui/mobile/runtime_ui_bindings.cpp');
  const controller=file('ui/mobile/openproof_connections_controller.cpp');
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

test('account-session browser client proves FID, then validates stored HttpOnly cookie',async()=>{
  const fid='321969',nonce='a'.repeat(43),csrf='b'.repeat(43);
  const calls=[];
  let cookie=false;
  const fetchImpl=async(path,options)=>{
    calls.push({path,options});
    if(path==='/api/v1/cloud/session') {
      return cookie?new Response(JSON.stringify({fid,csrf_token:csrf}),
        {status:200}):new Response(JSON.stringify({error:'authentication_required'}),
        {status:401});
    }
    if(path.endsWith('/challenge'))return new Response(JSON.stringify({fid,nonce}),
      {status:200});
    if(path.endsWith('/proof')) {
      const body=JSON.parse(options.body);
      assert.deepEqual(body,{fid,nonce,public_key:'c'.repeat(64),signature:'d'.repeat(128)});
      cookie=true;
      return new Response(JSON.stringify({fid,csrf_token:csrf}),{status:200});
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
    origin:'https://caster.genyleap.com'});
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
    origin:'https://caster.genyleap.com'});
  assert.deepEqual(calls.map(x=>x.path),['/api/v1/cloud/session']);
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
    fid:'321969',origin:'https://caster.genyleap.com',
    vault:{},fetchImpl
  }),/differs/);
});
