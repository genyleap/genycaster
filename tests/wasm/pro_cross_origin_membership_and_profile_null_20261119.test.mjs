import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
const controller=read('ui/app/identity/connections.cpp');
const header=read('ui/app/identity/connections.hpp');
const bridge=read('ui/app/browser/signer_bridge.hpp');
const runtime=read('ui/app/runtime/bindings.cpp');
const pro=read('ui/components/profile/GenyProDetailsDrawer.qml');
const streak=read('ui/components/profile/StreakCard.qml');
const profile=read('ui/components/profile/ProfileSurface.qml');
const pre=read('ui/wasm/protocol-signer-pre.js');

test('profile link previews tolerate closing or destroyed drawer delegates',()=>{
  assert.equal((profile.match(/mediaActive: Boolean\(profileLinksDrawer && profileLinksDrawer.opened\)/g)||[]).length,2);
  assert.equal((profile.match(/if \(profileLinksDrawer\) profileLinksDrawer.close\(\)/g)||[]).length,2);
  assert.ok(!profile.includes('mediaActive: profileLinksDrawer.opened'));
});
test('WASM cloud entitlement and Streak API are rebound to the exact browser app host',()=>{
  assert.match(controller,/jsonKey == QStringLiteral\("genycasterCloudApiUrl"\)/);
  assert.match(controller,/window.location\?\.origin === "https:\/\/app.genycaster.xyz"/);
  assert.match(controller,/endpoint\.setHost\(QStringLiteral\("app.genycaster.xyz"\)\)/);
  assert.match(controller,/#if defined\(Q_OS_WASM\)[\s\S]*?app.genycaster.xyz/);
  assert.match(pre,/https:\/\/app.genycaster.xyz/);
  assert.ok(!pre.includes('owner !== fid || !client'));
});
test('failed browser account session reaches a bounded controller error, not a silent void',()=>{
  assert.match(bridge,/setAccountSessionFailureObserver/);
  assert.match(bridge,/phase==QStringLiteral\("failed"\)/);
  assert.match(runtime,/connections\.reportBrowserAccountSessionFailure\(fid,error\)/);
  assert.match(header,/void reportBrowserAccountSessionFailure\(/);
  assert.match(controller,/void OpenProofConnectionsController::reportBrowserAccountSessionFailure\(/);
  assert.match(controller,/QStringLiteral\("account_signer_unavailable"\)/);
  assert.match(controller,/QStringLiteral\("account_session_unavailable"\)/);
  assert.match(controller,/m_proSessionRepairAttempted = true/);
  assert.match(controller,/m_proSessionRepairAttempted=false/);
});
test('Pro UI offers separate native and browser recovery instead of forcing renewed checkout',()=>{
  assert.match(pro,/readonly property bool browserFarcasterRecoveryNeeded/);
  assert.match(pro,/objectName: "genyProBrowserAccountSignerRecovery"/);
  assert.match(pro,/openProofConnections\.authorizeAccount\(\)/);
  assert.match(streak,/readonly property bool browserFarcasterRecoveryNeeded/);
  assert.match(streak,/root\.signerAuthorizationRequested\(\)/);
  assert.match(read('ui/components/profile/ProUpgradeNotice.qml'),/openProofConnections\.retryProEntitlements\(\)/);
});
