import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(p,'utf8');
const dock=read('ui/components/BottomNavigation.qml');
const shell=read('ui/components/AppShell.qml');
const feed=read('ui/components/FeedSurface.qml');
const profile=read('ui/components/ProfileSurface.qml');

test('mobile header gains vertical breathing room without duplicating iOS safe-area',()=>{
  const block=shell.slice(shell.indexOf('id: mobileWorkspace'),
      shell.indexOf('id: readingPane'));
  assert.match(block,/AppHeader \{[\s\S]*?Layout\.topMargin: root\.mobile \? Theme\.space3 : 0/);
  assert.match(block,/AppHeader \{[\s\S]*?Layout\.bottomMargin: root\.mobile \? Theme\.space3 : 0/);
  assert.doesNotMatch(block,/safeAreaTop|safe-area-inset-top/);
});

test('frosted nav samples only a small backdrop region and masks its edges',()=>{
  assert.match(dock,/import QtQuick\.Effects/);
  assert.match(dock,/glassEnabled: false/);
  assert.match(dock,/useBackdropBlur: glassEnabled && blurSourceItem !== null/);
  assert.match(shell,/blurSourceItem: readingContent\s+glassEnabled: !Theme\.powerSave/);
  const glass=dock.slice(dock.indexOf('id: navigationCapsule'),
      dock.indexOf('objectName: "mobileDockHome"'));
  assert.match(glass,/objectName: "mobileCapsuleBackdropCapture"/);
  assert.match(glass,/anchors\.fill: parent/);
  assert.match(glass,/sourceItem: root\.useBackdropBlur \? root\.blurSourceItem : null/);
  assert.match(glass,/sourceRect: \{/);
  assert.match(glass,/mapFromItem\(navigationCapsule, 0, 0\)/);
  assert.match(glass,/textureSize: Qt\.size\(Math\.max\(1, Math\.round\(navigationCapsule\.width \* 0\.7\)\)/);
  assert.match(glass,/Math\.round\(navigationCapsule\.height \* 0\.7\)/);
  assert.match(glass,/live: root\.useBackdropBlur/);
  assert.match(glass,/recursive: false/);
  assert.match(glass,/objectName: "mobileCapsuleGlassBlur"/);
  assert.match(glass,/blurMax: 32/);
  assert.match(glass,/blur: 0\.95/);
  assert.match(glass,/layer\.enabled: true/);
  assert.match(glass,/id: capsuleMask/);
  assert.match(glass,/radius: height \/ 2/);
  assert.match(glass,/maskEnabled: true/);
  assert.match(glass,/maskSource: capsuleMask/);
  assert.match(glass,/objectName: "mobileCapsuleGlassTint"/);
  assert.match(glass,/fillColor: Theme\.dark \? "#843A4151" : "#9AFFFFFF"/);
  assert.match(glass,/borderWidth: 0/);
});

test('floating content paints behind dock and last items still scroll into view',()=>{
  assert.match(shell,/anchors\.bottomMargin: 0/);
  assert.match(feed,/bottomMargin: root\.compact \? 96 : 0/);
  assert.match(profile,/bottomMargin: root\.compact \? 96 : 0/);
  assert.doesNotMatch(shell,/\? mobileBottomNavigation\.height : 0/);
  assert.match(dock,/objectName: "mobilePrimaryActionButton"/);
  assert.doesNotMatch(dock,/mobileFooterBackground|mobileContentLowerEdge/);
});
