import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const bookmarks=read('ui/components/BookmarksSurface.qml');
const shell=read('ui/components/AppShell.qml');
const sheet=read('ui/components/ContentFilterSheet.qml');

test('collection selection displays the server-confirmed collection rather than resetting to placeholder',()=>{
  assert.match(bookmarks,/function savedCollectionIndex\(hash\)/);
  assert.match(bookmarks,/String\(membership\.cast_hash \|\| ""\)/);
  assert.match(bookmarks,/currentIndex: root\.savedCollectionIndex\(/);
  assert.doesNotMatch(bookmarks,/addToCollectionPicker\.currentIndex=0/);
  assert.match(shell,/membershipItems: openProofConnections \? openProofConnections\.bookmarkMembershipItems : \[\]/);
  const controller=read('ui/app/openproof_connections_controller.cpp');
  assert.match(controller,/m_bookmarkMembershipItems=std::move\(items\)/);
  assert.match(controller,/sendBookmarkCollectionRequest\(\s*QStringLiteral\("bookmark_collection_items"\),\{\},false\)/);
});
test('filter sections have explicit scroll geometry and do not rely on a zero-height ScrollView',()=>{
  assert.match(sheet,/Flickable \{\s+id: filterScroller/);
  assert.match(sheet,/contentHeight: filterGroups\.implicitHeight \+ Theme\.space3/);
  assert.match(sheet,/Column \{\s+id: filterGroups/);
  assert.match(sheet,/readonly property bool phoneLayout/);
  assert.match(sheet,/width: parent \? \(phoneLayout \? parent\.width/);
});
