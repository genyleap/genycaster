import test from 'node:test';
import assert from 'node:assert/strict';
import { webcrypto } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { uploadBrowserImage } from '../../ui/wasm/browser_media_upload.mjs';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime/bindings.cpp', import.meta.url), 'utf8');
}

const fid='123456';
const key='ab'.repeat(32);
const assetUrl='https://media-farcaster.genyleap.com/m/v1/2026/10/photo.png';
function photo(size=128){
    const bytes=new Uint8Array(size);
    return {
        name:'photo.png',type:'image/png',size,
        slice:(a,b)=>new Blob([bytes.subarray(a,b)]),
    };
}
test('browser upload signs canonical media proof, transfers parts and inserts only asset metadata',async()=>{
    const challenges=[],phases=[],requests=[];
    const vault={
        load:async()=>({fid,publicKeyHex:key}),
        signMediaChallenge:async(_,bytes)=>{
            const text=new TextDecoder().decode(bytes);
            challenges.push(text);
            assert.equal(text.split('\n').length,8);
            assert.match(text,/^genycaster-media-upload-v1\n/);
            return {publicKeyHex:key,signature:'de'.repeat(64)};
        },
    };
    const fetchImpl=async(url,opts)=>{
        requests.push({url,opts});
        assert.equal(opts.headers['x-genycaster-fid'],fid);
        assert.equal(opts.credentials,'omit');
        if(url.endsWith('/v1/uploads'))
            return new Response(JSON.stringify({
                uploadId:'ticket_123',mode:'gateway-multipart',
                partCount:1,partSizeBytes:5*1024*1024,assetUrl,
                optimizationMode:'auto',
            }),{status:201});
        if(url.endsWith('/parts/1'))
            return new Response(JSON.stringify({partNumber:1,etag:'"part-1"'}));
        if(url.endsWith('/v1/uploads/complete'))
            return new Response(JSON.stringify({
                assetUrl,fileName:'photo.png',contentType:'image/png',
                sizeBytes:128,storedSizeBytes:96,optimized:true,
                managementToken:'opaque-token',
            }));
        throw new Error('unexpected upload request '+url);
    };
    const result=await uploadBrowserImage({
        file:photo(),fid,vault,crypto:webcrypto,fetchImpl,
        onProgress:(...args)=>phases.push(args),
    });
    assert.equal(result.assetUrl,assetUrl);
    assert.equal(result.sizeBytes,96,'actual QStorage object size must be reported');
    assert.equal(result.storedSizeBytes,96);
    const create=JSON.parse(new TextDecoder().decode(requests[0].opts.body));
    assert.equal(create.optimizationMode,'auto');
    assert.equal(requests.length,3);
    assert.equal(challenges.length,3);
    assert.deepEqual(phases.map(x=>x[0]),
        ['authorizing','uploading','uploading','optimizing','finalizing']);
    assert.equal(phases.find(x=>x[0]==='uploading')[2],128);
    assert.equal(result.sourceSizeBytes,128);
});
test('legacy gateway finalizes without falsely claiming compression',async()=>{
    const phases=[];
    const vault={
        load:async()=>({fid,publicKeyHex:key}),
        signMediaChallenge:async()=>({publicKeyHex:key,signature:'de'.repeat(64)}),
    };
    const fetchImpl=async(url)=>{
        if(url.endsWith('/v1/uploads'))return new Response(JSON.stringify({
            uploadId:'legacy_ticket',mode:'gateway-multipart',
            partCount:1,partSizeBytes:5*1024*1024,assetUrl,
        }),{status:201});
        if(url.endsWith('/parts/1'))return new Response(JSON.stringify({
            partNumber:1,etag:'"legacy-part"',
        }));
        if(url.endsWith('/v1/uploads/complete'))return new Response(JSON.stringify({
            assetUrl,contentType:'image/png',fileName:'photo.png',
            sizeBytes:128,managementToken:'legacy-opaque-token',
        }));
        throw new Error('unexpected request '+url);
    };
    const result=await uploadBrowserImage({
        file:photo(),fid,vault,crypto:webcrypto,fetchImpl,
        onProgress:(phase)=>phases.push(phase),
    });
    assert.equal(result.sizeBytes,128);
    assert.equal(result.sourceSizeBytes,128);
    assert.deepEqual(phases,
        ['authorizing','uploading','uploading','finalizing']);
    assert.ok(!phases.includes('optimizing'));
});

