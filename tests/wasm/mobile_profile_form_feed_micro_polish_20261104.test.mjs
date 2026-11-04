import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source = name => readFileSync(new URL('../../ui/components/' + name + '.qml', import.meta.url),'utf8');

test('profile edit/status sheets fill mobile viewport and retain independently scrolling forms',()=>{
 const qml=source('ProfileSurface');
 const edit=qml.slice(qml.indexOf('id: profileExtrasEditor'),qml.indexOf('id: profileStatusEditor'));
 const status=qml.slice(qml.indexOf('id: profileStatusEditor'),qml.indexOf('id: profileLinksDrawer'));
 for(const section of [edit,status]){
   assert.match(section,/interactive: false/);
   assert.match(section,/fullScreen: mobilePresentation/);
   assert.match(section,/height: parent \? parent\.height/);
   assert.match(section,/showHandle: false/);
   assert.match(section,/sheetPresentation: false/);
   assert.match(section,/Layout\.fillHeight: true/);
 }
 assert.match(edit,/id: profileEditorScroll/);
 assert.match(status,/id: statusFormScroll/);
 assert.match(status,/objectName: "profileStatusSave"/);
});

test('noninteractive bottom sheets can swipe-dismiss by grabber alone, preserving text selection and scroll',()=>{
 const qml=source('GenyDrawer');
 assert.match(qml,/visible: control\.bottomSheet && control\.showHandle/);
 assert.match(qml,/width: 96\s*height: 28/);
 assert.match(qml,/enabled: !control\.interactive/);
 assert.match(qml,/onReleased: mouse => \{\s*if \(mouse\.y - startY > 32\) control\.close\(\)/);
 assert.match(qml,/Accessible\.onPressAction: control\.close\(\)/);
 assert.match(qml,/Sheet|sheetTopInset/);
});

test('profile filter uses shared center sheet header and keeps the controls anchored',()=>{
 const qml=source('ContentFilterSheet');
 assert.match(qml,/topPadding: phoneLayout \? Theme\.space4 : contentPadding/);
 assert.match(qml,/ModalHeader \{\s*Layout\.fillWidth: true\s*title: root\.heading/);
 assert.match(qml,/sheetPresentation: root\.phoneLayout/);
 assert.match(qml,/onCloseRequested: root\.close\(\)/);
 assert.match(qml,/id: filterScroller/);
 assert.match(qml,/text: qsTrId\("genycaster\.filters\.apply"\)/);
});

test('RTL Explorer audience buttons have bounded widths with correct 2-segment mirrored selection',()=>{
 const qml=source('HomeFeedHeader');
 const control=qml.slice(qml.indexOf('id: audienceSelector'),qml.indexOf('ContentTabs {'));
 assert.match(control,/Layout\.preferredWidth: root\.compact \? 228 : 240/);
 assert.match(control,/Layout\.minimumWidth: Layout\.preferredWidth/);
 assert.match(control,/clip: true/);
 assert.match(control,/Layout\.minimumWidth: 0/);
 assert.match(control,/Layout\.maximumWidth: Layout\.preferredWidth/);
 assert.match(control,/layoutDirection: Theme\.uiDirection/);
 assert.match(control,/root\.audience === "following"\) !== Theme\.rtl/);
});

test('mobile media bar keeps GIF separate from emoji and does not overflow on Persian keyboard',()=>{
 const qml=source('ComposerDrawer');
 const bar=qml.slice(qml.indexOf('objectName: "mobileComposerMediaBar"'),qml.indexOf('objectName: "composerClearContentMobile"'));
 assert.match(bar,/spacing: Theme\.space3/);
 assert.match(bar,/layoutDirection: Theme\.uiDirection/);
 assert.match(bar,/objectName: "mobileComposerGifAction"/);
 assert.match(bar,/Layout\.minimumWidth: Theme\.scaleUi\(50\)/);
 assert.match(bar,/Layout\.maximumWidth: Theme\.scaleUi\(50\)/);
 assert.match(bar,/leftPadding: Theme\.space1/);
 assert.match(bar,/iconGlyph: Theme\.iconEmoji/);
 assert.match(bar,/iconGlyph: Theme\.iconImage/);
 assert.match(bar,/enabled: root\.canUploadMedia/);
});

test('Mini App card height includes exactly its artwork gutters and one-line captions do not reserve two lines',()=>{
 const qml=source('LinkPreview');
 assert.match(qml,/implicitHeight: content\.implicitHeight\s*\+ \(root\.layoutMiniApp \? Theme\.space4/);
 assert.match(qml,/anchors\.margins: root\.layoutMiniApp \? Theme\.space4/);
 assert.match(qml,/Layout\.preferredHeight: visible\s*\? Math\.min\(root\.bodyLineHeight \* 2, implicitHeight\) : 0/);
 assert.match(qml,/root\.layoutMiniApp\s*Layout\.fillWidth: true\s*spacing: Theme\.space2/);
 assert.match(qml,/alignToUi: true\s*text: root\.siteName\.length/);
 assert.match(qml,/alignToUi: true\s*text: root\.titleText/);
 assert.match(qml,/onClicked: root\.openPreview\(\)/);
});
