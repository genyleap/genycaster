import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('Farcaster custody recovery completes independently of optional signer KEY_ADD',()=>{
 const setup=read('ui/components/setup/InitialSetup.qml');
 const runtime=read('ui/app/runtime/bindings.cpp');
 const begin=setup.indexOf('objectName: "nativeRecoveryDeviceSignerConsent"');
 assert.ok(begin>0);
 const consent=setup.slice(begin,begin+680);
 assert.match(consent,/checked: false/,'New/Farcaster recovery must not trigger a signer write by default');
 const clear=setup.slice(setup.indexOf('function clearAll()'),setup.indexOf('function countWords()'));
 assert.match(clear,/deviceSignerConsent\.checked = false/,'Clearing recovery words must restore safe opt-in');
 assert.match(setup,/const authorizeDeviceSigner = deviceSignerConsent\.checked/);
 assert.match(runtime,/\/\/ Registry-confirmed custody authorizes sign-in independently/);
 assert.match(runtime,/commitVerifiedIdentity\(fid, address, std::move\(permit\), true, secret\)/);
 assert.match(runtime,/if \(authorizeSigner\) \{/);
});

test('Farcaster Android signer timeout has a read-only recovery-status action',()=>{
 const qml=read('ui/components/shell/AppShell.qml');
 const start=qml.indexOf('objectName: "checkDeviceSignerAfterTimeout"');
 assert.ok(start>0);
 const control=qml.slice(start,start+1050);
 assert.match(control,/presentationBridge\.isAuthenticated/);
 assert.match(control,/!presentationBridge\.signerBusy/);
 assert.match(control,/!presentationBridge\.writeReady/);
 assert.match(control,/presentationBridge\.refreshSigner\(\)/);
 assert.doesNotMatch(control,/authorizeNativeFarcasterSigner|provisionSigner|startRecovery/);
 const signer=read('src/application/signer.cpp');
 assert.match(signer,/auto inspected = m_impl->registry->inspect/);
 assert.match(signer,/auto registered = m_impl->registry->registerGasless/);
});

test('native HyperSnap pending results remain idempotent but confirmation polling continues after 2m',()=>{
 const src=read('ui/app/identity/native_publish.cpp');
 const begin=src.indexOf('void HyperSnapRegistrationPublisher::scheduleCheck(');
 assert.ok(begin>0);
 const schedule=src.slice(begin,begin+500);
 assert.match(schedule,/m_poll\.start\(m_checks < 60 \? 2000 : 15000\)/);
 assert.doesNotMatch(schedule,/\.submit\(/);
 const lookup=src.slice(src.indexOf('void HyperSnapRegistrationPublisher::lookup('),
                        src.indexOf('void HyperSnapRegistrationPublisher::updateRetryEligibility()'));
 assert.match(lookup,/m_pending && m_checks < 240/);
 assert.match(lookup,/LookupState::NotFound|State::NotFound/);
 const qml=read('ui/components/setup/InitialSetup.qml');
 assert.match(qml,/nativeRegistrationPublisher\.checkConfirmation\(\)/);
 assert.match(qml,/confirmationLookupState !== "not_found"/);
});
