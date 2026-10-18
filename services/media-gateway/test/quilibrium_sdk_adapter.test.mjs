import assert from 'node:assert/strict';
import test from 'node:test';

import {
  loadQuilibriumStorageConfig,
  QuilibriumSdkStorage,
} from '../storage/quilibrium_sdk.mjs';
import { createStorageAdapter } from '../storage/index.mjs';

function environment(overrides = {}) {
  return {
    GENYLEAP_MEDIA_STORAGE_PROVIDER: 'quilibrium-qstorage',
    GENYLEAP_MEDIA_STORAGE_ENDPOINT: 'https://qstorage.quilibrium.com',
    GENYLEAP_MEDIA_STORAGE_REGION: 'q-world-1',
    GENYLEAP_MEDIA_STORAGE_BUCKET: 'genycaster',
    GENYLEAP_MEDIA_STORAGE_ACCESS_KEY_ID: 'ACCESSKEY123',
    GENYLEAP_MEDIA_STORAGE_SECRET_ACCESS_KEY: 'secret-value-never-sent-to-client',
    ...overrides,
  };
}

function fakeNative() {
  const calls = [];
  return {
    calls,
    createQStorageSigner(config) {
      calls.push(['create', config]);
      return { opaque: true };
    },
    presignPut(handle, key, contentType, expiresSeconds) {
      calls.push(['put', handle, key, contentType, expiresSeconds]);
      return {
        url: `https://qstorage.test/${key}?put=1`,
        requiredHeaders: { 'content-type': contentType },
        expiresAtMs: 0,
      };
    },
    presignGet(handle, key, expiresSeconds) {
      calls.push(['get', handle, key, expiresSeconds]);
      return { url: `https://qstorage.test/${key}?get=1`, requiredHeaders: {}, expiresAtMs: 0 };
    },
    presignHead(handle, key, expiresSeconds) {
      calls.push(['head', handle, key, expiresSeconds]);
      return { url: `https://qstorage.test/${key}?head=1`, requiredHeaders: {}, expiresAtMs: 0 };
    },
    signCreateMultipart(handle, key, contentType) {
      calls.push(['createMultipart', handle, key, contentType]);
      return {
        url: `https://qstorage.test/${key}?uploads`,
        headers: { authorization: 'AWS4-HMAC-SHA256 create', 'content-type': contentType },
      };
    },
    signUploadPart(handle, key, uploadId, partNumber, body) {
      calls.push(['uploadPart', handle, key, uploadId, partNumber, Buffer.from(body)]);
      return {
        url: `https://qstorage.test/${key}?partNumber=${partNumber}&uploadId=${uploadId}`,
        headers: { authorization: 'AWS4-HMAC-SHA256 part', 'x-amz-content-sha256': 'hash' },
      };
    },
    signCompleteMultipart(handle, key, uploadId, body) {
      calls.push(['completeMultipart', handle, key, uploadId, Buffer.from(body)]);
      return {
        url: `https://qstorage.test/${key}?uploadId=${uploadId}`,
        headers: { authorization: 'AWS4-HMAC-SHA256 complete', 'content-type': 'application/xml' },
      };
    },
    signAbortMultipart(handle, key, uploadId) {
      calls.push(['abortMultipart', handle, key, uploadId]);
      return {
        url: `https://qstorage.test/${key}?uploadId=${uploadId}`,
        headers: { authorization: 'AWS4-HMAC-SHA256 abort' },
      };
    },
    signHeadObject(handle, key) {
      calls.push(['headObject', handle, key]);
      return {
        url: `https://qstorage.test/${key}`,
        headers: { authorization: 'AWS4-HMAC-SHA256 head', 'x-amz-content-sha256': 'hash' },
      };
    },
    signDeleteObject(handle, key) {
      calls.push(['deleteObject', handle, key]);
      return {
        url: `https://qstorage.test/${key}`,
        headers: { authorization: 'AWS4-HMAC-SHA256 delete' },
      };
    },
  };
}

