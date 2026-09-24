import assert from 'node:assert/strict';
import http from 'node:http';
import test from 'node:test';
import { once } from 'node:events';

import { createGateway, loadConfig } from '../gateway.mjs';

async function listen(server) {
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  return server.address().port;
}

async function close(server) {
  if (!server.listening) return;
  server.close();
  await once(server, 'close');
}

function config(upstreamPort, overrides = {}) {
  return {
    ...loadConfig({
      GENYLEAP_SNAPCHAIN_UPSTREAM_BASE_URL: `http://127.0.0.1:${upstreamPort}`,
      GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_HEADER: 'X-Upstream-Credential',
      GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_VALUE: 'server-only-secret',
      GENYLEAP_SNAPCHAIN_GET_RATE_LIMIT: '100',
      GENYLEAP_SNAPCHAIN_POST_RATE_LIMIT: '100',
    }),
    ...overrides,
  };
}

test('preserves indexed sequence query and injects only server-side credential', async () => {
  let observed;
  const upstream = http.createServer((request, response) => {
    observed = {
      url: request.url,
      credential: request.headers['x-upstream-credential'],
      spoofed: request.headers['x-api-key'],
    };
    response.writeHead(200, { 'content-type': 'application/json' });
    response.end('{"signers":[],"currentUserNonce":0}');
  });
  const upstreamPort = await listen(upstream);
  const gateway = createGateway(config(upstreamPort));
  const gatewayPort = await listen(gateway);

  try {
    const response = await fetch(
      `http://127.0.0.1:${gatewayPort}/v1/signersByFid?fid=42&requesterFids%5B0%5D=321969`,
      { headers: { 'x-api-key': 'client-spoof' } },
    );
    assert.equal(response.status, 200);
    assert.equal(observed.url, '/v1/signersByFid?fid=42&requesterFids%5B0%5D=321969');
    assert.equal(observed.credential, 'server-only-secret');
    assert.equal(observed.spoofed, undefined);
  } finally {
    await close(gateway);
    await close(upstream);
  }
});

test('forwards exact submitMessage bytes and content type', async () => {
  const expected = Buffer.from([0x01, 0x02, 0x03, 0xfe, 0xff]);
  let observedBody;
  let observedContentType;
  const upstream = http.createServer(async (request, response) => {
    const chunks = [];
    for await (const chunk of request) chunks.push(chunk);
    observedBody = Buffer.concat(chunks);
    observedContentType = request.headers['content-type'];
    response.writeHead(200, { 'content-type': 'application/json' });
    response.end('{"hash":"0x00"}');
  });
  const upstreamPort = await listen(upstream);
  const gateway = createGateway(config(upstreamPort));
  const gatewayPort = await listen(gateway);

  try {
    const response = await fetch(`http://127.0.0.1:${gatewayPort}/v1/submitMessage`, {
      method: 'POST',
      headers: { 'content-type': 'application/octet-stream' },
      body: expected,
    });
    assert.equal(response.status, 200);
    assert.deepEqual(observedBody, expected);
    assert.equal(observedContentType, 'application/octet-stream');
  } finally {
    await close(gateway);
    await close(upstream);
  }
});

test('rejects unowned routes and oversized bodies before upstream', async () => {
  let upstreamRequests = 0;
  const upstream = http.createServer((_request, response) => {
    upstreamRequests += 1;
    response.end('unexpected');
  });
  const upstreamPort = await listen(upstream);
  const gateway = createGateway(config(upstreamPort, { maxRequestBytes: 4 }));
  const gatewayPort = await listen(gateway);

  try {
    const unknown = await fetch(`http://127.0.0.1:${gatewayPort}/v1/anything`);
    assert.equal(unknown.status, 404);
    const oversized = await fetch(`http://127.0.0.1:${gatewayPort}/v1/submitMessage`, {
      method: 'POST',
      headers: { 'content-type': 'application/octet-stream' },
      body: Buffer.alloc(5),
    });
    assert.equal(oversized.status, 413);
    assert.equal(upstreamRequests, 0);
  } finally {
    await close(gateway);
    await close(upstream);
  }
});

test('rate limits by client identity before touching upstream', async () => {
  let upstreamRequests = 0;
  const upstream = http.createServer((_request, response) => {
    upstreamRequests += 1;
    response.writeHead(200, { 'content-type': 'application/json' });
    response.end('{}');
  });
  const upstreamPort = await listen(upstream);
  const gateway = createGateway(config(upstreamPort, { getRateLimit: 1 }));
  const gatewayPort = await listen(gateway);

  try {
    const first = await fetch(`http://127.0.0.1:${gatewayPort}/v1/castById?fid=1&hash=0x00`);
    const second = await fetch(`http://127.0.0.1:${gatewayPort}/v1/castById?fid=1&hash=0x00`);
    assert.equal(first.status, 200);
    assert.equal(second.status, 429);
    assert.ok(Number(second.headers.get('retry-after')) >= 1);
    assert.equal(upstreamRequests, 1);
  } finally {
    await close(gateway);
    await close(upstream);
  }
});
