import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('Explorer route re-entry preserves existing scroll; only active tab re-tap scrolls to top',()=>{
 const src=read('ui/components/AppShell.qml');
 const fn=src.slice(src.indexOf('    function openExplorer() {'),src.indexOf('    function openTrends() {'));
 assert.match(fn,/const wasExplorer =/);
 assert.match(fn,/if \(wasExplorer\) Qt\.callLater/);
 assert.match(fn,/feedSurfaceLoader\.item\.scrollToBeginning\(\)/);
 assert.doesNotMatch(fn,/presentationBridge\.refreshLatestFeed\(\)/);
});
test('large feed re-renders preserve a stable viewport anchor instead of resetting contentY',()=>{
 const src=read('ui/components/FeedSurface.qml');
 assert.match(src,/function captureViewportForRebuild\(\)/);
 assert.match(src,/function queueViewportRestore\(snapshot\)/);
 assert.match(src,/function restoreViewportAfterRebuild\(\)/);
 assert.match(src,/readList\.positionViewAtIndex\(index, ListView\.Beginning\)/);
 assert.match(src,/const snapshot = root\.captureViewportForRebuild\(\)/);
 assert.match(src,/root\.queueViewportRestore\(snapshot\)/);
 assert.match(src,/onMovementStarted: \{/);
 assert.match(src,/stableViewportRestoreTimer\.stop\(\)/);
 assert.match(src,/geometryAnchorDebounce/);
 assert.match(src,/function stabilizeGeometryAnchor\(\)/);
});
test('browser miniapp request is per-cast URL, preserving query ID to fetch signed 3:2 poster',()=>{
 const browser=read('ui/mobile/browser_link_metadata_controller.cpp');
 assert.match(browser,/query\.addQueryItem\(QStringLiteral\("url"\),request\.url\)/);
 assert.doesNotMatch(browser,/request\.kind == QLatin1String\("miniapp"\)\s*\? origin/);
 const component=read('ui/components/LinkPreview.qml');
 assert.match(component,/root\.browserMiniAppPreview\.providerKind === "miniapp"/);
 assert.match(component,/root\.previewData\.miniAppLaunchUrl \|\| root\.targetUrl/);
 assert.doesNotMatch(component,/metadata\.miniAppLaunchUrl = root\.targetUrl/);
});
