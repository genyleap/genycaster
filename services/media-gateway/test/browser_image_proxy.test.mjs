import test from 'node:test';
import assert from 'node:assert/strict';
import { isPublicAddress, validImageUrl, validPublicFaviconUrl,
  maximumBrowserImageBytes, retrieveBrowserImage,
  rasterMimeFromBytes } from '../browser_image_proxy.mjs';

test('public favicon exception allows only bounded site icon paths', () => {
  for (const allowed of [
    'https://linktr.ee/favicon.ico',
    'https://genycaster.xyz/favicon.ico',
    'https://example.com/favicon.png',
    'https://example.com/apple-touch-icon.png',
  ]) assert.equal(validPublicFaviconUrl(allowed)?.href, allowed);
  for (const denied of [
    'https://localhost/favicon.ico',
    'https://127.0.0.1/favicon.ico',
    'https://example.internal/favicon.ico',
    'http://example.com/favicon.ico',
    'https://user:pw@example.com/favicon.ico',
    'https://example.com:8443/favicon.ico',
    'https://example.com/favicon.ico?url=https://other.com',
    'https://example.com/favicon.svg',
    'https://example.com/private/profile.png',
    'https://example.com/favicon.ico#frag',
  ]) assert.equal(validPublicFaviconUrl(denied), null, denied);
});


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
  for (const hostname of ['media.tenor.com', 'media.giphy.com',
                          'lh3.googleusercontent.com', 'customer.imagekit.io', 'assets.geckoterminal.com']) {
    assert.equal(validImageUrl('https://' + hostname + '/opaque-image')?.hostname,
                 hostname, hostname);
  }
  assert.equal(validImageUrl('https://imagekit.io.evil.test/file'), null);
  const channelCid='bafkreigbei45ni5zsliszzeivotgee5auj2ykkh6zrzjwvm4izviidusny';
  assert.equal(validImageUrl('https://ipfs.decentralized-content.com/ipfs/'+channelCid)?.hostname,
               'ipfs.decentralized-content.com');
  for (const path of ['/','/admin','/ipfs/', '/ipfs/../../../admin',
                      '/api/v1/secrets', '/ipfs/'+channelCid+'/index.html']) {
    assert.equal(validImageUrl('https://ipfs.decentralized-content.com'+path),null,path);
  }
  assert.equal(validImageUrl('https://imagekit.io.evil.test/file'), null);
  for (const miniapp of ['atgif.com','universal-caster-income.vercel.app','assets.foundly.dev'])
    assert.equal(validImageUrl('https://'+miniapp+'/logo.png'),null,'publisher-specific hosts must not be hardcoded');
  assert.equal(validImageUrl('https://videodelivery.net/0123456789abcdef0123456789abcdef/thumbnails/thumbnail.jpg?time=1s')?.hostname,'videodelivery.net');
  assert.equal(validImageUrl('https://videodelivery.net/0123456789abcdef0123456789abcdef/manifest/video.m3u8'),null);
  assert.equal(validImageUrl('https://videodelivery.net/private/metadata'),null);
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

