import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('own-profile editing is authorized by authenticated navigation rather than the read network',()=>{
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  const header=read('src/presentation/qt/presentation_bridge.hpp');
  const profile=read('ui/components/profile/ProfileSurface.qml');
  const shell=read('ui/components/shell/AppShell.qml');
  assert.match(bridge,/void PresentationBridge::openMyProfile\(\)[\s\S]*?openProfileFid\(m_accountFid\);[\s\S]*?m_profileIsOwnAccount = true;/);
  assert.match(bridge,/void PresentationBridge::openProfileUsername\([\s\S]*?m_profileIsOwnAccount = false;/);
  assert.match(bridge,/void PresentationBridge::openProfileFid\([\s\S]*?m_profileIsOwnAccount = false;/);
  assert.match(header,/m_isAuthenticated && m_accountFid != 0 && m_profileIsOwnAccount/);
  assert.match(shell,/ownProfileNavigation: presentationBridge\.ownProfileNavigation/);
  assert.match(profile,/readonly property string profileNetwork: root\.ownProfileNavigation\s*\? root\.accountNetwork : root\.activeReadNetwork/);
  assert.match(profile,/root\.profileFidText === root\.accountFidText/);
  assert.match(profile,/root\.ownProfileNavigation\s*\|\| root\.activeReadNetwork === root\.accountNetwork/);
});

test('both account protocols expose signer authorization without bypassing protocol write permission',()=>{
  const profile=read('ui/components/profile/ProfileSurface.qml');
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  const action=profile.slice(profile.indexOf('objectName: "profileAuthorizeNativeSigner"'),profile.indexOf('objectName: "profileProtocolSave"'));
  assert.match(action,/visible: root\.isOwnProfile && !presentationBridge\.writeReady/);
  assert.match(action,/enabled: !presentationBridge\.signerBusy/);
  assert.match(action,/onClicked: root\.signerAuthorizationRequested\(\)/);
  assert.match(profile,/enabled: presentationBridge\.writeReady && !presentationBridge\.writeBusy/);
  assert.match(bridge,/if \(!m_isAuthenticated \|\| m_accountFid == 0 \|\| !m_writeReady/);
});

test('WASM setup explicitly shows protocol choices without faking desktop-only browser custody',()=>{
  const setup=read('ui/components/setup/InitialSetup.qml');
  const wallet=read('ui/app/identity/native_wallet_browser.cpp');
  assert.match(setup,/objectName: "setupBrowserAccountProtocols"/);
  assert.match(setup,/objectName: "setupBrowserFarcasterChoice"/);
  assert.match(setup,/objectName: "setupBrowserHyperSnapChoice"/);
  assert.match(setup,/objectName: "setupBrowserRecoveryProtocolChoice"/);
  assert.match(setup,/objectName: "setupBrowserRecoverFarcaster"/);
  assert.match(setup,/objectName: "setupBrowserRecoverHyperSnap"/);
  assert.match(setup,/enabled: !root\.browserReadOnly\s*\n\s*accountShowcase: true/);
  assert.match(setup,/!root\.browserReadOnly \|\| root\.recoveryProtocol === "farcaster-snapchain"/);
  assert.match(setup,/objectName: "setupBrowserHyperSnapRecoveryNotice"/);
  assert.match(wallet,/bool NativeCustodyWalletController::generate\(\)[\s\S]*?return false;/);
  assert.match(setup,/root\.nativeRecoverHyperSnapAccount\(words\)/);
  assert.match(setup,/root\.nativeRecoverFarcasterAccount\(words\)/);
});

test('successful Farcaster recovery adopts verified signer owner before signed cloud membership retry',()=>{
  const pre=read('ui/wasm/protocol-signer-pre.js');
  const session=read('ui/wasm/browser_account_session.mjs');
  const section=pre.slice(pre.indexOf('if (action === "recover-account") {',pre.indexOf('owner = fid;')),pre.indexOf('} else {\n                    const current=await createClient(base);'));
  const mark=section.indexOf('owner = recoveredFid;');
  const event=section.indexOf("report({kind:'setup-account',phase:'ready'");
  assert.ok(mark>0 && event>mark);
  const composition=read('ui/app/runtime/bindings.cpp');
  assert.match(composition,/const bool recoveryHasAccountGrant = !restoredDevice/);
  assert.match(composition,/sessionObserver\(std::move\(state\)\);[\s\S]*?QMetaObject::invokeMethod\(&bridge,[\s\S]*?connections\.retryAccountSession\(\)/);
  assert.match(section,/return current\.recoverAccountFromPhrase\(/);
  assert.match(section,/verifiedOpenProof\?\.accountTicket/);
  assert.match(session,/isVerifiedFarcasterOwner\(granted,fid\)/);
  assert.match(session,/const restored=await request\(fetchImpl,endpoint,\{method:'GET',signal\}\)/);
  assert.match(session,/if\(!isVerifiedFarcasterOwner\(restored,fid\)\)/);
});
