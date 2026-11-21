import http from 'node:http';
import {stat} from 'node:fs/promises';
import {resolve} from 'node:path';

const PUBLIC_ROOT = '/var/www/genycaster.xyz/public';
const ACCOUNT_URL = 'http://127.0.0.1:8797/api/v1/connections/providers';
const SNAPCHAIN_URL = 'http://127.0.0.1:8787/v1/signersByFid?fid=1';
const HYPERSNAP_URL = 'https://haatz.quilibrium.com/v1/info';
const MAX_JSON_BYTES = 64 * 1024;
const PROBE_TIMEOUT_MS = 4_000;
const CACHE_TTL_MS = 30_000;

async function readJson(url, fetchImpl, timeoutMs) {
  const response = await fetchImpl(url, {
    method: 'GET',
    headers: {Accept: 'application/json'},
    redirect: 'error',
    signal: AbortSignal.timeout(timeoutMs),
  });
  if (!response.ok || !/application\/json/i.test(response.headers.get('content-type') || '')) {
    return null;
  }
  if (Number(response.headers.get('content-length') || 0) > MAX_JSON_BYTES || !response.body) {
    return null;
  }
  const reader = response.body.getReader();
  const chunks = [];
  let size = 0;
  try {
    while (true) {
      const {done, value} = await reader.read();
      if (done) break;
      size += value.byteLength;
      if (size > MAX_JSON_BYTES) {
        await reader.cancel();
        return null;
      }
      chunks.push(Buffer.from(value));
    }
    return JSON.parse(Buffer.concat(chunks).toString('utf8'));
  } catch {
    return null;
  } finally {
    reader.releaseLock();
  }
}

async function asStatus(probe) {
  try {
    return (await probe()) ? 'operational' : 'unavailable';
  } catch {
    return 'unavailable';
  }
}

// Checks real read-only protocol requests rather than trusting the gateways' own 200s.
// The only disk check is the currently served WASM deployment, never a synthetic 200.
export async function collectChecks({
  webRoot = PUBLIC_ROOT,
  fetchImpl = globalThis.fetch,
  timeoutMs = PROBE_TIMEOUT_MS,
} = {}) {
  const [web, backend, snapchain, hypersnap] = await Promise.all([
    asStatus(async () => {
      const [html, js, wasm] = await Promise.all([
        stat(resolve(webRoot, 'genycaster.html')),
        stat(resolve(webRoot, 'genycaster.js')),
        stat(resolve(webRoot, 'genycaster.wasm')),
      ]);
      return [html, js, wasm].every(file => file.isFile() && file.size > 0);
    }),
    asStatus(async () => {
      const data = await readJson(ACCOUNT_URL, fetchImpl, timeoutMs);
      return Array.isArray(data?.providers) && data.providers.length > 0;
    }),
    asStatus(async () => {
      const data = await readJson(SNAPCHAIN_URL, fetchImpl, timeoutMs);
      return Array.isArray(data?.signers) && data.signers.length > 0;
    }),
    asStatus(async () => {
      const data = await readJson(HYPERSNAP_URL, fetchImpl, timeoutMs);
      return Number.isInteger(data?.numShards) && data.numShards > 0
        && Array.isArray(data.shardInfos) && data.shardInfos.length > 0;
    }),
  ]);
  return {web, backend, snapchain, hypersnap};
}

export function summarize(checks, timestamp = new Date().toISOString()) {
  const coreReady = checks.web === 'operational' && checks.backend === 'operational';
  const networkCount = Number(checks.snapchain === 'operational')
    + Number(checks.hypersnap === 'operational');
  const status = !coreReady || networkCount === 0
    ? 'unavailable'
    : networkCount === 2 ? 'operational' : 'degraded';
  return {
    code: status === 'unavailable' ? 503 : 200,
    body: {service: 'genycaster', status, checked_at: timestamp, checks},
  };
}

export function createHealthServer({
  collect = collectChecks,
  now = Date.now,
  ttlMs = CACHE_TTL_MS,
} = {}) {
  let cache = null;
  let pending = null;
  async function snapshot() {
    if (cache && now() < cache.expires) return cache.value;
    if (!pending) {
      pending = Promise.resolve().then(() => collect())
        .then(checks => summarize(checks, new Date(now()).toISOString()))
        .catch(() => summarize({
          web: 'unavailable', backend: 'unavailable',
          snapchain: 'unavailable', hypersnap: 'unavailable',
        }, new Date(now()).toISOString()))
        .then(value => {
          cache = {value, expires: now() + ttlMs};
          return value;
        }).finally(() => { pending = null; });
    }
    return pending;
  }
  return http.createServer(async (request, response) => {
    const pathname = new URL(request.url || '/', 'http://localhost').pathname;
    response.setHeader('Content-Type', 'application/json; charset=utf-8');
    response.setHeader('Cache-Control', 'no-store, max-age=0');
    response.setHeader('X-Content-Type-Options', 'nosniff');
    response.setHeader('Access-Control-Allow-Origin', '*');
    if (pathname !== '/api/health') {
      response.writeHead(404).end(JSON.stringify({error: 'not_found'}));
      return;
    }
    if (request.method !== 'GET' && request.method !== 'HEAD') {
      response.writeHead(405, {Allow: 'GET, HEAD'}).end(JSON.stringify({error: 'method_not_allowed'}));
      return;
    }
    const {code, body} = await snapshot();
    const output = JSON.stringify(body);
    response.setHeader('Content-Length', Buffer.byteLength(output));
    if (code === 503) response.setHeader('Retry-After', '30');
    response.writeHead(code);
    response.end(request.method === 'HEAD' ? undefined : output);
  });
}
