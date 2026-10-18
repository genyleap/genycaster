import assert from 'node:assert/strict';
import {
  generateKeyPairSync,
  sign,
} from 'node:crypto';
import test from 'node:test';

import {
  buildUploadChallenge,
  createMediaGateway,
  loadConfig,
} from '../gateway.mjs';

const nowMs = Date.parse('2026-08-30T12:00:00.000Z');
const fid = 4242;
const { publicKey, privateKey } = generateKeyPairSync('ed25519');
const publicDer = publicKey.export({ format: 'der', type: 'spki' });
const signerKey = publicDer.subarray(publicDer.length - 32).toString('hex');

function config() {
  return loadConfig({
    GENYLEAP_MEDIA_HOST: '127.0.0.1',
    GENYLEAP_MEDIA_PORT: '8790',
    GENYLEAP_MEDIA_PUBLIC_BASE_URL: 'https://media-farcaster.genyleap.com',
    GENYLEAP_MEDIA_SNAPCHAIN_BASE_URL: 'https://snapchain.genyleap.com',
    GENYLEAP_MEDIA_TICKET_SECRET: '0123456789abcdef0123456789abcdef',
    GENYLEAP_MEDIA_MULTIPART_PART_BYTES: String(5 * 1024 * 1024),
    GENYLEAP_MEDIA_MAX_IMAGE_BYTES: String(25 * 1024 * 1024),
    GENYLEAP_MEDIA_MAX_VIDEO_BYTES: String(512 * 1024 * 1024),
  });
}

function signerFetch(url) {
  const parsed = new URL(url);
  assert.equal(parsed.pathname, '/v1/signersByFid');
  const requestedFid = Number(parsed.searchParams.get('fid'));
  assert.ok(Number.isSafeInteger(requestedFid) && requestedFid > 0);
  return Promise.resolve(new Response(JSON.stringify({
    signers: [{
      key: signerKey,
      fid: requestedFid,
      source: 'SIGNER_SOURCE_OFFCHAIN',
      scopes: ['MESSAGE_TYPE_CAST_ADD'],
      expiresAt: Math.floor(nowMs / 1000) + 3600,
    }],
    nextPageToken: '',
  }), {
    status: 200,
    headers: { 'content-type': 'application/json' },
  }));
}

function proofHeaders(method, pathname, body, nonce, contentType = 'application/json', proofFid = fid) {
  const timestamp = Math.floor(nowMs / 1000);
  const challenge = buildUploadChallenge({
    method,
    pathname,
    fid: proofFid,
    signerKey,
    timestamp,
    nonce,
    body,
  });
  return {
    'content-type': contentType,
    'x-genycaster-fid': String(proofFid),
    'x-genycaster-signer-key': signerKey,
    'x-genycaster-timestamp': String(timestamp),
    'x-genycaster-nonce': nonce,
    'x-genycaster-signature': sign(null, Buffer.from(challenge), privateKey).toString('hex'),
  };
}

