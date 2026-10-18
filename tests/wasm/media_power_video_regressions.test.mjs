import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('motion preferences persist locally and GIFs are not paused by scroll',()=>{
 const theme=read('ui/theme/Theme.qml');
 const img=read('ui/components/SuperellipseImage.qml');
 const mini=read('ui/components/LinkPreview.qml');
 const settings=read('ui/components/LocaleSettingsPopup.qml');
 const setup=read('ui/components/InitialSetup.qml');
 assert.match(theme,/property Settings energyPreferences: Settings \{/);
 assert.match(theme,/property bool animateAvatars: true/);
 assert.match(theme,/function setPowerSave\(enabled\)/);
 assert.match(settings,/checked: Theme\.animateAvatars/);
 assert.match(settings,/checked: Theme\.powerSave/);
 assert.match(setup,/onToggled: Theme\.setPowerSave\(checked\)/);
 assert.doesNotMatch(img,/playing: visible && !root\.scrollMoving/);
 assert.match(img,/playing: visible && root\.visible && !Theme\.powerSave/);
 assert.match(img,/root\.mediaCategory !== "avatars" \|\| Theme\.animateAvatars/);
 assert.doesNotMatch(mini,/playing: visible && !root\.scrollMoving/);
});
test('viewer chrome anchors to stable viewport not aspect-fitted image bounds',()=>{
 const gallery=read('ui/components/MediaGallery.qml');
 assert.match(gallery,/x: Theme\.space3[\s\S]{0,240}y: root\.width < 700/);
 assert.doesNotMatch(gallery,/x: Math\.max\(Theme\.space3, parent\.chromeRect/);
});
test('HLS browser player uses responsive stage and real poster for Stream manifests',()=>{
 const video=read('ui/wasm/browser-hls-pre.js');
 const feed=read('ui/components/VideoPreview.qml');
 assert.match(video,/@media \(orientation:landscape\)/);
 assert.match(video,/className = "gc-hls-stage"/);
 assert.match(video,/className = "gc-hls-video"/);
 assert.match(video,/video\.poster = url\.origin/);
 assert.match(feed,/root\.embed\.posterUrl/);
});
test('link image allowlist leases are URL exact, not a public generic image proxy',()=>{
 const gateway=read('services/media-gateway/gateway.mjs');
 const preview=read('services/media-gateway/browser_link_preview.mjs');
 assert.match(gateway,/verifiedLinkImages\.get\(requested\)/);
 assert.match(gateway,/delete body\.verifiedImageUrl/);
 assert.match(preview,/media\.length > 0 && safePreviewPage\(candidate\.href\)/);
});
