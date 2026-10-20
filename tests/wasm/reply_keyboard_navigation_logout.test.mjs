import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=(name)=>readFileSync(new URL('../../ui/components/'+name,import.meta.url),'utf8');

test('reply editor stays visible when iOS visual viewport shrinks',()=>{
 const drawer=read('ComposerDrawer.qml');
 assert.match(drawer,/property real expandedViewportHeight: 0/);
 assert.match(drawer,/readonly property bool keyboardVisible:/);
 assert.match(drawer,/root\.parent\.height < root\.expandedViewportHeight - 125/);
 assert.match(drawer,/function ensureEditorVisible\(editor\)/);
 assert.match(drawer,/editor\.mapToItem\(composerContent, 0, 0\)/);
 assert.match(drawer,/onKeyboardVisibleChanged:/);
 assert.match(drawer,/visible: \(root\.parentCast !== null \|\| root\.quotedCast !== null\)/);
 assert.match(drawer,/&& !root\.keyboardVisible/);
 assert.match(drawer,/onActiveEditorChangedExternally: \(blockIndex, editor\)/);
 assert.match(drawer,/root\.ensureEditorVisible\(threadEditor\)/);
});
test('confirmed reply returns to parent conversation and is shown first exactly once',()=>{
 const app=read('AppShell.qml');
 const detail=read('DetailSurface.qml');
 const reply=read('ThreadReplyItem.qml');
 assert.match(app,/root\.submittedReplyParentId = root\.composerParent/);
 assert.match(app,/presentationBridge\.writePhase === "confirmed"/);
 assert.match(app,/root\.confirmedReply = \{/);
 assert.match(app,/presentationBridge\.openCast\(parentId\)/);
 assert.match(detail,/readonly property var prioritizedReplies:/);
 assert.match(detail,/if \(!found\) ordered\.unshift\(root\.confirmedReply\)/);
 assert.match(detail,/ordered\.unshift\(reply\)/);
 assert.match(detail,/ListView \{/);
 assert.match(detail,/model: root\.activeView === "cast" \? root\.prioritizedReplies : \[\]/);
 assert.match(detail,/reuseItems: true/);
 assert.match(detail,/function focusConfirmedReply\(\)/);
 assert.match(reply,/property bool highlighted: false/);
 assert.match(reply,/genycaster\.detail\.your_confirmed_reply/);
});
test('logout requires confirmation and reopens Setup from first step',()=>{
 const app=read('AppShell.qml');
 const settings=read('LocaleSettingsPopup.qml');
 const setup=read('InitialSetup.qml');
 assert.match(app,/objectName: "signOutConfirmation"/);
 assert.match(app,/GenyAdaptiveDialog \{\s*id: signOutConfirm/);
 assert.match(app,/blurSourceItem: shellContent/);
 assert.match(app,/function requestSignOut\(\)/);
 assert.match(app,/onClicked: root\.requestSignOut\(\)/);
 assert.match(app,/onSignOutConfirmationRequested: root\.requestSignOut\(\)/);
 assert.match(app,/objectName: "confirmSignOutButton"/);
 assert.match(settings,/root\.signOutConfirmationRequested\(\)/);
 assert.match(setup,/highestReachedStep = 0/);
 assert.doesNotMatch(setup,/switchStep\(returningAfterLogout \? 4 : 0\)/);
});
test('beta label is short and is translated in every locale',()=>{
 const settings=read('LocaleSettingsPopup.qml');
 assert.match(settings,/text: qsTrId\("genycaster\.settings\.beta_release_short"\)/);
 assert.doesNotMatch(settings,/current_beta_release"\)\s*\.arg\(updateController\.appVersion\)/);
 for (const lang of ['en_US','fa_IR','ar_SA','fr_FR','de_DE','es_ES','pt_BR','ru_RU','hi_IN','ja_JP','ko_KR','zh_CN']){
  const data=readFileSync(new URL('../../ui/i18n/qml_'+lang+'.ts',import.meta.url),'utf8');
  for (const id of ['genycaster.detail.your_confirmed_reply','genycaster.settings.beta_release_short','genycaster.session.logout_confirm_title','genycaster.session.logout_confirm_description'])
   assert.ok(data.includes('id="'+id+'"'),lang+': '+id);
 }
});

test('mobile keyboard expands Composer to visible Qt viewport instead of a reading pane',()=>{
 const app=read('AppShell.qml');
 const composer=read('ComposerDrawer.qml');
 const adaptive=read('GenyAdaptiveDialog.qml');
 assert.match(app,/hostItem: root\.mobile \? root : readingPane/);
 assert.match(composer,/height: parent \? \(compact \? \(keyboardVisible/);
 assert.match(composer,/Math\.max\(1, parent\.height - Theme\.space2\)/);
 assert.match(composer,/visible: !root\.keyboardVisible\n\s+Layout\.fillWidth: true\n\s+mode: root\.editorMode/);
 assert.match(composer,/root\.height - Theme\.space5 \* 7/);
 assert.match(adaptive,/property Item blurSourceItem: null/);
 assert.match(adaptive,/Overlay\.modal: DrawerBackdrop/);
 assert.match(adaptive,/sourceItem: control\.blurSourceItem/);
 assert.match(adaptive,/blurSourceItem: control\.blurSourceItem/);
});
