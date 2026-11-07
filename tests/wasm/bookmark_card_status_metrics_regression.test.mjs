import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source = name => readFileSync(new URL('../../ui/components/' + componentPath(name) + '.qml', import.meta.url), 'utf8');

test('bookmark cards measure their width from the viewport instead of the grid itself', () => {
  const qml = source('BookmarksSurface');
  const grid = qml.slice(qml.indexOf('GridLayout {\n            id: savedCastsGrid'));
  const card = grid.slice(grid.indexOf('delegate: Superellipse {'), grid.indexOf('ColumnLayout {\n                    id: savedContent'));
  assert.match(grid,/readonly property real gridAvailableWidth: Math\.max\(0,/);
  assert.match(grid,/root\.width - 2 \* \(root\.compact \? Theme\.mobileGutter : Theme\.contentGutter\)/);
  assert.match(grid,/readonly property real cardWidth: Math\.min\(520/);
  assert.match(grid,/columns: columnCount/);
  assert.match(card,/Layout\.preferredWidth: savedCastsGrid\.cardWidth/);
  assert.match(card,/Layout\.minimumWidth: savedCastsGrid\.cardWidth/);
  assert.match(card,/Layout\.maximumWidth: savedCastsGrid\.cardWidth/);
  assert.doesNotMatch(card,/savedCastsGrid\.width/);
  assert.doesNotMatch(card,/Layout\.maximumWidth: Layout\.preferredWidth/);
  assert.match(card,/Layout\.preferredHeight: implicitHeight/);
  assert.match(card,/implicitHeight: Math\.max\(110,/);
  assert.match(grid,/Layout\.preferredHeight: visible \? Math\.max\(1, implicitHeight\) : 0/);
  assert.match(grid,/objectName: "bookmarkRichCast"/);
  assert.match(grid,/onMediaRequested: \(mediaItems, index\) => root\.mediaRequested\(mediaItems, index\)/);
});

test('compact bookmark grid does not force media into tiny three-column cells at ordinary desktop widths',()=>{
  const qml=source('BookmarksSurface');
  assert.match(qml,/root\.compact \|\| gridAvailableWidth < 660/);
  assert.match(qml,/gridAvailableWidth < 1680 \? 2 : 3/);
  assert.match(qml,/columnSpacing: Theme\.space3/);
  assert.match(qml,/rowSpacing: Theme\.space3/);
  assert.match(qml,/Layout\.alignment: Qt\.AlignTop \| Qt\.AlignLeft/);
  // Simulate the same thresholds to catch accidentally oversized or zero cells.
  for (const [available,expectedColumns] of [[330,1],[659,1],[660,2],[1200,2],[1679,2],[1680,3],[2000,3]]) {
    const columns=available<660?1:available<1680?2:3;
    const width=Math.min(600,Math.max(0,(available-(columns-1)*12)/columns));
    assert.equal(columns,expectedColumns);
    assert.ok(width > 0 && width <= 600);
  }
});

test('Live and HyperSnap labels use measured glyph advances, not a 120px clamp',()=>{
  const qml=source('StatusFooter');
  assert.match(qml,/TextMetrics \{\s*id: liveStatusMetrics/);
  assert.match(qml,/TextMetrics \{\s*id: providerStatusMetrics/);
  assert.match(qml,/font: liveStatusLabel\.font/);
  assert.match(qml,/font: providerLabel\.font/);
  assert.match(qml,/Math\.ceil\(liveStatusMetrics\.advanceWidth\) \+ 7 \+ Theme\.space2/);
  assert.match(qml,/Math\.ceil\(providerStatusMetrics\.advanceWidth\)/);
  assert.match(qml,/Layout\.minimumWidth: Math\.ceil\(liveStatusMetrics\.advanceWidth\)/);
  assert.match(qml,/Layout\.minimumWidth: Math\.ceil\(providerStatusMetrics\.advanceWidth\)/);
  assert.doesNotMatch(qml,/Math\.min\(120,/);
  assert.doesNotMatch(qml,/\? 12 : 112/);
  assert.match(qml,/text: nodeProviderLabel/);
});
