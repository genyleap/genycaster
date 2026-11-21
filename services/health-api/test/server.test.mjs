import assert from 'node:assert/strict';
import {mkdtemp, writeFile, rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import test from 'node:test';
import {collectChecks, createHealthServer, summarize} from '../server.mjs';

const healthy = Object.freeze({
  web: 'operational', backend: 'operational',
  snapchain: 'operational', hypersnap: 'operational',
});

test('aggregates operational, degraded, and unavailable without hiding failures', () => {
  assert.equal(summarize(healthy).code, 200);
  assert.equal(summarize(healthy).body.status, 'operational');
  assert.equal(summarize({...healthy, hypersnap: 'unavailable'}).body.status, 'degraded');
  assert.equal(summarize({...healthy, snapchain: 'unavailable'}).code, 200);
  assert.equal(summarize({...healthy, snapchain: 'unavailable', hypersnap: 'unavailable'}).code, 503);
  assert.equal(summarize({...healthy, backend: 'unavailable'}).code, 503);
  assert.equal(summarize({...healthy, web: 'unavailable'}).code, 503);
});

test('collectChecks validates JSON shapes and the deployed WASM files', async t => {
  const webRoot = await mkdtemp(join(tmpdir(), 'genycaster-health-'));
  t.after(() => rm(webRoot, {recursive: true, force: true}));
  for (const name of ['genycaster.html', 'genycaster.js', 'genycaster.wasm'])
    await writeFile(join(webRoot, name), 'nonempty');
  const stub = async url => {
    if (url.includes('/connections/providers')) return json({providers: ['farcaster']});
    if (url.includes('/signersByFid')) return json({signers: [{fid: 1}]});
    if (url.includes('/v1/info')) return json({numShards: 2, shardInfos: [{shardId: 1}]});
    throw new Error('unexpected_probe');
  };
  assert.deepEqual(await collectChecks({webRoot, fetchImpl: stub}), healthy);
  const badBackend = async url => url.includes('/connections/providers')
    ? json({providers: []}) : stub(url);
  assert.equal((await collectChecks({webRoot, fetchImpl: badBackend})).backend, 'unavailable');
  const badSnapchain = async url => url.includes('/signersByFid')
    ? new Response('upstream unavailable', {status: 503}) : stub(url);
  assert.equal((await collectChecks({webRoot, fetchImpl: badSnapchain})).snapchain, 'unavailable');
  const missingWasm = await mkdtemp(join(tmpdir(), 'genycaster-health-empty-'));
  t.after(() => rm(missingWasm, {recursive: true, force: true}));
  assert.equal((await collectChecks({webRoot: missingWasm, fetchImpl: stub})).web, 'unavailable');
});

test('HTTP method, cache, HEAD and unavailable status are explicit', async t => {
  let clock = Date.UTC(2026, 10, 22);
  let calls = 0;
  let state = {...healthy};
  const server = createHealthServer({
    now: () => clock,
    ttlMs: 30_000,
    collect: async () => { calls++; return state; },
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(() => new Promise(resolve => server.close(resolve)));
  const base = 'http://127.0.0.1:' + server.address().port + '/api/health';

  const [a, b] = await Promise.all([fetch(base), fetch(base)]);
  assert.equal(a.status, 200);
  assert.equal(b.status, 200);
  assert.equal((await a.json()).status, 'operational');
  assert.equal(calls, 1);
  assert.match(a.headers.get('cache-control'), /no-store/);

  assert.equal((await fetch(base, {method: 'HEAD'})).status, 200);
  assert.equal((await fetch(base, {method: 'POST'})).status, 405);
  assert.equal((await fetch(base + '/extra')).status, 404);
  assert.equal(calls, 1);

  clock += 31_000;
  state = {...healthy, hypersnap: 'unavailable'};
  const degraded = await fetch(base);
  assert.equal(degraded.status, 200);
  assert.equal((await degraded.json()).status, 'degraded');
  assert.equal(calls, 2);

  clock += 31_000;
  state = {...healthy, snapchain: 'unavailable', hypersnap: 'unavailable'};
  const unavailable = await fetch(base);
  assert.equal(unavailable.status, 503);
  assert.equal(unavailable.headers.get('retry-after'), '30');
  assert.equal((await unavailable.json()).status, 'unavailable');
  assert.equal(calls, 3);
});

function json(value) {
  return new Response(JSON.stringify(value), {
    status: 200,
    headers: {'content-type': 'application/json'},
  });
}
