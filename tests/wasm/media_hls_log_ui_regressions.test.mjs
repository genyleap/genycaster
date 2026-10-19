import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';
const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path,root),'utf8');

test('log regressions: Avatar tolerates null resolver, GIF decoding has no obsolete Qt.atob',()=>{
 const avatar=read('ui/components/Avatar.qml');
 const image=read('ui/components/SuperellipseImage.qml');
 assert.match(avatar,/typeof linkPreviewResolver !== "undefined"/);
 assert.match(avatar,/linkPreviewResolver !== null/);
 assert.doesNotMatch(image,/mediaUrl\s*=\s*Qt\.atob\(/);
 assert.match(image,/function decodeBase64Url\(input\)/);
 const match=/function decodeBase64Url\(input\) \{([\s\S]*?)\n    \}/.exec(image);
 assert.ok(match);
 const decoder=Function('input','const f=function(input) {'+match[1]+'};return f(input)');
 for(const url of ['https://atgif.com/funny.gif',
   'https://media-farcaster.genyleap.com/m/v1/logo.gif?x=1',
   'https://site.example/emoji-💠.gif']) {
   assert.equal(decoder(Buffer.from(url,'utf8').toString('base64url')),url);
 }
 assert.equal(decoder('@@@'),'');
});
test('browser native HLS modal and local hls.js work without third-party runtime CDN dependency',()=>{
 const qml=read('ui/components/VideoPreview.qml');
 const player=read('src/presentation/qt/media_playback_coordinator.cpp');
 const header=read('src/presentation/qt/media_playback_coordinator.hpp');
 const js=read('ui/wasm/browser-hls-pre.js');
 const cmake=read('ui/CMakeLists.txt');
 const packageScript=read('tools/package_wasm_assets.py');
 assert.match(qml,/mediaPlayback\.openBrowserHls\(candidate\)/);
 assert.match(header,/Q_INVOKABLE bool openBrowserHls\(const QString& source\) const/);
 assert.match(player,/__genycasterBrowserHls\?\.open/);
 assert.match(js,/video\.canPlayType\("application\/vnd\.apple\.mpegurl"\)/);
 assert.match(js,/new Hls\(/);
 assert.match(js,/vendor\/hls-1\.6\.15\.min\.js/);
 assert.match(cmake,/browser-hls-pre\.js/);
 assert.match(packageScript,/vendor\/hls-1\.6\.15\.min\.js/);
 const main=read('ui/mobile/main.cpp');
 assert.match(main,/path\(\)\.endsWith\(QLatin1StringView\{"\.m3u8"\}, Qt::CaseInsensitive\)/);
 assert.match(main,/dispatch\(true\)/);
});
test('home composer focus is visible and link artwork leads the responsive editorial card',()=>{
 const home=read('ui/components/HomeDashboard.qml');
 const link=read('ui/components/LinkPreview.qml');
 assert.match(home,/desktopComposerSurface[\s\S]*borderColor: desktopDraftEditor\.activeFocus \? Theme\.accent/);
 assert.match(home,/mobileComposerSurface[\s\S]*borderColor: mobileDraftEditor\.activeFocus \? Theme\.accent/);
 assert.match(link,/localizationController\.isRightToLeftText\(root\.castCaption\)/);
 assert.match(link,/Layout\.preferredHeight: shouldReserve \? \(root\.compact \? 116 : 156\) : 0/);
});
test('gallery avoids subpixel inverse covers on Metal and WebGL',()=>{
 const gallery=read('ui/components/GalleryLayout.qml');
 const tile=read('ui/components/GalleryTile.qml');
 assert.match(gallery,/useLayeredMask: Qt\.platform\.os === "wasm"/);
 assert.match(gallery,/Qt\.platform\.os === "osx" && root\.visibleCount === 1/);
 assert.match(tile,/clip: true/);
});
