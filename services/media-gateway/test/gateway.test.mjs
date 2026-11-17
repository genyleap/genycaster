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
    castViewService: overrides.castViewService,
    browserReactionService: overrides.browserReactionService,
    limiter: overrides.limiter,
    uploadLimiter: overrides.uploadLimiter,
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

test('authenticated uploads do not spend public image browsing quota and remain individually rate limited', async () => {
  const body = Buffer.from(JSON.stringify({
    fileName:'photo.jpg', contentType:'image/jpeg', sizeBytes:512,
  }));
  const request = async origin => fetch(origin + '/v1/uploads', {
    method:'POST',
    headers:proofHeaders('POST','/v1/uploads',body,'independent_upload_nonce'),
    body,
  });
  await withGateway(async ({origin}) => {
    const reply = await request(origin);
    assert.equal(reply.status,201);
  }, {
    limiter:{check(){return {allowed:false,remaining:0};}},
    uploadLimiter:{check(){return {allowed:true,remaining:12};}},
  });
  await withGateway(async ({origin}) => {
    const reply = await request(origin);
    assert.equal(reply.status,429);
    assert.equal(reply.headers.get('retry-after'),'60');
  }, {
    limiter:{check(){return {allowed:true,remaining:12};}},
    uploadLimiter:{check(){return {allowed:false,remaining:0};}},
  });
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
      method: 'OPTIONS', headers: { origin: 'https://genycaster.xyz' },
    });
    assert.equal(preflight.status, 204);
    assert.equal(preflight.headers.get('access-control-allow-origin'), 'https://genycaster.xyz');
    const rejected = await fetch(origin + '/v1/browser/tokens', {
      method: 'OPTIONS', headers: { origin: 'https://evil.test' },
    });
    assert.equal(rejected.status, 403);
    const url = origin + '/v1/browser/tokens?network=base&address=' + tokenAddress;
    const first = await fetch(url, { headers: { origin: 'https://genycaster.xyz' } });
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


test('browser OpenProof provider catalog is CORS-safe, bounded and excludes provider URL injection', async () => {
  const called = [];
  await withGateway(async({origin}) => {
    const path = '/v1/browser/openproof/providers';
    const preflight = await fetch(origin+path,{method:'OPTIONS',headers:{
      origin:'https://genycaster.xyz',
      'access-control-request-method':'GET'
    }});
    assert.equal(preflight.status,204);
    assert.equal(preflight.headers.get('access-control-allow-origin'),'*');
    const unsupported = await fetch(origin+path,{method:'POST'});
    assert.equal(unsupported.status,405);
    const response = await fetch(origin+path+'?url=https://attacker.invalid');
    assert.equal(response.status,200);
    assert.equal(response.headers.get('access-control-allow-origin'),'*');
    assert.equal(response.headers.get('cache-control'),'public, max-age=180');
    assert.deepEqual(await response.json(),{
      providers:['x','github'],
      challenge_providers:['farcaster','ethereum-wallet','local','passkey']
    });
    assert.equal(called.length,1);
  },{fetchImpl:async(url,options)=>{
    assert.equal(url,'https://openproof.genyleap.com/auth/providers');
    assert.equal(options.method,'GET');
    assert.equal(options.redirect,'error');
    assert.equal(options.headers.authorization,undefined);
    called.push(url);
    return new Response(JSON.stringify({
      providers:['x','github','x','invalid!'],
      challenge_providers:['farcaster','ethereum-wallet','local','passkey'],
      upstreamSecrets:'never exposed'
    }),{status:200,headers:{'content-type':'application/json'}});
  }});
});

test('browser OpenProof provider catalog rejects malformed upstream data',async()=>{
  await withGateway(async({origin})=>{
    const response=await fetch(origin+'/v1/browser/openproof/providers');
    assert.equal(response.status,502);
  },{fetchImpl:async()=>new Response(JSON.stringify({providers:['x']}),{
    status:200,headers:{'content-type':'application/json'}
  })});
});

