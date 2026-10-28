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
  assert.match(bookmarks,/iconOnly: root\.compact[\s\S]*?text: qsTrId\("genycaster\.bookmarks\.new_collection"\)/);
  assert.match(bookmarks,/iconOnly: true[\s\S]*?text: qsTrId\("genycaster\.bookmarks\.refresh_collections"\)/);
  assert.match(bookmarks,/id: collectionNameEditor[\s\S]*?Layout\.minimumWidth: 0/);
});
test('collection refresh uses GenyCaster spinner in its fixed-size action, not Qt BusyIndicator',()=>{
  assert.doesNotMatch(bookmarks,/\bBusyIndicator\s*\{/);
  assert.match(bookmarks,/text: qsTrId\("genycaster\.bookmarks\.refresh_collections"\)\s*busy: root\.collectionsBusy/);
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

test('compact bookmark filters have bounded widths and do not consume the full row',()=>{
  assert.match(bookmarks,/id: bookmarkSortPicker[\s\S]*?Layout\.preferredWidth: root\.compact \? 144 : 184/);
  assert.match(bookmarks,/Layout\.maximumWidth: root\.compact \? 144 : 184/);
  assert.match(bookmarks,/id: collectionPicker[\s\S]*?Layout\.preferredWidth: root\.compact \? 168 : 200/);
  assert.match(bookmarks,/Layout\.maximumWidth: root\.compact \? 168 : 240/);
  assert.match(bookmarks,/visible: !root\.compact[\s\S]*?text: qsTrId\("genycaster\.filters\.collections"\)/);
});
test('owned Farcaster images fall back to their original HTTPS URL only after a provider error',()=>{
  const tile=read('ui/components/GalleryTile.qml');
  assert.match(tile,/readonly property string ownMediaUrl:/);
  assert.match(tile,/media-farcaster/);
  assert.match(tile,/if \(root\.originFallbackAttempted && root\.ownMediaUrl\.length > 0\)/);
  assert.match(tile,/onLoadFailedChanged: \{/);
  assert.match(tile,/dataStorage\.autoDownloadAllowed\("photos"\)/);
  assert.match(tile,/root\.originFallbackAttempted = true/);
  assert.match(tile,/onEmbedChanged: root\.originFallbackAttempted = false/);
});
