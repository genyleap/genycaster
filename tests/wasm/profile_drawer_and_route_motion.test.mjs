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
