import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = (path) => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('native profile writes use durable UserDataAdd, not the unsupported placeholder', () => {
  const binding = read('ui/app/runtime/bindings.cpp');
  const start = binding.indexOf('PresentationBridge::protocolProfileUpdateRequested');
  const end = binding.indexOf('PresentationBridge::followingMutationRequested', start);
  assert.ok(start > 0 && end > start);
  const route = binding.slice(start, end);
  assert.ok(route.includes('runtime->writeService->updateProfileField('));
  assert.ok(route.includes('domain::write::ProfileField'));
  assert.ok(!route.includes('emit bridge.protocolProfileWriteCompleted(false,\n                qtTrId("genycaster.profile.protocol_native_unavailable"))'));
  assert.ok(route.includes('QStringLiteral("username")'));
  assert.ok(route.includes('batch->remaining'));
  assert.ok(route.includes('accountFid() != accountFid'));
});

test('synchronous completion cannot race the QML pending flag', () => {
  const qml = read('ui/components/profile/ProfileSurface.qml');
  const submitStart = qml.indexOf('function publishProtocolChanges(changes)');
  const submit = qml.slice(submitStart, qml.indexOf('if (!accepted', submitStart));
  assert.ok(submitStart > 0);
  const pending = submit.indexOf('root.protocolProfilePending = true');
  const bridge = submit.indexOf('presentationBridge.updateProtocolProfile(changes)');
  assert.ok(pending > 0 && bridge > pending,
    'pending must be set before the native handler can synchronously complete');
  assert.ok(qml.includes('root.publishProtocolChanges(changes)'));
  assert.ok(qml.includes('profileWriteWatchdog.stop()'));
});

test('username is separately displayed but never submitted with display name', () => {
  const qml = read('ui/components/profile/ProfileSurface.qml');
  assert.ok(qml.includes('objectName: "profileProtocolUsername"'));
  assert.ok(qml.includes('objectName: "profileProtocolDisplay"'));
  assert.ok(qml.includes('genycaster.profile.username_separate'));
  assert.ok(qml.includes('objectName: "profileAuthorizeNativeSigner"'));
  assert.ok(qml.includes('onClicked: presentationBridge.provisionSigner()'));
  const start = qml.indexOf('const fields = {', qml.indexOf('objectName: "profileProtocolSave"'));
  const end = qml.indexOf('const existing = {', start);
  const payload = qml.slice(start, end);
  assert.ok(payload.includes('display: displayProtocolEditor.text.trim()'));
  assert.ok(!payload.includes('username:'));
  const scopes = read('src/domain/signer.cpp');
  const startScopes = scopes.indexOf('standardSocialScopes()');
  const endScopes = scopes.indexOf('bool isAdmissibleScope(', startScopes);
  const defaults = scopes.slice(startScopes, endScopes);
  assert.ok(defaults.includes('Scope::UserDataAdd'));
  assert.ok(!defaults.includes('Scope::UsernameProof'));
  const canonical = read('src/protocol/message.cpp');
  assert.ok(canonical.includes('messageTypeUserDataAdd = 11U'));
  assert.ok(canonical.includes('field == 1U || field == 2U || field == 3U'));
  assert.ok(canonical.includes('field == 6U'));
  assert.ok(qml.includes('objectName: "profileProtocolUsernameSave"'));
  assert.ok(qml.includes('root.publishProtocolChanges({username: candidate})'));
});