test('OpenProof issuer configuration never permits cleartext token forwarding', () => {
  assert.throws(() => loadConfig({
    GENYLEAP_MEDIA_TICKET_SECRET:'0123456789abcdef0123456789abcdef',
    GENYCASTER_OPENPROOF_ISSUER:'http://identity.example.test'
  }), /must use HTTPS/);
});

test('browser OpenProof account handoff binds provider and local return path server-side', async () => {
  const upstreamCalls = [];
  await withGateway(async ({ origin }) => {
    const base = '/v1/browser/openproof/connections/handoff';
    const preflight = await fetch(origin + base, {method:'OPTIONS', headers: {
      origin:'https://genycaster.xyz', 'access-control-request-method':'POST',
      'access-control-request-headers':'authorization,content-type'
    }});
    assert.equal(preflight.status,204);
    assert.match(preflight.headers.get('access-control-allow-headers')||'',/content-type/);
    const noBearer = await fetch(origin + base, {method:'POST', headers:{
      'content-type':'application/json'
    },body:JSON.stringify({provider:'x'})});
    assert.equal(noBearer.status,401);
    const badProvider = await fetch(origin + base, {method:'POST', headers:{
      authorization:'Bearer valid-token','content-type':'application/json'
    },body:JSON.stringify({provider:'local'})});
    assert.equal(badProvider.status,400);
    const injectedReturn = await fetch(origin + base, {method:'POST', headers:{
      authorization:'Bearer valid-token','content-type':'application/json'
    },body:JSON.stringify({provider:'x',return_to:'https://evil.example'})});
    assert.equal(injectedReturn.status,400);
    const accepted = await fetch(origin + base, {method:'POST', headers:{
      authorization:'Bearer valid-token','content-type':'application/json'
    },body:JSON.stringify({provider:'x'})});
    assert.equal(accepted.status,200);
    assert.deepEqual(await accepted.json(),{
      handoff_url:'https://genyleap.com/openproof/account/connections/handoff?ticket=one-time',
      expires_at:'2026-10-21T12:02:00Z',
    });
    assert.equal(upstreamCalls.length,1);
    assert.deepEqual(upstreamCalls[0].payload,{
      provider:'x',return_to:'/genycaster-connect-complete.html'
    });
    assert.equal(upstreamCalls[0].authorization,'Bearer valid-token');
  }, {fetchImpl: async(url,options)=>{
    assert.equal(url,'https://openproof.genyleap.com/account/connections/handoff');
    assert.equal(options.method,'POST');
    assert.equal(options.redirect,'error');
    upstreamCalls.push({payload:JSON.parse(options.body),authorization:options.headers.authorization});
    return new Response(JSON.stringify({
      handoff_url:'https://openproof.genyleap.com/account/connections/handoff?ticket=one-time',
      expires_at:'2026-10-21T12:02:00Z'
    }),{status:201,headers:{'content-type':'application/json'}});
  }});
});

test('browser connection redirect honors the OIDC callback host and rejects unsafe browser base', () => {
  const env = {GENYLEAP_MEDIA_TICKET_SECRET:'0123456789abcdef0123456789abcdef'};
  const conf = loadConfig(env);
  assert.equal(conf.connectionBrowserBase.href,'https://genyleap.com/openproof');
  const callback = new URL('https://genyleap.com/openproof/auth/federated/callback');
  const handoff = new URL('account/connections/handoff',conf.connectionBrowserBase.href+'/');
  assert.equal(handoff.host,callback.host,
    'OAuth state cookies must be minted on the registered callback host');
  assert.throws(() => loadConfig({...env,
    GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE:'http://genyleap.com/openproof'}),
    /must be HTTPS/);
  assert.throws(() => loadConfig({...env,
    GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE:'https://genyleap.com/user-controlled'}),
    /must be HTTPS \/openproof/);
});

