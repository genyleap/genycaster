import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(path,'utf8');
const dock=read('ui/components/BottomNavigation.qml');
const shell=read('ui/components/AppShell.qml');
const html=read('ui/wasm/genycaster-shell.html');

test('floating navigation paints no full-width footer or separator, including PWA chrome',()=>{
  assert.match(dock,/objectName: "mobileActionDock"/);
  assert.doesNotMatch(dock,/footerBackground|mobileFooterBackground|contentLowerEdge|mobileContentLowerEdge|footerColor/);
  assert.doesNotMatch(dock,/Rectangle\s*\{\s*anchors\.fill:\s*parent/);
  assert.doesNotMatch(html,/@media\(max-width:700px\)\{body::after/);
  assert.doesNotMatch(html,/background-color:#202128/);
  assert.match(shell,/id: mobileBottomNavigation[\s\S]*?height: 88/);
  assert.match(shell,/anchors\.bottomMargin: 0/);
  assert.match(read('ui/components/FeedSurface.qml'),/bottomMargin: root\.compact \? 96 : 0/);
  assert.match(read('ui/components/ProfileSurface.qml'),/bottomMargin: root\.compact \? 96 : 0/);
  assert.match(dock,/anchors\.topMargin: 8/);
  assert.match(dock,/anchors\.leftMargin: Theme\.mobileGutter/);
  assert.match(dock,/anchors\.rightMargin: Theme\.mobileGutter/);
});

test('capsule, selected states and action button are border-free in light and dark themes',()=>{
  const destination=dock.slice(dock.indexOf('component DockDestination: Button {'),dock.indexOf('\n    RowLayout {\n        id: dockRow'));
  const capsule=dock.slice(dock.indexOf('objectName: "mobileNavigationCapsule"'),dock.indexOf('objectName: "mobileDockHome"'));
  const primary=dock.slice(dock.indexOf('objectName: "mobilePrimaryActionButton"'));
  assert.match(destination,/background: Superellipse \{[\s\S]*?borderWidth: 0/);
  assert.doesNotMatch(destination,/borderColor:/);
  assert.match(capsule,/fillColor: "transparent"/);
  assert.match(capsule,/objectName: "mobileCapsuleGlassTint"/);
  assert.match(capsule,/fillColor: Theme\.dark \? "#843A4151" : "#9AFFFFFF"/);
  assert.match(capsule,/borderWidth: 0/);
  assert.match(primary,/fillColor: Theme\.mobileActionColor[\s\S]*?borderWidth: 0/);
  assert.doesNotMatch(primary,/borderColor:/);
});

test('easing is responsive, interruptible and respects reduced motion',()=>{
  assert.match(dock,/readonly property bool animateDock: !Theme\.reduceMotion/);
  for(const section of ['implicitWidth','fillColor','scale','opacity'])
    assert.match(dock,new RegExp('Behavior on '+section+' \\{'));
  assert.match(dock,/NumberAnimation \{ duration: 210; easing\.type: Easing\.OutCubic \}/);
  assert.match(dock,/ColorAnimation \{ duration: 185; easing\.type: Easing\.OutCubic \}/);
  assert.match(dock,/ColorAnimation \{ duration: 230; easing\.type: Easing\.OutCubic \}/);
  assert.match(dock,/scale: mobilePrimaryAction\.down \? 0\.91 : 1/);
  const allBehaviors=[...dock.matchAll(/Behavior on (?:fillColor|scale|color|opacity|implicitWidth) \{\s*enabled: ([^\n]+)/g)];
  assert.ok(allBehaviors.length>=6);
  assert.ok(allBehaviors.every(match=>match[1].trim()==='root.animateDock'));
});

test('action routing, RTL mirroring and destinations are unchanged',()=>{
  assert.match(dock,/layoutDirection: Theme\.uiDirection/);
  const destinations=['mobileDockHome','mobileDockExplore','mobileDockApps','mobileBottomBookmarksButton','mobileDockProfile'];
  for(const name of destinations)
    assert.match(dock,new RegExp('objectName: "'+name+'"'));
  for(const [action,signal] of [['photo','photoRequested'],['search','searchRequested'],['settings','settingsRequested']])
    assert.match(dock,new RegExp('case "'+action+'": root\\.'+signal+'\\(\\)'));
  assert.match(dock,/default: root\.createRequested\(\)/);
  assert.match(shell,/onCreateRequested: root\.openComposer\(null\)/);
  assert.match(shell,/onPhotoRequested: root\.openComposer\(null, "media"\)/);
  assert.match(shell,/onSearchRequested: searchPalette\.open\(\)/);
});
