import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const get=p=>readFileSync(p,'utf8');
const uploads=get('ui/components/composer/MediaUploadPanel.qml');
const android=get('ui/app/media/android_document_picker.hpp');
const composer=get('ui/components/composer/ComposerDrawer.qml');
const picker=get('ui/components/composer/StickerPicker.qml');
const tile=get('ui/components/composer/StickerDraftTile.qml');
const feedTile=get('ui/components/media/GalleryTile.qml');
const viewer=get('ui/components/media/MediaGallery.qml');
const bridge=get('ui/app/runtime/bindings.cpp');
const native=get('src/transport/qt_network/media_upload.cpp');
const manifest=get('services/media-gateway/sticker_preview_index.json');
const catalogue=get('ui/components/composer/StickerCatalog.js');
const sandbox=vm.createContext({});
vm.runInContext(catalogue.replace(/^\.pragma library\s*/,''),sandbox);

test('Android image-picking is routed to Android Activity thread and never uses Qt native FileDialog',()=>{
    assert.match(composer,/function openMediaPicker\(\) \{[\s\S]*?Qt\.platform\.os === "android"/);
    assert.match(bridge,/static_cast<int>\(controller\.maxImageBytes\(\)\)/);
    assert.match(android,/QtAndroidPrivate::startActivity\(intent, RequestCode, receiver\.data\(\)\)/);
    assert.match(android,/QNativeInterface::QAndroidApplication::runOnAndroidMainThread\(/);
    assert.match(android,/const QPointer<AndroidComposerMediaPicker> receiver\{this\}/);
    assert.match(android,/jni\.checkAndClearExceptions\(\)/);
    assert.match(android,/activityJni\.checkAndClearExceptions\(\)/);
    assert.match(android,/QMetaObject::invokeMethod\(receiver\.data\(\)/);
    assert.match(android,/android\.intent\.action\.OPEN_DOCUMENT/);
    assert.match(android,/content:\/\//);
    assert.doesNotMatch(android,/READ_MEDIA_IMAGES|READ_EXTERNAL_STORAGE/);
});

test('composer uploading card moves status/progress below thumbnail at narrow widths',()=>{
    const begin=uploads.indexOf('objectName: "responsiveComposerUploadStatusCard"');
    const end=uploads.indexOf('visible: root.hasAttachedMedia',begin);
    const card=uploads.slice(begin,end);
    assert.ok(begin>=0 && end>begin);
    assert.match(card,/id: uploadContent[\s\S]*?spacing: Theme\.space2/);
    assert.match(card,/objectName: "composerUploadingFileName"/);
    assert.match(card,/elide: Text\.ElideMiddle/);
    assert.match(card,/objectName: "composerUploadFullWidthStatus"/);
    assert.match(card,/wrapMode: Text\.Wrap/);
    assert.match(card,/objectName: "smartMediaPipelineProgressSteps"/);
    assert.match(card,/columns: root\.width < 480 \? 1 : 3/);
    assert.match(card,/mediaUpload\.phase === "uploading"/);
    assert.match(card,/Math\.max\(0\.015, mediaUpload\.progress\)/);
    assert.doesNotMatch(card,/Layout\.preferredWidth: 126/);
    assert.match(card,/mediaUpload\.cancel\(\)/);
    assert.match(card,/mediaUpload\.retry\(\)/);
});

test('all 276 sticker preview IDs map only to pinned official sources; publishing stays original',()=>{
    const index=JSON.parse(manifest);
    const values=Object.entries(index.items);
    assert.equal(values.length,276);
    for(const [id,item] of values){
        const canonical=sandbox.itemForId(id);
        assert.ok(canonical,id);
        assert.equal(item.source,canonical.pack==='njal'?canonical.url:canonical.thumbnailUrl);
        assert.ok(item.source.startsWith(canonical.pack==='njal'
            ? 'https://red-geographical-clownfish-714.mypinata.cloud/ipfs/'
            : 'https://mssgs-stickers.com/thumbs/'));
    }
    const base='https://storage-farcaster.genyleap.com/v1/browser/sticker-previews?id=';
    for(const [id] of values.slice(0,4)){
        const preview=sandbox.displayPreviewUrl(id,base,true);
        assert.equal(preview,base+encodeURIComponent(id));
        assert.equal(sandbox.displayPreviewUrl(id,base,false),sandbox.previewUrl(id));
        assert.equal(sandbox.embedUrl(id),sandbox.itemForId(id).url);
        assert.equal(sandbox.previewForPublishedUrl(sandbox.embedUrl(id),base,true),preview);
    }
    assert.equal(sandbox.displayPreviewUrl('../evil',base,true),'');
    assert.equal(sandbox.displayPreviewUrl('02-big-smile','http://bad.test/',true),'');
    assert.equal(sandbox.previewForPublishedUrl('https://unknown.test/other.png',base,true),
                 'https://unknown.test/other.png');
});

test('sticker library does not falsely claim zero packs while the cloud is unavailable',()=>{
    assert.match(picker,/visible: root\.libraryReady && root\.installedIds\.length === 0/);
    assert.match(picker,/objectName: "stickerLibraryUnavailableState"/);
    assert.match(picker,/visible: !root\.libraryReady/);
    assert.match(picker,/onAuthorizedChanged: \{/);
    assert.doesNotMatch(picker,/function onStateChanged\(\)[\s\S]*?fetchStickerPacks\(\)/);
    assert.match(picker,/openProofConnections\.retryAccountSession\(\)/);
    assert.match(picker,/openProofConnections\.fetchStickerPacks\(\)/);
    assert.match(picker,/root\.stickerDisplayUrl\(stickerTile\.modelData\.id\)/);
    assert.match(tile,/StickerCatalog\.previewForPublishedUrl\(root\.imageUrl/);
    assert.match(feedTile,/StickerCatalog\.previewForPublishedUrl\(canonical/);
    assert.match(feedTile,/Qt\.platform\.os === "wasm" \|\| Qt\.platform\.os === "android"/);
    assert.match(viewer,/function compatibilityStickerUrl\(item\)/);
    assert.match(viewer,/root\.compatibilityStickerUrl\(mediaPage\.modelData\)/);
    assert.match(viewer,/dataStorage\.remoteImageSource\(root\.sourceFor/);
    assert.match(picker,/root\.verifiedPro/);
});

test('native upload completion allows bounded network/storage finalization while preserving write authorization',()=>{
    assert.match(native,/m_config\.requestTimeout/);
    assert.match(native,/std::max\(m_config\.requestTimeout, std::chrono::milliseconds\{90'000\}\)/);
    assert.match(native,/auto completeHeaders = authorizationHeaders\(/);
    assert.match(native,/m_config\.uploadTimeout/);
    assert.match(bridge,/genycaster\.media\.upload_rate_limited/);
    assert.match(bridge,/genycaster\.media\.upload_timeout/);
    assert.match(bridge,/genycaster\.media\.upload_signer_network_unavailable/);
    assert.match(bridge,/mediaUploadFailureText\(state\.error->code,state\.error->message\)/);
});
