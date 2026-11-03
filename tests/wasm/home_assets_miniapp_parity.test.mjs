import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source=relative=>readFileSync(new URL('../../'+relative,import.meta.url),'utf8');

test('Home desktop uses one aligned measured rail; mobile keeps matching side gutters',()=>{
 const home=source('ui/components/HomeDashboard.qml');
 assert.match(home,/readonly property real homeContentMaxWidth: Theme.scaleUi\(760\)/);
 assert.match(home,/objectName: "homeAlignedContentRail"/);
 assert.match(home,/width: Math.max\(1, Math.min\(root.homeContentMaxWidth,/);
 assert.match(home,/Layout.leftMargin: root.compact \? Theme.space3 : 0/);
 assert.match(home,/Layout.rightMargin: root.compact \? Theme.space3 : 0/);
 assert.match(home,/Layout.maximumWidth: root.homeContentMaxWidth/);
 assert.match(home,/id: mobileComposerSurface[\s\S]*?Layout.leftMargin: Theme.space3[\s\S]*?Layout.rightMargin: Theme.space3/);
});
test('My Assets on WASM has public CORS Base RPC and an authenticated account fallback',()=>{
 const native=source('ui/mobile/live_insights_controller.cpp');
 const rail=source('ui/components/ContextRail.qml');
 const controller=source('ui/mobile/openproof_connections_controller.cpp');
 const cloud=source('services/account-sync/cloud_state.mjs');
 assert.match(native,/#elif defined\(__EMSCRIPTEN__\)[\s\S]*?https:\/\/mainnet.base.org/);
 assert.match(native,/QStringLiteral\("eth_chainId"\)/);
 assert.match(native,/QStringLiteral\("0x313ce567"\)/);
 assert.match(native,/QStringLiteral\("0x70a08231"\)/);
 assert.match(native,/walletAddresses!=allWallets/);
 assert.match(rail,/Qt.platform.os === "wasm"[\s\S]*?liveInsights.genyHolding.available/);
 assert.match(rail,/openProofConnections.fetchGenyAssets\(true\)/);
 assert.match(controller,/setPath\(QStringLiteral\("\/api\/v1\/cloud\/geny\/assets"\)\)/);
 assert.match(cloud,/if\(entity==='geny\/assets'\)/);
 assert.match(cloud,/resolveGenyAssets\(owner.fid\)/);
});
test('Mini App web presents in configured mode with independent fullscreen and no forced external tabs',()=>{
 const web=source('ui/wasm/miniapp-host-pre.js');
 const host=source('ui/mobile/miniapp_host_controller.cpp');
 const android=source('ui/components/AndroidMiniAppWebView.qml');
 const macos=source('ui/mobile/miniapp_browser_macos.mm');
 for(const mode of ['window','bottom','left','right'])assert.ok(web.includes('"'+mode+'"'));
 assert.match(web,/function applyPresentation\(\)/);
 assert.match(web,/function setMode\(mode\)/);
 assert.match(web,/async function toggleFullscreen\(\)/);
 assert.match(web,/requestFullscreen/);
 assert.doesNotMatch(web,/window\.open\(/);
 assert.match(host,/host\.setMode\(UTF8ToString\(\$0,\$1\)\)/);
 assert.match(host,/UTF8ToString\(\$6,\$7\)/);
 assert.match(android,/property bool miniAppFullscreen: false/);
 assert.match(android,/onClicked: root.miniAppFullscreen = true/);
 assert.match(android,/iconGlyph: Theme.iconCompress/);
 assert.match(macos,/toggleFullscreen:/);
 assert.match(macos,/m_drawer->m_fullscreen/);
 assert.match(macos,/NSWindowCollectionBehaviorFullScreenPrimary/);
});