test('Quilibrium SDK adapter keeps QStorage credentials behind the gateway boundary', () => {
  const native = fakeNative();
  const config = loadQuilibriumStorageConfig(environment());
  const storage = new QuilibriumSdkStorage(config, { native });

  const upload = storage.createUpload({
    key: 'v1/2026/08/object.jpg',
    contentType: 'image/jpeg',
    expiresSeconds: 900,
  });
  const download = storage.createDownload({
    key: 'v1/2026/08/object.jpg',
    method: 'GET',
    expiresSeconds: 300,
  });

  assert.equal(upload.method, 'PUT');
  assert.equal(upload.headers['content-type'], 'image/jpeg');
  assert.match(upload.uploadUrl, /\?put=1$/);
  assert.match(download, /X-Amz-Algorithm=AWS4-HMAC-SHA256/);
  assert.match(download, /X-Amz-Signature=[0-9a-f]{64}$/);
  assert.equal(JSON.stringify({ upload, download }).includes(config.secretAccessKey), false);
  assert.deepEqual(native.calls[0][1], {
    endpoint: 'https://qstorage.quilibrium.com',
    region: 'q-world-1',
    bucket: 'genycaster',
    accessKeyId: 'ACCESSKEY123',
    secretAccessKey: 'secret-value-never-sent-to-client',
    sessionToken: '',
  });
});

