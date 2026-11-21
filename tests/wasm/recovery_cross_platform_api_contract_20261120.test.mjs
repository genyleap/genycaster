import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const auth=read('src/providers/compatibility/farcaster_auth/auth.cpp');
const binding=read('ui/app/runtime/bindings.cpp');
const catalog=read('ui/i18n/catalog.cpp');
const source=read('ui/wasm/protocol-signer-pre.js');
const build=read('ui/CMakeLists.txt');
const assets=read('ci/package_wasm_assets.py');

test('desktop RPC 401/403 cannot masquerade as an unregistered Farcaster FID',()=>{
  assert.match(auth,/response\.statusCode == 401 \|\| response\.statusCode == 403[\s\S]*?ErrorCode::Unauthorized/);
  assert.match(auth,/if \(\*fid == 0U\) \{[\s\S]*?ErrorCode::NotFound/);
  assert.match(auth,/IdRegistry does not confirm custody for this FID/);
  assert.match(auth,/ErrorCode::Conflict,[\s\S]*?IdRegistry does not confirm custody/);
  assert.match(binding,/if \(category == Code::NotFound\)[\s\S]*?"unregistered"/);
  assert.match(binding,/if \(category == Code::Conflict\)[\s\S]*?"ownership"/);
  assert.match(binding,/category == Code::Unauthorized[\s\S]*?"network"/);
});

test('all recovery error keys have compiled-catalog source definitions',()=>{
  const ids=['farcaster_custody_unregistered','farcaster_custody_rpc_unavailable',
    'farcaster_custody_mismatch'];
  for(const id of ids){
    const key='genycaster.setup.'+id;
    assert.ok(catalog.includes('qtTrId("'+key+'")'));
    for(const locale of ['en_US','fa_IR','de_DE','ja_JP','ko_KR','zh_CN',
      'hi_IN','ar_SA','es_ES','fr_FR','pt_BR','ru_RU']){
      assert.ok(read('ui/i18n/qml_'+locale+'.ts')
        .includes('<message id="'+key+'">'),locale+' '+key);
    }
  }
});

test('web Farcaster recovery authorizes the device signer without gating account login',()=>{
  const section=source.slice(source.indexOf('} else if (action === "recover-account") {'),
    source.indexOf('} else {\n                    const options=payloadString?JSON.parse(payloadString):{};'));
  assert.ok(section.includes("await import('./farcaster_custody_recovery.mjs')"));
  assert.ok(section.includes('return verifyFarcasterRecovery('));
  assert.match(section,/await createClient\(base\)/);
  assert.match(section,/await signer\.provisionFromRecovery\(verifiedFid,phrase\)/);
  assert.match(section,/catch\(error\)[\s\S]*?Farcaster signer activation is pending/);
  assert.ok(build.includes('wasm/farcaster_custody_recovery.mjs'));
  assert.ok(assets.includes('"farcaster_custody_recovery.mjs"'));
});

test('native HyperSnap lookup retries read-only on canonical node without signer gating',()=>{
  const section=binding.slice(binding.indexOf('nativeHyperSnapRecoveryRequested'),
    binding.indexOf('if (nativePublisher) nativePublisher->setIdentityRuntimeReady(false)'));
  assert.match(section,/selectedNode != trustedNode/);
  assert.match(section,/State::Unavailable[\s\S]*?State::InvalidResponse/);
  assert.match(section,/fallback->lookupByCustody\(address/);
  assert.doesNotMatch(section,/!nativePublisher->registrationSupported\(\)/);
});


test('both desktop protocol imports permit read-only recovery without consenting to a signer',()=>{
  const setup=read('ui/components/setup/InitialSetup.qml');
  const shell=read('ui/components/shell/AppShell.qml');
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  const header=read('src/presentation/qt/presentation_bridge.hpp');
  assert.match(setup,/const authorizeDeviceSigner = deviceSignerConsent\.checked/);
  assert.match(setup,/root\.nativeRecoverHyperSnapAccount\(words, authorizeDeviceSigner\)/);
  assert.match(setup,/root\.nativeRecoverFarcasterAccount\(words, authorizeDeviceSigner\)/);
  assert.match(setup,/id: deviceSignerConsent[\s\S]*?checked: false/);
  const action=setup.slice(setup.indexOf('objectName: "nativeRecoveryDeviceSignerConsent"'),
    setup.indexOf('root.recoverySubmitPending = true'));
  assert.doesNotMatch(action,/&& deviceSignerConsent\.checked/);
  assert.match(shell,/beginNativeHyperSnapRecoverySetup\(recoveryWords, authorizeSigner\)/);
  assert.match(header,/nativeHyperSnapRecoveryRequested\(QString recoveryWords, bool authorizeSigner\)/);
  assert.match(bridge,/m_hyperSnapRecoverySignerConsent = authorizeSigner/);
  const native=binding.slice(binding.indexOf('nativeHyperSnapRecoveryRequested'),
    binding.indexOf('if (nativePublisher) nativePublisher->setIdentityRuntimeReady(false)'));
  assert.match(native,/QString words, const bool authorizeSigner/);
  assert.match(native,/if \(authorizeSigner\) \{[\s\S]*?makeOneShotRecoveryAuthorizer/);
});