async function withGateway(run, overrides = {}) {
  const stored = new Map();
  let createCalls = 0;
  const storage = {
    async beginMultipartUpload({ key, contentType }) {
      createCalls += 1;
      stored.set(key, {
        contentType,
        parts: new Map(),
        completed: false,
        providerUploadId: `provider-${createCalls}`,
      });
      return { providerUploadId: `provider-${createCalls}` };
    },
    async uploadPart({ key, providerUploadId, partNumber, body }) {
      const value = stored.get(key);
      assert.ok(value);
      assert.equal(providerUploadId, value.providerUploadId);
      value.parts.set(partNumber, Buffer.from(body));
      return { etag: `"part-${partNumber}"` };
    },
    async completeMultipartUpload({ key, providerUploadId, parts }) {
      const value = stored.get(key);
      assert.ok(value);
      assert.equal(providerUploadId, value.providerUploadId);
      assert.deepEqual(parts, [...value.parts.keys()].map((partNumber) => ({
        partNumber,
        etag: `"part-${partNumber}"`,
      })));
      value.completed = true;
    },
    async abortMultipartUpload({ key, providerUploadId }) {
      const value = stored.get(key);
      assert.ok(value);
      assert.equal(providerUploadId, value.providerUploadId);
      stored.delete(key);
    },
    async deleteObject(key) {
      stored.delete(key);
    },
    async readImageObject(key, maximumBytes) {
      const value = stored.get(key);
      if (!value?.completed) {
        const error = new Error('Missing image');
        error.statusCode = 404;
        throw error;
      }
      const bytes = Buffer.concat([...value.parts.values()]);
      assert.ok(bytes.length <= maximumBytes);
      return bytes;
    },
    async headObject(key) {
      const value = stored.get(key);
      if (!value?.completed) return { sizeBytes: null, contentType: '' };
      const sizeBytes = [...value.parts.values()].reduce((sum, body) => sum + body.length, 0);
      return { sizeBytes, contentType: value.contentType, etag: '"complete"' };
    },
    createDownload({ key, method, expiresSeconds }) {
      assert.ok(['GET', 'HEAD'].includes(method));
      assert.equal(expiresSeconds, 900);
      return `https://private.storage.test/${key}?signed-read=1&method=${method}`;
    },
  };
  const server = createMediaGateway(config(), storage, {
    fetchImpl: overrides.fetchImpl ?? signerFetch,
    farcasterBroker: overrides.farcasterBroker,
    now: () => nowMs,
  });
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const address = server.address();
  const origin = `http://127.0.0.1:${address.port}`;
  try {
    await run({ origin, stored, createCalls: () => createCalls });
  } finally {
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
}

test('gateway configuration rejects malformed integer values and unsafe multipart sizes', () => {
  assert.throws(
    () => loadConfig({
      GENYLEAP_MEDIA_TICKET_SECRET: '0123456789abcdef0123456789abcdef',
      GENYLEAP_MEDIA_TICKET_LIFETIME_SECONDS: '900seconds',
    }),
    /must be a positive integer/,
  );
  assert.throws(
    () => loadConfig({
      GENYLEAP_MEDIA_TICKET_SECRET: '0123456789abcdef0123456789abcdef',
      GENYLEAP_MEDIA_MULTIPART_PART_BYTES: '1048576',
    }),
    /between 5 MiB and 64 MiB/,
  );
});

test('authorized Farcaster signer uploads a gateway-relayed multipart object and receives stable asset URL', async () => {
  await withGateway(async ({ origin, stored }) => {
    const payload = Buffer.alloc(3210, 0x5a);
    payload.set([0xff, 0xd8, 0xff], 0);
    const createDocument = {
      fileName: 'photo.jpg',
      contentType: 'image/jpeg',
      sizeBytes: payload.length,
    };
    const createBody = Buffer.from(JSON.stringify(createDocument));
    const created = await fetch(`${origin}/v1/uploads`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads', createBody, 'create_nonce_0000000001'),
      body: createBody,
    });
    assert.equal(created.status, 201);
    const ticket = await created.json();
    assert.equal(ticket.mode, 'gateway-multipart');
    assert.equal(ticket.partCount, 1);
    assert.equal(ticket.partSizeBytes, 5 * 1024 * 1024);
    assert.match(ticket.assetUrl, /^https:\/\/media-farcaster\.genyleap\.com\/m\/v1\//);
    assert.equal('uploadUrl' in ticket, false);
    assert.equal(stored.size, 1);

    const partPath = `/v1/uploads/${ticket.uploadId}/parts/1`;
    const uploadedPart = await fetch(`${origin}${partPath}`, {
      method: 'PUT',
      headers: proofHeaders('PUT', partPath, payload, 'part_nonce_000000000001', 'application/octet-stream'),
      body: payload,
    });
    assert.equal(uploadedPart.status, 200);
    const part = await uploadedPart.json();
    assert.deepEqual(part, { partNumber: 1, etag: '"part-1"', sizeBytes: payload.length });

    const completeDocument = {
      uploadId: ticket.uploadId,
      parts: [{ partNumber: 1, etag: part.etag }],
    };
    const completeBody = Buffer.from(JSON.stringify(completeDocument));
    const completed = await fetch(`${origin}/v1/uploads/complete`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads/complete', completeBody, 'complete_nonce_0000001'),
      body: completeBody,
    });
    assert.equal(completed.status, 200);
    const asset = await completed.json();
    assert.equal(asset.assetUrl, ticket.assetUrl);
    assert.equal(asset.contentType, 'image/jpeg');
    assert.equal(asset.fileName, 'photo.jpg');
    assert.equal(asset.sizeBytes, payload.length);
    assert.equal(typeof asset.managementToken, 'string');
    assert.ok(asset.managementToken.length > 32);

    const stable = await fetch(ticket.assetUrl.replace('https://media-farcaster.genyleap.com', origin), {
      redirect: 'manual',
    });
    assert.equal(stable.status, 200);
    assert.equal(stable.headers.get('content-type'), 'image/jpeg');
    assert.equal(stable.headers.get('content-length'), String(payload.length));
    assert.equal(stable.headers.get('location'), null);
    assert.deepEqual(Buffer.from(await stable.arrayBuffer()), payload);
    assert.equal(stable.headers.get('cache-control'), 'public, max-age=120');

    const deleteBody = Buffer.from(JSON.stringify({ managementToken: asset.managementToken }));
    const deleted = await fetch(`${origin}/v1/assets`, {
      method: 'DELETE',
      headers: proofHeaders('DELETE', '/v1/assets', deleteBody, 'delete_nonce_0000000001'),
      body: deleteBody,
    });
    assert.equal(deleted.status, 204);
    assert.equal(stored.size, 0);
  });
});

