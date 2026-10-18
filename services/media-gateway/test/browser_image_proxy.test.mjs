import test from 'node:test';
import assert from 'node:assert/strict';
import { isPublicAddress, validImageUrl, maximumBrowserImageBytes, retrieveBrowserImage,
  rasterMimeFromBytes } from '../browser_image_proxy.mjs';

test('public image URL policy rejects credentialed/non-HTTPS/internal destinations', () => {
  for (const target of [
    'http://imagedelivery.net/x', 'https://localhost/image.png',
    'https://127.0.0.1/a', 'https://169.254.169.254/metadata',
    'https://user:pass@imagedelivery.net/a', 'https://imagedelivery.net:8443/a',
    'https://example.com/a', 'https://imagedelivery.net.evil.test/a',
    'https://media-farcaster.genyleap.com/v1/browser/images?url=https://imagedelivery.net/a',
  ]) assert.equal(validImageUrl(target), null, target);
  assert.equal(validImageUrl('https://imagedelivery.net/x/y/public')?.hostname, 'imagedelivery.net');
  assert.equal(validImageUrl('https://media-farcaster.genyleap.com/m/v1/file.jpg')?.hostname,
               'media-farcaster.genyleap.com');
});

test('DNS pinning denies local/nonpublic and accepts public addresses', () => {
  for (const ip of ['127.0.0.1', '10.2.3.4', '172.16.0.1', '192.168.1.1',
                    '169.254.169.254', '100.64.0.1', '224.0.0.1']) {
    assert.equal(isPublicAddress(ip, 4), false, ip);
  }
  assert.equal(isPublicAddress('8.8.8.8', 4), true);
  assert.equal(isPublicAddress('::1', 6), false);
  assert.equal(isPublicAddress('fe80::1', 6), false);
  assert.equal(isPublicAddress('2606:4700:4700::1111', 6), true);
});

test('DNS rebinding/private DNS is rejected before creating a socket', async () => {
  let called = false;
  const result = await retrieveBrowserImage('https://imagedelivery.net/a.png', {
    lookupHost: async () => [{address: '127.0.0.1', family: 4}],
    requester: () => { called = true; throw Error('must not call'); },
  });
  assert.equal(result.status, 403);
  assert.equal(called, false);
});

test('raster signatures win over mislabelled server Content-Type without accepting XML or SVG',()=>{
  const png=Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),Buffer.alloc(24)]);
  const webp=Buffer.concat([Buffer.from('RIFF'),Buffer.alloc(4),Buffer.from('WEBP')]);
  assert.equal(rasterMimeFromBytes(png),'image/png');
  assert.equal(rasterMimeFromBytes(webp),'image/webp');
  assert.equal(rasterMimeFromBytes(Buffer.from('<?xml version="1.0"?><svg></svg>')),null);
  assert.equal(rasterMimeFromBytes(Buffer.from('<!DOCTYPE html><script>alert(1)</script>')),null);
});

test('configured image size budget is bounded', () => {
  assert.ok(maximumBrowserImageBytes <= 8 * 1024 * 1024);
});

test('private qStorage signed image redirects work only behind stable first-party media URLs', async () => {
  const { PassThrough } = await import('node:stream');
  const path = 'genycaster/v1/2026/10/12345678-1234-1234-1234-123456789abc.jpg';
  const presign = new URL('https://qstorage.quilibrium.com/' + path);
  presign.searchParams.set('X-Amz-Algorithm', 'AWS4-HMAC-SHA256');
  presign.searchParams.set('X-Amz-Credential', 'test/20261018/q-world-1/s3/aws4_request');
  presign.searchParams.set('X-Amz-Expires', '900');
  presign.searchParams.set('X-Amz-SignedHeaders', 'host');
  presign.searchParams.set('X-Amz-Signature', 'a'.repeat(64));
  const calls = [];
  const requester = (url, options, callback) => {
    calls.push(new URL(url));
    const request = new PassThrough();
    request.end = () => {
      const response = new PassThrough();
      if (url.hostname === 'media-farcaster.genyleap.com') {
        response.statusCode = 307;
        response.headers = {location: presign.href};
        callback(response);
        response.end();
      } else {
        response.statusCode = 200;
        response.headers = {'content-type':'image/jpeg','content-length':'4'};
        callback(response);
        response.end(Buffer.from([0xff,0xd8,0xff,0xd9]));
      }
    };
    return request;
  };
  const lookupHost = async () => [{address:'8.8.8.8', family:4}];
  const source='https://media-farcaster.genyleap.com/m/v1/2026/10/asset.jpg';
  const image=await retrieveBrowserImage(source,{lookupHost,requester});
  assert.equal(image.status,200);
  assert.equal(image.contentType,'image/jpeg');
  assert.equal(calls.length,2);
  assert.ok(calls[0].searchParams.has('gc_image_request'));
  assert.equal(calls[1].hostname,'qstorage.quilibrium.com');
  assert.equal((await retrieveBrowserImage(presign.href,{lookupHost,requester})).status,400);
  assert.equal(calls.length,2);
});

test('signed storage redirects reject unrelated keys, missing signatures, and unexpected origins', async () => {
  const { PassThrough } = await import('node:stream');
  const source='https://media-farcaster.genyleap.com/m/v1/2026/10/test.jpg';
  const lookupHost = async () => [{address:'8.8.4.4',family:4}];
  for(const location of [
    'https://evil.test/x',
    'https://qstorage.quilibrium.com/another-private-bucket/a.jpg',
    'https://qstorage.quilibrium.com/genycaster/v1/2026/10/a.jpg',
    'https://qstorage.quilibrium.com/genycaster/v1/2026/10/a.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Expires=10000&X-Amz-SignedHeaders=host&X-Amz-Signature='+ 'a'.repeat(64)
  ]) {
    const requester=(url,options,callback)=>{
      const req =new PassThrough();
      req.end=()=>{
        const response=new PassThrough();
        response.statusCode=307;
        response.headers={location};
        callback(response);
        response.end();
      };
      return req;
    };
    const response=await retrieveBrowserImage(source,{lookupHost,requester});
    assert.notEqual(response.status,200,location);
  }
});
