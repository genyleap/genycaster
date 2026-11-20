import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const setup=read('ui/components/setup/InitialSetup.qml');
const shell=read('ui/components/shell/AppShell.qml');
const bridge=read('src/presentation/qt/presentation_bridge.cpp');
const header=read('src/presentation/qt/presentation_bridge.hpp');
const runtime=read('ui/app/runtime/bindings.cpp');
const endpoints=read('ui/app/bootstrap/providers.cpp');
const composition=read('ui/app/runtime/composition.cpp');

test('desktop Farcaster recovery does not require device signer consent to sign in',()=>{
  const section=setup.split('objectName: "nativeRecoveryDeviceSignerConsent"')[1]
    ?.split('root.switchStep(5)')[0];
  assert.ok(section);
  assert.match(setup,/root\.nativeRecoverFarcasterAccount\(words, deviceSignerConsent\.checked\)/);
  assert.match(section,/if \(root\.recoveryProtocol === "hypersnap-v2"\)\s+root\.nativeRecoverHyperSnapAccount\([^\n]+\)\s+else\s+root\.nativeRecoverFarcasterAccount\(words, deviceSignerConsent\.checked\)/);
  assert.match(shell,/beginNativeFarcasterRecoverySetup\(recoveryWords, authorizeSigner\)/);
  assert.match(header,/nativeFarcasterRecoveryRequested\(QString recoveryWords, bool authorizeSigner\)/);
  assert.match(bridge,/m_farcasterRecoverySignerConsent = authorizeSigner/);
  assert.match(runtime,/const bool authorizeSigner\)/);
  assert.match(runtime,/if \(authorizeSigner\) \{[\s\S]*?makeOneShotRecoveryAuthorizer/);
});

test('verified custody sign-in is accepted before any optional signer provisioning',()=>{
  const section=runtime.slice(runtime.indexOf('auto commitVerifiedIdentity = '),
    runtime.indexOf('    // New HyperSnap identity is verified'));
  assert.match(section,/if \(!runtime->sessionService\)/);
  assert.doesNotMatch(section,/if \(!permit \|\|/);
  assert.match(section,/acceptVerifiedFarcasterCustody/);
  assert.match(section,/if \(permit && runtime->router && runtime->signerService/);
  assert.match(section,/else if \(permit\)/);
});

test('desktop Farcaster recovery preserves distinct invalid phrase, unregistered FID and RPC failure messages',()=>{
  assert.match(runtime,/QStringLiteral\("phrase"\)/);
  assert.match(runtime,/QStringLiteral\("unregistered"\)/);
  assert.match(runtime,/QStringLiteral\("network"\)/);
  assert.match(runtime,/genycaster\.setup\.farcaster_custody_rpc_unavailable/);
  assert.match(runtime,/resolveRecoveryCustodyFid\(\*runtime->transport/);
});

test('Farcaster has a separate public indexed profile source by default with exact social counts',()=>{
  assert.match(endpoints,/readBaseUrl = readApiKey\.isEmpty\(\)/);
  assert.match(endpoints,/https:\/\/haatz\.quilibrium\.com/);
  assert.match(endpoints,/https:\/\/api\.neynar\.com/);
  assert.match(composition,/\.providerId = farcaster::provider::model::ProviderId\{"farcaster-snapchain"\}/);
  assert.match(composition,/\.providesProfileCounts = true/);
  assert.match(runtime,/const std::string pinnedProvider = provider\.empty\(\)/);
});

test('the signed-in account sidebar hydrates from its Farcaster index rather than the feed network',()=>{
  const composition=read('ui/app/runtime/composition.cpp');
  const runtimeModule=read('ui/app/runtime/composition.cppm');
  assert.match(runtime,/runtime->farcasterReadGateway->profile\(/);
  assert.match(runtime,/runtime->keepAccountIndexedRequest\(std::move\(\*indexed\)\)/);
  assert.match(runtime,/profile->fid\.value != accountFid/);
  assert.match(runtime,/target->accountIdentityIssuer\(\) != expectedIssuer/);
  assert.match(runtime,/expectedIssuer != QStringLiteral\("hypersnap:native-registry"\)/);
  assert.match(runtime,/expectedIssuer != QStringLiteral\("genycaster:device-local"\)/);
  assert.match(composition,/activeAccountIndexedRequest->cancel\(\)/);
  assert.match(runtimeModule,/accountProfileRequestGeneration/);
});
