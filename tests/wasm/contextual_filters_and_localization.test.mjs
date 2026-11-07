import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
const app=read('ui/components/AppShell.qml');
const feed=read('ui/components/FeedSurface.qml');
const profile=read('ui/components/ProfileSurface.qml');
const sheet=read('ui/components/ContentFilterSheet.qml');
const bookmarks=read('ui/components/BookmarksSurface.qml');

test('topic discovery updates existing Explorer/Profile lists, never a new route',()=>{
 assert.match(app,/visible: surfaceRequested/);
 assert.doesNotMatch(app,/visible: surfaceRequested && !root\.topicBrowseActive/);
 assert.match(app,/active: false && root\.topicBrowseActive/);
 assert.match(app,/rows: root\.withoutMutedActors\(\s*root\.topicBrowseActive && root\.topicBrowseScope==="explorer"/);
 assert.match(app,/rows: root\.topicBrowseActive && root\.topicBrowseScope==="profile"/);
 assert.match(app,/onLoadMoreRequested: \{\s*if\(root\.topicBrowseActive && root\.topicBrowseScope==="explorer"\)/);
 assert.match(app,/onLoadMoreRequested: \{\s*if\(root\.topicBrowseActive && root\.topicBrowseScope==="profile"\)/);
 assert.match(app,/openProofConnections\.selectCastTopic\(chosen,root\.topicBrowseAuthorFid\)/);
 assert.match(app,/presentationBridge\.hydrateTopicCasts\(pending\)/);
 assert.match(feed,/if \(root\.topicFilterActive\) \{\s*result\.push/);
 assert.match(profile,/readonly property var displayRows: root\.sortedRows\(\)/);
 assert.match(profile,/topicFilterActive: false/);
});

test('filter labels are translatable in every available UI language',()=>{
 const languages=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
 for(const locale of languages) {
  const catalog=read('ui/i18n/qml_'+locale+'.ts');
  for(const key of ['explorer','profile','bookmarks','topic','audience','content','sort','collections','all_topics','all_bookmarks','clear_all','apply','hint','audience_hint'])
   assert.match(catalog,new RegExp('id="genycaster\\.filters\\.'+key+'"'),locale+': '+key);
 }
 assert.match(sheet,/qsTrId\("genycaster\.filters\.apply"\)/);
 assert.match(sheet,/qsTrId\("genycaster\.filters\.clear_all"\)/);
 assert.match(bookmarks,/qsTrId\("genycaster\.filters\.all_bookmarks"\)/);
});

test('filter options live in labeled rounded groups without losing touch access',()=>{
 assert.match(sheet,/delegate: Superellipse \{/);
 assert.match(sheet,/id: sectionGroup/);
 assert.match(sheet,/id: sectionContent/);
 assert.match(sheet,/Flow \{[\s\S]*?layoutDirection: Theme\.uiDirection/);
 assert.match(sheet,/implicitHeight: 44/);
 assert.match(sheet,/contentHeight: filterGroups\.implicitHeight \+ Theme\.space3/);
 assert.match(sheet,/onOpened: resetDraft\(\)/);
 assert.match(sheet,/root\.applied\(Object\.assign\(\{\}, root\.draft\)\)/);
});
