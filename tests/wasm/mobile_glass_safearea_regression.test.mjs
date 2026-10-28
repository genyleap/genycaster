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
  assert.match(shell,/blurSourceItem: readingContent\s+glassEnabled: true/);
  // The compositor shares its live source texture with the dock: no
  // separate re-render of the scrolled ListView at the previous scroll offset.
  assert.match(shell,/id: readingContent[\s\S]*?layer\.enabled: root\.mobile/);
  assert.match(shell,/layer\.smooth: true/);
  assert.doesNotMatch(dock,/contentScrolling|viewportScrollRecent|viewportGlassSettle/);
  assert.match(dock,/live: root\.useBackdropBlur/);
  const glass=dock.slice(dock.indexOf('id: navigationCapsule'),
      dock.indexOf('objectName: "mobileDockHome"'));
  assert.match(glass,/objectName: "mobileCapsuleBackdropCapture"/);
  assert.match(glass,/anchors\.fill: parent/);
  assert.match(glass,/sourceItem: root\.useBackdropBlur \? root\.blurSourceItem : null/);
  assert.match(glass,/sourceRect: \{/);
  assert.match(glass,/mapFromItem\(navigationCapsule, 0, 0\)/);
  // Convolution needs a larger source than its final 56px visible mask:
  // otherwise edge texels stretch text and image highlights along the pill.
  assert.match(glass,/readonly property real blurOverscan: 36/);
  assert.match(glass,/id: expandedBackdrop/);
  assert.match(glass,/x: -navigationCapsule\.sampleLeft/);
  assert.match(glass,/y: -navigationCapsule\.sampleTop/);
  assert.match(glass,/Math\.min\(blurOverscan, Math\.max\(0, backdropOrigin\.x\)\)/);
  assert.match(glass,/root\.blurSourceItem\.height - backdropOrigin\.y - height/);
  assert.match(glass,/expandedBackdrop\.width, expandedBackdrop\.height/);
  assert.match(glass,/textureSize: Qt\.size\(Math\.max\(1, Math\.round\(expandedBackdrop\.width\)\)/);
  assert.match(glass,/Math\.round\(expandedBackdrop\.height\)/);
  assert.doesNotMatch(glass,/navigationCapsule\.width \* 0\.9/);
  assert.match(glass,/live: root\.useBackdropBlur/);
  assert.match(glass,/recursive: false/);
  assert.match(glass,/objectName: "mobileCapsuleGlassBlur"/);
  assert.match(glass,/blurMax: 32/);
  assert.match(glass,/blur: 0\.72/);
  assert.match(glass,/layer\.enabled: true/);
  assert.match(glass,/id: capsuleMask/);
  assert.match(glass,/radius: height \/ 2/);
  assert.match(glass,/maskEnabled: true/);
  assert.match(glass,/maskSource: capsuleMask/);
  assert.match(glass,/objectName: "mobileCapsuleGlassTint"/);
  assert.match(glass,/fillColor: Theme\.dark \? "#703A4151" : "#8CFFFFFF"/);
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
