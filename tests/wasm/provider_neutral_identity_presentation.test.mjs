import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('QML identity labels are provided by verified presenter state, never raw issuer comparisons', () => {
  const header = read('src/presentation/qt/presentation_bridge.hpp');
  const bridge = read('src/presentation/qt/presentation_bridge.cpp');
  const shell = read('ui/components/shell/AppShell.qml');
  const profile = read('ui/components/profile/ProfileSurface.qml');
  assert.match(header, /Q_PROPERTY\(bool nativeCustodyAccount READ nativeCustodyAccount NOTIFY authenticationStateChanged\)/);
  assert.match(header, /Q_PROPERTY\(QString accountIdentityBadge READ accountIdentityBadge NOTIFY authenticationStateChanged\)/);
  assert.match(bridge, /bool PresentationBridge::nativeCustodyAccount\(\) const noexcept[\s\S]*?return m_isAuthenticated/);
  assert.match(bridge, /QString PresentationBridge::accountIdentityBadge\(\) const/);
  assert.match(shell, /nativeAccount:\s*presentationBridge.nativeCustodyAccount/);
  assert.match(shell, /text: presentationBridge.accountIdentityBadge/);
  assert.doesNotMatch(shell, /"hypersnap:native-registry"/);
  assert.doesNotMatch(profile, /"hypersnap:native-registry"/);
  assert.match(profile, /accountIsNativeCustody/);
  assert.match(shell, /accountIsNativeCustody: presentationBridge.nativeCustodyAccount/);
});

test('local native KEY_ADD still demands one-shot custody authorization after the UI refactor', () => {
  const signer = read('src/application/signer.cpp');
  const bridge = read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(signer, /session->identityIssuer == "hypersnap:native-registry"[\s\S]*?&& !oneShotAuthorizer/);
  assert.match(bridge, /!nativeCustodyAccount\(\) \|\| m_accountFid < \(1ULL << 32\)/);
  assert.match(bridge, /if \(beginSignerRecovery\(recoveryWords\)\)\s+emit nativeHyperSnapSignerAuthorizationRequested/);
  assert.match(bridge, /recoveryWords\.size\(\) > 4095 \|\| !exactly24Words\(recoveryWords\)/);
  assert.match(bridge, /nativeHyperSnapSignerAuthorizationRequested\(std::move\(recoveryWords\)\)/);
});

test('unconfigured channel surface must not silently choose the independent protocol', () => {
  const channels = read('ui/components/social/ChannelsSurface.qml');
  assert.match(channels, /property string readContract: ""/);
  assert.match(channels, /channelSource\.configureReadSource\(readContract, readEndpoint\)/);
});