test('browser OpenProof disconnect requires exact provider and subject, refuses last-method removal',async()=>{
  const calls = [];
  await withGateway(async({origin})=>{
    const base='/v1/browser/openproof/connections/disconnect';
    const bad = await fetch(origin+base,{method:'POST',headers:{
      authorization:'Bearer verified','content-type':'application/json'},
      body:JSON.stringify({provider:'github',subject:'',account:'other'})});
    assert.equal(bad.status,400);
    const refused = await fetch(origin+base,{method:'POST',headers:{
      authorization:'Bearer verified','content-type':'application/json'},
      body:JSON.stringify({provider:'github',subject:'login-1'})});
    assert.equal(refused.status,412);
    assert.deepEqual(await refused.json(),{error:'last_sign_in_method'});
    assert.equal(calls.length,1);
  },{fetchImpl: async(url,opts)=>{
    assert.equal(url,'https://openproof.genyleap.com/account/connections/disconnect');
    assert.equal(opts.method,'POST');
    calls.push(JSON.parse(opts.body));
    return new Response(JSON.stringify({error:'LAST_METHOD'}),{
      status:412,headers:{'content-type':'application/json'}});
  }});
});

test('browser OpenProof refuses forged off-issuer handoff URLs',async()=>{
  await withGateway(async({origin})=>{
    const response=await fetch(origin+'/v1/browser/openproof/connections/handoff',{
      method:'POST',headers:{authorization:'Bearer verified','content-type':'application/json'},
      body:JSON.stringify({provider:'x'})
    });
    assert.equal(response.status,502);
    assert.deepEqual(await response.json(),{error:'openproof_invalid_handoff'});
  },{fetchImpl:async()=>new Response(JSON.stringify({
    handoff_url:'https://attacker.example/connect?ticket=secret',
    expires_at:'2026-10-21T12:02:00Z'
  }),{status:201,headers:{'content-type':'application/json'}})});
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

test('browser cast view endpoint enforces bounded batch POST and trusted CORS',async()=>{
  const hash='0x9aea66ddab7a51563cc54e881931a5acb9e2bf16';
  const requests=[];
  const castViewService={resolve:async casts=>{
    if(!Array.isArray(casts))return {status:400,error:'invalid_cast_view_batch'};
    requests.push(casts);
    return {status:200,data:{counts:{[hash]:1300}}};
  }};
  await withGateway(async ({origin})=>{
    const url=origin+'/v1/browser/cast-views';
    const preflight=await fetch(url,{method:'OPTIONS',headers:{
      origin:'https://genycaster.xyz',
      'access-control-request-method':'POST',
      'access-control-request-headers':'content-type',
    }});
    assert.equal(preflight.status,204);
    assert.equal(preflight.headers.get('access-control-allow-origin'),'https://genycaster.xyz');
    const denied=await fetch(url,{method:'OPTIONS',headers:{
      origin:'https://untrusted.example',
      'access-control-request-method':'POST',
    }});
    assert.equal(denied.status,403);
    const cast={castId:hash,authorFid:321969,text:'Remember when the early Farcaster client supported'};
    const result=await fetch(url,{method:'POST',headers:{
      origin:'https://genycaster.xyz',
      'content-type':'application/json',
    },body:JSON.stringify({casts:[cast]})});
    assert.equal(result.status,200);
    assert.equal(result.headers.get('access-control-allow-origin'),'https://genycaster.xyz');
    assert.deepEqual(await result.json(),{counts:{[hash]:1300}});
    assert.deepEqual(requests[0],[cast]);
    const wrongMethod=await fetch(url,{method:'GET'});
    assert.equal(wrongMethod.status,405);
    const invalid=await fetch(url,{method:'POST',headers:{'content-type':'application/json'},body:'not-json'});
    assert.equal(invalid.status,400);
    const excessive=await fetch(url,{method:'POST',headers:{'content-type':'application/json'},
      body:JSON.stringify({casts:Array(300).fill(cast)})});
    assert.equal(excessive.status,413);
  },{castViewService});
});

test('read-only browser cast reactions endpoint enforces CORS, rate, and the batch contract',async()=>{
  const hash='0x9aea66ddab7a51563cc54e881931a5acb9e2bf16';
  const calls=[];
  const browserReactionService={resolve:async document=>{
    calls.push(document);
    if(!document || !Array.isArray(document.casts))return {status:400,error:'invalid_reaction_batch'};
    return {status:200,data:{reactions:{[hash]:{liked:true,recasted:true}}}};
  }};
  await withGateway(async({origin})=>{
    const url=origin+'/v1/browser/cast-reactions';
    const options=await fetch(url,{method:'OPTIONS',headers:{
      origin:'https://genycaster.xyz',
      'access-control-request-method':'POST',
      'access-control-request-headers':'content-type',
    }});
    assert.equal(options.status,204);
    assert.equal(options.headers.get('access-control-allow-origin'),'https://genycaster.xyz');
    const rejected=await fetch(url,{method:'OPTIONS',headers:{origin:'https://unknown.example'}});
    assert.equal(rejected.status,403);
    const body={viewerFid:234616,casts:[hash]};
    const response=await fetch(url,{method:'POST',headers:{
      origin:'https://genycaster.xyz','content-type':'application/json'
    },body:JSON.stringify(body)});
    assert.equal(response.status,200);
    assert.deepEqual(await response.json(),{reactions:{[hash]:{liked:true,recasted:true}}});
    assert.deepEqual(calls[0],body);
    assert.equal((await fetch(url,{method:'GET'})).status,405);
    assert.equal((await fetch(url,{method:'POST',headers:{'content-type':'application/json'},body:'not-json'})).status,400);
    assert.equal((await fetch(url,{method:'POST',headers:{'content-type':'application/json'},body:'X'.repeat(3000)})).status,413);
  },{browserReactionService});
});

test('direct Farcaster phrase-recovery broker endpoints pass only fixed proofs, no phrase',async()=>{
  const calls=[];
  const fake={
    async startDirect(fid,address){
      calls.push(['start',fid,address]);
      return {ticket:'Q'.repeat(43),
        message:'genyleap.com Farcaster Auth farcaster://fid/123 '.padEnd(55,'.'),
        fid,expires_in:120};
    },
    async completeDirect(ticket,signature){
      calls.push(['complete',ticket,signature]);
      return {authenticated:true,subject:'canonical-123',
        fid:'123',issuer:'https://openproof.genyleap.com'};
    }
  };
  await withGateway(async({origin})=>{
    const prefix='/v1/browser/farcaster/recovery/';
    const opts={method:'POST',headers:{'content-type':'application/json',
      origin:'https://genycaster.xyz'}};
    const preflight=await fetch(origin+prefix+'start',{method:'OPTIONS',
      headers:{origin:'https://genycaster.xyz'}});
    assert.equal(preflight.status,204);
    assert.equal(preflight.headers.get('access-control-allow-origin'),'*');
    const attack=await fetch(origin+prefix+'start',{...opts,body:JSON.stringify({
      fid:'123',address:'0x'+'11'.repeat(20),mnemonic:'secret',
    })});
    assert.equal(attack.status,400);
    const started=await fetch(origin+prefix+'start',{...opts,body:JSON.stringify({
      fid:'123',address:'0x'+'11'.repeat(20)
    })});
    assert.equal(started.status,200);
    assert.equal((await started.json()).fid,'123');
    const complete=await fetch(origin+prefix+'complete',{...opts,body:JSON.stringify({
      ticket:'Q'.repeat(43),signature:'0x'+'ab'.repeat(64)+'1b'
    })});
    assert.equal(complete.status,200);
    assert.deepEqual(await complete.json(),{authenticated:true,subject:'canonical-123',
      fid:'123',issuer:'https://openproof.genyleap.com'});
  },{farcasterBroker:fake});
  assert.deepEqual(calls,[['start','123','0x'+'11'.repeat(20)],
    ['complete','Q'.repeat(43),'0x'+'ab'.repeat(64)+'1b']]);
});

test('temporary recovered OpenProof account capability reuses server cookie without OAuth signup',async()=>{
  const secret='Bearer recovery.'+'R'.repeat(43);
  const seen=[];
  const fakeBroker={
    authorizeRecoveryAccount:token=>token===secret
      ?{fid:'123',subject:'canonical-123',cookie:'__Host-openproof-session=hidden-cookie'}:null
  };
  await withGateway(async({origin})=>{
    const route='/v1/browser/openproof/connections';
    const r=await fetch(origin+route,{headers:{authorization:secret}});
    assert.equal(r.status,200);
    assert.deepEqual(await r.json(),{connections:[{provider:'farcaster',fid:'123'},
      {provider:'x',subject:'x-123'}]});
    const handoff=await fetch(origin+route+'/handoff',{
      method:'POST',headers:{authorization:secret,'content-type':'application/json'},
      body:JSON.stringify({provider:'x'})});
    assert.equal(handoff.status,200);
    assert.equal((await handoff.json()).handoff_url,
      'https://genyleap.com/openproof/account/connections/handoff?ticket=fresh');
    const rejected=await fetch(origin+route,{headers:{
      authorization:'Bearer recovery.'+'Z'.repeat(43)}});
    assert.equal(rejected.status,401);
    assert.equal(seen.length,2,'unauthorized tickets must not contact OpenProof');
  },{farcasterBroker:fakeBroker,fetchImpl:async(url,opts)=>{
    seen.push({url,opts});
    assert.equal(opts.headers.cookie,'__Host-openproof-session=hidden-cookie');
    assert.equal(opts.headers.authorization,undefined);
    if(url.endsWith('/account/connections'))
      return new Response(JSON.stringify({connections:[
        {provider:'farcaster',fid:'123'},{provider:'x',subject:'x-123'}
      ]}),{status:200,headers:{'content-type':'application/json'}});
    if(url.endsWith('/account/connections/handoff'))
      return new Response(JSON.stringify({handoff_url:
        'https://openproof.genyleap.com/account/connections/handoff?ticket=fresh',
        expires_at:'2026-10-21T12:00:00Z'}),
        {status:201,headers:{'content-type':'application/json'}});
    throw new Error('unexpected upstream '+url);
  }});
});

test('native HyperSnap media uploader verifies signer only at its native node',async()=>{
  const nativeFid=4294967296;
  const lookedUp=[];
  const body=Buffer.from(JSON.stringify({
    fileName:'native.jpg',contentType:'image/jpeg',sizeBytes:512,
  }));
  await withGateway(async({origin})=>{
    const response=await fetch(origin+'/v1/uploads',{
      method:'POST',
      headers:proofHeaders('POST','/v1/uploads',body,'native_upload_nonce_000001',
        'application/json',nativeFid),
      body,
    });
    assert.equal(response.status,201);
    assert.equal(lookedUp.length,1);
    assert.equal(lookedUp[0].origin,'https://haatz.quilibrium.com');
    assert.equal(lookedUp[0].searchParams.get('fid'),String(nativeFid));
  },{fetchImpl:async url=>{
    const selected=new URL(url);
    lookedUp.push(selected);
    if(selected.origin!=='https://haatz.quilibrium.com')
      throw new Error('native account attempted a Farcaster signer lookup');
    return new Response(JSON.stringify({signers:[{
      key:signerKey,fid:String(nativeFid),keyType:1,
      source:'SIGNER_SOURCE_OFFCHAIN',
      scopes:['MESSAGE_TYPE_CAST_ADD'],
      expiresAt:Math.floor(nowMs/1000)-1609459200+600,
    }],nextPageToken:''}),{status:200,
      headers:{'content-type':'application/json'}});
  }});
});

test('native media upload fails closed when native registrar has no matching signer',async()=>{
  const nativeFid=4294967296;
  const body=Buffer.from(JSON.stringify({
    fileName:'native.jpg',contentType:'image/jpeg',sizeBytes:512,
  }));
  await withGateway(async({origin})=>{
    const response=await fetch(origin+'/v1/uploads',{
      method:'POST',body,
      headers:proofHeaders('POST','/v1/uploads',body,'native_upload_nonce_000002',
        'application/json',nativeFid),
    });
    assert.equal(response.status,403);
  },{fetchImpl:async url=>{
    assert.equal(new URL(url).origin,'https://haatz.quilibrium.com');
    return new Response(JSON.stringify({signers:[],nextPageToken:''}),{
      status:200,headers:{'content-type':'application/json'}});
  }});
});
