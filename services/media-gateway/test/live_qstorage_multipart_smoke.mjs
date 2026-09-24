import assert from 'node:assert/strict';
import { createHash, randomUUID } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import process from 'node:process';

import {
  loadQuilibriumStorageConfig,
  QuilibriumSdkStorage,
} from '../storage/quilibrium_sdk.mjs';

async function loadDotEnv(fileName = '.env') {
  const content = await readFile(path.resolve(fileName), 'utf8');
  for (const rawLine of content.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith('#') || !line.includes('=')) continue;
    const separator = line.indexOf('=');
    const key = line.slice(0, separator).trim();
    let value = line.slice(separator + 1).trim();
    if ((value.startsWith('"') && value.endsWith('"')) ||
        (value.startsWith("'") && value.endsWith("'"))) {
      value = value.slice(1, -1);
    }
    if (!(key in process.env)) process.env[key] = value;
  }
}

function sha256(value) {
  return createHash('sha256').update(value).digest('hex');
}

await loadDotEnv();

const config = loadQuilibriumStorageConfig(process.env);
const storage = new QuilibriumSdkStorage(config);

const sizeBytes = 5 * 1024 * 1024;
const payload = Buffer.alloc(sizeBytes, 0x5a);
const expectedHash = sha256(payload);
const key = `diagnostics/node-multipart-smoke-${Date.now()}-${randomUUID()}.bin`;

let providerUploadId = '';
let completed = false;

console.log('Genycaster QStorage multipart live smoke test');
console.log(`endpoint=${config.endpoint}`);
console.log(`region=${config.region}`);
console.log(`bucket=${config.bucket}`);
console.log(`key=${key}`);
console.log(`bytes=${payload.length}`);
console.log(`sha256=${expectedHash}`);

try {
  console.log('\n[1/6] CreateMultipartUpload');
  const created = await storage.beginMultipartUpload({
    key,
    contentType: 'application/octet-stream',
  });
  providerUploadId = created.providerUploadId;
  assert.ok(providerUploadId);
  console.log('PASS');

  console.log('\n[2/6] UploadPart #1');
  const uploaded = await storage.uploadPart({
    key,
    providerUploadId,
    partNumber: 1,
    body: payload,
  });
  assert.ok(uploaded.etag);
  console.log(`PASS etag=${uploaded.etag}`);

  console.log('\n[3/6] CompleteMultipartUpload');
  const completion = await storage.completeMultipartUpload({
    key,
    providerUploadId,
    parts: [{ partNumber: 1, etag: uploaded.etag }],
  });
  completed = true;
  console.log(`PASS etag=${completion.etag || '(provider omitted aggregate ETag)'}`);

  console.log('\n[4/6] Presigned HEAD/GET object verification');
  const metadata = await storage.headObject(key);
  assert.equal(metadata.sizeBytes, payload.length);
  assert.equal(
    metadata.contentType.split(';', 1)[0].trim().toLowerCase(),
    'application/octet-stream',
  );
  console.log(`PASS size=${metadata.sizeBytes} contentType=${metadata.contentType}`);

  console.log('\n[5/6] Presigned GET + byte integrity');
  const downloadUrl = storage.createDownload({ key, method: 'GET', expiresSeconds: 120 });
  const response = await fetch(downloadUrl, { redirect: 'manual' });
  assert.equal(response.status, 200);
  const downloaded = Buffer.from(await response.arrayBuffer());
  assert.equal(downloaded.length, payload.length);
  const actualHash = sha256(downloaded);
  assert.equal(actualHash, expectedHash);
  console.log(`PASS sha256=${actualHash}`);

  console.log('\n[6/6] DeleteObject cleanup');
  await storage.deleteObject(key);
  const deletedHeadUrl = storage.createDownload({ key, method: 'HEAD', expiresSeconds: 120 });
  const deletedHead = await fetch(deletedHeadUrl, { method: 'HEAD', redirect: 'manual' });
  assert.equal(deletedHead.status, 404);
  console.log('PASS object no longer exists');

  console.log('\nRESULT: PASS — live QStorage multipart upload/download/delete works.');
} catch (error) {
  console.error('\nRESULT: FAIL');
  console.error(error?.stack || error);

  if (providerUploadId && !completed) {
    try {
      console.error('\nAttempting AbortMultipartUpload cleanup...');
      await storage.abortMultipartUpload({ key, providerUploadId });
      console.error('Abort cleanup: PASS');
    } catch (abortError) {
      console.error('Abort cleanup: FAIL');
      console.error(abortError?.stack || abortError);
    }
  }

  process.exitCode = 1;
}
