import test from 'node:test';
import assert from 'node:assert/strict';
import sharp from 'sharp';
import {readFileSync} from 'node:fs';
import {createStickerPreviews,previewEntry} from '../sticker_previews.mjs';

test('only pinned public catalogue IDs may request a preview',()=>{
  assert.equal(previewEntry('../secret'),null);
  assert.equal(previewEntry('https://localhost/admin'),null);
  assert.equal(previewEntry('unknown-sticker'),null);
  assert.equal(previewEntry('cream-cat-soup-sip')?.pack,'cream-cat-pro');
  const entry=previewEntry('02-big-smile');
  assert.equal(entry.pack,'njal');
  assert.ok(entry.source.startsWith('https://red-geographical-clownfish-714.mypinata.cloud/ipfs/'));
});
test('mobile preview uses real raster bytes, bounded PNG and one pending fetch',async()=>{
  const sample=await sharp({create:{width:32,height:32,channels:4,
    background:{r:255,g:0,b:15,alpha:1}}}).webp().toBuffer();
  let count=0;
  const preview=createStickerPreviews({retrieve:async(url,options)=>{
    count++;
    assert.equal(url,previewEntry('cream-cat-soup-sip').source);
    assert.deepEqual(options.allowedHosts,[
      'mssgs-stickers.com','red-geographical-clownfish-714.mypinata.cloud']);
    return {status:200,contentType:'image/webp',body:sample};
  }});
  const [a,b]=await Promise.all([preview.get('cream-cat-soup-sip'),
    preview.get('cream-cat-soup-sip')]);
  assert.equal(a.status,200);
  assert.equal(b.status,200);
  assert.equal(a.contentType,'image/png');
  assert.equal(count,1);
  assert.ok(a.body.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10])));
  const metadata=await sharp(a.body).metadata();
  assert.equal(metadata.format,'png');
  assert.equal(metadata.width,32);
  assert.equal((await preview.get('cream-cat-soup-sip')).cached,true);
  assert.equal(count,1);
});
test('a CDN substitution cannot replace a pinned Njal sticker',async()=>{
  const sample=await sharp({create:{width:12,height:12,channels:4,
    background:{r:2,g:2,b:2,alpha:1}}}).webp().toBuffer();
  const preview=createStickerPreviews({retrieve:async()=>({
    status:200,contentType:'image/webp',body:sample
  })});
  assert.deepEqual(await preview.get('02-big-smile'),
    {status:502,error:'sticker_integrity_failed'});
  assert.deepEqual(await preview.get('bogus'),{status:404,error:'sticker_not_found'});
});
test('the gateway exposes a strict ID-only, public raster preview route',()=>{
  const source=readFileSync(new URL('../gateway.mjs',import.meta.url),'utf8');
  assert.match(source,/requestUrl\.pathname === '\/v1\/browser\/sticker-previews'/);
  assert.match(source,/requestUrl\.searchParams\.size!==1 \|\| !previewEntry\(id\)/);
  assert.match(source,/const result=await stickerPreviews\.get\(id\)/);
  assert.match(source,/'content-type':'image\/png'/);
  const cpp=readFileSync(new URL('../../../ui/app/identity/connections.cpp',import.meta.url),'utf8');
  assert.match(cpp,/stickerPreviewBaseUrl\(\) const/);
  const picker=readFileSync(new URL('../../../ui/components/composer/StickerPicker.qml',import.meta.url),'utf8');
  assert.match(picker,/root\.stickerDisplayUrl\(modelData\.id\)/);
  assert.match(picker,/needsPreviewCompatibility: Qt\.platform\.os === "wasm"/);
});