test('tampered managed-asset token cannot delete storage objects', async () => {
  await withGateway(async ({ origin, stored }) => {
    const payload = Buffer.from('managed-media');
    const createBody = Buffer.from(JSON.stringify({
      fileName: 'photo.jpg',
      contentType: 'image/jpeg',
      sizeBytes: payload.length,
    }));
    const created = await fetch(`${origin}/v1/uploads`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads', createBody, 'create_delete_nonce_001'),
      body: createBody,
    });
    const ticket = await created.json();
    const partPath = `/v1/uploads/${ticket.uploadId}/parts/1`;
    const uploaded = await fetch(`${origin}${partPath}`, {
      method: 'PUT',
      headers: proofHeaders('PUT', partPath, payload, 'part_delete_nonce_00001', 'application/octet-stream'),
      body: payload,
    });
    const part = await uploaded.json();
    const completeBody = Buffer.from(JSON.stringify({
      uploadId: ticket.uploadId,
      parts: [{ partNumber: 1, etag: part.etag }],
    }));
    const completed = await fetch(`${origin}/v1/uploads/complete`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads/complete', completeBody, 'complete_delete_nonce01'),
      body: completeBody,
    });
    const asset = await completed.json();
    const tamperedToken = `${asset.managementToken.slice(0, -1)}${asset.managementToken.endsWith('A') ? 'B' : 'A'}`;
    const deleteBody = Buffer.from(JSON.stringify({ managementToken: tamperedToken }));
    const deleted = await fetch(`${origin}/v1/assets`, {
      method: 'DELETE',
      headers: proofHeaders('DELETE', '/v1/assets', deleteBody, 'delete_tamper_nonce_001'),
      body: deleteBody,
    });
    assert.equal(deleted.status, 403);
    assert.equal(stored.size, 1);
  });
});


test('managed-asset token cannot be used by a different authenticated FID', async () => {
  await withGateway(async ({ origin, stored }) => {
    const payload = Buffer.from('owned-managed-media');
    const createBody = Buffer.from(JSON.stringify({
      fileName: 'owned.jpg',
      contentType: 'image/jpeg',
      sizeBytes: payload.length,
    }));
    const created = await fetch(`${origin}/v1/uploads`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads', createBody, 'create_owner_nonce_0001'),
      body: createBody,
    });
    const ticket = await created.json();
    const partPath = `/v1/uploads/${ticket.uploadId}/parts/1`;
    const uploaded = await fetch(`${origin}${partPath}`, {
      method: 'PUT',
      headers: proofHeaders('PUT', partPath, payload, 'part_owner_nonce_000001', 'application/octet-stream'),
      body: payload,
    });
    const part = await uploaded.json();
    const completeBody = Buffer.from(JSON.stringify({
      uploadId: ticket.uploadId,
      parts: [{ partNumber: 1, etag: part.etag }],
    }));
    const completed = await fetch(`${origin}/v1/uploads/complete`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads/complete', completeBody, 'complete_owner_nonce01'),
      body: completeBody,
    });
    const asset = await completed.json();

    const deleteBody = Buffer.from(JSON.stringify({ managementToken: asset.managementToken }));
    const deleted = await fetch(`${origin}/v1/assets`, {
      method: 'DELETE',
      headers: proofHeaders(
        'DELETE',
        '/v1/assets',
        deleteBody,
        'delete_wrong_owner_001',
        'application/json',
        fid + 1,
      ),
      body: deleteBody,
    });
    assert.equal(deleted.status, 403);
    assert.equal(stored.size, 1);
  });
});

