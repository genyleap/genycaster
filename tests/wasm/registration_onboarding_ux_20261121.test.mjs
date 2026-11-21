import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const file=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('HyperSnap explains blockchain catch-up with identical stage names on native and browser',()=>{
 const qml=file('ui/components/setup/InitialSetup.qml');
 const js=file('ui/wasm/browser_account_create_dialog.mjs');
 assert.match(qml,/objectName: "setupNativeChainStatusCard"/);
 assert.match(qml,/objectName: "setupBrowserHyperSnapNetworkStatus"/);
 for(const stage of ['backup','request','confirmed']) {
   assert.ok(qml.includes('qsTrId("genycaster.setup.'+stage+'")'),stage);
 }
 assert.match(qml,/genycaster.setup.network_syncing/);
 assert.match(qml,/genycaster.setup.confirmation_pending_dont_retry/);
 assert.match(qml,/genycaster.setup.registration_unavailable/);
 assert.match(qml,/objectName: "setupCancelPausedRegistration"/);
 assert.match(qml,/objectName: "setupBrowserCancelPausedRegistration"/);
 assert.match(qml,/objectName: "setupNativeDeveloperDiagnostics"/);
 assert.ok(qml.includes('confirmation.http_status='));
 assert.ok(qml.includes('registration.max_block_delay_s=120'));
 assert.match(qml,/onClicked: root\.previousStep\(\)/);
 assert.match(qml,/qsTrId\("genycaster.setup.keep_recovery_words"\)/);
 assert.match(js,/stepOne:'Backup'/);
 assert.match(js,/stepTwo:'Request'/);
 assert.match(js,/stepThree:'Confirmation'/);
 assert.match(js,/HYPERSNAP BLOCKCHAIN/);
 assert.doesNotMatch(js,/progressCaption\.textContent=t\.progress\+' · '/);
});

test('WASM blocks irreversible HyperSnap registration when shard-0 lags, not read-only recoveries',()=>{
 const qml=file('ui/components/setup/InitialSetup.qml');
 const browser=file('ui/wasm/hypersnap_native_account.mjs');
 const pre=file('ui/wasm/protocol-signer-pre.js');
 assert.match(qml,/objectName: "setupBrowserCreateHyperSnap"[\s\S]*?nativeRegistrationStatus\.syncedForRegistration/);
 assert.match(browser,/export function parseNativeNetworkHealth/);
 assert.match(browser,/blockDelay<=120/);
 assert.match(browser,/await requireHealthyBlockchain\(signal\)/);
 assert.match(browser,/return Object\.freeze\(\{lookup,challenge,networkHealth,recover,register\}\)/);
 assert.match(pre,/await nativeRegistry\.networkHealth\(controller\.signal\)/);
});

test('Farcaster signup defaults to official path, never hides paid enrollment behind a mystery code',()=>{
 const qml=file('ui/components/setup/InitialSetup.qml');
 const browser=file('ui/wasm/browser_account_create_dialog.mjs');
 const service=file('ui/app/identity/neynar_registration_controller.cpp');
 const header=file('ui/app/identity/neynar_registration_controller.hpp');
 assert.match(qml,/objectName: "setupFarcasterOfficialSignup"/);
 assert.match(qml,/Qt\.openUrlExternally\("https:\/\/farcaster\.xyz\/"\)/);
 assert.match(qml,/objectName: "setupFarcasterExistingAccount"/);
 assert.match(qml,/objectName: "setupFarcasterSponsorCodeChoice"[\s\S]*?neynarRegistration\.available/);
 assert.match(qml,/root\.farcasterSponsoredEnrollmentChosen[\s\S]*?neynarRegistration\.available/);
 assert.match(qml,/genycaster.setup.sponsor_unavailable/);
 assert.match(browser,/https:\/\/farcaster\.xyz\//);
 assert.match(browser,/Genycaster operator after verified payment or sponsorship/);
 assert.match(header,/Q_PROPERTY\(bool available READ available NOTIFY changed\)/);
 assert.match(service,/paid_enrollment_required/);
 assert.match(service,/status == 401/);
 assert.doesNotMatch(service,/m_available\{canonicalEndpoint/);
});
