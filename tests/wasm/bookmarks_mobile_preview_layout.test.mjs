import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const bookmarks=read('ui/components/BookmarksSurface.qml');
const link=read('ui/components/LinkPreview.qml');
const cast=read('ui/components/CastItem.qml');

test('mobile bookmark header never asks three text buttons and a 260px select to share one row',()=>{
  assert.match(bookmarks,/ColumnLayout \{[\s\S]*?bookmarkSortPicker/);
  assert.match(bookmarks,/id: collectionPicker[\s\S]*?Layout\.minimumWidth: 0/);
  assert.match(bookmarks,/iconOnly: root\.compact[\s\S]*?text: "New collection"/);
  assert.match(bookmarks,/iconOnly: true[\s\S]*?text: "Refresh collections"/);
  assert.match(bookmarks,/id: collectionNameEditor[\s\S]*?Layout\.minimumWidth: 0/);
});
test('collection refresh uses GenyCaster spinner in its fixed-size action, not Qt BusyIndicator',()=>{
  assert.doesNotMatch(bookmarks,/\bBusyIndicator\s*\{/);
  assert.match(bookmarks,/text: "Refresh collections"\s*busy: root\.collectionsBusy/);
});
test('cast channel renders the canonical /slug rather than a mutable human display name',()=>{
  assert.match(cast,/text: "\/" \+ root\.castChannel/);
  assert.match(cast,/forceLeftToRight: true\s*text: "\/" \+ root\.castChannel/);
});
test('ordinary link pictures get a full-width aspect-preserving frame on mobile',()=>{
  const generic=link.slice(link.indexOf('id: genericLinkCard'));
  assert.match(generic,/GridLayout \{/);
  assert.match(generic,/columns: root\.compact \? 1 : 2/);
  assert.match(generic,/Layout\.fillWidth: root\.compact && shouldReserve/);
  assert.match(generic,/Layout\.preferredHeight:[\s\S]*?root\.previewImageAspect/);
  assert.match(generic,/fillMode: Image\.PreserveAspectFit/);
  assert.doesNotMatch(generic,/root\.compact \? 116 : 156/);
});