test('multipart part size is enforced before provider upload', async () => {
  await withGateway(async ({ origin, stored }) => {
    const createBody = Buffer.from(JSON.stringify({
      fileName: 'photo.jpg',
      contentType: 'image/jpeg',
      sizeBytes: 10,
    }));
    const created = await fetch(`${origin}/v1/uploads`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads', createBody, 'create_size_nonce_00001'),
      body: createBody,
    });
    const ticket = await created.json();
    const shortBody = Buffer.from('short');
    const partPath = `/v1/uploads/${ticket.uploadId}/parts/1`;
    const part = await fetch(`${origin}${partPath}`, {
      method: 'PUT',
      headers: proofHeaders('PUT', partPath, shortBody, 'part_size_nonce_0000001', 'application/octet-stream'),
      body: shortBody,
    });
    assert.equal(part.status, 409);
    assert.equal([...stored.values()][0].parts.size, 0);
  });
});

test('client can abort an allocated multipart upload', async () => {
  await withGateway(async ({ origin, stored }) => {
    const createBody = Buffer.from(JSON.stringify({ fileName: 'photo.jpg', contentType: 'image/jpeg', sizeBytes: 3210 }));
    const created = await fetch(`${origin}/v1/uploads`, {
      method: 'POST',
      headers: proofHeaders('POST', '/v1/uploads', createBody, 'create_abort_nonce_0001'),
      body: createBody,
    });
    const ticket = await created.json();
    assert.equal(stored.size, 1);

    const abortBody = Buffer.from(JSON.stringify({ uploadId: ticket.uploadId }));
    const aborted = await fetch(`${origin}/v1/uploads`, {
      method: 'DELETE',
      headers: proofHeaders('DELETE', '/v1/uploads', abortBody, 'abort_nonce_00000000001'),
      body: abortBody,
    });
    assert.equal(aborted.status, 204);
    assert.equal(stored.size, 0);
  });
});

test('invalid proof is rejected before storage allocation', async () => {
  await withGateway(async ({ origin, stored, createCalls }) => {
    const document = { fileName: 'photo.jpg', contentType: 'image/jpeg', sizeBytes: 3210 };
    const body = Buffer.from(JSON.stringify(document));
    const headers = proofHeaders('POST', '/v1/uploads', body, 'invalid_nonce_00000001');
    headers['x-genycaster-signature'] = '00'.repeat(64);
    const response = await fetch(`${origin}/v1/uploads`, { method: 'POST', headers, body });
    assert.equal(response.status, 401);
    assert.equal(stored.size, 0);
    assert.equal(createCalls(), 0);
  });
});

test('malformed public media path is a bounded 404 instead of crashing the gateway', async () => {
  await withGateway(async ({ origin }) => {
    const response = await fetch(`${origin}/m/%E0%A4%A`, { redirect: 'manual' });
    assert.equal(response.status, 404);
  });
});

test('permanent image GET never redirects and rejects invalid or deleted image bytes', async () => {
  await withGateway(async ({ origin, stored }) => {
    const key = 'v1/2026/10/12345678-1234-1234-1234-123456789abc.jpg';
    const url = `${origin}/m/${key}`;
    const unavailable = await fetch(url, { redirect: 'manual' });
    assert.equal(unavailable.status, 404);
    stored.set(key, {
      completed: true,
      parts: new Map([[1, Buffer.from('<html>not an image</html>')]]),
    });
    const invalid = await fetch(url, { redirect: 'manual' });
    assert.equal(invalid.status, 415);
    assert.equal(invalid.headers.get('location'), null);
    stored.delete(key);
    assert.equal((await fetch(url, { redirect: 'manual' })).status, 404);
  });
});

