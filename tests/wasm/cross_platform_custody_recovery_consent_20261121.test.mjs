import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('Farcaster custody recovery also requests a signer without a separate checkbox',()=>{
 const setup=read('ui/components/setup/InitialSetup.qml');
 const runtime=read('ui/app/runtime/bindings.cpp');
 assert.match(setup,/objectName: "nativeRecoveryAutomaticSignerNotice"/);
 assert.doesNotMatch(setup,/objectName: "nativeRecoveryDeviceSignerConsent"/);
 assert.match(setup,/const authorizeDeviceSigner = true/);
 assert.match(runtime,/\/\/ Registry-confirmed custody authorizes sign-in independently/);
 assert.match(runtime,/commitVerifiedIdentity\(fid, address, std::move\(permit\), true, secret\)/);
 assert.match(runtime,/if \(authorizeSigner\) \{/);
});

test('Farcaster Android signer timeout has a read-only recovery-status action',()=>{
 const qml=read('ui/components/shell/AppShell.qml');
 const start=qml.indexOf('readonly property bool canRetryVerification:');
 assert.ok(start>0);
 const control=qml.slice(qml.lastIndexOf('ActionButton {',start),
                         qml.indexOf('ActionButton {',start));
 assert.match(control,/presentationBridge\.isAuthenticated/);
 assert.match(control,/!presentationBridge\.signerBusy/);
 assert.match(control,/!presentationBridge\.writeReady/);
 assert.match(control,/signerPhase === "unavailable"/);
 assert.match(control,/signerPhase === "failed"/);
 assert.match(control,/onClicked: canRetryVerification \? presentationBridge\.refreshSigner\(\)\s*:\s*root\.requestInteractionAuthorization\(\)/);
 const dialog=qml.slice(qml.indexOf('objectName: "authorizeSelectedProtocolSigner"'));
 assert.match(dialog,/if \(interactionAuthorizationDialog\.authorizationSubmitted\) \{\s*presentationBridge\.refreshSigner\(\)\s*return\s*\}/);
 const signer=read('src/application/signer.cpp');
 assert.match(signer,/auto inspected = m_impl->registry->inspect/);
 assert.match(signer,/auto registered = m_impl->registry->registerGasless/);
});

test('native HyperSnap pending results remain idempotent but confirmation polling continues after 2m',()=>{
 const src=read('ui/app/identity/native_publish.cpp');
 const begin=src.indexOf('void HyperSnapRegistrationPublisher::scheduleCheck(');
 assert.ok(begin>0);
 const schedule=src.slice(begin,begin+950);
 assert.match(schedule,/m_poll\.start\(m_checks < 30 \? 2000 : m_checks < 90 \? 15000 : 60000\)/);
 assert.doesNotMatch(schedule,/\.submit\(/);
 const lookup=src.slice(src.indexOf('void HyperSnapRegistrationPublisher::lookup('),
                        src.indexOf('void HyperSnapRegistrationPublisher::updateRetryEligibility()'));
 assert.match(lookup,/if \(m_pending\) scheduleCheck\(generation\)/);
 assert.match(lookup,/LookupState::NotFound|State::NotFound/);
 const qml=read('ui/components/setup/InitialSetup.qml');
 assert.match(qml,/nativeRegistrationPublisher\.checkConfirmation\(\)/);
 assert.match(qml,/nativeRegistrationStatus\.shardZeroBlockDelay > 120/);
});


test('native registration is blocked on a lagging shard-0 without blocking pending identity reads',()=>{
 const statusHeader=read('ui/app/identity/native_registration.hpp');
 const status=read('ui/app/identity/native_registration.cpp');
 const main=read('ui/app/main.cpp');
 const setup=read('ui/components/setup/InitialSetup.qml');
 const publisher=read('ui/app/identity/native_publish.cpp');
 assert.match(statusHeader,/Q_PROPERTY\(bool syncedForRegistration READ syncedForRegistration/);
 assert.match(statusHeader,/Q_PROPERTY\(int shardZeroBlockDelay READ shardZeroBlockDelay/);
 assert.match(status,/MaxAcceptableShardZeroLagSeconds = 120/);
 assert.match(status,/m_shardZeroDelay <= MaxAcceptableShardZeroLagSeconds/);
 assert.match(status,/QStringLiteral\("\/v1\/info"\)/);
 assert.match(status,/QStringLiteral\("shardInfos"\)/);
 assert.match(status,/QStringLiteral\("shardId"\)\)\.toInt\(-1\) != 0/);
 assert.match(status,/QStringLiteral\("blockDelay"\)/);
 assert.match(main,/\.challenge = \[&nativeRegistrationStatus\][\s\S]*?syncedForRegistration\(\)/);
 assert.match(main,/\.anchorIsFresh = \[&nativeRegistrationStatus\][\s\S]*?syncedForRegistration\(\)/);
 assert.match(setup,/readonly property bool registrationNodeUsable:[\s\S]*?syncedForRegistration/);
 assert.match(setup,/objectName: "setupBeginRegistration"[\s\S]*?enabled: root\.registrationNodeUsable/);
 assert.match(setup,/objectName: "setupNativePendingDescription"[\s\S]*?shardZeroBlockDelay > 120/);
 assert.match(setup,/id: nativeRegistrationNetworkHealthTimer[\s\S]*?nativeRegistrationStatus\.refresh\(\)/);
 const poll=publisher.slice(publisher.indexOf('void HyperSnapRegistrationPublisher::scheduleCheck('));
 assert.match(poll,/if \(generation != m_generation \|\| !m_pending \|\| confirmed\(\)\) return/);
 assert.match(poll,/m_poll\.start\(m_checks < 30 \? 2000 : m_checks < 90 \? 15000 : 60000\)/);
 assert.doesNotMatch(poll.slice(0,poll.indexOf('bool HyperSnapRegistrationPublisher::checkConfirmation()')),/\.submit\(/);
});
