import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('mac avatars tolerate temporarily unavailable metadata resolver',()=>{
 const avatar=read('ui/components/Avatar.qml');
 assert.match(avatar,/typeof linkPreviewResolver !== "undefined"/);
 assert.match(avatar,/linkPreviewResolver !== null/);
});
test('GIF URL decoding no longer invokes deprecated Qt.atob on every delegate',()=>{
 const media=read('ui/components/SuperellipseImage.qml');
 assert.doesNotMatch(media,/\bQt\.atob\s*\(/);
 assert.match(media,/function decodeBase64Url\(input\)/);
});
test('the HLS browser overlay is wired to the actual QML mediaPlayback object',()=>{
 const qml=read('ui/components/VideoPreview.qml');
 const cpp=read('ui/mobile/main.cpp');
 const bridge=read('src/presentation/qt/media_playback_coordinator.cpp');
 const pre=read('ui/wasm/browser-hls-pre.js');
 const cmake=read('ui/CMakeLists.txt');
 assert.match(cpp,/setContextProperty\(QStringLiteral\("mediaPlayback"\)/);
 assert.match(qml,/mediaPlayback\.openBrowserHls\(candidate\)/);
 assert.match(bridge,/MediaPlaybackCoordinator::openBrowserHls/);
 assert.match(pre,/video\.canPlayType\("application\/vnd\.apple\.mpegurl"\)/);
 assert.match(pre,/Hls\.isSupported\(\)/);
 assert.match(pre,/instance\.on\(Hls\.Events\.MANIFEST_PARSED, requestPlay\)/);
 assert.match(cmake,/browser-hls-pre\.js/);
});
test('HLS native backend does not reject Farcaster manifest redirects in HEAD preflight',()=>{
 const main=read('ui/mobile/main.cpp');
 assert.match(main,/QUrl\{source\}\.path\(\)\.endsWith\(QLatin1StringView\{"\.m3u8"\}/);
});
test('Telegram-like small link poster and focus rings on both home composers',()=>{
 const link=read('ui/components/LinkPreview.qml');
 const home=read('ui/components/HomeDashboard.qml');
 assert.match(link,/layoutDirection: Qt\.RightToLeft/);
 assert.match(link,/root\.compact \? 66 : 86/);
 assert.match(link,/visible: root\.authorText\.length > 0 \|\| root\.dateText\.length > 0/);
 assert.match(home,/desktopDraftEditor\.activeFocus \? Theme\.accent/);
 assert.match(home,/mobileDraftEditor\.activeFocus \? Theme\.accent/);
});
test('single photo and WASM galleries use alpha mask rather than inverse edge fills',()=>{
 const gallery=read('ui/components/GalleryLayout.qml');
 assert.match(gallery,/useLayeredMask: Qt\.platform\.os === "wasm"/);
 assert.match(gallery,/Qt\.platform\.os === "osx" && root\.visibleCount === 1/);
});
