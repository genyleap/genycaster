import test from 'node:test';
import assert from 'node:assert/strict';
import { webcrypto } from 'node:crypto';
import { uploadBrowserImage } from '../../ui/wasm/browser_media_upload.mjs';

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
            }),{status:201});
        if(url.endsWith('/parts/1'))
            return new Response(JSON.stringify({partNumber:1,etag:'"part-1"'}));
        if(url.endsWith('/v1/uploads/complete'))
            return new Response(JSON.stringify({
                assetUrl,fileName:'photo.png',contentType:'image/png',
                sizeBytes:128,managementToken:'opaque-token',
            }));
        throw new Error('unexpected upload request '+url);
    };
    const result=await uploadBrowserImage({
        file:photo(),fid,vault,crypto:webcrypto,fetchImpl,
        onProgress:(...args)=>phases.push(args),
    });
    assert.equal(result.assetUrl,assetUrl);
    assert.equal(requests.length,3);
    assert.equal(challenges.length,3);
    assert.equal(phases.at(-1)[0],'verifying');
    assert.equal(phases.find(x=>x[0]==='uploading')[2],128);
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
    const qml=readFileSync(new URL('../../ui/components/ComposerDrawer.qml',import.meta.url),'utf8');
    const main=readFileSync(new URL('../../ui/mobile/main.cpp',import.meta.url),'utf8');
    assert.match(js,/picker\.click\(\)/);
    assert.match(js,/import\('\.\/browser_media_upload\.mjs'\)/);
    assert.match(qml,/mediaUpload\.browseBrowserImage\(root\.pendingMediaBlockIndex\)/);
    assert.match(main,/connectBrowserMediaUpload\(mediaUploadController, presentationBridge\)/);
    assert.match(main,/event\.phase != BrowserOpenProofState::Phase::SignedOut/);
});

test('verified public release includes browser uploader in both copy and manifest lists',async()=>{
    const {readFileSync}=await import('node:fs');
    const packageScript=readFileSync(new URL('../../tools/package_wasm_assets.py',import.meta.url),'utf8');
    assert.equal(packageScript.split('"browser_media_upload.mjs"').length-1,2);
});
