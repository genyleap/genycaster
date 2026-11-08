import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime/bindings.cpp', import.meta.url), 'utf8');
}
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('mac avatars tolerate temporarily unavailable metadata resolver',()=>{
 const avatar=read('ui/components/content/Avatar.qml');
 assert.match(avatar,/typeof linkPreviewResolver !== "undefined"/);
 assert.match(avatar,/linkPreviewResolver !== null/);
});
test('GIF URL decoding no longer invokes deprecated Qt.atob on every delegate',()=>{
 const media=read('ui/components/visual/SuperellipseImage.qml');
 assert.doesNotMatch(media,/\bQt\.atob\s*\(/);
 assert.match(media,/function decodeBase64Url\(input\)/);
});
test('the HLS browser overlay is wired to the actual QML mediaPlayback object',()=>{
 const qml=read('ui/components/media/VideoPreview.qml');
 const cpp=readGenycasterMain();
 const bridge=read('src/presentation/qt/media_playback_coordinator.cpp');
 const pre=read('ui/wasm/browser-hls-pre.js');
 const cmake=read('ui/CMakeLists.txt');
 assert.match(cpp,/\{"mediaPlayback", &mediaPlayback\}/);
 assert.match(qml,/mediaPlayback\.openBrowserHls\(candidate\)/);
 assert.match(bridge,/MediaPlaybackCoordinator::openBrowserHls/);
 assert.match(pre,/video\.canPlayType\("application\/vnd\.apple\.mpegurl"\)/);
 assert.match(pre,/Hls\.isSupported\(\)/);
 assert.match(pre,/instance\.on\(Hls\.Events\.MANIFEST_PARSED, requestPlay\)/);
 assert.match(cmake,/browser-hls-pre\.js/);
});
test('idle video players defer audio-device probing until playback', () => {
 const player=read('ui/components/media/GenyVideoPlayer.qml');
 assert.match(player, /id: audioOutput[\s\S]*?active: root\.audioOutputEnabled\s*&& \(typeof nativeAudioOutputEnabled === "undefined" \|\| nativeAudioOutputEnabled\)\s*&& \(activation\.activated \|\| \(root\.eager && root\.hasSource\)\)/);
 assert.match(player, /audioOutput: audioOutput\.item/);
 const audio=read('ui/components/media/AudioPreview.qml');
 assert.match(audio, /active: root\.activated && root\.localPlaybackAvailable/);
});

test('HLS native backend does not reject Farcaster manifest redirects in HEAD preflight',()=>{
 const main=readGenycasterMain();
 assert.match(main,/QUrl\{source\}\.path\(\)\.endsWith\(QLatin1StringView\{"\.m3u8"\}/);
});
test('editorial link artwork and focus rings on both home composers',()=>{
 const link=read('ui/components/media/LinkPreview.qml');
 const home=read('ui/components/home/HomeDashboard.qml');
 assert.match(link,/localizationController\.isRightToLeftText\(root\.castCaption\)/);
 assert.match(link,/Layout\.fillWidth: root\.compact && shouldReserve[\s\S]*?Math\.min\(root\.compact \? 300 : Theme\.scaleUi\(192\)/);
 assert.match(link,/visible: root\.authorText\.length > 0 \|\| root\.dateText\.length > 0/);
 assert.match(home,/desktopDraftEditor\.activeFocus \? Theme\.accent/);
 assert.match(home,/objectName: "homeComposerLauncher"/);
});
test('single photo and WASM galleries use alpha mask rather than inverse edge fills',()=>{
 const gallery=read('ui/components/media/GalleryLayout.qml');
 assert.match(gallery,/useLayeredMask: Qt\.platform\.os === "wasm"/);
 assert.match(gallery,/Qt\.platform\.os === "osx" && root\.visibleCount === 1/);
});
