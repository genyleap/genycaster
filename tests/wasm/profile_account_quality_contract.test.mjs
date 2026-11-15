import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=(path)=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('profile cache and optimistic writes merge by canonical 20-byte cast hash',()=>{
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(bridge,/void deduplicateCastRows\(QVariantList& rows\)/);
  assert.match(bridge,/const QString key = canonicalCastKey\(row\.value\(QStringLiteral\("castId"\)\)\.toString\(\)\)/);
  assert.match(bridge,/if \(existingLocal && !incomingLocal\)/);
  assert.match(bridge,/deduplicateCastRows\(m_rows\)/);
  assert.match(bridge,/canonicalCastKey\(networkRow\.toMap\(\)\.value\(QStringLiteral\("castId"\)\)\.toString\(\)\)/);
  assert.match(bridge,/canonicalCastKey\(writeRow\.toMap\(\)\.value\(QStringLiteral\("castId"\)\)\.toString\(\)\)/);
  assert.ok(!bridge.includes('deduplicateCastRows(m_rows, text)'),
    'distinct authored posts sharing text must not be collapsed');
});

test('local write labels no longer invent You/@you and hydrate from verified profile',()=>{
  const runtime=read('ui/app/runtime/bindings.cpp');
  const start=runtime.indexOf('[[nodiscard]] QVariantMap writePresentation(');
  const end=runtime.indexOf('[[nodiscard]] QVariantList reactionProjectionRows(',start);
  const projection=runtime.slice(start,end);
  assert.ok(!projection.includes('genycaster.runtime.you'));
  assert.match(projection,/QStringLiteral\("fid:"\)/);
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  const profile=bridge.slice(bridge.indexOf('void PresentationBridge::applyAccountProfile('),
    bridge.indexOf('void PresentationBridge::applyAuthenticationError('));
  assert.ok(profile.includes('hydrateOwnRows(m_rows)'));
  assert.ok(profile.includes('hydrateOwnRows(m_writeRows)'));
});

test('native HyperSnap accounts never request Farcaster SIWF just to view connections',()=>{
  const popup=read('ui/components/profile/ConnectionsPopup.qml');
  assert.ok(popup.includes('property bool nativeCustodyAccount: false'));
  assert.ok(popup.includes('visible: !picker.nativeCustodyAccount && !picker.authorized'));
  assert.ok(popup.includes('genycaster.connections.native_identity_note'));
  const profile=read('ui/components/profile/ProfileSurface.qml');
  assert.ok(profile.includes('nativeCustodyAccount: root.accountIsNativeCustody'));
});

test('drawers keep the originating edge square and 24 recovery slots are isolated',()=>{
  const drawer=read('ui/components/controls/GenyDrawer.qml');
  assert.ok(drawer.includes('control.edge !== Qt.RightEdge'));
  assert.ok(drawer.includes('control.edge !== Qt.LeftEdge'));
  assert.ok(drawer.includes('control.bottomSheet'));
  const shell=read('ui/components/shell/AppShell.qml');
  assert.ok(shell.includes('objectName: "nativeSignerRecoveryGrid"'));
  assert.ok(shell.includes('id: nativeSignerWordGrid'));
  assert.ok(shell.includes('model: 24'));
  assert.ok(shell.includes('objectName: "nativeSignerRecoveryWord" + (index + 1)'));
  assert.ok(shell.includes('interactionAuthorizationDialog.recoveryReady()'));
  assert.ok(shell.includes('interactionAuthorizationDialog.clearRecoveryWords()'));
  assert.ok(shell.includes('presentationBridge.authorizeNativeHyperSnapSigner('));
  assert.ok(!shell.includes('id: nativeSignerWords'));
});

test('media error returns diagnostic category without exposing credentials to UI',()=>{
  const runtime=read('ui/app/runtime/bindings.cpp');
  for(const suffix of ['upload_authorization_failed','upload_invalid_file',
    'upload_network_failed','upload_protocol_failed']){
    assert.ok(runtime.includes('genycaster.media.'+suffix));
  }
  assert.ok(!runtime.includes('controllerGuard->applyFailed(QString::fromStdString(state.error->message))'));
});
