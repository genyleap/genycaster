import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=readFileSync(new URL('../../ui/components/profile/BookmarksSurface.qml',import.meta.url),'utf8');
test('desktop bookmark grid uses bounded widths without a circular child layout',()=>{
 assert.match(source,/readonly property real cardWidth: Math\.min\(520, Math\.max\(0,/);
 assert.match(source,/gridAvailableWidth - \(columnCount - 1\) \* Theme\.space3/);
 assert.match(source,/Layout\.preferredWidth: savedCastsGrid\.cardWidth/);
 assert.match(source,/clip: true/);
});
test('desktop bookmarks show a capped excerpt with an explicit detail action',()=>{
 assert.match(source,/objectName: "desktopBookmarkSummary"/);
 assert.match(source,/visible: !root\.compact && !castSkeletonFallback\.visible/);
 assert.match(source,/objectName: "desktopBookmarkExcerpt"/);
 assert.match(source,/maximumLineCount: 4/);
 assert.match(source,/elide: Text\.ElideRight/);
 assert.match(source,/objectName: "desktopBookmarkOpenCast"/);
 assert.match(source,/onClicked: root\.castRequested\(/);
});
test('full CastItem rendering is retained only for mobile bookmarks',()=>{
 assert.match(source,/id: savedCast[\s\S]*?visible: root\.compact && !Boolean\(castSkeletonFallback\.visible\)/);
 assert.match(source,/onReactionRequested: \(kind, cast, active\)/);
 assert.match(source,/id: bookmarkActions/);
 assert.match(source,/objectName: "bookmarkAssignCollectionPicker"/);
});