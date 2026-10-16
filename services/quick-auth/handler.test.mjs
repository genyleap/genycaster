import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { once } from 'node:events';
import { test } from 'node:test';
import { createHandler } from './handler.mjs';

const secret = 'a-valid-internal-secret-value-with-32-characters';
async function withServer(verifyToken, execute, ErrorTypes = {}) {
  const server = createServer(createHandler({
    verifyToken, domain: 'genycaster.com', internalSecret: secret,
    hubErrorClass: ErrorTypes.HubError, invalidTokenClass: ErrorTypes.InvalidTokenError,
  }));
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  try {
    const { port } = server.address();
    return await execute('http://127.0.0.1:' + port + '/v1/protocol-auth/verify');
  } finally {
    server.close();
    await once(server, 'close');
  }
}
const request = (url, token, customSecret = secret) => fetch(url, {
  method: 'POST', headers: {
    Authorization: 'Bearer ' + token,
    'X-Genycaster-Internal-Secret': customSecret,
  },
});
test('accepts verified protocol token but never creates an application session', async () => {
  await withServer(async ({ token, domain }) => {
    assert.equal(token, 'signed-token');
    assert.equal(domain, 'genycaster.com');
    return { sub: 42, method: 'authAddress', address: '0x' + 'a'.repeat(40) };
  }, async (url) => {
    const result = await request(url, 'signed-token');
    assert.equal(result.status, 200);
    assert.deepEqual(await result.json(), { fid: 42, method: 'authAddress', address: '0x' + 'a'.repeat(40) });
  });
});
test('legacy JWT method accepted without a wallet address', async () => {
  await withServer(async () => ({ sub: 100, method: 'quickAuthServer' }), async (url) => {
    const result = await request(url, 'legacy-jwt');
    assert.deepEqual(await result.json(), { fid: 100, method: 'quickAuthServer', address: null });
  });
});
test('rejects missing shared secret before calling the verifier', async () => {
  await withServer(() => { throw Error('must not verify'); }, async (url) => {
    assert.equal((await request(url, 'signed-token', 'wrong')).status, 403);
  });
});
test('rejects forged or incomplete FID proof', async () => {
  await withServer(async () => ({ sub: 0, method: 'verified', address: '0x' + 'a'.repeat(40) }), async (url) => {
    assert.equal((await request(url, 'signed-token')).status, 401);
  });
});
test('maps invalid tokens to 401 and hub failures to retryable 503', async () => {
  class InvalidTokenError extends Error {}
  class HubError extends Error {}
  await withServer(async () => { throw new InvalidTokenError(); }, async (url) => {
    assert.equal((await request(url, 'signed-token')).status, 401);
  }, { InvalidTokenError, HubError });
  await withServer(async () => { throw new HubError(); }, async (url) => {
    assert.equal((await request(url, 'signed-token')).status, 503);
  }, { InvalidTokenError, HubError });
});
