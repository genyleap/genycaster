import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('followers and following never reserve the Explorer feed header',()=>{
  const source=read('ui/components/feed/FeedSurface.qml');
  assert.equal((source.match(/header:\s*root\.socialMode \? null : feedHeaderComponent/g)||[]).length,2);
  assert.match(source,/implicitHeight:\s*root\.socialMode \? 0/);
  assert.match(source,/root\.socialMode \? profileRowComponent/);
});

test('attached drawer corners are square across themes and display directions',()=>{
  const drawer=read('ui/components/controls/GenyDrawer.qml');
  assert.match(drawer,/neonAmbient:\s*!control\.fullScreen/);
  assert.match(drawer,/neonInteractive:\s*!control\.fullScreen/);
  assert.match(drawer,/curveTopLeft:\s*!control\.fullScreen && \(control\.bottomSheet \|\| control\.edge !== Qt\.LeftEdge\)/);
  assert.match(drawer,/curveTopRight:\s*!control\.fullScreen && \(control\.bottomSheet \|\| control\.edge !== Qt\.RightEdge\)/);
  assert.match(drawer,/curveBottomLeft:\s*!control\.fullScreen && !control\.bottomSheet && control\.edge !== Qt\.LeftEdge/);
  assert.match(drawer,/curveBottomRight:\s*!control\.fullScreen && !control\.bottomSheet && control\.edge !== Qt\.RightEdge/);
  assert.match(drawer,/shadowEnabled:\s*false/);
  assert.match(drawer,/leftInset:\s*floatingSheet && edge !== Qt\.LeftEdge/);
  assert.match(drawer,/rightInset:\s*floatingSheet && edge !== Qt\.RightEdge/);
  assert.match(drawer,/bottomInset:\s*floatingSheet && edge !== Qt\.BottomEdge/);
});