test('multipart relay uses SDK-signed Authorization-header requests and preserves provider ETags', async () => {
  const native = fakeNative();
  const requests = [];
  const storage = new QuilibriumSdkStorage(loadQuilibriumStorageConfig(environment()), {
    native,
    fetchImpl: async (url, options) => {
      requests.push({ url: String(url), options });
      if (String(url).endsWith('?uploads')) {
        return new Response('<InitiateMultipartUploadResult><UploadId>provider-upload-123</UploadId></InitiateMultipartUploadResult>', {
          status: 200,
          headers: { 'content-type': 'application/xml' },
        });
      }
      if (options.method === 'PUT') {
        return new Response(null, { status: 200, headers: { etag: '"part-etag"' } });
      }
      if (options.method === 'POST') {
        return new Response('<CompleteMultipartUploadResult><ETag>"complete"</ETag></CompleteMultipartUploadResult>', { status: 200 });
      }
      if (options.method === 'DELETE') {
        return new Response(null, { status: 204 });
      }
      throw new Error('unexpected request');
    },
  });

  const started = await storage.beginMultipartUpload({
    key: 'v1/2026/08/object.jpg',
    contentType: 'image/jpeg',
  });
  assert.deepEqual(started, { providerUploadId: 'provider-upload-123' });

  const uploaded = await storage.uploadPart({
    key: 'v1/2026/08/object.jpg',
    providerUploadId: started.providerUploadId,
    partNumber: 1,
    body: Buffer.from('part-body'),
  });
  assert.deepEqual(uploaded, { etag: '"part-etag"' });

  const completed = await storage.completeMultipartUpload({
    key: 'v1/2026/08/object.jpg',
    providerUploadId: started.providerUploadId,
    parts: [{ partNumber: 1, etag: uploaded.etag }],
  });
  assert.equal(completed.etag, '"complete"');

  await storage.abortMultipartUpload({
    key: 'v1/2026/08/object.jpg',
    providerUploadId: started.providerUploadId,
  });
  await storage.deleteObject('v1/2026/08/object.jpg');

  assert.equal(requests[0].options.method, 'POST');
  assert.match(requests[0].options.headers.authorization, /^AWS4-HMAC-SHA256/);
  assert.equal(requests[1].options.method, 'PUT');
  assert.deepEqual(Buffer.from(requests[1].options.body), Buffer.from('part-body'));
  assert.equal(requests[2].options.method, 'POST');
  assert.match(Buffer.from(requests[2].options.body).toString(), /<PartNumber>1<\/PartNumber><ETag>\"part-etag\"<\/ETag>/);
  assert.equal(requests[3].options.method, 'DELETE');
  assert.equal(requests[4].options.method, 'DELETE');
  assert.equal(native.calls.at(-1)[0], 'deleteObject');
  assert.equal(JSON.stringify(requests).includes('secret-value-never-sent-to-client'), false);
});


test('image relay reads bounded bytes with a signed QStorage GET and rejects oversized objects', async () => {
  const native = fakeNative();
  const bytes = Buffer.from([0xff, 0xd8, 0xff, 0x00, 0x01, 0x02]);
  let responseBytes = bytes;
  const requests = [];
  const storage = new QuilibriumSdkStorage(loadQuilibriumStorageConfig(environment()), {
    native,
    fetchImpl: async (url, options) => {
      requests.push({ url: String(url), options });
      return new Response(responseBytes, {
        status: 200,
        headers: { 'content-type': 'image/jpeg', 'content-length': String(responseBytes.length) },
      });
    },
  });
  const key = 'v1/2026/10/12345678-1234-1234-1234-123456789abc.jpg';
  assert.deepEqual(await storage.readImageObject(key, 1024), bytes);
  assert.equal(requests[0].options.method, 'GET');
  assert.equal(requests[0].options.redirect, 'manual');
  assert.match(requests[0].url, /^https:\/\/qstorage\.quilibrium\.com\/genycaster\/v1\/2026\/10\//);
  assert.match(requests[0].url, /X-Amz-Signature=[0-9a-f]{64}/);
  await assert.rejects(storage.readImageObject(key, 5), { statusCode: 413 });
  responseBytes = Buffer.from('not an image');
  await assert.rejects(storage.readImageObject(key, 6), { statusCode: 413 });
});

test('DeleteObject falls back to JavaScript SigV4 when the native addon is older', async () => {
  const native = fakeNative();
  delete native.signDeleteObject;
  const requests = [];
  const storage = new QuilibriumSdkStorage(loadQuilibriumStorageConfig(environment()), {
    native,
    fetchImpl: async (url, options) => {
      requests.push({ url: String(url), options });
      return new Response(null, { status: 204 });
    },
  });

  await storage.deleteObject('v1/2026/08/object.jpg');

  assert.equal(requests.length, 1);
  assert.equal(requests[0].options.method, 'DELETE');
  assert.equal(requests[0].url, 'https://qstorage.quilibrium.com/genycaster/v1/2026/08/object.jpg');
  assert.match(requests[0].options.headers.authorization, /^AWS4-HMAC-SHA256 Credential=ACCESSKEY123\//);
  assert.equal(
    requests[0].options.headers['x-amz-content-sha256'],
    'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
  );
  assert.equal(native.calls.some((call) => call[0] === 'deleteObject'), false);
});

test('completed-object verification uses QStorage-compatible hashed-payload GET query authentication', async () => {
  const native = fakeNative();
  const requests = [];
  const storage = new QuilibriumSdkStorage(loadQuilibriumStorageConfig(environment()), {
    native,
    fetchImpl: async (url, options) => {
      requests.push({ url: String(url), options });
      return new Response(null, {
        status: 200,
        headers: {
          'content-length': '1234',
          'content-type': 'video/mp4',
          etag: '"abc"',
        },
      });
    },
  });

  const object = await storage.headObject('v1/2026/08/object.mp4');
  assert.deepEqual(object, {
    sizeBytes: 1234,
    contentType: 'video/mp4',
    etag: '"abc"',
  });
  assert.equal(requests.length, 1);
  assert.equal(requests[0].options.method, 'GET');
  assert.match(requests[0].url, /X-Amz-Algorithm=AWS4-HMAC-SHA256/);
  assert.match(requests[0].url, /X-Amz-Signature=[0-9a-f]{64}$/);
  assert.equal(native.calls.some((call) => call[0] === 'head' || call[0] === 'get'), false);
});

test('storage configuration rejects fractional or malformed presign lifetimes', () => {
  assert.throws(
    () => loadQuilibriumStorageConfig(environment({
      GENYLEAP_MEDIA_STORAGE_VERIFICATION_URL_LIFETIME_SECONDS: '60.5',
    })),
    /between 1 and 604800 seconds/,
  );
  assert.throws(
    () => loadQuilibriumStorageConfig(environment({
      GENYLEAP_MEDIA_STORAGE_VERIFICATION_URL_LIFETIME_SECONDS: '60seconds',
    })),
    /between 1 and 604800 seconds/,
  );
});

test('storage factory isolates the selected provider from the media gateway', () => {
  const native = fakeNative();
  const storage = createStorageAdapter(environment(), { native });
  assert.ok(storage instanceof QuilibriumSdkStorage);
  assert.throws(
    () => createStorageAdapter(environment({ GENYLEAP_MEDIA_STORAGE_PROVIDER: 'unknown' }), { native }),
    /Unsupported media storage provider/,
  );
});
