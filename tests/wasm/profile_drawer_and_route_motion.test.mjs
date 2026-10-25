import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const profile=read('ui/components/ProfileSurface.qml');
const shell=read('ui/components/AppShell.qml');
const settings=read('ui/components/LocaleSettingsPopup.qml');
test('mobile profile binds avatar and connections to the same header band',()=>{
  assert.match(profile,/readonly property int profileAvatarX: root\.compact\s*\? root\.profileContentX \+ Theme\.space2/);
  assert.match(profile,/id: ownerSocialBadgeStrip[\s\S]*?visible: root\.isOwnProfile && root\.ownerSocialLinks\.length > 0/);
  assert.doesNotMatch(profile,/objectName: "compactVerifiedSocialBadges"/);
  assert.match(profile,/Layout\.preferredWidth: Theme\.scaleUi\(root\.compact \? 29 : 34\)/);
});
test('profile extras are compact, labelled and reachable through a mobile sheet',()=>{
  assert.match(profile,/objectName: "profileMoreLinksInline"/);
  assert.match(profile,/objectName: "ownerPersonalLinks"/);
  assert.match(profile,/id: profileLinksDrawer[\s\S]*?bottomSheet: mobilePresentation/);
  assert.match(profile,/id: profileExtrasEditor[\s\S]*?bottomSheet: mobilePresentation/);
  assert.match(profile,/id: profileLinksDrawer[\s\S]*?title: qsTr\("Links"\)/);
  assert.match(profile,/root\.profileExtraLabel\(linkSheetRow\.index\)/);
  assert.match(profile,/labels: unique\.map\(e=>e\.label\)/);
  assert.match(profile,/linksEditor\.text = saved\.map/);
  assert.match(profile,/objectName: "profileOwnerActions"/);
  assert.match(profile,/text: qsTr\("Save changes"\)/);
  assert.doesNotMatch(profile,/Popup \{\s*id: profileExtrasEditor/);
});
test('owner actions are short matching Edit/Share pills, not a full-width Edit and redundant Links button',()=>{
  const start=profile.indexOf('objectName: "profileOwnerActions"');
  const end=profile.indexOf('DirectionAwareLabel {',start);
  const actions=profile.slice(start,end);
  assert.ok(start>0 && end>start);
  assert.match(actions,/objectName: "profileShareAction"/);
  assert.match(actions,/objectName: "profileEditAction"/);
  assert.match(actions,/Layout\.maximumWidth: Theme\.scaleUi\(180\)/);
  assert.equal((actions.match(/compact: true/g)||[]).length,2);
  assert.equal((actions.match(/pill: true/g)||[]).length,2);
  assert.match(actions,/text: qsTr\("Share profile"\)/);
  assert.match(actions,/text: qsTr\("Edit profile"\)/);
  assert.match(actions,/presentationBridge\.shareUrl\(/);
  assert.match(actions,/profileExtrasEditor\.open\(\)/);
  assert.doesNotMatch(actions,/text: qsTr\("Links"\)/);
});
test('website and its extra-link count are grouped at the physical right edge',()=>{
  const start=profile.indexOf('objectName: "profilePrimaryLinkRow"');
  const end=profile.indexOf('visible: root.showLocation',start);
  const primary=profile.slice(start,end);
  assert.ok(start>0 && end>start);
  assert.match(primary,/layoutDirection: Qt\.LeftToRight/);
  assert.match(primary,/Item \{ Layout\.fillWidth: true \}/);
  assert.match(primary,/Layout\.maximumWidth: Math\.max\(100, root\.profileContentWidth \* 0\.53\)/);
  assert.match(primary,/objectName: "profileMoreLinksInline"/);
  assert.match(primary,/onClicked: profileLinksDrawer\.open\(\)/);
  const fallback=profile.slice(profile.indexOf('objectName: "ownerPersonalLinks"'),
    profile.indexOf('objectName: "profileOwnerActions"'));
  assert.match(fallback,/layoutDirection: Qt\.LeftToRight/);
  assert.match(fallback,/Item \{ Layout\.fillWidth: true \}/);
});
test('global navigation shares cubic slide/fade and honors reduced-motion settings',()=>{
  for (const id of ['appsSurfaceLoader','notificationsRouteLoader','feedSurfaceLoader','profileSurfaceLoader']) {
    const start=shell.indexOf('id: '+id);
    assert.ok(start>=0,id);
    const body=shell.slice(start,start+1450);
    assert.match(body,/transform: Translate/);
    assert.match(body,/duration: Theme\.reduceMotion \? 0 : Theme\.motionEmphasis/);
  }
  assert.match(shell,/id: homeDashboard[\s\S]{0,1000}transform: Translate/);
  assert.match(settings,/id: settingsPages/);
  assert.match(settings,/onCurrentIndexChanged: \{/);
  assert.match(settings,/target: settingsPages/);
  assert.match(settings,/duration: Theme\.motionEmphasis/);
  assert.match(settings,/!Theme\.reduceMotion && root\.opened/);
});
