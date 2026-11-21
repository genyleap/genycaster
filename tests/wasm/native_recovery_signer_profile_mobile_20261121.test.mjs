import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('native recovery snapshots signer consent before clearing sensitive fields', () => {
  const setup = source('ui/components/setup/InitialSetup.qml');
  const submit = setup.slice(setup.indexOf('let words = nativeRecoveryGrid.phrase()'),
                             setup.indexOf('root.switchStep(5)', setup.indexOf('let words = nativeRecoveryGrid.phrase()')));
  assert.ok(submit.includes('const authorizeDeviceSigner = deviceSignerConsent.checked'));
  assert.match(setup,/id: deviceSignerConsent[\s\S]*?checked: false/);
  assert.match(setup,/deviceSignerConsent\.checked = false/);
  assert.ok(submit.indexOf('const authorizeDeviceSigner') < submit.indexOf('nativeRecoveryGrid.clearAll()'));
  assert.match(submit, /nativeRecoverFarcasterAccount\(words, authorizeDeviceSigner\)/);
  assert.match(submit, /nativeRecoverHyperSnapAccount\(words, authorizeDeviceSigner\)/);
  assert.doesNotMatch(submit, /nativeRecover(Farcaster|HyperSnap)Account\(words, deviceSignerConsent.checked\)/);
});

test('recovery-created own profile stays pinned to authenticated protocol and not Follow', () => {
  const cpp = source('src/presentation/qt/presentation_bridge.cpp');
  const open = cpp.slice(cpp.indexOf('void PresentationBridge::openProfileFid('),
                         cpp.indexOf('void PresentationBridge::selectProfileFeed('));
  assert.ok(open.includes('m_activeView == QStringLiteral("profile") && m_profileFid == fid'));
  assert.doesNotMatch(open.slice(0, open.indexOf('rememberDetailLocation()')), /m_profileIsOwnAccount = false/);
  const auth = cpp.slice(cpp.indexOf('void PresentationBridge::applyAuthenticationState('),
                         cpp.indexOf('void PresentationBridge::applyAccountProfile('));
  assert.match(auth, /const bool recoveredOwnProfile =/);
  assert.match(auth, /m_authenticationRoute == QStringLiteral\("farcaster-native-recovery"\)/);
  assert.match(auth, /m_authenticationRoute == QStringLiteral\("hypersnap-recovery"\)/);
  assert.match(auth, /if \(recoveredOwnProfile\)\s+openMyProfile\(\)/);
  assert.ok(source('tests/unit/presentation_icon_tests.cpp')
      .includes('testRecoveredProfileKeepsOwnAccountAffiliation()'));
});

test('authentication-needed in social writers routes signed-in session to signer, not account', () => {
  const shell = source('ui/components/shell/AppShell.qml');
  const handler = shell.slice(shell.indexOf('function requestWriteInteraction()'),
                              shell.indexOf('function resolvePendingComposer()'));
  assert.match(handler, /if \(!presentationBridge.isAuthenticated\)\s+root.openAccountPopup\(\)/);
  assert.match(handler, /else if \(!presentationBridge.writeReady\)\s+root.requestInteractionAuthorization\(\)/);
  assert.ok((shell.match(/onAuthenticationRequested: root.requestWriteInteraction\(\)/g) || []).length >= 5);
  assert.match(shell, /HomeComposer's "authenticationRequested" also means a/);
  // Explicit account management and app sign-in requests still open account.
  assert.ok((shell.match(/onAuthenticationRequested: root.openAccountPopup\(\)/g) || []).length >= 1);
});

test('mobile recovery entry is focusable with keyboard or full-phrase paste', () => {
  const shell = source('ui/components/shell/AppShell.qml');
  assert.match(shell, /objectName: "nativeSignerRecoveryPhraseInput"/);
  assert.match(shell, /mobileRecoveryInput: Qt.platform.os === "android"/);
  assert.match(shell, /fullScreen: Qt.platform.os === "android" \|\| Qt.platform.os === "ios"/);
  assert.match(shell, /presentationBridge\.authenticationRoute === "farcaster"/);
  // OpenProof's Farcaster-linked account can use a local custody proof
  // without external app redirection; delegated login remains available.
  assert.match(shell, /\|\| presentationBridge\.farcasterCustodyAccount/);
  assert.match(shell, /placeholderText: qsTrId\("genycaster.setup.native_recovery_placeholder"\)/);
  assert.match(shell, /mobileRecoveryInput\)\s+return String\(nativeSignerPhraseInput.text/);
  assert.match(shell, /words.length === 24/);
  assert.match(shell, /nativeSignerPhraseInput.text = ""/);
  assert.match(shell, /nativeSignerPhraseInput.forceActiveFocus\(\)/);
  assert.match(shell, /Qt.inputMethod.show\(\)/);
  assert.match(shell, /!interactionAuthorizationDialog.mobileRecoveryInput/);
  assert.match(shell, /!root.overlayOpen/);
  assert.match(shell, /interactionAuthorizationDialog && interactionAuthorizationDialog.opened/);
});

test('every mobile bottom sheet has circular corners; no native rectangle hacks', () => {
  const drawer = source('ui/components/controls/GenyDrawer.qml');
  assert.match(drawer, /exponent: control.bottomSheet \? 2\.0 : Theme.superellipseExponent/);
  assert.match(drawer, /curveTopLeft: !control.fullScreen && \(control.bottomSheet/);
  assert.match(drawer, /curveTopRight: !control.fullScreen && \(control.bottomSheet/);
  assert.match(drawer, /curveBottomLeft: !control.fullScreen && !control.bottomSheet/);
  assert.match(drawer, /curveBottomRight: !control.fullScreen && !control.bottomSheet/);
});
