import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const base=new URL('../../',import.meta.url);
const read=path=>readFileSync(new URL(path,base),'utf8');

test('compact mobile Mini App explorer is a two-column grid of short cards',()=>{
 const apps=read('ui/components/AppsSurface.qml');
 assert.match(apps,/columns: 3[\s\S]*?Layout\.preferredHeight: root\.compact \? 54 : 64/);
 assert.match(apps,/targetColumns: root\.compact \? \(width < 340 \? 1 : 2\)/);
 assert.match(apps,/cellHeight: root\.compact \? 136/);
 assert.match(apps,/visible: !root\.compact[\s\S]{0,120}Layout\.minimumHeight: 40/);
 assert.match(apps,/iconOnly: root\.compact/);
 assert.match(apps,/visible: !root\.compact; Layout\.fillHeight: true/);
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
 assert.match(link,/browser\/link-preview\?url=/);
 assert.match(link,/root\.requestBrowserLinkPreview\(\)/);
 assert.match(link,/root\.previewSource\.length > 0/);
 assert.match(link,/visible: root\.summaryText\.length > 0/);
 assert.match(link,/pendingBrowserLinkPreview/);
 assert.match(link,/pendingBrowserMiniAppPreview/);
 assert.match(server,/requestUrl\.pathname === '\/v1\/browser\/link-preview'/);
 assert.match(server,/await publicLinkPreview\(rawUrl\)/);
});
