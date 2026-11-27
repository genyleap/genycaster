import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
const read=p=>readFileSync(new URL("../../"+p,import.meta.url),"utf8");
test("WASM compressed media is not duplicated in the MEMFS disk cache",()=>{
 const storage=read("ui/app/data/storage.cpp");
 const provider=read("ui/app/media/remote_image.cpp");
 assert.match(storage,/#if defined\(Q_OS_WASM\)[\s\S]*?policy\.diskCacheEnabled = false;/);
 assert.match(storage,/setDiskCacheEnabled\(const bool enabled\)[\s\S]*?#if defined\(Q_OS_WASM\)\s*if \(enabled\) return;/);
 assert.match(provider,/void writeDiskCachedBytes\([\s\S]*?#if defined\(Q_OS_WASM\)[\s\S]*?return;/);
 assert.match(provider,/QByteArray readDiskCachedBytes\([\s\S]*?#if defined\(Q_OS_WASM\)[\s\S]*?return \{\};/);
 assert.match(provider,/imageCacheCapacityKiB = 24 \* 1024/);
 assert.match(provider,/imageDecodeQueueBytes = 24U \* 1024U \* 1024U/);
 assert.match(provider,/limits\.maximumActiveRequests = 4U/);
 assert.match(provider,/!acceptableImageDimensions\(originalSize\)[\s\S]*?return \{\};/);
});
test("shared raster lifetime follows viewport, releases pixels without losing intrinsic geometry",()=>{
 const qml=read("ui/components/visual/SuperellipseImage.qml");
 assert.match(qml,/browserRemoteRaster: Qt\.platform\.os === "wasm" && root\.remoteRaster/);
 assert.match(qml,/viewportMargin: Qt\.platform\.os === "wasm" \? 320 : 480/);
 assert.match(qml,/retentionMargin: Qt\.platform\.os === "wasm" \? 640 : 960/);
 assert.match(qml,/requireOwnerVisible: false/);
 assert.match(qml,/id: rasterReleaseTimer[\s\S]*?interval: root\.rasterPageVisible \? 1100 : 250[\s\S]*?root\.activeSource = ""/);
 assert.match(qml,/if \(!root\.componentReady \|\| !root\.rasterEligible\)/);
 assert.match(qml,/rememberedIntrinsicWidth : imageSource\.implicitWidth/);
 assert.match(qml,/onSourceChanged: \{\s*root\.rememberedIntrinsicWidth = 0/);
 assert.match(qml,/cache: root\.remoteSource \|\| root\.browserRemoteRaster \? false/);
});
test("Mini Apps has a real bounded viewport and mirrors the outer scroll through model resets",()=>{
 const qml=read("ui/components/apps/AppsSurface.qml");
 assert.match(qml,/Math\.ceil\(root\.visibleApps\.length \/ appGrid\.targetColumns\)/);
 assert.match(qml,/height: root\.compact \? Math\.min\(parent\.height, appsScroller\.height\)/);
 assert.match(qml,/onModelChanged: Qt\.callLater\(appGrid\.syncCompactViewport\)/);
 assert.match(qml,/onCompactViewportOffsetChanged: syncCompactViewport\(\)/);
 assert.match(qml,/source: root\.compact \? String\(cardCell\.modelData\.iconSource/);
 assert.match(qml,/source: !root\.compact \? String\(cardCell\.modelData\.iconSource/);
 assert.doesNotMatch(qml,/Math\.max\(1, appGrid\.contentHeight\)/);
});
test("browser gallery neighbors, stickers, and embedded animations are independently bounded",()=>{
 const gallery=read("ui/components/media/MediaGallery.qml");
 assert.match(gallery,/mediaPage\.index === gallery\.currentIndex \? 2048 : 768/);
 const stickers=read("ui/components/composer/StickerPicker.qml");
 assert.match(stickers,/root\.visible && root\.page === "library"/);
 assert.match(stickers,/root\.visible && root\.page === "detail"/);
 assert.match(stickers,/sourceSize: Qt\.platform\.os === "wasm" \? Qt\.size\(256, 256\)/);
 const link=read("ui/components/media/LinkPreview.qml");
 assert.match(link,/active: root\.animatedPosterUrl\.length > 0 && !Theme\.powerSave\s*&& posterAnimationScope\.eligible/);
});
