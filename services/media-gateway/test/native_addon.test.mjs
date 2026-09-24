import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const defaultPath = fileURLToPath(new URL('../storage/native/genyleap_quilibrium_storage.node', import.meta.url));
const configured = String(process.env.GENYLEAP_MEDIA_QUILIBRIUM_ADDON || '').trim();
const addonPath = configured ? path.resolve(configured) : defaultPath;

assert.ok(
  fs.existsSync(addonPath),
  `Quilibrium SDK native addon is missing at ${addonPath}; run npm run build:native first.`,
);

test('native addon delegates read presigning and multipart header signing to Quilibrium SDK SigV4', () => {
  const require = createRequire(import.meta.url);
  const native = require(addonPath);
  const signer = native.createQStorageSigner({
    endpoint: 'https://storage.example.test',
    region: 'test-region-1',
    bucket: 'media-bucket',
    accessKeyId: 'ACCESSKEY123',
    secretAccessKey: 'SECRET_NEVER_IN_URL',
    sessionToken: '',
  });

  const upload = native.presignPut(signer, 'dir/image.jpg', 'image/jpeg', 900);
  const get = native.presignGet(signer, 'dir/image.jpg', 300);
  const head = native.presignHead(signer, 'dir/image.jpg', 300);

  const uploadUrl = new URL(upload.url);
  assert.equal(uploadUrl.origin, 'https://storage.example.test');
  assert.equal(uploadUrl.pathname, '/media-bucket/dir/image.jpg');
  assert.equal(uploadUrl.searchParams.get('X-Amz-Algorithm'), 'AWS4-HMAC-SHA256');
  assert.match(uploadUrl.searchParams.get('X-Amz-Credential') ?? '', /\/test-region-1\/s3\/aws4_request$/);
  assert.equal(uploadUrl.searchParams.get('X-Amz-Expires'), '900');
  assert.equal(upload.requiredHeaders['content-type'], 'image/jpeg');
  assert.equal(upload.url.includes('SECRET_NEVER_IN_URL'), false);
  assert.notEqual(new URL(get.url).searchParams.get('X-Amz-Signature'), new URL(head.url).searchParams.get('X-Amz-Signature'));

  const created = native.signCreateMultipart(signer, 'dir/video.mp4', 'video/mp4');
  const createdUrl = new URL(created.url);
  assert.equal(createdUrl.pathname, '/media-bucket/dir/video.mp4');
  assert.equal(createdUrl.search, '?uploads');
  assert.match(created.headers.authorization, /^AWS4-HMAC-SHA256 Credential=ACCESSKEY123\//);
  assert.match(created.headers.authorization, /\/test-region-1\/s3\/aws4_request/);
  assert.equal(created.headers['content-type'], 'video/mp4');
  assert.match(created.headers['x-amz-content-sha256'], /^[0-9a-f]{64}$/);
  assert.equal('host' in created.headers, false);

  const partBody = Buffer.from('multipart-body');
  const part = native.signUploadPart(signer, 'dir/video.mp4', 'upload-id/with+chars', 3, partBody);
  const partUrl = new URL(part.url);
  assert.equal(partUrl.searchParams.get('partNumber'), '3');
  assert.equal(partUrl.searchParams.get('uploadId'), 'upload-id/with+chars');
  assert.match(part.headers.authorization, /^AWS4-HMAC-SHA256 /);
  assert.match(part.headers['x-amz-content-sha256'], /^[0-9a-f]{64}$/);

  const completionXml = Buffer.from('<CompleteMultipartUpload/>');
  const completed = native.signCompleteMultipart(signer, 'dir/video.mp4', 'upload-id/with+chars', completionXml);
  assert.equal(completed.headers['content-type'], 'application/xml');
  assert.match(completed.headers.authorization, /^AWS4-HMAC-SHA256 /);

  const aborted = native.signAbortMultipart(signer, 'dir/video.mp4', 'upload-id/with+chars');
  assert.match(aborted.headers.authorization, /^AWS4-HMAC-SHA256 /);

  const deleted = native.signDeleteObject(signer, 'dir/video.mp4');
  const deletedUrl = new URL(deleted.url);
  assert.equal(deletedUrl.pathname, '/media-bucket/dir/video.mp4');
  assert.equal(deletedUrl.search, '');
  assert.match(deleted.headers.authorization, /^AWS4-HMAC-SHA256 /);
  assert.equal(JSON.stringify({ created, part, completed, aborted, deleted }).includes('SECRET_NEVER_IN_URL'), false);

  assert.throws(() => native.presignGet(signer, 'dir/image.jpg', 0), /between 1 and 604800/);
  assert.throws(() => native.presignGet(signer, 'dir/image.jpg', 1.5), /integer between 1 and 604800/);
  assert.throws(() => native.signUploadPart(signer, 'dir/video.mp4', 'upload-id', 0, Buffer.alloc(0)), /between 1 and 10000/);
});
