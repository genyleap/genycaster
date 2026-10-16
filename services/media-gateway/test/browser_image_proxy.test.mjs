import test from 'node:test';
import assert from 'node:assert/strict';
import { isPublicAddress, validImageUrl, maximumBrowserImageBytes, retrieveBrowserImage } from '../browser_image_proxy.mjs';

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

test('configured image size budget is bounded', () => {
  assert.ok(maximumBrowserImageBytes <= 8 * 1024 * 1024);
});