test('Qt WASM compatibility retry reaches CDN with bounded JPEG-first negotiation', async () => {
  const {PassThrough} = await import('node:stream');
  const accepts = [];
  const requester = (_url, options, callback) => {
    accepts.push(options.headers.accept);
    const request = new PassThrough();
    request.end = () => {
      const response = new PassThrough();
      response.statusCode = 200;
      response.headers = {'content-type':'image/jpeg','content-length':'4'};
      callback(response);
      response.end(Buffer.from([0xff, 0xd8, 0xff, 0xd9]));
    };
    return request;
  };
  const source = 'https://imagedelivery.net/public/avatar/original';
  const options = {
    lookupHost: async () => [{address:'8.8.8.8',family:4}], requester,
  };
  assert.equal((await retrieveBrowserImage(source, options)).status, 200);
  assert.equal((await retrieveBrowserImage(source, {...options,preferJpeg:true})).status, 200);
  assert.equal(accepts.length, 2);
  assert.match(accepts[0], /^image\/webp,/);
  assert.match(accepts[1], /^image\/jpeg,/);
  assert.ok(!accepts[0].includes('image\/avif'));
  assert.ok(!accepts[1].includes('image\/avif'));
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

test('same-origin Mini App GIF redirects remain bounded and re-check DNS', async () => {
  const {PassThrough}=await import('node:stream');
  let lookups=0;
  const urls=[];
  const requester=(url,options,callback)=>{
    urls.push(url.href);
    const request=new PassThrough();
    request.end=()=>{
      const response=new PassThrough();
      if(url.pathname === '/api/opengraph-image') {
        response.statusCode=302;
        response.headers={location:'https://atgif.com/logo_full.gif'};
        callback(response);response.end();
      } else {
        response.statusCode=200;
        response.headers={'content-type':'text/plain'};
        callback(response);response.end(Buffer.from('GIF89a', 'ascii'));
      }
    };
    return request;
  };
  const image=await retrieveBrowserImage('https://atgif.com/api/opengraph-image',{
    allowedHosts:['atgif.com'],
    lookupHost:async()=>{lookups++;return [{address:'8.8.8.8',family:4}];},requester,
  });
  assert.equal(image.status,200);
  assert.equal(image.contentType,'image/gif');
  assert.equal(lookups,2);
  assert.equal(urls.length,2);
  assert.equal(new URL(urls[1]).pathname,'/logo_full.gif');
  const disallowed=await retrieveBrowserImage('https://atgif.com/api/opengraph-image',{
    allowedHosts:['atgif.com'],
    lookupHost:async()=>[{address:'8.8.8.8',family:4}],
    requester:(url,options,callback)=>{
      const request=new PassThrough();request.end=()=>{
        const response=new PassThrough();response.statusCode=302;
        response.headers={location:'https://evil.test/asset.gif'};
        callback(response);response.end();
      };return request;
    },
  });
  assert.equal(disallowed.status,502);
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


test('Arweave content-addressed raster redirects work but unrelated origins stay denied', async () => {
  const {PassThrough} = await import('node:stream');
  const id = '47YO_NMniMUYzYHHDRlEnkkTwT39IO938FK0Cn6JVfk';
  const source = 'https://arweave.net/' + id;
  const expected = 'https://4o3a57gte6emkggnqhdq2gketzerhqj57uqo657qkk2au7ujkx4q.arweave.net/' + id;
  const lookupHost = async () => [{address:'8.8.8.8', family:4}];
  const makeRequester = (redirect, calls) => (url, options, callback) => {
    calls.push(url.href);
    const request = new PassThrough();
    request.end = () => {
      const response = new PassThrough();
      if (url.hostname === 'arweave.net') {
        response.statusCode = 302;
        response.headers = {location:redirect};
        callback(response);
        response.end();
      } else {
        response.statusCode = 200;
        response.headers = {'content-length':'4'};
        callback(response);
        response.end(Buffer.from([0xff,0xd8,0xff,0xd9]));
      }
    };
    return request;
  };
  const calls = [];
  const good = await retrieveBrowserImage(source,{
    lookupHost, requester:makeRequester(expected,calls)
  });
  assert.equal(good.status,200);
  assert.equal(good.contentType,'image/jpeg');
  assert.deepEqual(calls,[source,expected]);
  for (const disallowed of [
    'https://evil.test/' + id,
    'https://different.arweave.net/' + id,
    expected + '?bypass=1',
    expected.slice(0,-1) + 'x',
    'http://' + expected.slice('https://'.length)
  ]) {
    const deniedCalls = [];
    const denied = await retrieveBrowserImage(source,{
      lookupHost,requester:makeRequester(disallowed,deniedCalls)
    });
    assert.equal(denied.status,502,disallowed);
    assert.equal(denied.error,'image_redirect_not_allowed');
    assert.equal(deniedCalls.length,1);
  }
});

test('Tenor media1 GET-only GIFs are admitted narrowly without a generic external proxy',()=>{
  const good='https://media1.tenor.com/m/Kzqq0MjDxYcAAAAd/pepe-pepe-the-frog.gif';
  assert.equal(validImageUrl(good)?.href,good);
  for(const bad of [
    'https://media1.tenor.com/admin',
    'https://media1.tenor.com/m/Kzqq0MjDxYcAAAAd/pepe-pepe-the-frog.gif?token=abc',
    'https://media1.tenor.com/m/Kzqq0MjDxYcAAAAd/test.svg',
    'https://media1.tenor.com/m/../admin',
    'https://attacker.media1.tenor.com/m/Kzqq0MjDxYcAAAAd/test.gif',
    'https://media1.tenor.com.evil.test/m/Kzqq0MjDxYcAAAAd/test.gif'
  ]) assert.equal(validImageUrl(bad),null,bad);
});

test('Tenor animated GIF is retrieved using GET with validated raster bytes, not HEAD',async()=>{
  const {PassThrough}=await import('node:stream');
  const methods=[];
  const requester=(url,options,callback)=>{
    methods.push(options.method);
    const request=new PassThrough();
    request.end=()=>{
      const response=new PassThrough();
      response.statusCode=200;
      response.headers={'content-type':'image/gif','content-length':'8'};
      callback(response);
      response.end(Buffer.from('GIF89a00'));
    };
    return request;
  };
  const result=await retrieveBrowserImage(
    'https://media1.tenor.com/m/Kzqq0MjDxYcAAAAd/pepe-pepe-the-frog.gif',
    {lookupHost:async()=>[{address:'8.8.8.8',family:4}],requester});
  assert.equal(result.status,200);
  assert.equal(result.contentType,'image/gif');
  assert.deepEqual(methods,['GET']);
});
