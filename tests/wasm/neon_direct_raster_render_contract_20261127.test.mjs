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

test('reflection capture and both blur stages retain their previous texture during reader movement',()=>{
 const source=read('ui/components/visual/NeonReflectionSource.qml');
 assert.match(source,/running: root\.browserRenderer && root\.capturing && !root\.resizingCapture && !root\.motionActive/);
 assert.equal((source.match(/live: root\.capturing[^\n]*!root\.motionActive/g)||[]).length,3);
 assert.match(source,/onMotionActiveChanged:\s*\{\s*if \(!motionActive && capturing && !resizingCapture\)/);
 const shell=read('ui/components/shell/AppShell.qml');
 assert.match(shell,/motionActive: Boolean\([\s\S]*?feedSurfaceLoader\.item\.readList\.moving[\s\S]*?profileSurfaceLoader\.item\.readList\.moving[\s\S]*?homeDashboard\.scrolling/);
});

test('top refresh is driven by overshoot without moving content outside its bounds',()=>{
 const indicator=read('ui/components/feed/PullToRefresh.qml');
 assert.match(indicator,/-Number\(flickable\.verticalOvershoot \|\| 0\)/);
 assert.match(indicator,/function onVerticalOvershootChanged\(\) \{ control\.recordPullDistance\(\) \}/);
 assert.match(indicator,/y: Math\.min\(24,/);
 for(const path of ['feed/FeedSurface','feed/FeedMasonryView','feed/NotificationSurface','profile/BookmarksSurface','profile/ProfileSurface'])
  assert.match(read('ui/components/'+path+'.qml'),/boundsMovement: Flickable\.StopAtBounds/);
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
