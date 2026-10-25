import http from 'node:http';
import {createCloudHandler} from './cloud_state.mjs';

const MAX_RESPONSE_BYTES = 16 * 1024;
const DISCOVERY_TTL_MS = 5 * 60_000;
const PROVIDER_ID = /^[a-z][a-z0-9-]{0,39}$/;

export function normalizeCatalog(raw) {
  if (!raw || !Array.isArray(raw.providers) || !Array.isArray(raw.challenge_providers))
    throw new Error('invalid-provider-catalog');
  const values = [...raw.providers, ...raw.challenge_providers]
    .filter(value => typeof value === 'string' && PROVIDER_ID.test(value))
    .filter(value => value !== 'local' && value !== 'passkey');
  return {providers: [...new Set(values)].sort()};
}

export function createAccountApi({
  catalogUrl, fetchImpl = globalThis.fetch, now = Date.now,
  store = null, issuer = null,
} = {}) {
  const endpoint = new URL(catalogUrl);
  if (endpoint.protocol !== 'https:' || endpoint.username || endpoint.password
      || endpoint.hash || endpoint.port || endpoint.search)
    throw new Error('Account API requires a credential-free HTTPS catalog URL');

  const cloud = store && issuer ? createCloudHandler({store,issuer,fetchImpl}) : null;
  let cached = null;
  let pending = null;
  async function catalog() {
    if (cached && cached.until > now()) return cached.data;
    if (pending) return pending;
    pending = (async () => {
      const response = await fetchImpl(endpoint, {
        method:'GET', headers:{Accept:'application/json'},
        redirect:'error', signal:AbortSignal.timeout(5_000)
      });
      if (!response.ok || Number(response.headers.get('content-length') || 0) > MAX_RESPONSE_BYTES)
        throw new Error('catalog-fetch-failed');
      if (!response.body) throw new Error('missing-catalog-body');
      const chunks = [];
      let size = 0;
      for await (const chunk of response.body) {
        size += chunk.byteLength;
        if (size > MAX_RESPONSE_BYTES) {
          throw new Error('catalog-too-large');
        }
        chunks.push(chunk);
      }
      const parsed = JSON.parse(new TextDecoder().decode(
        Buffer.concat(chunks.map(chunk => Buffer.from(chunk)))));
      const value = normalizeCatalog(parsed);
      cached = {data:value, until:now() + DISCOVERY_TTL_MS};
      return value;
    })().finally(() => { pending = null; });
    return pending;
  }
  return http.createServer(async (req, res) => {
    res.setHeader('Cache-Control','no-store');
    res.setHeader('X-Content-Type-Options','nosniff');
    res.setHeader('Content-Type','application/json; charset=utf-8');
    const path = new URL(req.url, 'http://localhost').pathname;
    if (path.startsWith('/api/v1/cloud/')) {
      if (!cloud) {
        res.writeHead(503).end(JSON.stringify({error:'cloud_not_configured'}));
        return;
      }
      await cloud(req,res,path);
      return;
    }
    if (req.method !== 'GET') {
      res.writeHead(405,{'Allow':'GET'}).end(JSON.stringify({error:'method_not_allowed'}));
      return;
    }
    if (path === '/healthz') {
      res.writeHead(200).end(JSON.stringify({ready:true}));
      return;
    }
    if (path !== '/api/v1/connections/providers') {
      res.writeHead(404).end(JSON.stringify({error:'not_found'}));
      return;
    }
    try {
      const data = await catalog();
      res.writeHead(200).end(JSON.stringify(data));
    } catch {
      // Never expose upstream responses, secrets or bearer headers.
      res.writeHead(503).end(JSON.stringify({error:'provider_catalog_unavailable'}));
    }
  });
}

if (process.argv[1] && import.meta.url === new URL('file://' + process.argv[1]).href) {
  const origin = process.env.GENYCASTER_OPENPROOF_PROVIDER_CATALOG_URL;
  if (!origin) throw new Error('GENYCASTER_OPENPROOF_PROVIDER_CATALOG_URL required');
  const address = process.env.GENYCASTER_API_BIND_ADDRESS || '127.0.0.1';
  const port = Number(process.env.GENYCASTER_API_PORT || 8797);
  if (!Number.isInteger(port) || port <= 0 || port > 65535)
    throw new Error('Invalid GENYCASTER_API_PORT');
  // Cloud storage is disabled unless an isolated Genycaster PostgreSQL database
  // has been provisioned and its restricted runtime role configured.
  const url=process.env.GENYCASTER_DATABASE_URL;
  const store=url ? await (await import('./pg_store.mjs')).createPgStore(url) : null;
  createAccountApi({catalogUrl: origin,store,
    issuer:process.env.GENYCASTER_OPENPROOF_ISSUER||'https://openproof.genyleap.com'})
    .listen(port,address);
}
