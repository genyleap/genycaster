import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('static clipped images sample Qt image textures without allocating offscreen image and mask layers',()=>{
 const qml=read('ui/components/visual/SuperellipseImage.qml');
 const direct=qml.slice(qml.indexOf('    Loader {\n        id: directRaster'),qml.indexOf('    // GIF animation'));
 assert.match(direct,/property variant source: imageSource/);
 assert.match(direct,/fragmentShader: "qrc:\/genycaster\/effects\/raster-contour\.frag\.qsb"/);
 assert.match(direct,/active: root\.directMask && root\.activeSource/);
 assert.doesNotMatch(direct,/ShaderEffectSource|MultiEffect|layer\.enabled/);
 assert.match(qml,/layer\.enabled: root\.maskActive && !root\.directMask/);
 assert.match(qml,/active: root\.legacyMaskActive/);
 assert.match(qml,/GraphicsInfo\.api !== GraphicsInfo\.Software/);
 assert.match(qml,/ShaderEffect\.Error\) root\.directRasterFailed = true/);
 assert.match(qml,/visible: !root\.directMask && root\.stillVisible/);
 const shader=read('ui/shaders/raster-contour.frag');
 assert.match(shader,/vec4 qt_SubRect_source;/);
 assert.match(shader,/texture\(source, atlasUv\) \* \(coverage \* qt_Opacity\)/);
 assert.doesNotMatch(shader,/fwidth\(|dFdx\(|dFdy\(/);
 const cmake=read('ui/CMakeLists.txt');
 assert.match(cmake,/foreach\(_shader IN ITEMS [^\n]*raster-contour\)/);
 assert.match(cmake,/shaders\/raster-contour\.frag\.qsb/);
});

test('scrolling uses one viewport sample for both media lifetime boundaries',()=>{
 const image=read('ui/components/visual/SuperellipseImage.qml');
 assert.equal((image.match(/id: rasterScope\b/g)||[]).length,1);
 assert.doesNotMatch(image,/rasterRetentionScope/);
 const scope=read('ui/components/media/MediaResourceScope.qml');
 assert.match(scope,/viewportDistance: ownerAvailable && observeViewport\s*\? distanceToViewport\(\)/);
 assert.match(scope,/eligible: ownerAvailable && viewportDistance < viewportMargin/);
 assert.match(scope,/retained: ownerAvailable && viewportDistance < retentionMargin/);
});

test('reflection updates during reader movement with bounded coherent snapshots',()=>{
 const source=read('ui/components/visual/NeonReflectionSource.qml');
 assert.match(source,/interval: root\.motionActive \? \(root\.browserRenderer \? 96 : 32\) : 240/);
 assert.match(source,/running: root\.capturing && !root\.resizingCapture\s*&& \(root\.browserRenderer \|\| root\.motionActive\)/);
 assert.equal((source.match(/live: root\.capturing[^\n]*!root\.motionActive/g)||[]).length,3);
 assert.match(source,/onMotionActiveChanged: root\.refreshCapture\(\)/);
 const refresh=source.slice(source.indexOf('function refreshCapture()'),source.indexOf('onMotionActiveChanged:'));
 assert.match(refresh,/if \(!root\.capturing \|\| root\.resizingCapture/);
 assert.doesNotMatch(refresh,/root\.motionActive/);
 assert.match(source,/capture\.scheduleUpdate\(\)\s*horizontalTexture\.scheduleUpdate\(\)\s*blurredTexture\.scheduleUpdate\(\)/);
 const shell=read('ui/components/shell/AppShell.qml');
 assert.match(shell,/motionActive: Boolean\([\s\S]*?feedSurfaceLoader\.item\.readList\.moving[\s\S]*?profileSurfaceLoader\.item\.readList\.moving[\s\S]*?homeDashboard\.scrolling/);
});

test('inactive reflection receivers do not observe the source coordinate chain',()=>{
 for (const file of ['NeonReflection','NeonHalo','NeonSurface']) {
  const receiver=read('ui/components/visual/'+file+'.qml');
  assert.match(receiver,/NeonGeometry\.translatedRect\([^,]+,\s*reflectionSource && reflectionSource\.capturing \? reflectionSource\.sourceItem : null\)/);
 }
});

test('top refresh moves the reading canvas and places the indicator inside the exposed gap',()=>{
 const indicator=read('ui/components/feed/PullToRefresh.qml');
 assert.match(indicator,/Math\.max\(0, flickable\.originY - flickable\.contentY\)/);
 assert.match(indicator,/y: Math\.max\(0, \(offset - height\) \* 0\.5\)/);
 assert.match(indicator,/visible: startedAtTop && offset >= height/);
 for(const path of ['feed/FeedSurface','feed/FeedMasonryView','feed/NotificationSurface','profile/BookmarksSurface','profile/ProfileSurface'])
  assert.match(read('ui/components/'+path+'.qml'),/boundsMovement: Flickable\.FollowBoundsBehavior/);
});

test('startup reveal waits for compositor turns, and authenticated greeting keeps its slot before profile hydration',()=>{
 const loader=read('ui/wasm/genycaster-loader.js');
 assert.match(loader,/if\(failed\|\|finished\|\|!qtRuntimeLoaded\|\|!qmlRootReady\|\|!accountRestoreSettled/);
 assert.match(loader,/requestAnimationFrame\(\(\)=>requestAnimationFrame\(\(\)=>\{/);
 assert.match(loader,/readyRenderBoundary=true;\s*revealReady\(\)/);
 const home=read('ui/components/home/HomeDashboard.qml');
 assert.match(home,/root\.authenticated \? "" : qsTrId\("genycaster\.home\.hub_tagline"\)/);
 assert.match(home,/Layout\.preferredHeight: Math\.ceil\(mobileGreetingMetrics\.height\)/);
 assert.match(home,/FontMetrics \{ id: mobileGreetingMetrics; font: mobileGreeting\.font \}/);
});
