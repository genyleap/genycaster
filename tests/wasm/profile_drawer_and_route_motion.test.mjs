import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const profile=read('ui/components/profile/ProfileSurface.qml');
const shell=read('ui/components/shell/AppShell.qml');
const settings=read('ui/components/settings/SettingsPopup.qml');
test('profile profession and category are selectable, validated, and retain saved roles',()=>{
  assert.match(profile,/readonly property var profileCategoryOptions:/);
  assert.match(profile,/readonly property var profileRoleOptions:/);
  assert.match(profile,/objectName: "profileCategoryPicker"/);
  assert.match(profile,/objectName: "profileProfessionPicker"/);
  assert.match(profile,/GenyComboBox \{\s*id: categoryEditor/);
  assert.match(profile,/GenyMultiSelect \{\s*id: professionEditor/);
  assert.match(profile,/root\.profileRoleOptions\.indexOf\(role\)/);
  assert.match(profile,/root\.profileCategoryOptions\.indexOf\(categoryName\)/);
  assert.match(profile,/category: categoryName/);
  assert.match(profile,/root\.profileExtras\.category/);
  assert.match(profile,/savedRole = String\(root\.profileExtras\.profession/);
  assert.match(profile,/profileExtrasEditor\.roleSelection\.length/);
  assert.match(profile,/busy: root\.busy[\s\S]*?enabled: !root\.busy/);
});
test('mobile profile binds avatar and connections to the same header band',()=>{
  assert.match(profile,/readonly property int profileAvatarX: root\.profileContentRtl/);
  assert.match(profile,/id: ownerSocialBadgeStrip[\s\S]*?visible: root\.isOwnProfile\s/);
  assert.match(profile,/objectName: "profileConnectionsButton"/);
  assert.doesNotMatch(profile,/objectName: "compactVerifiedSocialBadges"/);
  assert.match(profile,/Layout\.preferredWidth: Theme\.scaleUi\(root\.compact \? 29 : 34\)/);
});
test('profile extras are compact, labelled and reachable through a mobile sheet',()=>{
  assert.match(profile,/objectName: "profileMoreLinksInline"/);
  assert.match(profile,/objectName: "profileIdentityLinks"/);
  assert.match(profile,/id: profileLinksDrawer[\s\S]*?bottomSheet: mobilePresentation/);
  assert.match(profile,/id: profileExtrasEditor[\s\S]*?bottomSheet: mobilePresentation/);
  assert.match(profile,/id: profileLinksDrawer[\s\S]*?title: qsTrId\("genycaster\.profile\.links_title"\)/);
  assert.match(profile,/userLabel: root\.profileExtraLabel\(index\)/);
  assert.match(profile,/labels: unique\.map\(e=>e\.label\)/);
  assert.match(profile,/linksEditor\.text = saved\.map/);
  assert.match(profile,/objectName: "profileOwnerActions"/);
  assert.match(profile,/text: qsTrId\("genycaster\.profile\.save_additional_details"\)/);
  assert.doesNotMatch(profile,/Popup \{\s*id: profileExtrasEditor/);
});
test('mobile uses equal-width Share and Edit buttons; desktop uses a real profile menu',()=>{
  const start=profile.indexOf('objectName: "profileOwnerActions"');
  const end=profile.indexOf('DirectionAwareLabel {',start);
  const actions=profile.slice(start,end);
  assert.ok(start>0 && end>start);
  assert.match(actions,/objectName: "profileShareAction"/);
  assert.match(actions,/objectName: "profileEditAction"/);
  assert.match(actions,/visible: root\.isOwnProfile && root\.compact/);
  assert.equal((actions.match(/Layout\.fillWidth: true/g)||[]).length,3);
  assert.equal((actions.match(/Layout\.minimumWidth: 0/g)||[]).length,2);
  assert.equal((actions.match(/Layout\.preferredWidth: Math\.max\(0, \(root\.profileContentWidth - Theme\.space2\) \/ 2\)/g)||[]).length,2);
  assert.match(profile,/objectName: "profileOptionsButton"/);
  assert.match(profile,/objectName: "profileOptionsButton"[\s\S]*?visible: true[\s\S]*?iconGlyph: Theme\.iconMore/);
  assert.match(profile,/objectName: "profileOptionsMenu"/);
  assert.match(profile,/visible: root\.isOwnProfile[\s\S]*?text: qsTrId\("genycaster\.profile\.edit_profile"\)/);
  assert.equal((actions.match(/compact: true/g)||[]).length,2);
  assert.equal((actions.match(/pill: true/g)||[]).length,2);
  assert.match(actions,/text: qsTrId\("genycaster\.profile\.share_profile"\)/);
  assert.match(actions,/text: qsTrId\("genycaster\.profile\.edit_profile"\)/);
  assert.match(actions,/presentationBridge\.shareUrl\(/);
  assert.match(actions,/profileExtrasEditor\.open\(\)/);
  assert.doesNotMatch(actions,/text: qsTrId\("genycaster\.profile\.links_title"\)/);
});
test('profile link and provider focus use shared superellipse geometry',()=>{
  assert.match(profile,/background: Superellipse \{[\s\S]*?fillColor: socialBadge\.down/);
  assert.match(profile,/background: PopupSurface \{[\s\S]*?curveDepth: Theme\.curveCompact/);
  assert.match(profile,/background: NeonStatePlate \{[\s\S]*?hovered: identityMoreLinks\.hovered/);
  assert.match(profile,/focused: Theme\.keyboardFocusVisible\(identityMoreLinks\)/);
  assert.doesNotMatch(profile,/color: identityMoreLinks\.hovered \? Theme\.surfaceHover/);
});

test('website and additional links are in the identity row, not below the biography',()=>{
  const start=profile.indexOf('objectName: "profileIdentityRow"');
  const end=profile.indexOf('objectName: "profileProfessionalCategory"',start);
  const identity=profile.slice(start,end);
  assert.ok(start>0 && end>start);
  assert.match(identity,/objectName: "profileIdentityLinks"/);
  assert.match(identity,/objectName: "profileIdentityLinks"[\s\S]*?layoutDirection: Theme\.uiDirection/);
  assert.match(identity,/objectName: "profileIdentityPrimaryLink"/);
  assert.match(identity,/objectName: "profileMoreLinksInline"/);
  assert.match(identity,/onClicked: profileLinksDrawer\.open\(\)/);
  assert.match(profile,/readonly property string primaryProfileLinkUrl:/);
  assert.match(profile,/readonly property int additionalProfileLinksCount:/);
  assert.doesNotMatch(profile,/objectName: "ownerPersonalLinks"|objectName: "profilePrimaryLinkRow"/);
  assert.ok(profile.indexOf('objectName: "profileIdentityLinks"')
    < profile.indexOf('sourceText: root.normalizedBio'));
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
