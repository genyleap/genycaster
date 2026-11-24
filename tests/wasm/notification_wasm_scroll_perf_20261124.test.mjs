import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {validImageUrl} from '../../services/media-gateway/browser_image_proxy.mjs';

const text = name => readFileSync(new URL('../../' + name,import.meta.url),'utf8');

test('WASM direct wheel scroll enters an explicit gesture state before changing contentY',()=>{
  const wheel=text('ui/app/platform/wheel.cpp');
  const start=wheel.indexOf('bool moveAxis('),end=wheel.indexOf('enum class Result',start);
  const section=wheel.slice(start,end);
  assert.match(section,/const QVariant wheelActive = item->property\("browserWheelInProgress"\)/);
  const guard=section.indexOf('item->setProperty("browserWheelInProgress", true)');
  const scroll=section.indexOf('item->setProperty(positionName, next)');
  assert.ok(guard>0 && scroll>guard,'the QML scroll guard precedes the contentY write');
  assert.match(wheel,/pendingWheelInput\.pixels \+= pixels/);
  assert.match(wheel,/emscripten_request_animation_frame\(deliverBrowserWheelFrame/);
});

test('notification anchors never override active browser wheel movement',()=>{
  const qml=text('ui/components/feed/NotificationSurface.qml');
  assert.match(qml,/property bool browserWheelInProgress: false/);
  assert.match(qml,/function scheduleViewportRestore[\s\S]*?list\.browserWheelInProgress/);
  assert.match(qml,/function stabilizeViewportAfterGeometryChange[\s\S]*?list\.browserWheelInProgress/);
  assert.match(qml,/function requestStableModelSync[\s\S]*?list\.browserWheelInProgress/);
  assert.match(qml,/id: viewportRestoreTimer[\s\S]*?if \(list\.browserWheelInProgress \|\| list\.moving\)/);
  assert.match(qml,/onBrowserWheelInProgressChanged:[\s\S]*?viewportRestoreTimer\.stop\(\)/);
  assert.match(qml,/onContentYChanged:[\s\S]*?if \(list\.browserWheelInProgress\)/);
  assert.match(qml,/onContentHeightChanged:[\s\S]*?if \(!list\.moving && !list\.browserWheelInProgress\)/);
  assert.match(qml,/id: browserWheelSettleTimer[\s\S]*?list\.browserWheelInProgress = false/);
  assert.match(qml,/id: browserWheelSettleTimer[\s\S]*?root\.captureViewportAnchor\(\)/);
  assert.match(qml,/id: browserWheelSettleTimer[\s\S]*?root\.requestStableModelSync\(\)/);
});

test('WASM notifications use bounded delegate cache and defer shader resources during scrolling',()=>{
  const qml=text('ui/components/feed/NotificationSurface.qml');
  assert.match(qml,/cacheBuffer: Math\.min\(600, Math\.max\(240, height \* 0\.45\)\)/);
  assert.match(qml,/reuseItems: true/);
  assert.match(qml,/ambientNeon:[\s\S]*?!list\.browserWheelInProgress/);
  assert.match(qml,/Qt\.platform\.os !== "wasm" \|\| notificationRow\.latestActivity/);
  assert.match(qml,/interactive: !list\.moving && !list\.browserWheelInProgress/);
});

test('recycled notifications do not start costly speculative miniapp and link preview requests',()=>{
  const qml=text('ui/components/feed/NotificationSurface.qml');
  const link=text('ui/components/media/LinkPreview.qml');
  const controller=text('ui/app/browser/link_metadata.cpp');
  assert.match(qml,/metadataDiscoveryEnabled: false/);
  assert.match(link,/property bool metadataDiscoveryEnabled: true/);
  assert.match(link,/if \(!root\.metadataDiscoveryEnabled \|\| Qt\.platform\.os !== 'wasm'/);
  assert.match(link,/if \(!root\.metadataDiscoveryEnabled \|\| Qt\.platform\.os !== "wasm"/);
  assert.match(link,/!root\.metadataDiscoveryEnabled \|\| root\.metadataManagedExternally/);
  assert.match(link,/Boolean\(root\.scrollHost\.browserWheelInProgress\)/);
  assert.match(controller,/notFoundRetryMs\{5 \* 60 \* 1000\}/);
  assert.match(controller,/status == 404 \? notFoundRetryMs/);
});

test('legitimate Terminus cast images are allowed without turning browser gateway into open proxy',()=>{
  const good='https://media.onterminus.com/cbae63ae6740e06b05684dddf05bbe8b.jpg';
  assert.ok(validImageUrl(good));
  assert.equal(validImageUrl('https://media.onterminus.com/admin/index.html'),null);
  assert.equal(validImageUrl(good+'?url=https://127.0.0.1/private'),null);
  assert.equal(validImageUrl('http://media.onterminus.com/cbae63ae6740e06b05684dddf05bbe8b.jpg'),null);
  assert.equal(validImageUrl('https://media.onterminus.com/api/private'),null);
});

test('immutable WASM gzip reuses binary identity rather than deployment fingerprint',()=>{
  const loader=text('ui/wasm/genycaster-loader.js');
  const section=loader.slice(loader.indexOf('async function compressedBrowserEngine'),
    loader.indexOf('window.fetch=async function'));
  assert.match(section,/const releaseToken=name\.match\(\/\^genycaster\\\.\(\[0-9a-f\]\{20\}\)\\\.wasm\$\/\)/);
  assert.match(section,/cache:"force-cache"/);
  assert.doesNotMatch(section,/const releaseToken=String\(releaseInfo\.version/);
  assert.match(section,/new DecompressionStream\("gzip"\)/);
});

test('offline precache is bounded to two concurrent assets while the large WASM loads',()=>{
  const worker=text('ui/wasm/genycaster-sw.js');
  assert.match(worker,/let next=0;/);
  assert.match(worker,/const precache=async\(\)=>/);
  assert.match(worker,/Math\.min\(2,urls\.length\)/);
  assert.doesNotMatch(worker,/Promise\.all\(urls\.map/);
  assert.match(worker,/WASM_HASH\.test\(relative\)\)return;/);
});