test('image upload stages and actual cloud sizes are exposed to Composer, without fake compression progress',()=>{
    const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');
    const panel=read('ui/components/composer/MediaUploadPanel.qml');
    const preview=read('ui/components/media/MediaPreview.qml');
    const controller=read('src/presentation/qt/media_upload_controller.cpp');
    const header=read('src/presentation/qt/media_upload_controller.hpp');
    const native=read('src/transport/qt_network/media_upload.cpp');
    const wasm=read('ui/wasm/browser_media_upload.mjs');
    const bridge=read('ui/app/runtime/bindings.cpp');
    const composer=read('ui/components/composer/ComposerDrawer.qml');
    assert.match(panel,/objectName: "smartMediaPipelineProgressSteps"/);
    assert.match(panel,/mediaUpload\.optimizationSeen/);
    assert.match(panel,/qsTrId\("genycaster\.media\.optimizing"\)/);
    assert.match(panel,/qsTrId\("genycaster\.media\.finalizing"\)/);
    assert.match(panel,/root\.processingStep === 0/);
    assert.match(panel,/root\.savingsText\(modelData\.originalSizeBytes,\s*modelData\.sizeBytes\)/);
    assert.match(panel,/objectName: "composerFullResolutionMediaInspector"/);
    assert.match(panel,/fullQualityImage: true/);
    assert.match(panel,/objectName: "composerOriginalImageStrip"/);
    assert.match(preview,/requestedSourceWidth: root\.imageDecodeEdge/);
    assert.match(header,/Q_PROPERTY\(qulonglong sourceBytes READ sourceBytes NOTIFY stateChanged\)/);
    assert.match(controller,/m_sourceBytes = m_totalBytes/);
    assert.match(controller,/m_optimizationSeen = true/);
    assert.match(native,/serverOptimizesImage/);
    assert.match(native,/UploadStage::Optimizing/);
    assert.match(native,/UploadStage::Finalizing/);
    assert.match(wasm,/ticket\.optimizationMode==='auto'/);
    assert.match(bridge,/UploadPhase::Optimizing/);
    assert.match(bridge,/UploadPhase::Finalizing/);
    assert.match(composer,/originalSizeBytes: Number\(mediaUpload\.sourceBytes/);
});

test('composer renders only one source-quality attachment, never the 96px selection JPEG',()=>{
    const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');
    const composer=read('ui/components/composer/ComposerDrawer.qml');
    const panel=read('ui/components/composer/MediaUploadPanel.qml');
    const editor=read('ui/components/composer/RichTextEditor.qml');
    const preview=read('ui/components/media/MediaPreview.qml');
    assert.match(composer,/localPreviewUrl: canonicalUrl/);
    assert.doesNotMatch(composer,/localPreviewUrl:.*mediaUpload\.localPreviewUrl/);
    assert.match(panel,/previewUrl: url,/);
    assert.match(panel,/objectName: "composerOriginalImageStrip"/);
    assert.match(panel,/fullQualityImage: String\(modelData\.mediaType/);
    assert.match(panel,/minimalChrome: true/);
    assert.match(panel,/fillColor: "transparent"/);
    assert.match(panel,/borderWidth: 0/);
    assert.doesNotMatch(panel,/objectName: "visualEditorMediaPlaceholder"/);
    assert.match(editor,/if \(kind === "media" && !StickerCatalog\.isStickerUrl/);
    assert.match(editor,/return null/);
    assert.match(preview,/property bool fullQualityImage: false/);
    assert.match(preview,/source: visible \? root\.source : ""/);
    assert.match(preview,/Full-fidelity image for authored attachments/);
    assert.match(composer,/root\.attachedMediaCount > 0/);
    // In-flight tiny thumbnails can still be used *only* for the progress
    // indicator, but never as the persisted asset's authoring preview.
    const bridge=read('ui/wasm/protocol-signer-pre.js');
    assert.match(bridge,/Math\.min\(1,96\/Math\.max\(bitmap\.width,bitmap\.height\)\)/);
});

test('2 MiB basic cap rejects before any request; verified Pro client may select up to 5 MiB',async()=>{
    const vault={load:async()=>({fid,publicKeyHex:key}),
        signMediaChallenge:async()=>({publicKeyHex:key,signature:'ab'.repeat(64)})};
    let attempts=0;
    const file=photo(2*1024*1024+1);
    await assert.rejects(uploadBrowserImage({file,fid,vault,crypto:webcrypto,
        fetchImpl:async()=>{attempts++;throw Error('must not request');}}),
        /2 MiB upload limit/);
    assert.equal(attempts,0);
    await assert.rejects(uploadBrowserImage({file:photo(5*1024*1024+1),
        fid,vault,crypto:webcrypto,maxImageBytes:5*1024*1024,
        fetchImpl:async()=>{attempts++;throw Error('must not request');}}),
        /5 MiB upload limit/);
    assert.equal(attempts,0);
});
test('cancel aborts the active fetch and separately releases the signed server ticket',async()=>{
    const controller=new AbortController();
    const requests=[];
    const vault={load:async()=>({fid,publicKeyHex:key}),
        signMediaChallenge:async()=>({publicKeyHex:key,signature:'ab'.repeat(64)})};
    const fetchImpl=async(url,opts)=>{
        requests.push({url,opts});
        if(url.endsWith('/v1/uploads')&&opts.method==='POST')
            return Response.json({uploadId:'cancel_ticket_123',
                mode:'gateway-multipart',assetUrl,partCount:1,
                partSizeBytes:5*1024*1024},{status:201});
        if(url.includes('/parts/1')) {
            controller.abort();
            throw new DOMException('Upload cancelled','AbortError');
        }
        if(opts.method==='DELETE') {
            assert.equal(opts.signal.aborted,false);
            assert.deepEqual(JSON.parse(new TextDecoder().decode(opts.body)),
                {uploadId:'cancel_ticket_123'});
            return Response.json({deleted:true});
        }
        throw Error('unexpected request');
    };
    await assert.rejects(uploadBrowserImage({file:photo(),fid,vault,
        signal:controller.signal,crypto:webcrypto,fetchImpl}),/cancel/i);
    assert.equal(requests.filter(r=>r.opts.method==='DELETE').length,1);
});

test('unsupported formats and missing signer do not reach media gateway',async()=>{
    let calls=0;
    const fetchImpl=async()=>{calls++;throw new Error('no');};
    const vault={load:async()=>null};
    await assert.rejects(uploadBrowserImage({
        file:{...photo(),name:'payload.svg',type:'image/svg+xml'},
        fid,vault,crypto:webcrypto,fetchImpl,
    }),/Choose|Unsupported/);
    await assert.rejects(uploadBrowserImage({
        file:photo(),fid,vault,crypto:webcrypto,fetchImpl,
    }),/Authorize your Farcaster publishing signer/);
    assert.equal(calls,0);
});
test('mobile browser picker is synchronous and signing key stays in vault',async()=>{
    const {readFileSync}=await import('node:fs');
    const js=readFileSync(new URL('../../ui/wasm/protocol-signer-pre.js',import.meta.url),'utf8');
    const qml=readFileSync(new URL('../../ui/components/composer/ComposerDrawer.qml',import.meta.url),'utf8');
    const main=readGenycasterMain();
    assert.match(js,/picker\.click\(\)/);
    assert.match(js,/import\('\.\/browser_media_upload\.mjs'\)/);
    assert.match(qml,/mediaUpload\.browseBrowserImage\(root\.pendingMediaBlockIndex\)/);
    assert.match(main,/connectBrowserMediaUpload\(\s*attachment\.mediaUploadController, attachment\.presentationBridge,\s*attachment\.richComposerController\)/);
    assert.match(main,/event\.phase != BrowserOpenProofState::Phase::SignedOut/);
});

test('verified public release includes browser uploader in both copy and manifest lists',async()=>{
    const {readFileSync}=await import('node:fs');
    const packageScript=readFileSync(new URL('../../ci/package_wasm_assets.py',import.meta.url),'utf8');
    assert.equal(packageScript.split('"browser_media_upload.mjs"').length-1,2);
});

test('managed media deletion is signed, owner-scoped and never accepts raw asset URLs',async()=>{
    const {deleteBrowserAsset}=await import('../../ui/wasm/browser_media_upload.mjs');
    const managementToken='opaque-managed-token-for-test';
    const signed=[];
    const vault={
        load:async id=>id===fid?{publicKeyHex:key}:null,
        signMediaChallenge:async(id,bytes)=>{
            const challenge=new TextDecoder().decode(bytes);
            signed.push(challenge);
            assert.equal(id,fid);
            assert.ok(challenge.startsWith('genycaster-media-upload-v1\nDELETE\n/v1/assets\n'));
            return {publicKeyHex:key,signature:'de'.repeat(64)};
        },
    };
    const fetchImpl=async(url,opts)=>{
        assert.equal(url,'https://storage-farcaster.genyleap.com/v1/assets');
        assert.equal(opts.method,'DELETE');
        assert.equal(opts.credentials,'omit');
        assert.equal(JSON.parse(new TextDecoder().decode(opts.body)).managementToken,managementToken);
        return new Response(JSON.stringify({deleted:true}),{status:200});
    };
    const result=await deleteBrowserAsset({fid,managementToken,vault,crypto:webcrypto,fetchImpl});
    assert.equal(result.deleted,true);
    assert.equal(signed.length,1);
    await assert.rejects(deleteBrowserAsset({fid,managementToken:'https://x',vault,crypto:webcrypto,fetchImpl}),/Invalid managed/);
    await assert.rejects(deleteBrowserAsset({fid,managementToken,vault:{load:async()=>null},crypto:webcrypto,fetchImpl}),/verified Farcaster signer/);
});
test('WASM cleanup is durable, signer-gated, and only triggered by confirmed deletion',async()=>{
    const {readFileSync}=await import('node:fs');
    const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
    const main=readGenycasterMain();
    const signer=read('ui/app/browser/signer_bridge.hpp');
    const pre=read('ui/wasm/protocol-signer-pre.js');
    const panel=read('ui/components/composer/MediaUploadPanel.qml');
    const shell=read('ui/components/shell/AppShell.qml');
    const reply=read('ui/components/feed/ThreadReplyItem.qml');
    // The polling loop must reach cleanup even when there are no upload events.
    assert.match(main,/if\(received == 0\) break;/);
    assert.doesNotMatch(main,/if\(received == 0\) return;/);
    assert.match(main,/richComposer\.pendingManagedAssetCleanup\(\)/);
    assert.match(main,/richComposer\.markManagedAssetCleanupCompleted\(token\)/);
    assert.match(main,/bridge\.writeReady\(\)/);
    assert.match(main,/cleanupNextAttempt/);
    assert.match(signer,/releaseManagedAssetsForCast\(castId\)/);
    assert.match(signer,/operation == QStringLiteral\("remove"\)/);
    assert.match(pre,/targetCastId: action==='remove'/);
    assert.match(pre,/kind:'cleanup-completed'/);
    assert.match(pre,/preview\.length<=6200/);
    // One full-resolution attachment surface across desktop and mobile.
    assert.match(panel,/objectName: "composerOriginalImageStrip"/);
    assert.doesNotMatch(panel,/visible: root\.width >= 620/);
    assert.match(shell,/"embeds": root\.submittedReplyEmbeds/);
    assert.match(reply,/objectName: "replyMoreButton"/);
    assert.match(reply,/replyMenuLoader/);
    assert.match(reply,/root\.removeCastRequested\(root\.reply\)/);
    assert.doesNotMatch(reply,/objectName: "deleteOwnedReply"/);
});

test('gateway HTTP 204 means media deletion completed without a JSON body',async()=>{
    const {deleteBrowserAsset}=await import('../../ui/wasm/browser_media_upload.mjs');
    const vault={
        load:async()=>({publicKeyHex:key}),
        signMediaChallenge:async()=>({publicKeyHex:key,signature:'de'.repeat(64)}),
    };
    const deleted=await deleteBrowserAsset({
        fid,managementToken:'opaque-managed-token-for-test',
        vault,crypto:webcrypto,
        fetchImpl:async()=>new Response(null,{status:204}),
    });
    assert.deepEqual(deleted,{deleted:true});
});

test('media URLs stay out of visual-editor text and cancel queues storage cleanup',async()=>{
    const {readFileSync}=await import('node:fs');
    const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');
    const avatar=read('ui/components/content/Avatar.qml');
    const editor=read('ui/components/composer/RichTextEditor.qml');
    const drawer=read('ui/components/composer/ComposerDrawer.qml');
    const cpp=read('src/presentation/qt/rich_composer_controller.cpp');
    assert.match(avatar,/linkPreviewResolver\.imageSourceForUrl\(sourceUrl\)/);
    assert.match(editor,/qsTrId\("genycaster\.composer\.media"\)/);
    assert.doesNotMatch(editor,/text:\s*String\(enhancementSurface\.parent\.blockData\.url/);
    assert.match(drawer,/richComposer\.discardUnpublishedManagedMedia\(\)/);
    assert.match(cpp,/int RichComposerController::discardUnpublishedManagedMedia\(\)/);
    assert.match(cpp,/removeBlock\(static_cast<int>\(index - 1\)\)/);
});

test('confirmed cast deletion invalidates nested detail replies and pinned reply',async()=>{
    const {readFileSync}=await import('node:fs');
    const read=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');
    const bridge=read('src/presentation/qt/presentation_bridge.cpp');
    const header=read('src/presentation/qt/presentation_bridge.hpp');
    const shell=read('ui/components/shell/AppShell.qml');
    assert.match(header,/Q_PROPERTY\(QString lastRemovedCastId READ lastRemovedCastId NOTIFY stateChanged\)/);
    assert.match(bridge,/void removeReplyPresentation\(QVariantMap& parent, const QString& castId\)/);
    assert.match(bridge,/removeReplyPresentation\(row, castId\)/);
    assert.match(bridge,/removeReplyPresentation\(m_detail, targetCastId\)/);
    assert.match(bridge,/m_lastRemovedCastId = targetCastId/);
    assert.match(bridge,/m_writePhase == QStringLiteral\("confirmed"\)/);
    assert.match(shell,/root.confirmedReply = \(\{\}\)/);
    assert.match(shell,/lastRemovedCastId/);
});
