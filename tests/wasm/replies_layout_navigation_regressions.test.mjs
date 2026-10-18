import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const read=relative=>readFileSync(new URL('../../'+relative,import.meta.url),'utf8');

test('nested and profile replies keep their measured hit targets while scrolling',()=>{
  const profile=read('ui/components/ProfileSurface.qml');
  const feed=read('ui/components/FeedSurface.qml');
  assert.match(profile,/resolvedHeight > profileCastDelegate\.committedHeight/);
  assert.match(feed,/resolvedHeight > rowDelegate\.committedHeight/);
  assert.match(profile,/onReplyRequested: cast => root\.replyRequested\(cast\)/);
  assert.match(feed,/onReactionRequested: \(kind, cast, active\) => root\.reactionRequested/);
});
test('nested replies are prepared using the canonical target author identity',()=>{
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  const header=read('src/presentation/qt/presentation_bridge.hpp');
  const shell=read('ui/components/AppShell.qml');
  assert.match(header,/Q_INVOKABLE bool prepareReplyToCast\(/);
  assert.match(bridge,/bool PresentationBridge::prepareReplyToCast\(/);
  assert.match(bridge,/m_replyParentAuthorFid = static_cast<qulonglong>\(parentAuthorFid\)/);
  assert.match(shell,/presentationBridge\.prepareReplyToCast\(String\(parentCast\.castId\), authorFid\)/);
});
test('QML bootstrap binds signals only to components that actually declare them',()=>{
  const shell=read('ui/components/AppShell.qml');
  const header=read('ui/components/AppHeader.qml');
  const side=read('ui/components/SideNavigation.qml');
  const sideBinding=shell.split('SideNavigation {')[1].split('CompactNavigationRail {')[0];
  const headerBinding=shell.split('AppHeader {')[1].split('onNotificationsRequested:')[0];
  assert.doesNotMatch(sideBinding,/onOwnProfileRequested\s*:/,
    'SideNavigation has no ownProfileRequested signal; this crashes QQmlApplicationEngine');
  assert.match(header,/signal ownProfileRequested\(\)/);
  assert.match(headerBinding,/onOwnProfileRequested\s*:/);
  assert.doesNotMatch(side,/signal ownProfileRequested\(\)/);
  const html=read('ui/wasm/genycaster-shell.html');
  assert.match(html,/<meta name="mobile-web-app-capable" content="yes">/);
});
test('settings scrolling does not dismiss the drawer; header identity opens profile',()=>{
  const settings=read('ui/components/LocaleSettingsPopup.qml');
  const header=read('ui/components/AppHeader.qml');
  const shell=read('ui/components/AppShell.qml');
  assert.match(settings,/interactive: false\s*closePolicy: Popup\.CloseOnEscape/);
  assert.match(header,/root\.isAuthenticated \? root\.ownProfileRequested\(\) : root\.accountRequested\(\)/);
  assert.match(shell,/onOwnProfileRequested: root\.openMyProfileFromNavigation\(\)/);
  assert.match(settings,/objectName: "settingsRecoveryRevoke"/);
  assert.match(settings,/objectName: "settingsRecoverySignOut"/);
});
test('compact Mini Apps use equal tab widths, saved mark and a separate remove action',()=>{
  const app=read('ui/components/AppsSurface.qml');
  assert.match(app,/id: destinationCard[\s\S]{0,900}Layout\.preferredWidth: 0/);
  assert.match(app,/visible: Boolean\(cardCell\.modelData\.saved\)[\s\S]{0,350}fillColor: Theme\.positive/);
  assert.match(app,/saved \? \(inLibrary \? Theme\.iconTrash : Theme\.iconCheck\)/);
});
test('image overlay controls are pinned to opposite safe viewport edges',()=>{
  const gallery=read('ui/components/MediaGallery.qml');
  assert.match(gallery,/id: galleryCounter[\s\S]{0,150}anchors\.bottom: parent\.bottom/);
  assert.match(gallery,/y: root\.width < 700 \? Math\.max\(Theme\.space3, 48\)/);
});
test('unverified token branding cannot replace market-source artwork',()=>{
  const logo=read('ui/components/MarketAssetIcon.qml');
  const card=read('ui/components/TokenAddressPreview.qml');
  assert.doesNotMatch(logo,/if \(root\.symbol === "GENY"\) return "brands\/geny"/);
  assert.match(logo,/remoteSource: String\(root\.asset/);
  assert.match(card,/MouseArea \{\s*anchors.fill: parent[\s\S]{0,180}onClicked: root\.expanded/);
});
