import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = p => readFileSync(new URL('../../' + p, import.meta.url), 'utf8');
const sheet=read('ui/components/ContentFilterSheet.qml');
const shell=read('ui/components/AppShell.qml');
const settings=read('ui/components/LocaleSettingsPopup.qml');
const bookmarks=read('ui/components/BookmarksSurface.qml');
const profile=read('ui/components/ProfileSurface.qml');
const topics=read('ui/components/TopicBrowseSurface.qml');
test('the QML manifest registers a real product-owned filter sheet',()=>{
  assert.match(read('ui/CMakeLists.txt'), /components\/ContentFilterSheet\.qml/);
  assert.match(sheet, /GenyDrawer \{/);
  assert.match(sheet, /bottomSheet: true/);
  assert.match(sheet, /implicitHeight: 44/);
  assert.match(sheet, /signal applied\(var values\)/);
  assert.match(sheet, /onOpened: resetDraft\(\)/);
  assert.match(sheet, /onClicked: root\.clearDraft\(\)/);
  assert.match(sheet, /root\.applied\(Object\.assign\(\{\}, root\.draft\)\)/);
  assert.match(sheet, /root\.close\(\)/);
});
test('Explorer keeps desktop menu and opens touch sheet on phones',()=>{
  assert.match(shell, /visible: !root\.mobile && presentationBridge\.isAuthenticated/);
  assert.match(shell, /onClicked: explorerFilterSheet\.open\(\)/);
  assert.match(shell, /root\.chooseExplorerAudience\(String\(values\.audience/);
  assert.match(shell, /root\.openTopicBrowser\(String\(values\.topic\),"explorer",0\)/);
});
test('Profile and topic discovery use touch filter actions without discarding desktop sort',()=>{
  assert.match(profile, /id: profileFilterSheet/);
  assert.match(profile, /onClicked: profileFilterSheet\.open\(\)/);
  assert.match(profile, /visible: !root\.compact\s+Layout\.maximumWidth: 136/);
  assert.match(profile, /root\.profileFeedRequested\(String\(values\.feed/);
  assert.match(topics, /id: topicFilterSheet/);
  assert.match(topics, /onClicked: topicFilterSheet\.open\(\)/);
  assert.match(topics, /visible: !root\.compact\s+Layout\.preferredWidth: 200/);
});
test('mobile bookmarks do not stack collection and sort dropdowns above content',()=>{
  assert.match(bookmarks, /id: bookmarkFilterSheet/);
  assert.match(bookmarks, /onClicked: bookmarkFilterSheet\.open\(\)/);
  assert.match(bookmarks, /visible: !root\.compact[\s\S]{0,220}id: bookmarkSortPicker/);
  assert.match(bookmarks, /visible: !root\.compact[\s\S]{0,220}id: collectionPicker/);
  assert.match(bookmarks, /root\.collectionSelected\(String\(values\.collection/);
});
test('Settings leads to verified account profile without impersonating an edit API',()=>{
  assert.match(settings,/objectName: "settingsProfileCard"/);
  assert.match(settings,/visible: presentationBridge\.isAuthenticated/);
  assert.match(settings,/signal profileRequested\(\)/);
  assert.match(settings,/onTapped: root\.profileRequested\(\)/);
  assert.match(shell,/onProfileRequested: \{\s*close\(\)\s*root\.openMyProfileFromNavigation\(\)/);
});