test('native FID session renewal only after matching shard-zero registry and local custody',()=>{
  const transport=read('ui/app/identity/native_publish.cpp');
  const runtime=read('ui/app/runtime/bindings.cpp');
  assert.match(transport,/lookupByCustody\(custody/);
  assert.match(runtime,/nativeRegistry->verifyCustodyForSession\(address/);
  assert.match(runtime,/\*confirmed != previous\.fid\.value/);
  assert.match(runtime,/!protectedCustody->backupVerified\(\)/);
  assert.match(runtime,/current->identityIssuer != previous\.identityIssuer/);
  assert.match(runtime,/current->authenticatedAt != previous\.authenticatedAt/);
  assert.match(runtime,/acceptVerifiedNativeCustody\(/);
  assert.ok(!runtime.includes('renewed.identityIssuer = "farcaster:optimism-id-registry"'));
});

test('expired non-registry sessions do not continuously retry queued writes',()=>{
  const runtime=read('ui/app/runtime/bindings.cpp');
  const writes=read('src/application/write.cpp');
  assert.match(runtime,/const auto resumeAuthorizedWrites = \[runtime, writeObserver\]/);
  assert.match(runtime,/if \(!session \|\| !domain::session::isProtocolSessionEligible\(/);
  assert.match(runtime,/outboxRetryTimer->setInterval\(30'000\)/);
  assert.match(runtime,/runtimeRecoveryTimer->setInterval\(60'000\)/);
  assert.match(runtime,/resumeAuthorizedWrites\(\)/);
  assert.doesNotMatch(runtime,/writeService->resume\(runtime->currentSessionSnapshot\(\), writeObserver\)/);
  assert.doesNotMatch(writes,/Farcaster session has expired/);
  assert.match(writes,/verified account session has expired/);
});

test('streak records every confirmed authored cast, not only the rich composer path',()=>{
  const shell=read('ui/components/shell/AppShell.qml');
  const start=shell.indexOf('Streak is not a rich-composer feature');
  const rich=shell.indexOf('if (root.awaitingRichCommit',start);
  assert.ok(start>0 && rich>start);
  assert.match(shell.slice(start,rich),/openProofConnections\.recordStreakCast\(castId\)/);
  const connections=read('ui/app/identity/connections.cpp');
  assert.match(connections,/void OpenProofConnectionsController::retryAccountSession\(\)/);
  const editor=read('ui/components/profile/ProfileSurface.qml');
  assert.match(editor,/openProofConnections\.retryAccountSession\(\)/);
  assert.match(editor,/genycaster\.streak\.verified_session_required/);
});

test('HyperSnap Streak and Pro surfaces expose safe recovery without forging entitlements',()=>{
  const profile=read('ui/components/profile/StreakCard.qml');
  assert.match(profile,/objectName: "streakRestoreVerifiedSession"/);
  assert.match(profile,/!openProofConnections\.cloudSessionBusy/);
  assert.match(profile,/!openProofConnections\.accountProofBusy/);
  assert.match(profile,/readonly property bool signerAuthorizationNeeded:/);
  assert.match(profile,/root\.signerAuthorizationRequested\(\)/);
  assert.match(profile,/openProofConnections\.retryAccountSessionFromUser\(\)/);
  const owner=read('ui/components/profile/ProfileSurface.qml');
  const shell=read('ui/components/shell/AppShell.qml');
  assert.match(owner,/onSignerAuthorizationRequested:\s*\{[\s\S]*?root\.signerAuthorizationRequested\(\)/);
  assert.match(owner,/objectName: "profileAuthorizeNativeSigner"[\s\S]*?onClicked: root\.signerAuthorizationRequested\(\)/);
  assert.doesNotMatch(owner,/onClicked: presentationBridge\.provisionSigner\(\)/);
  assert.match(shell,/onSignerAuthorizationRequested: root\.requestInteractionAuthorization\(\)/);
  const pro=read('ui/components/profile/GenyProDetailsDrawer.qml');
  assert.match(pro,/visible: root\.appSignedIn && !root\.cloudReady && !root\.eligible/);
  assert.doesNotMatch(pro,/!root\.eligible\s*&& root\.restoringSession/);
  assert.match(pro,/root\.proofError\.length > 0/);
  assert.match(pro,/presentationBridge\.signerPhase === "authorization-required"/);
});

test('native connections identify separate OpenProof wallet links, never fake FID verification',()=>{
  const popup=read('ui/components/profile/ConnectionsPopup.qml');
  assert.match(popup,/objectName:\s*"nativeOpenProofConnections"/);
  assert.match(popup,/genycaster\.connections\.native_links_not_fid_verified/);
  assert.match(popup,/https:\/\/genyleap\.com\/openproof/);
  const runtime=read('ui/app/runtime/bindings.cpp');
  assert.match(runtime,/genycaster\.profile\.username_proof_missing/);
  const profile=read('ui/components/profile/ProfileSurface.qml');
  assert.doesNotMatch(profile,/genycaster\.profile\.native_username_registration_pending/);
  assert.match(profile,/objectName: "profileProtocolUsername"[\s\S]*?visible: !root\.accountIsNativeCustody/);
});

test('signer readiness and pending writes are scoped to the currently authenticated identity',()=>{
  const runtime=read('ui/app/runtime/bindings.cpp');
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(runtime,/current->identityIssuer != observedSession->identityIssuer/);
  assert.match(runtime,/current->identitySubject != observedSession->identitySubject/);
  assert.match(runtime,/applySignerState\(bridge, std::move\(state\)\);[\s\S]*?invalidatePendingAuthority\(\s*observedSession, writeObserver\)/);
  assert.match(runtime,/#if !defined\(Q_OS_WASM\)\s*\/\/ The browser bridge owns its WebCrypto signer lifecycle/);
  assert.match(bridge,/else if \(accountRowsChanged\) \{[\s\S]*?m_writeReady = false;[\s\S]*?m_signerPhase = QStringLiteral\("checking"\)/);
});
