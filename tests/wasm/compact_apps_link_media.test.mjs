import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const base=new URL('../../',import.meta.url);
const read=path=>readFileSync(new URL(path,base),'utf8');

test('mobile and desktop Explorer use compact store rows and real launch actions',()=>{
 const apps=read('ui/components/AppsSurface.qml');
 assert.match(apps,/columns: 3[\s\S]*?Layout\.preferredHeight: root\.compact \? 48 : 48/);
 assert.match(apps,/targetColumns: root\.compact\s*\? 1/);
 assert.match(apps,/cellHeight: root\.compact \? 116/);
 assert.match(apps,/id: miniAppStoreTile[\s\S]*?visible: root\.compact/);
 assert.match(apps,/objectName: "miniAppMobileOpenButton"/);
 assert.match(apps,/objectName: "miniAppMobileTopicFilter"/);
 assert.match(apps,/objectName: "miniAppDesktopStoreRow"/);
 assert.match(apps,/TapHandler \{ onTapped: root\.showAppDetails\(cardCell\.modelData\) \}/);
});
test('WASM gallery corners use one shared image mask, not vector inverse covers',()=>{
 const layout=read('ui/components/GalleryLayout.qml');
 const tile=read('ui/components/GalleryTile.qml');
 assert.match(layout,/useLayeredMask: Qt\.platform\.os === "wasm"/);
 assert.match(layout,/Qt\.platform\.os === "osx" && root\.visibleCount === 1/);
 assert.match(tile,/visible: !root\.useLayeredMask && preview\.ready/);
 assert.match(tile,/clipToSuperellipse: root\.useLayeredMask/);
});
test('verified market logos take precedence over decorative bundled GENY artwork',()=>{
 const icon=read('ui/components/MarketAssetIcon.qml');
 assert.match(icon,/root\.semanticIcon\.length > 0 && root\.remoteSource\.length === 0/);
 assert.match(icon,/source: root\.remoteSource/);
});
test('browser link metadata is separate from Mini App manifests and media slots are populated',()=>{
 const link=read('ui/components/LinkPreview.qml');
 const server=read('services/media-gateway/gateway.mjs');
 assert.match(link,/function requestBrowserLinkPreview\(\)/);
 const adapter=read('ui/mobile/browser_link_metadata_controller.cpp');
 assert.match(link,/browserLinkMetadataController\.requestPreview\(target, "link"\)/);
 assert.match(adapter,/\/v1\/browser\/link-preview/);
 assert.doesNotMatch(link,/XMLHttpRequest|JSON\.parse/);
 assert.match(link,/root\.requestBrowserLinkPreview\(\)/);
 assert.match(link,/root\.previewSource\.length > 0/);
 assert.match(link,/visible: root\.summaryText\.length > 0/);
 assert.match(link,/pendingBrowserLinkPreview/);
 assert.match(link,/pendingBrowserMiniAppPreview/);
 assert.match(server,/requestUrl\.pathname === '\/v1\/browser\/link-preview'/);
 assert.match(server,/await publicLinkPreview\(rawUrl\)/);
});
