import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(p,'utf8');
const dock=read('ui/components/navigation/BottomNavigation.qml');
const shell=read('ui/components/shell/AppShell.qml');
const feed=read('ui/components/feed/FeedSurface.qml');
const profile=read('ui/components/profile/ProfileSurface.qml');

test('mobile header gains vertical breathing room without duplicating iOS safe-area',()=>{
  const block=shell.slice(shell.indexOf('id: mobileWorkspace'),
      shell.indexOf('id: readingPane'));
  assert.match(block,/AppHeader \{[\s\S]*?Layout\.topMargin: 0/);
  assert.match(block,/AppHeader \{[\s\S]*?Layout\.bottomMargin: 0/);
  assert.doesNotMatch(block,/safeAreaTop|safe-area-inset-top/);
});

test('frosted nav samples only a small backdrop region and masks its edges',()=>{
  assert.match(dock,/import QtQuick\.Effects/);
  assert.match(dock,/glassEnabled: false/);
  assert.match(dock,/useBackdropBlur: glassEnabled && blurSourceItem !== null/);
  // The sampled page includes its canvas and excludes the dock itself.
  assert.match(shell,/id: readingBackdrop[\s\S]*?color: Theme\.canvas/);
  assert.match(shell,/blurSourceItem: readingBackdrop/);
  assert.doesNotMatch(shell,/layer\.enabled: root\.mobile && (?:feedSurfaceLoader|profileSurfaceLoader)/);
  assert.doesNotMatch(dock,/contentScrolling|viewportScrollRecent|viewportGlassSettle/);
  assert.match(dock,/live: root\.useBackdropBlur/);
  const glass=dock.slice(dock.indexOf('id: navigationCapsule'),
      dock.indexOf('objectName: "mobileDockHome"'));
  assert.match(glass,/objectName: "mobileCapsuleBackdropCapture"/);
  assert.match(glass,/anchors\.fill: parent/);
  assert.match(glass,/sourceItem: root\.useBackdropBlur \? root\.blurSourceItem : null/);
  assert.match(glass,/sourceRect: Qt\.rect\([\s\S]*?expandedBackdrop\.width, expandedBackdrop\.height/);
  assert.match(glass,/mapFromItem\(navigationCapsule, 0, 0\)/);
  assert.match(glass,/source: capsuleBackdrop/);
  assert.match(glass,/navigationCapsule\.backdropOrigin\.x - navigationCapsule\.sampleLeft/);
  assert.match(glass,/navigationCapsule\.backdropOrigin\.y - navigationCapsule\.sampleTop/);
  // Convolution needs a larger source than its final 56px visible mask:
  // otherwise edge texels stretch text and image highlights along the pill.
  assert.match(glass,/readonly property real blurOverscan: 48/);
  assert.match(glass,/id: expandedBackdrop/);
  assert.match(glass,/x: -navigationCapsule\.sampleLeft/);
  assert.match(glass,/y: -navigationCapsule\.sampleTop/);
  assert.match(glass,/Math\.min\(blurOverscan, Math\.max\(0, backdropOrigin\.x\)\)/);
  assert.match(glass,/root\.blurSourceItem\.height - backdropOrigin\.y - height/);
  assert.doesNotMatch(glass,/navigationCapsule\.width \* 0\.9/);
  assert.match(glass,/live: root\.useBackdropBlur/);
  assert.match(glass,/recursive: false/);
  assert.match(glass,/objectName: "mobileCapsuleGlassBlur"/);
  assert.match(glass,/blurMax: 32/);
  assert.match(glass,/blur: 0\.87/);
  assert.match(glass,/blurMultiplier: 1\.3/);
  assert.match(glass,/id: capsuleBackdrop[\s\S]*?visible: false/);
  assert.match(glass,/id: capsuleMask/);
  assert.match(glass,/curveDepth: height \/ 2/);
  assert.match(glass,/objectName: "mobileNavigationChromaticReflection"/);
  assert.match(glass,/reflectionSource: root\.reflectionSource/);
  assert.match(glass,/maskEnabled: true/);
  assert.match(glass,/maskSource: capsuleMaskTexture/);
  assert.match(glass,/objectName: "mobileCapsuleGlassTint"/);
  assert.match(glass,/fillColor: root\.useBackdropBlur[\s\S]*?Theme\.navigationGlassTint : Theme\.surfaceElevated/);
  assert.match(glass,/borderWidth: 0/);
});

test('floating content paints behind dock and last items still scroll into view',()=>{
  assert.match(shell,/anchors\.bottomMargin: 0/);
  assert.match(feed,/bottomMargin: root\.compact\s+\? Math\.max\(96, root\.bottomOverlayInset \+ Theme\.space2\) : 0/);
  assert.match(profile,/bottomMargin: root\.compact\s+\? Math\.max\(96, root\.bottomOverlayInset \+ Theme\.space2\) : 0/);
  assert.match(shell,/bottomOverlayInset: mobileBottomNavigation\.visible \? mobileBottomNavigation\.height : 0/);
  assert.match(dock,/objectName: "mobilePrimaryActionButton"/);
  assert.doesNotMatch(dock,/mobileFooterBackground|mobileContentLowerEdge/);
});
