import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const pro=read('ui/components/profile/GenyProDetailsDrawer.qml');
const streak=read('ui/components/profile/StreakCard.qml');
const controller=read('ui/app/identity/connections.cpp');
const cloud=read('services/account-sync/cloud_state.mjs');
const billing=read('services/account-sync/pro_billing.mjs');

test('native imported Farcaster accounts can reverify owner after missing vault signer',()=>{
  assert.match(pro,/readonly property bool nativeFarcasterRecoveryNeeded:/);
  assert.match(streak,/readonly property bool nativeFarcasterRecoveryNeeded:/);
  for (const source of [pro,streak]) {
    assert.match(source,/!presentationBridge.nativeCustodyAccount/);
    assert.match(source,/Qt.platform.os !== "wasm"/);
    assert.match(source,/"account_signer_unavailable"/);
    assert.match(source,/"account_proof_not_accepted"/);
    assert.match(source,/"account_session_unavailable"/);
    assert.match(source,/openProofConnections.authorizeAccount\(\)/);
    assert.match(source,/Qt.openUrlExternally\(openProofConnections.approvalUrl\)/);
    assert.match(source,/openProofConnections.authorizing/);
  }
});
test('Pro retry distinguishes native custody signer provisioning from Farcaster owner proof',()=>{
  assert.match(pro,/readonly property bool signerAuthorizationNeeded:[^\n]+\n\s*&& presentationBridge.nativeCustodyAccount/);
  assert.match(pro,/if \(root.nativeFarcasterRecoveryNeeded\) \{/);
  assert.match(pro,/objectName: "genyProNativeAccountVerification"/);
  assert.match(pro,/objectName: "genyProOpenAccountVerification"/);
  assert.match(pro,/if \(!root.membershipVerified\)/);
  assert.match(pro,/if \(root.eligible\) \{ root.close\(\); return \}/);
  assert.doesNotMatch(pro,/root.nativeFarcasterRecoveryNeeded[\s\S]{0,250}requestProPaymentQuote/);
});
test('Streak fallback uses explicit, non-automatic approval without inventing activity',()=>{
  assert.match(streak,/objectName: "streakRestoreVerifiedSession"/);
  assert.match(streak,/objectName: "streakOpenAccountVerification"/);
  assert.match(streak,/else if \(root.nativeFarcasterRecoveryNeeded\)\s*openProofConnections.authorizeAccount\(\)/);
  assert.match(streak,/root.signerAuthorizationRequested\(\)/);
  assert.doesNotMatch(streak,/Qt.openUrlExternally\(openProofConnections.approvalUrl\)[\s\S]{0,100}Component.onCompleted/);
});
test('owner proof remains verified and matching FID before native recovery bootstraps session',()=>{
  assert.match(controller,/if \(generation != m_grantGeneration \|\| !requiredFid/);
  assert.match(controller,/m_viewerFid != requiredFid \|\| fid != requiredFid/);
  assert.match(controller,/acceptRecoveryGrant\(std::move\(ticket\)/);
  assert.match(controller,/bootstrapAccountSession\(\);/);
  assert.match(cloud,/const membership=await resolvePro\(owner.fid,owner\)/);
  assert.match(cloud,/subscriptionEntitlements\(/);
  assert.match(billing,/const eligible=isActive\(until,now\)/);
  assert.match(billing,/eligible\?\'pro\':\'basic\'/);
});
