import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const between=(s,start,end)=>s.slice(s.indexOf(start),s.indexOf(end,s.indexOf(start)));

test('native HyperSnap editor contains no visible username controls or explanatory placeholder',()=>{
  const profile=read('ui/components/profile/ProfileSurface.qml');
  const editor=between(profile,'objectName: "profileEditorScroll"','objectName: "profileProtocolDisplay"');
  const form=between(editor,'// The username editor is Snapchain-only.','objectName: "profileProtocolDisplay"');
  assert.match(form,/objectName: "profileProtocolUsername"\s+visible: !root\.accountIsNativeCustody/);
  assert.match(form,/objectName: "profileProtocolUsernameSave"\s+visible: !root\.accountIsNativeCustody/);
  assert.match(form,/text: qsTrId\("genycaster\.composer\.username"\)/);
  assert.match(form,/text: qsTrId\("genycaster\.profile\.username_separate"\)/);
  assert.equal((form.match(/visible: !root\.accountIsNativeCustody/g)||[]).length,4,
    'label, field, note and save action must all be absent on HyperSnap');
  assert.doesNotMatch(profile,/genycaster\.profile\.native_username_registration_pending/);
  assert.match(profile,/objectName: "profileProtocolDisplay"/);
  assert.match(profile,/objectName: "profileProtocolBio"/);
  assert.match(profile,/objectName: "profileProtocolAvatar"/);
  assert.match(profile,/objectName: "profileProtocolBanner"/);
  assert.match(profile,/visible: root\.showUsername\s+Layout\.fillWidth: true/);
  assert.match(profile,/readonly property bool showUsername: !\(root\.isOwnProfile && root\.accountIsNativeCustody\)/);
});

test('C++ account profile and write boundary preserve Snapchain username while hiding native usernames',()=>{
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(bridge,/QString PresentationBridge::accountUsername\(\) const[\s\S]*?nativeCustodyAccount\(\) \? QString\{\} : m_accountUsername/);
  const handler=between(bridge,'bool PresentationBridge::updateProtocolProfile(','void PresentationBridge::openProfile()');
  assert.match(handler,/if \(key == QStringLiteral\("username"\)\)/);
  assert.match(handler,/if \(m_accountIdentityIssuer == QStringLiteral\("hypersnap:native-registry"\)\)\s+return false;/);
  assert.ok(handler.includes('QStringLiteral("^[a-z0-9]'));
  assert.doesNotMatch(handler,/fields\.clear\(\)/);
  const save=between(read('ui/components/profile/ProfileSurface.qml'),'objectName: "profileProtocolSave"','onClicked: {');
  assert.ok(save.includes('presentationBridge.writeReady'));
});

test('unregistered fid:* values never become @handles in cast, bookmarks, notification and search surfaces',()=>{
  const cases=[
    ['ui/components/feed/CastItem.qml',/visible: root\.authorHandle\.length > 0/],
    ['ui/components/feed/DetailSurface.qml',/!\(root\.accountIsNativeCustody/],
    ['ui/components/profile/ProfileListItem.qml',/visible: root\.authorHandle\.length > 0/],
    ['ui/components/profile/BookmarksSurface.qml',/visible: actualHandle\.length > 0 && !\/\^fid:/],
    ['ui/components/feed/NotificationSurface.qml',/visible: actualHandle\.length > 0 && !\/\^fid:/],
    ['ui/components/social/ChannelManagementPanel.qml',/visible: actualHandle\.length > 0 && !\/\^fid:/],
    ['ui/components/media/EmbedCard.qml',/visible: actualHandle\.length > 0 && !\/\^fid:/],
    ['ui/components/navigation/SearchPalette.qml',/actualHandle\.length > 0/],
    ['ui/components/navigation/SocialSidebar.qml',/visible: root\.shownHandle\.length > 0/],
    ['ui/components/navigation/ContextRail.qml',/visible: actualHandle\.length > 0 && !\/\^fid:/],
    ['ui/components/profile/StreakCard.qml',/visible: actualHandle\.length > 0 && !\/\^fid:/],
  ];
  for(const [path,pattern] of cases)
    assert.match(read(path),pattern,path);
  const settings=read('ui/components/settings/SettingsPopup.qml');
  assert.match(settings,/visible: !presentationBridge\.nativeCustodyAccount[\s\S]*?Boolean\(presentationBridge\.accountUsername\)/);
  assert.doesNotMatch(settings,/text: "@" \+ String\(presentationBridge\.accountProfile\.username/);
});

test('suggested profiles without username navigate by FID rather than searching for an empty name',()=>{
  const rail=read('ui/components/navigation/ContextRail.qml');
  assert.match(rail,/signal profileFidRequested\(double fid\)/);
  assert.match(rail,/if \(fid > 0\) root\.profileFidRequested\(fid\)/);
  const shell=read('ui/components/shell/AppShell.qml');
  assert.equal((shell.match(/onProfileFidRequested: fid => root\.openProfileFromContent\(fid\)/g)||[]).length,2);
});

test('native username text is retained only for backward-compatible locale lookup',()=>{
  const profile=read('ui/components/profile/ProfileSurface.qml');
  assert.doesNotMatch(profile,/qsTrId\("genycaster\.profile\.network_description"\)/);
  assert.doesNotMatch(profile,/qsTrId\("genycaster\.profile\.native_username_registration_pending"\)/);
  const retained=read('ci/i18n_retained_ids.txt');
  assert.match(retained,/genycaster\.profile\.native_username_registration_pending/);
});