test('browser token insight route enforces public token identity and first-party CORS', async () => {
  const tokenAddress = '0x' + 'a'.repeat(40);
  await withGateway(async ({ origin }) => {
    const invalid = await fetch(origin + '/v1/browser/tokens?network=base&address=localhost');
    assert.equal(invalid.status, 400);
    const preflight = await fetch(origin + '/v1/browser/tokens', {
      method: 'OPTIONS', headers: { origin: 'https://caster.genyleap.com' },
    });
    assert.equal(preflight.status, 204);
    assert.equal(preflight.headers.get('access-control-allow-origin'), 'https://caster.genyleap.com');
    const rejected = await fetch(origin + '/v1/browser/tokens', {
      method: 'OPTIONS', headers: { origin: 'https://evil.test' },
    });
    assert.equal(rejected.status, 403);
    const url = origin + '/v1/browser/tokens?network=base&address=' + tokenAddress;
    const first = await fetch(url, { headers: { origin: 'https://caster.genyleap.com' } });
    assert.equal(first.status, 200);
    const snapshot = await first.json();
    assert.equal(snapshot.network, 'base');
    assert.equal(snapshot.address, tokenAddress);
    assert.equal(snapshot.name, 'Test Token');
    const cached = await fetch(url);
    assert.equal(cached.status, 200);
  }, {
    fetchImpl: async (url) => new Response(JSON.stringify(
      String(url).includes('/pools?page=1') ? { data: [] }
      : { data: { attributes: { name: 'Test Token', symbol: 'TEST', price_usd: '1.25' } } },
    ), { headers: { 'content-type': 'application/json' } }),
  });
});

test('authorization proof nonce is single-use', async () => {
  await withGateway(async ({ origin, stored }) => {
    const document = { fileName: 'photo.jpg', contentType: 'image/jpeg', sizeBytes: 3210 };
    const body = Buffer.from(JSON.stringify(document));
    const headers = proofHeaders('POST', '/v1/uploads', body, 'replay_nonce_0000000001');
    const first = await fetch(`${origin}/v1/uploads`, { method: 'POST', headers, body });
    assert.equal(first.status, 201);
    assert.equal(stored.size, 1);

    const second = await fetch(`${origin}/v1/uploads`, { method: 'POST', headers, body });
    assert.equal(second.status, 409);
    assert.equal(stored.size, 1);
  });
});

test('stable media HEAD is served by gateway metadata verification instead of QStorage HEAD', async () => {
  await withGateway(async ({ origin }) => {
    const key = 'v1/2026/08/12345678-1234-1234-1234-123456789abc.jpg';
    const response = await fetch(`${origin}/m/${key}`, { method: 'HEAD', redirect: 'manual' });
    assert.equal(response.status, 200);
    assert.equal(response.headers.get('cache-control'), 'public, max-age=300');
    assert.equal(response.headers.get('x-content-type-options'), 'nosniff');
  });
});

test('browser media endpoint permits public CORS but never proxies arbitrary hosts', async () => {
  await withGateway(async ({ origin }) => {
    const options = await fetch(`${origin}/v1/browser/images`, {
      method: 'OPTIONS',
      headers: { origin: 'http://127.0.0.1:30000' },
    });
    assert.equal(options.status, 204);
    assert.equal(options.headers.get('access-control-allow-origin'), '*');
    const rejected = await fetch(`${origin}/v1/browser/images?url=${encodeURIComponent('https://127.0.0.1/private')}`);
    assert.equal(rejected.status, 400);
    assert.equal(rejected.headers.get('access-control-allow-origin'), '*');
    assert.equal((await rejected.json()).error, 'image_source_not_allowed');
  });
});

test('browser OpenProof connections route requires bearer and permits explicit CORS', async () => {
  const calls = [];
  await withGateway(async ({ origin }) => {
    const path = '/v1/browser/openproof/connections';
    const preflight = await fetch(origin + path, {
      method: 'OPTIONS',
      headers: { origin: 'http://127.0.0.1:30000',
                 'access-control-request-headers': 'authorization',
                 'access-control-request-method': 'GET' },
    });
    assert.equal(preflight.status, 204);
    assert.equal(preflight.headers.get('access-control-allow-origin'), '*');
    assert.match(preflight.headers.get('access-control-allow-headers') || '', /authorization/);
    const empty = await fetch(origin + path);
    assert.equal(empty.status, 401);
    const bad = await fetch(origin + path, { headers: {
      authorization: 'Bearer token\\r\\nX-Unsafe: yes',
    } });
    assert.equal(bad.status, 401);
    assert.equal(calls.length, 0);
    const verified = await fetch(origin + path, { headers: { authorization: 'Bearer token-123' } });
    assert.equal(verified.status, 200);
    assert.equal(verified.headers.get('cache-control'), 'no-store');
    assert.deepEqual(await verified.json(), { connections: [{ provider: 'farcaster', fid: '42' }] });
    assert.deepEqual(calls, ['token-123']);
  }, { fetchImpl: async (url, options) => {
    assert.equal(url, 'https://openproof.genyleap.com/account/connections');
    assert.equal(options.method, 'GET');
    assert.equal(options.redirect, 'error');
    assert.equal(options.headers.authorization.startsWith('Bearer '), true);
    calls.push(options.headers.authorization.slice(7));
    return new Response(JSON.stringify({
      connections: [{ provider: 'farcaster', fid: '42' }],
      unrelated: 'not transmitted',
    }), { status: 200, headers: { 'content-type': 'application/json' } });
  } });
});

test('browser Mini App catalogue uses only fixed public discovery routes', async () => {
  const calls = [];
  await withGateway(async ({ origin }) => {
    const path = '/v1/browser/miniapps';
    const preflight = await fetch(origin + path, {
      method: 'OPTIONS',
      headers: { origin: 'http://127.0.0.1:30000' },
    });
    assert.equal(preflight.status, 204);
    assert.equal(preflight.headers.get('access-control-allow-origin'), '*');
    const bad = await fetch(origin + path + '?kind=arbitrary&url=https://localhost');
    assert.equal(bad.status, 400);
    const catalogue = await fetch(origin + path + '?kind=catalog');
    assert.equal(catalogue.status, 200);
    assert.equal(catalogue.headers.get('access-control-allow-origin'), '*');
    assert.deepEqual(await catalogue.json(), { frames: [] });
    const search = await fetch(origin + path + '?kind=search&q=cast');
    assert.equal(search.status, 200);
    assert.equal(calls.length, 2);
    assert.match(calls[0], /api\.neynar\.com\/v2\/farcaster\/frame\/catalog\//);
    assert.match(calls[1], /api\.neynar\.com\/v2\/farcaster\/frame\/search\//);
    assert.match(calls[1], /q=cast/);
    assert.match(calls[0], /limit=20/);
  }, { fetchImpl: async (url, options) => {
    assert.equal(url.hostname, 'api.neynar.com');
    assert.equal(url.protocol, 'https:');
    assert.equal(options.method, 'GET');
    assert.equal(options.redirect, 'error');
    assert.equal(options.headers['user-agent'], 'Genycaster/0.4.0');
    calls.push(url.toString());
    return new Response(JSON.stringify({ frames: [] }), {
      status: 200, headers: { 'content-type': 'application/json' },
    });
  } });
});

test('Farcaster browser sign-in endpoints never put the secret ticket in a URL', async () => {
  const calls = [];
  const ticket = 'A'.repeat(43);
  await withGateway(async ({ origin }) => {
    const path = '/v1/browser/farcaster/';
    const preflight = await fetch(origin + path + 'poll', {
      method: 'OPTIONS',
      headers: { origin: 'http://127.0.0.1:30000',
        'access-control-request-method': 'POST',
        'access-control-request-headers': 'content-type' },
    });
    assert.equal(preflight.status, 204);
    assert.equal(preflight.headers.get('access-control-allow-origin'), '*');
    const rejected = await fetch(origin + path + 'poll');
    assert.equal(rejected.status, 405);
    const started = await fetch(origin + path + 'start', { method: 'POST' });
    assert.equal(started.status, 200);
    assert.equal((await started.json()).ticket, ticket);
    const polled = await fetch(origin + path + 'poll', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ ticket }),
    });
    assert.equal(polled.status, 200);
    assert.equal((await polled.json()).authenticated, true);
    assert.deepEqual(calls, ['start', 'poll:' + ticket, 'consume:' + ticket]);
  }, { farcasterBroker: {
    async start() { calls.push('start'); return { ticket, approval_url: 'https://warpcast.com' }; },
    async poll(value) { calls.push('poll:' + value); return { authenticated: true }; },
    consume(value) { calls.push('consume:' + value); },
  } });
});
