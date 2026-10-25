import http from 'node:http';
import { defaultBrowserImageHosts, rasterMimeFromBytes, retrieveBrowserImage, validImageUrl } from './browser_image_proxy.mjs';
import { createBrowserFarcasterBroker } from './browser_farcaster_auth.mjs';
import { publicTokenSnapshot } from './browser_token_insights.mjs';
import { publicMiniAppPreview, validMiniAppTarget } from './browser_miniapp_preview.mjs';
import { publicLinkPreview } from './browser_link_preview.mjs';
import { createCastViewService } from './browser_cast_views.mjs';
import { createBrowserReactionService } from './browser_cast_reactions.mjs';
import {
  createHash,
  createHmac,
  createPublicKey,
  randomUUID,
  timingSafeEqual,
  verify,
} from 'node:crypto';

const mediaTypes = new Map([
  ['image/jpeg', { kind: 'image', extension: '.jpg' }],
  ['image/png', { kind: 'image', extension: '.png' }],
  ['image/webp', { kind: 'image', extension: '.webp' }],
  ['image/gif', { kind: 'image', extension: '.gif' }],
  ['image/avif', { kind: 'image', extension: '.avif' }],
  ['video/mp4', { kind: 'video', extension: '.mp4' }],
  ['video/webm', { kind: 'video', extension: '.webm' }],
  ['video/quicktime', { kind: 'video', extension: '.mov' }],
]);

function positiveInteger(value, fallback, name) {
  if (value === undefined || value === '') return fallback;
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed <= 0) throw new Error(`${name} must be a positive integer.`);
  return parsed;
}

function normalizeOrigin(value, name) {
  const url = new URL(value);
  if (url.protocol !== 'https:' && url.protocol !== 'http:') throw new Error(`${name} must use http or https.`);
  if (url.username || url.password || url.search || url.hash) throw new Error(`${name} must be a credential-free origin.`);
  url.pathname = url.pathname.replace(/\/+$/, '');
  return url;
}

export function loadConfig(env = process.env) {
  const openProofIssuer = normalizeOrigin(
    env.GENYCASTER_OPENPROOF_ISSUER || 'https://openproof.genyleap.com',
    'GENYCASTER_OPENPROOF_ISSUER',
  );
  if (openProofIssuer.protocol !== 'https:')
    throw new Error('GENYCASTER_OPENPROOF_ISSUER must use HTTPS');
  // This host MUST match the registered OAuth redirect host. OpenProof's
  // __Host- federation state cookies cannot cross from the issuer subdomain
  // to the main-domain callback registered with upstream providers.
  const connectionBrowserBase = normalizeOrigin(
    env.GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE ||
      'https://genyleap.com/openproof',
    'GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE',
  );
  if (connectionBrowserBase.protocol !== 'https:' ||
      connectionBrowserBase.pathname !== '/openproof')
    throw new Error('GENYCASTER_OPENPROOF_CONNECTION_BROWSER_BASE must be HTTPS /openproof');
  const ticketSecret = String(env.GENYLEAP_MEDIA_TICKET_SECRET || '');
  if (ticketSecret.length < 32) throw new Error('GENYLEAP_MEDIA_TICKET_SECRET must contain at least 32 characters.');
  const multipartPartBytes = positiveInteger(
    env.GENYLEAP_MEDIA_MULTIPART_PART_BYTES,
    8 * 1024 * 1024,
    'GENYLEAP_MEDIA_MULTIPART_PART_BYTES',
  );
  if (multipartPartBytes < 5 * 1024 * 1024 || multipartPartBytes > 64 * 1024 * 1024) {
    throw new Error('GENYLEAP_MEDIA_MULTIPART_PART_BYTES must be between 5 MiB and 64 MiB.');
  }
  return Object.freeze({
    host: env.GENYLEAP_MEDIA_HOST?.trim() || '127.0.0.1',
    port: positiveInteger(env.GENYLEAP_MEDIA_PORT, 8790, 'GENYLEAP_MEDIA_PORT'),
    publicBaseUrl: normalizeOrigin(
      env.GENYLEAP_MEDIA_PUBLIC_BASE_URL || 'https://media-farcaster.genyleap.com',
      'GENYLEAP_MEDIA_PUBLIC_BASE_URL',
    ),
    snapchainBaseUrl: normalizeOrigin(
      env.GENYLEAP_MEDIA_SNAPCHAIN_BASE_URL || 'https://snapchain.genyleap.com',
      'GENYLEAP_MEDIA_SNAPCHAIN_BASE_URL',
    ),
    ticketSecret,
    proofLifetimeSeconds: positiveInteger(env.GENYLEAP_MEDIA_PROOF_LIFETIME_SECONDS, 300, 'GENYLEAP_MEDIA_PROOF_LIFETIME_SECONDS'),
    multipartPartBytes,
    downloadUrlLifetimeSeconds: positiveInteger(env.GENYLEAP_MEDIA_DOWNLOAD_URL_LIFETIME_SECONDS, 900, 'GENYLEAP_MEDIA_DOWNLOAD_URL_LIFETIME_SECONDS'),
    ticketLifetimeSeconds: positiveInteger(env.GENYLEAP_MEDIA_TICKET_LIFETIME_SECONDS, 1800, 'GENYLEAP_MEDIA_TICKET_LIFETIME_SECONDS'),
    maxImageBytes: positiveInteger(env.GENYLEAP_MEDIA_MAX_IMAGE_BYTES, 25 * 1024 * 1024, 'GENYLEAP_MEDIA_MAX_IMAGE_BYTES'),
    maxVideoBytes: positiveInteger(env.GENYLEAP_MEDIA_MAX_VIDEO_BYTES, 512 * 1024 * 1024, 'GENYLEAP_MEDIA_MAX_VIDEO_BYTES'),
    maxJsonBytes: positiveInteger(env.GENYLEAP_MEDIA_MAX_JSON_BYTES, 64 * 1024, 'GENYLEAP_MEDIA_MAX_JSON_BYTES'),
    rateWindowMs: positiveInteger(env.GENYLEAP_MEDIA_RATE_WINDOW_MS, 60_000, 'GENYLEAP_MEDIA_RATE_WINDOW_MS'),
    rateLimit: positiveInteger(env.GENYLEAP_MEDIA_RATE_LIMIT, 60, 'GENYLEAP_MEDIA_RATE_LIMIT'),
    partRateLimit: positiveInteger(env.GENYLEAP_MEDIA_PART_RATE_LIMIT, 240, 'GENYLEAP_MEDIA_PART_RATE_LIMIT'),
    trustCloudflare: env.GENYLEAP_MEDIA_TRUST_CLOUDFLARE === '1',
    openProofIssuer,
    connectionBrowserBase,
  });
}

function sha256Hex(value) {
  return createHash('sha256').update(value).digest('hex');
}

function hexBytes(text, expectedBytes) {
  const value = String(text || '').replace(/^0x/i, '');
  if (value.length !== expectedBytes * 2 || !/^[0-9a-f]+$/i.test(value)) return null;
  return Buffer.from(value, 'hex');
}

export function buildUploadChallenge({ method, pathname, fid, signerKey, timestamp, nonce, body }) {
  return [
    'genycaster-media-upload-v1',
    method,
    pathname,
    String(fid),
    signerKey.toLowerCase().replace(/^0x/, ''),
    String(timestamp),
    nonce,
    sha256Hex(body),
  ].join('\n');
}

function signerPublicKey(keyBytes) {
  const prefix = Buffer.from('302a300506032b6570032100', 'hex');
  return createPublicKey({ key: Buffer.concat([prefix, keyBytes]), format: 'der', type: 'spki' });
}

class ReplayCache {
  constructor(lifetimeSeconds) {
    this.lifetimeMs = lifetimeSeconds * 1000;
    this.entries = new Map();
  }
  claim(key, nowMs) {
    for (const [entryKey, expiry] of this.entries) if (expiry <= nowMs) this.entries.delete(entryKey);
    if (this.entries.has(key)) return false;
    this.entries.set(key, nowMs + this.lifetimeMs);
    return true;
  }
}

class FixedWindowRateLimiter {
  constructor(windowMs, limit) {
    this.windowMs = windowMs;
    this.limit = limit;
    this.entries = new Map();
  }
  check(key, now = Date.now()) {
    let entry = this.entries.get(key);
    if (!entry || now >= entry.resetAt) entry = { count: 0, resetAt: now + this.windowMs };
    entry.count += 1;
    this.entries.set(key, entry);
    return { allowed: entry.count <= this.limit, remaining: Math.max(0, this.limit - entry.count) };
  }
}

function clientAddress(request, trustCloudflare) {
  if (trustCloudflare) {
    const address = request.headers['cf-connecting-ip'];
    if (typeof address === 'string' && address.trim()) return address.trim();
  }
  return request.socket.remoteAddress || 'unknown';
}

function writeJson(response, status, payload, headers = {}) {
  const body = Buffer.from(JSON.stringify(payload));
  response.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': String(body.length),
    'cache-control': 'no-store',
    'x-content-type-options': 'nosniff',
    ...headers,
  });
  response.end(body);
}

async function readBody(request, maxBytes) {
  const chunks = [];
  let total = 0;
  for await (const chunk of request) {
    total += chunk.length;
    if (total > maxBytes) {
      const error = new Error('request body too large');
      error.statusCode = 413;
      throw error;
    }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks, total);
}

function safeJson(body) {
  try {
    const parsed = JSON.parse(body.toString('utf8'));
    if (!parsed || Array.isArray(parsed) || typeof parsed !== 'object') return null;
    return parsed;
  } catch {
    return null;
  }
}

function validObjectKey(key) {
  return /^v1\/[0-9]{4}\/[0-9]{2}\/[0-9a-f-]{36}\.[a-z0-9]{2,5}$/i.test(String(key || ''));
}

function assetUrl(config, key) {
  const url = new URL(config.publicBaseUrl);
  url.pathname = `${url.pathname}/m/${key}`.replace(/\/{2,}/g, '/');
  return url.toString();
}

function sanitizeFileName(value) {
  const base = String(value || 'media').split(/[\\/]/).pop().trim();
  return base.replace(/[\u0000-\u001f\u007f]/g, '').slice(0, 180) || 'media';
}

function decodeCanonicalBase64Url(encoded) {
  if (typeof encoded !== 'string' || !/^[A-Za-z0-9_-]+$/.test(encoded)) return null;
  try {
    const bytes = Buffer.from(encoded, 'base64url');
    return bytes.toString('base64url') === encoded ? bytes : null;
  } catch {
    return null;
  }
}

function encodeTicket(config, payload) {
  const encoded = Buffer.from(JSON.stringify(payload)).toString('base64url');
  const signature = createHmac('sha256', config.ticketSecret).update(encoded).digest('base64url');
  return `${encoded}.${signature}`;
}

function decodeTicket(config, token, nowSeconds) {
  const [encoded, signature, extra] = String(token || '').split('.');
  if (!encoded || !signature || extra) return null;
  const expected = createHmac('sha256', config.ticketSecret).update(encoded).digest();
  const provided = decodeCanonicalBase64Url(signature);
  if (!provided || provided.length !== expected.length ||
      !timingSafeEqual(provided, expected)) return null;
  const raw = decodeCanonicalBase64Url(encoded);
  if (!raw) return null;
  let payload;
  try { payload = JSON.parse(raw.toString('utf8')); } catch { return null; }
  if (!payload || ![1, 2].includes(payload.v) || !Number.isSafeInteger(payload.exp) || payload.exp < nowSeconds) return null;
  return payload;
}

function encodeAssetManagementToken(config, payload) {
  const encoded = Buffer.from(JSON.stringify({
    v: 1,
    kind: 'managed-media-asset',
    key: payload.key,
    fid: payload.fid,
  })).toString('base64url');
  const signature = createHmac('sha256', config.ticketSecret).update(encoded).digest('base64url');
  return `${encoded}.${signature}`;
}

function decodeAssetManagementToken(config, token) {
  const [encoded, signature, extra] = String(token || '').split('.');
  if (!encoded || !signature || extra) return null;
  const expected = createHmac('sha256', config.ticketSecret).update(encoded).digest();
  const provided = decodeCanonicalBase64Url(signature);
  if (!provided || provided.length !== expected.length ||
      !timingSafeEqual(provided, expected)) return null;
  const raw = decodeCanonicalBase64Url(encoded);
  if (!raw) return null;
  let payload;
  try { payload = JSON.parse(raw.toString('utf8')); } catch { return null; }
  if (!payload || payload.v !== 1 || payload.kind !== 'managed-media-asset' ||
      !Number.isSafeInteger(payload.fid) || payload.fid <= 0 || !validObjectKey(payload.key)) {
    return null;
  }
  return payload;
}

async function signerIsAuthorized(config, fid, signerKey, fetchImpl, nowSeconds) {
  let pageToken = '';
  for (let page = 0; page < 10; page += 1) {
    const url = new URL(config.snapchainBaseUrl);
    url.pathname = `${url.pathname}/v1/signersByFid`.replace(/\/{2,}/g, '/');
    url.searchParams.set('fid', String(fid));
    url.searchParams.set('pageSize', '100');
    if (pageToken) url.searchParams.set('pageToken', pageToken);
    const response = await fetchImpl(url, { headers: { accept: 'application/json' } });
    if (!response.ok) {
      const error = new Error(`signer verification failed with HTTP ${response.status}`);
      error.statusCode = response.status >= 500 ? 503 : 401;
      throw error;
    }
    const document = await response.json();
    if (!Array.isArray(document.signers)) throw new Error('Snapchain signer response is malformed.');
    for (const signer of document.signers) {
      const key = String(signer?.key || '').toLowerCase().replace(/^0x/, '');
      if (key !== signerKey || Number(signer?.fid) !== fid) continue;
      if (signer.source === 'SIGNER_SOURCE_ONCHAIN') return true;
      if (signer.source !== 'SIGNER_SOURCE_OFFCHAIN') continue;
      if (!Array.isArray(signer.scopes) || !signer.scopes.includes('MESSAGE_TYPE_CAST_ADD')) continue;
      if (signer.expiresAt !== undefined && signer.expiresAt !== null && Number(signer.expiresAt) <= nowSeconds) continue;
      return true;
    }
    pageToken = String(document.nextPageToken || '');
    if (!pageToken) break;
  }
  return false;
}

async function verifyProof(config, request, body, pathname, dependencies, replayCache, options = {}) {
  const fid = Number.parseInt(String(request.headers['x-genycaster-fid'] || ''), 10);
  const signerKeyText = String(request.headers['x-genycaster-signer-key'] || '').toLowerCase().replace(/^0x/, '');
  const timestamp = Number.parseInt(String(request.headers['x-genycaster-timestamp'] || ''), 10);
  const nonce = String(request.headers['x-genycaster-nonce'] || '');
  const signatureText = String(request.headers['x-genycaster-signature'] || '');
  const publicKeyBytes = hexBytes(signerKeyText, 32);
  const signatureBytes = hexBytes(signatureText, 64);
  const nowMs = dependencies.now();
  const nowSeconds = Math.floor(nowMs / 1000);
  if (!Number.isSafeInteger(fid) || fid <= 0 || !publicKeyBytes || !signatureBytes ||
      !Number.isSafeInteger(timestamp) || Math.abs(nowSeconds - timestamp) > config.proofLifetimeSeconds ||
      !/^[A-Za-z0-9_-]{16,128}$/.test(nonce)) {
    const error = new Error('invalid media upload authorization proof');
    error.statusCode = 401;
    throw error;
  }
  const replayKey = `${fid}:${signerKeyText}:${timestamp}:${nonce}`;
  if (!replayCache.claim(replayKey, nowMs)) {
    const error = new Error('media upload authorization proof was already used');
    error.statusCode = 409;
    throw error;
  }
  const challenge = buildUploadChallenge({
    method: request.method,
    pathname,
    fid,
    signerKey: signerKeyText,
    timestamp,
    nonce,
    body,
  });
  if (!verify(null, Buffer.from(challenge), signerPublicKey(publicKeyBytes), signatureBytes)) {
    const error = new Error('media upload authorization signature is invalid');
    error.statusCode = 401;
    throw error;
  }
  if (options.verifySignerState !== false && !await signerIsAuthorized(config, fid, signerKeyText, dependencies.fetchImpl, nowSeconds)) {
    const error = new Error('media upload signer is not active for this Farcaster account');
    error.statusCode = 403;
    throw error;
  }
  return { fid, signerKey: signerKeyText };
}

function mediaDefinition(contentType, sizeBytes, config) {
  const definition = mediaTypes.get(contentType);
  if (!definition) return null;
  const limit = definition.kind === 'image' ? config.maxImageBytes : config.maxVideoBytes;
  return sizeBytes > 0 && sizeBytes <= limit ? definition : null;
}

function objectKey(definition, nowMs) {
  const date = new Date(nowMs);
  const year = String(date.getUTCFullYear());
  const month = String(date.getUTCMonth() + 1).padStart(2, '0');
  return `v1/${year}/${month}/${randomUUID()}${definition.extension}`;
}

function multipartPartCount(sizeBytes, partSizeBytes) {
  return Math.ceil(sizeBytes / partSizeBytes);
}

function expectedPartBytes(sizeBytes, partSizeBytes, partNumber) {
  const count = multipartPartCount(sizeBytes, partSizeBytes);
  if (!Number.isSafeInteger(partNumber) || partNumber < 1 || partNumber > count) return null;
  if (partNumber < count) return partSizeBytes;
  return sizeBytes - (count - 1) * partSizeBytes;
}

function ticketMatchesAuthorization(ticket, authorization) {
  return ticket.fid === authorization.fid &&
    (!ticket.signerKey || ticket.signerKey === authorization.signerKey);
}

function validatedCompletionParts(value, expectedCount) {
  if (!Array.isArray(value) || value.length !== expectedCount) return null;
  const parts = [];
  for (let index = 0; index < value.length; index += 1) {
    const candidate = value[index];
    const expectedPartNumber = index + 1;
    if (!candidate || Number(candidate.partNumber) !== expectedPartNumber) return null;
    const etag = String(candidate.etag || '').trim();
    if (!etag || etag.length > 512 || /[\r\n]/.test(etag)) return null;
    parts.push({ partNumber: expectedPartNumber, etag });
  }
  return parts;
}

// Mini App icon and wide cover artwork may live on distinct CDNs. Only exact
// URLs declared by the trusted catalogue are eligible for the browser image
// proxy; unrelated URLs on the same host remain forbidden.
const miniAppIconFields = ['icon_url','iconUrl','icon','logo_url','logoUrl'];
const miniAppCoverFields = [
  'hero_image_url','heroImageUrl','cover_url','coverUrl',
  'banner_url','bannerUrl','og_image_url','ogImageUrl',
  'splash_image_url','splashImageUrl','poster_url','posterUrl',
];
function miniAppManifestUrls(document, includeCovers) {
  const frames = Array.isArray(document?.frames) ? document.frames
    : Array.isArray(document?.miniapps) ? document.miniapps
    : Array.isArray(document?.apps) ? document.apps : [];
  const result = new Set();
  for (const row of frames.slice(0, 100)) {
    if (!row || typeof row !== 'object') continue;
    let manifest = row.manifest;
    if (typeof manifest === 'string') {
      try { manifest = JSON.parse(manifest); } catch { manifest = null; }
    }
    const sources = [manifest?.miniapp, manifest?.frame, manifest, row.miniapp, row.frame, row];
    for (const source of sources) {
      if (!source || typeof source !== 'object') continue;
      const fields = includeCovers ? [...miniAppIconFields,...miniAppCoverFields] : miniAppIconFields;
      const candidates = fields.map(key => source[key]);
      if (includeCovers) {
        for (const key of ['screenshot_urls','screenshotUrls']) {
          if (Array.isArray(source[key]))
            candidates.push(...source[key].slice(0,3));
        }
      }
      for (const raw of candidates) {
        if (typeof raw !== 'string' || raw.length > 4096) continue;
        let url;
        try {
          const home = source.home_url || source.homeUrl ||
                       manifest?.miniapp?.home_url || manifest?.frame?.home_url ||
                       row.frames_url;
          url = home ? new URL(raw,home) : new URL(raw);
        } catch { continue; }
        if (url.protocol !== 'https:' || url.username || url.password || url.hash
            || (url.port && url.port !== '443')) continue;
        result.add(url.href);
      }
    }
  }
  return result;
}
export function miniAppIconUrls(document) {
  return miniAppManifestUrls(document,false);
}
export function miniAppArtworkUrls(document) {
  return miniAppManifestUrls(document,true);
}

export function createMediaGateway(config, storage, dependencies = {}) {
  const runtime = {
    fetchImpl: dependencies.fetchImpl ?? globalThis.fetch,
    now: dependencies.now ?? (() => Date.now()),
  };
  const replayCache = dependencies.replayCache ?? new ReplayCache(config.proofLifetimeSeconds);
  const limiter = dependencies.limiter ?? new FixedWindowRateLimiter(config.rateWindowMs, config.rateLimit);
  // Uploads carry verified signer proofs and must not compete with public
  // artwork/feed browsing for one IP's fixed-window budget. Keep a separate,
  // equally bounded window; do not bypass proof verification or replay checks.
  const uploadLimiter = dependencies.uploadLimiter
    ?? new FixedWindowRateLimiter(config.rateWindowMs, config.rateLimit);
  // Verified Mini App artwork is shown in a 3-column scrolling gallery.
  // It needs a separate *bounded* allowance so a fast fling cannot exhaust
  // the generic API budget. Only exact URLs from trusted catalogues qualify.
  const miniAppArtworkLimiter = dependencies.miniAppArtworkLimiter
    ?? new FixedWindowRateLimiter(config.rateWindowMs, Math.max(config.rateLimit, 180));
  const partLimiter = dependencies.partLimiter ?? new FixedWindowRateLimiter(config.rateWindowMs, config.partRateLimit);
  const farcasterBroker = dependencies.farcasterBroker ?? createBrowserFarcasterBroker({
    ...runtime, issuerOrigin:config.openProofIssuer.origin,
  });
  // OpenProof's own session remains authoritative. A freshly recovered
  // custody proof may temporarily authorize the same account without making
  // the user repeat the device-grant ceremony. The cookie lives server-side.
  const accountHeaders = authorization => {
    if (authorization.startsWith('Bearer recovery.')) {
      const verified = farcasterBroker.authorizeRecoveryAccount?.(authorization);
      return verified?.cookie ? {cookie:verified.cookie,accept:'application/json'} : null;
    }
    return {authorization,accept:'application/json'};
  };
  const retrieveImage = dependencies.retrieveImage ?? retrieveBrowserImage;
  const resolveMiniAppPreview = dependencies.resolveMiniAppPreview ?? publicMiniAppPreview;
  const verifiedMiniAppIcons = new Map();
  const channelDirectoryCache = new Map();
  const tokenSnapshots = new Map();
  const miniAppPreviews = new Map();
  const miniAppArtworkScopes = new Map();
  const webLinkPreviews = new Map();
  const castViewService = dependencies.castViewService ?? createCastViewService(runtime);
  const browserReactionService = dependencies.browserReactionService ?? createBrowserReactionService(runtime);
  const verifiedLinkImages = new Map();
  const verifiedMiniAppImages = new Map();
  // Cache only successfully verified Mini App raster bytes. Per-cast images
  // can be slow/intermittent on third-party CDN origins, and multiple visible
  // delegates may request the same artwork together. Bound both count and
  // total memory; authorization always precedes this cache lookup.
  const verifiedArtworkCache = new Map();
  const verifiedArtworkPending = new Map();
  let verifiedArtworkCacheBytes = 0;
  const artworkCacheBudgetBytes = 12 * 1024 * 1024;
  const artworkCacheEntryMaxBytes = 768 * 1024;

  function rememberVerifiedArtwork(key, result) {
    const size = result.body?.length || 0;
    if (!size || size > artworkCacheEntryMaxBytes) return;
    const previous = verifiedArtworkCache.get(key);
    if (previous) verifiedArtworkCacheBytes -= previous.body.length;
    verifiedArtworkCache.delete(key);
    while (verifiedArtworkCache.size >= 64
        || verifiedArtworkCacheBytes + size > artworkCacheBudgetBytes) {
      const oldestKey = verifiedArtworkCache.keys().next().value;
      if (oldestKey === undefined) break;
      verifiedArtworkCacheBytes -= verifiedArtworkCache.get(oldestKey).body.length;
      verifiedArtworkCache.delete(oldestKey);
    }
    verifiedArtworkCache.set(key,{
      body: result.body, contentType: result.contentType,
      expires: runtime.now() + 180000,
    });
    verifiedArtworkCacheBytes += size;
  }
  let refreshIconsPromise = null;
  let lastIconRefreshAttempt = -Infinity;

  function rememberCatalogueIcons(document) {
    const expires = runtime.now() + 60 * 60 * 1000;
    for (const url of miniAppArtworkUrls(document)) {
      verifiedMiniAppIcons.set(url, expires);
      if (verifiedMiniAppIcons.size > 1200)
        verifiedMiniAppIcons.delete(verifiedMiniAppIcons.keys().next().value);
    }
  }

  async function refreshMiniAppIconCatalogue() {
    if (refreshIconsPromise) return refreshIconsPromise;
    if (runtime.now() - lastIconRefreshAttempt < 2 * 60 * 1000) return;
    lastIconRefreshAttempt = runtime.now();
    refreshIconsPromise = (async () => {
      // The catalogue is a fixed Neynar endpoint, never caller-selected.
      const url = new URL('https://api.neynar.com/v2/farcaster/frame/catalog/');
      // Match the first page returned by /v1/browser/miniapps.
      // A 100-item request can exceed supported source catalogue limits.
      url.searchParams.set('limit', '20');
      url.searchParams.set('time_window', '7d');
      try {
        const reply = await runtime.fetchImpl(url, {
          method:'GET', redirect:'error',
          headers:{ accept:'application/json',
            'x-api-key':process.env.GENYCASTER_MINIAPP_NEYNAR_API_KEY || 'NEYNAR_API_DOCS' },
          signal:AbortSignal.timeout(8_000),
        });
        if (!reply.ok || !String(reply.headers.get('content-type') || '').startsWith('application/json')) {
          await reply.body?.cancel();
          return;
        }
        // Bound the catalogue payload independent of Content-Length.
        const content = await reply.text();
        if (content.length > 2 * 1024 * 1024) return;
        rememberCatalogueIcons(JSON.parse(content));
      } catch { /* retain previously verified entries */ }
    })().finally(() => { refreshIconsPromise = null; });
    return refreshIconsPromise;
  }

  return http.createServer(async (request, response) => {
    response.setHeader('server', '');
    let requestUrl;
    try { requestUrl = new URL(request.url || '/', 'http://media.invalid'); }
    catch { writeJson(response, 400, { error: 'invalid_request_url' }); return; }

    if (requestUrl.pathname.startsWith('/v1/browser/farcaster/')) {
      // OpenProof validates the Farcaster SIWF proof. The browser receives
      // only an opaque, short-lived attempt ticket; relay and OpenProof
      // cookies never leave this server's process.
      response.setHeader('access-control-allow-origin', '*');
      response.setHeader('access-control-allow-methods', 'GET, POST, OPTIONS');
      response.setHeader('access-control-allow-headers', 'content-type');
      response.setHeader('cache-control', 'no-store');
      response.setHeader('x-content-type-options', 'nosniff');
      if (request.method === 'OPTIONS') { response.writeHead(204); response.end(); return; }
      const isStart = requestUrl.pathname === '/v1/browser/farcaster/start'
        && request.method === 'POST';
      const isPoll = requestUrl.pathname === '/v1/browser/farcaster/poll'
        && request.method === 'POST';
      const isRecoveryStart = requestUrl.pathname === '/v1/browser/farcaster/recovery/start'
        && request.method === 'POST';
      const isRecoveryComplete = requestUrl.pathname === '/v1/browser/farcaster/recovery/complete'
        && request.method === 'POST';
      if (!isStart && !isPoll && !isRecoveryStart && !isRecoveryComplete) {
        writeJson(response, 405, { error: 'method_not_allowed' }); return;
      }
      const rate = limiter.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) { writeJson(response, 429, { error: 'rate_limited' }); return; }
      let ticket = null;
      try {
        if (isStart) {
          const raw = await readBody(request, 128);
          if (raw.length && !safeJson(raw)) {
            writeJson(response, 400, { error: 'invalid_request_json' });
            return;
          }
        }
        let requestBody;
        if (isPoll) {
          const payload = safeJson(await readBody(request, 256));
          ticket = payload?.ticket;
        } else if (isRecoveryStart || isRecoveryComplete) {
          requestBody = safeJson(await readBody(request, isRecoveryStart ? 256 : 512));
          if (!requestBody || typeof requestBody !== 'object'
              || Array.isArray(requestBody)
              || Object.keys(requestBody).some(k => !(
                isRecoveryStart ? ['fid','address'].includes(k)
                                : ['ticket','signature'].includes(k)))) {
            writeJson(response,400,{error:'auth_invalid_recovery_request'});return;
          }
        }
        const output = isRecoveryStart
          ? await farcasterBroker.startDirect(requestBody.fid,requestBody.address)
          : isRecoveryComplete
            ? await farcasterBroker.completeDirect(requestBody.ticket,requestBody.signature)
            : isStart ? await farcasterBroker.start()
                      : await farcasterBroker.poll(ticket);
        if (output.authenticated && !isRecoveryComplete) farcasterBroker.consume(ticket);
        writeJson(response, output.pending ? 202 : 200, output);
      } catch (error) {
        writeJson(response, Number.isInteger(error.status) ? error.status : 502, {
          error: typeof error.code === 'string' && error.code.startsWith('auth_')
            ? error.code : 'auth_service_unavailable',
        });
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/cast-views') {
      // Bounded, read-only client analytics enrichment. Never confuse
      // missing view counts with real zero impressions. Origin isolation
      // and one per-batch rate decision protect the public indexer.
      const origin = request.headers.origin;
      if (origin === 'https://caster.genyleap.com') {
        response.setHeader('access-control-allow-origin', origin);
        response.setHeader('vary', 'origin');
      }
      response.setHeader('access-control-allow-methods', 'POST, OPTIONS');
      response.setHeader('access-control-allow-headers', 'content-type, accept');
      if (request.method === 'OPTIONS') {
        response.writeHead(origin === 'https://caster.genyleap.com' ? 204 : 403);
        response.end(); return;
      }
      if (request.method !== 'POST') {
        writeJson(response, 405, {error:'method_not_allowed'}); return;
      }
      const rate = limiter.check(clientAddress(request,config.trustCloudflare),runtime.now());
      if (!rate.allowed) {
        writeJson(response,429,{error:'rate_limited'},{'retry-after':'60'});return;
      }
      try {
        const document = safeJson(await readBody(request, 5 * 1024));
        const result = await castViewService.resolve(document?.casts);
        writeJson(response,result.status,result.status === 200 ? result.data : {error:result.error});
      } catch (error) {
        writeJson(response, error?.statusCode === 413 ? 413 : 502,
          {error:error?.statusCode === 413 ? 'request_too_large' : 'cast_views_unavailable'});
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/cast-reactions') {
      // Public positive reaction evidence for WASM (no password/token, no
      // inference from an incomplete list). Origin and request size are
      // bounded independently of other media endpoints.
      const origin = request.headers.origin;
      if (origin === 'https://caster.genyleap.com') {
        response.setHeader('access-control-allow-origin', origin);
        response.setHeader('vary', 'origin');
      }
      response.setHeader('access-control-allow-methods', 'POST, OPTIONS');
      response.setHeader('access-control-allow-headers', 'content-type, accept');
      if (request.method === 'OPTIONS') {
        response.writeHead(origin === 'https://caster.genyleap.com' ? 204 : 403);
        response.end(); return;
      }
      if (request.method !== 'POST') {
        writeJson(response, 405, {error:'method_not_allowed'}); return;
      }
      const rate = limiter.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) {
        writeJson(response, 429, {error:'rate_limited'}, {'retry-after':'60'}); return;
      }
      try {
        const document = safeJson(await readBody(request, 2048));
        const result = await browserReactionService.resolve(document);
        writeJson(response, result.status, result.status === 200
          ? result.data : {error:result.error});
      } catch (error) {
        writeJson(response, error?.statusCode === 413 ? 413 : 502,
          {error:error?.statusCode === 413
            ? 'request_too_large' : 'cast_reactions_unavailable'});
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/link-preview') {
      const origin = request.headers.origin;
      if (origin === 'https://caster.genyleap.com') {
        response.setHeader('access-control-allow-origin', origin);
        response.setHeader('vary', 'origin');
      }
      response.setHeader('access-control-allow-methods','GET, OPTIONS');
      response.setHeader('access-control-allow-headers','accept');
      if (request.method === 'OPTIONS') {
        response.writeHead(origin === 'https://caster.genyleap.com' ? 204 : 403);
        response.end(); return;
      }
      if (request.method !== 'GET') {
        writeJson(response,405,{error:'method_not_allowed'});return;
      }
      const rawUrl = requestUrl.searchParams.get('url') || '';
      if (!rawUrl || rawUrl.length > 2048 ||
          [...requestUrl.searchParams.keys()].some(key => key !== 'url')) {
        writeJson(response,400,{error:'invalid_preview_url'});return;
      }
      const cached = webLinkPreviews.get(rawUrl);
      if (cached && cached.expires > runtime.now()) {
        writeJson(response,cached.status,cached.body,{'cache-control':'public, max-age=60'});
        return;
      }
      const rate = limiter.check(clientAddress(request,config.trustCloudflare),runtime.now());
      if (!rate.allowed) {writeJson(response,429,{error:'rate_limited'});return;}
      const resolved = await publicLinkPreview(rawUrl);
      const body = resolved.status === 200 ? {...resolved.data} : {error:resolved.error};
      if (body.verifiedImageUrl) {
        if (verifiedLinkImages.size > 240) verifiedLinkImages.clear();
        verifiedLinkImages.set(body.verifiedImageUrl, runtime.now()+600000);
        // URL leases stay private to the gateway. Only a canonical
        // image://remote source is exposed to browser clients.
        delete body.verifiedImageUrl;
      }
      if (webLinkPreviews.size > 240) webLinkPreviews.clear();
      if (resolved.status !== 400)
        webLinkPreviews.set(rawUrl,{status:resolved.status,body,expires:runtime.now()+300000});
      writeJson(response,resolved.status,body,{'cache-control':'public, max-age=60'});
      return;
    }

    if (requestUrl.pathname === '/v1/browser/miniapp-preview') {
      const origin = request.headers.origin;
      if (origin === 'https://caster.genyleap.com') {
        response.setHeader('access-control-allow-origin', origin);
        response.setHeader('vary', 'origin');
      }
      response.setHeader('access-control-allow-methods','GET, OPTIONS');
      response.setHeader('access-control-allow-headers','accept');
      if (request.method === 'OPTIONS') {
        response.writeHead(origin === 'https://caster.genyleap.com' ? 204 : 403);
        response.end(); return;
      }
      if (request.method !== 'GET') {
        writeJson(response,405,{error:'method_not_allowed'}); return;
      }
      const rawUrl = requestUrl.searchParams.get('url') || '';
      if (!rawUrl || rawUrl.length > 2048 ||
          [...requestUrl.searchParams.keys()].some(key => key !== 'url')) {
        writeJson(response,400,{error:'invalid_miniapp_url'}); return;
      }
      const cached = miniAppPreviews.get(rawUrl);
      if (cached && cached.expires > runtime.now()) {
        writeJson(response,cached.status,cached.body,{'cache-control':'public, max-age=60'});
        return;
      }
      const rate = limiter.check(clientAddress(request,config.trustCloudflare),runtime.now());
      if (!rate.allowed) {writeJson(response,429,{error:'rate_limited'});return;}
      const resolved = await resolveMiniAppPreview(rawUrl);
      const body = resolved.status === 200 ? {...resolved.data} : {error:resolved.error};
      if (Array.isArray(body.verifiedImageUrls)) {
        if (verifiedMiniAppImages.size > 600) verifiedMiniAppImages.clear();
        for (const imageUrl of body.verifiedImageUrls.slice(0, 5))
          verifiedMiniAppImages.set(imageUrl,runtime.now()+300000);
        // Do not expose the private grant list in a public preview response.
        delete body.verifiedImageUrls;
      }
      if (miniAppPreviews.size > 180) miniAppPreviews.clear();
      // A missing/blocked upstream manifest can be transient (403 from a
      // publisher's bot filter, DNS or an origin timeout). Do not pin a failed
      // Mini App card for two minutes; let the browser retry soon.
      if (resolved.status !== 400)
        miniAppPreviews.set(rawUrl,{status:resolved.status,body,
          expires:runtime.now()+(resolved.status===200
            && !resolved.data?.castMetadataPending ? 120000 : 5000)});
      writeJson(response,resolved.status,body,{'cache-control':'public, max-age=60'});
      return;
    }

    if (requestUrl.pathname === '/v1/browser/channels/search-channels'
        || requestUrl.pathname === '/v1/browser/channels/channel') {
      // Public presentation metadata only. The signed cast parent URL and
      // membership authorization continue to belong to the selected read node.
      // Fixed upstream and fixed paths: neither is caller-configurable.
      response.setHeader('access-control-allow-origin', '*');
      response.setHeader('access-control-allow-methods', 'GET, OPTIONS');
      response.setHeader('access-control-allow-headers', 'accept');
      response.setHeader('x-content-type-options', 'nosniff');
      if (request.method === 'OPTIONS') {
        response.writeHead(204); response.end(); return;
      }
      if (request.method !== 'GET') {
        writeJson(response, 405, {error:'method_not_allowed'}); return;
      }
      const isSearch = requestUrl.pathname.endsWith('/search-channels');
      const allowed = isSearch ? new Set(['q','limit']) : new Set(['key']);
      if ([...requestUrl.searchParams.keys()].some(key => !allowed.has(key))) {
        writeJson(response, 400, {error:'invalid_channel_query'}); return;
      }
      const value = String(requestUrl.searchParams.get(isSearch ? 'q' : 'key') || '').trim();
      const limitText = requestUrl.searchParams.get('limit') || '25';
      const limit = Number(limitText);
      if ((isSearch && (value.length < 1 || value.length > 64))
          || (!isSearch && !/^[a-z0-9][a-z0-9_-]{0,63}$/i.test(value))
          || (isSearch && (!Number.isInteger(limit) || limit < 1 || limit > 50))) {
        writeJson(response, 400, {error:'invalid_channel_query'}); return;
      }
      const cacheKey = (isSearch ? 's:' : 'c:') + value.toLowerCase()
        + (isSearch ? ':' + limit : '');
      const old = channelDirectoryCache.get(cacheKey);
      if (old && old.expiresAt > runtime.now()) {
        writeJson(response, 200, old.data,
          {'cache-control':isSearch ? 'public, max-age=30' : 'public, max-age=90'});
        return;
      }
      const rate = limiter.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) { writeJson(response, 429, {error:'rate_limited'}); return; }
      const upstream = new URL('https://client.warpcast.com/v2/'
          + (isSearch ? 'search-channels' : 'channel'));
      upstream.searchParams.set(isSearch ? 'q' : 'key', value);
      if (isSearch) upstream.searchParams.set('limit', String(limit));
      try {
        const reply = await runtime.fetchImpl(upstream, {
          method:'GET', redirect:'error',
          headers:{accept:'application/json','user-agent':'Genycaster/1.0'},
          signal:AbortSignal.timeout(9000),
        });
        if (reply.status !== 200 || !reply.body
            || !String(reply.headers.get('content-type') || '').toLowerCase()
                .startsWith('application/json')) {
          await reply.body?.cancel();
          writeJson(response, 502, {error:'channel_directory_unavailable'}); return;
        }
        const reader = reply.body.getReader();
        const chunks = [];
        let bytes = 0;
        let oversized = false;
        while (true) {
          const {done,value:chunk} = await reader.read();
          if (done) break;
          bytes += chunk.byteLength;
          if (bytes > 512 * 1024) {
            oversized = true;
            await reader.cancel(); break;
          }
          chunks.push(Buffer.from(chunk));
        }
        if (oversized) {
          writeJson(response, 502, {error:'channel_directory_too_large'}); return;
        }
        const data = safeJson(Buffer.concat(chunks,bytes));
        const result = data?.result;
        if (!result || (isSearch ? !Array.isArray(result.channels)
                                  : typeof result.channel !== 'object'
                                    || !result.channel)) {
          writeJson(response, 502, {error:'invalid_channel_directory_response'}); return;
        }
        if (channelDirectoryCache.size >= 250)
          channelDirectoryCache.delete(channelDirectoryCache.keys().next().value);
        channelDirectoryCache.set(cacheKey,
          {data,expiresAt:runtime.now() + (isSearch ? 45_000 : 120_000)});
        writeJson(response, 200, data,
          {'cache-control':isSearch ? 'public, max-age=30' : 'public, max-age=90'});
      } catch {
        writeJson(response, 502, {error:'channel_directory_unavailable'});
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/miniapps') {
      // A bounded public catalogue relay, not a generic URL proxy. Browser
      // clients receive no upstream credentials or unrestricted destination.
      response.setHeader('access-control-allow-origin', '*');
      response.setHeader('access-control-allow-methods', 'GET, OPTIONS');
      response.setHeader('access-control-allow-headers', 'accept');
      response.setHeader('x-content-type-options', 'nosniff');
      if (request.method === 'OPTIONS') { response.writeHead(204); response.end(); return; }
      if (request.method !== 'GET') {
        writeJson(response, 405, { error: 'method_not_allowed' }); return;
      }
      const kind = requestUrl.searchParams.get('kind') || 'catalog';
      if (!['catalog', 'search', 'relevant'].includes(kind)) {
        writeJson(response, 400, { error: 'invalid_catalogue_kind' }); return;
      }
      const query = String(requestUrl.searchParams.get('q') || '').trim();
      const fid = String(requestUrl.searchParams.get('viewer_fid') || '');
      const cursor = requestUrl.searchParams.get('cursor') || '';
      // The Qt provider uses limit=20 for search. Keep a strict bounded
      // query contract, but do not reject the legitimate client request.
      const requestedLimit = requestUrl.searchParams.get('limit');
      const accepted = new Set(kind === 'catalog' ? ['kind','cursor']
        : kind === 'search' ? ['kind','q','limit'] : ['kind','viewer_fid']);
      if ([...requestUrl.searchParams.keys()].some(key=>!accepted.has(key))
          || (kind !== 'catalog' && cursor)
          || (cursor && !/^[A-Za-z0-9%_=+-]{1,480}$/.test(cursor))
          || (kind === 'search' && (query.length === 0 || query.length > 32))
          || (requestedLimit !== null
              && (kind !== 'search' || !/^(?:[1-9]|1[0-9]|20)$/.test(requestedLimit)))
          || (kind === 'relevant' && !/^[1-9][0-9]{0,18}$/.test(fid))) {
        writeJson(response, 400, { error: 'invalid_catalogue_parameters' }); return;
      }
      const rate = limiter.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) {
        writeJson(response, 429, { error: 'rate_limited' }, { 'retry-after': '60' }); return;
      }
      const route = kind === 'catalog'
        ? '/v2/farcaster/frame/catalog/'
        : kind === 'search'
          ? '/v2/farcaster/frame/search/'
          : '/v2/farcaster/frame/relevant/';
      const upstreamUrl = new URL('https://api.neynar.com' + route);
      if (kind === 'catalog') {
        // Keep edge/CDN responses small; larger catalogue snapshots were
        // truncated in the public reverse-proxy path.
        upstreamUrl.searchParams.set('limit', '20');
        upstreamUrl.searchParams.set('time_window', '7d');
        if (cursor) upstreamUrl.searchParams.set('cursor',cursor);
      } else if (kind === 'search') {
        upstreamUrl.searchParams.set('q', query);
        upstreamUrl.searchParams.set('limit', requestedLimit || '20');
      } else {
        upstreamUrl.searchParams.set('viewer_fid', fid);
        upstreamUrl.searchParams.set('time_window', '7d');
      }
      try {
        const upstream = await runtime.fetchImpl(upstreamUrl, {
          method: 'GET',
          redirect: 'error',
          headers: {
            accept: 'application/json',
            'user-agent': 'Genycaster/0.4.0',
            'x-api-key': process.env.GENYCASTER_MINIAPP_NEYNAR_API_KEY || 'NEYNAR_API_DOCS',
          },
          signal: AbortSignal.timeout(10_000),
        });
        if (!upstream.ok || !String(upstream.headers.get('content-type') || '')
            .toLowerCase().startsWith('application/json') || !upstream.body) {
          upstream.body?.cancel();
          writeJson(response, 502, { error: 'catalogue_unavailable' }); return;
        }
        const reader = upstream.body.getReader();
        const chunks = [];
        let size = 0;
        while (true) {
          const part = await reader.read();
          if (part.done) break;
          size += part.value.byteLength;
          if (size > 2 * 1024 * 1024) {
            await reader.cancel();
            writeJson(response, 502, { error: 'catalogue_too_large' }); return;
          }
          chunks.push(Buffer.from(part.value));
        }
        const payload = Buffer.concat(chunks, size);
        rememberCatalogueIcons(JSON.parse(payload.toString('utf8')));
        response.writeHead(200, {
          'content-type': 'application/json; charset=utf-8',
          'cache-control': kind === 'catalog' ? 'public, max-age=60' : 'no-store',
          'content-length': String(payload.length),
        });
        response.end(payload);
      } catch {
        writeJson(response, 502, { error: 'catalogue_unavailable' });
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/openproof/providers') {
      // OpenProof deliberately has no cross-origin browser contract. Proxy
      // only its PUBLIC provider capabilities; never reflect cookies or
      // account state and never permit a caller-selected upstream URL.
      response.setHeader('access-control-allow-origin', '*');
      response.setHeader('access-control-allow-methods', 'GET, OPTIONS');
      response.setHeader('access-control-allow-headers', 'accept');
      response.setHeader('x-content-type-options', 'nosniff');
      if (request.method === 'OPTIONS') {
        response.writeHead(204); response.end(); return;
      }
      if (request.method !== 'GET') {
        writeJson(response, 405, {error:'method_not_allowed'}); return;
      }
      const rate = limiter.check(clientAddress(request,config.trustCloudflare),runtime.now());
      if (!rate.allowed) {
        writeJson(response,429,{error:'rate_limited'}); return;
      }
      try {
        const upstream = await runtime.fetchImpl(
          config.openProofIssuer.href.replace(/\/$/,'') + '/auth/providers', {
            method:'GET',
            headers:{accept:'application/json'},
            redirect:'error',
            signal:AbortSignal.timeout(8_000),
          });
        if (upstream.status !== 200 || !upstream.body ||
            !String(upstream.headers.get('content-type') || '').toLowerCase()
              .startsWith('application/json')) {
          upstream.body?.cancel();
          writeJson(response,502,{error:'provider_catalog_unavailable'});return;
        }
        const reader = upstream.body.getReader();
        const chunks = [];
        let size = 0;
        while (true) {
          const {done,value} = await reader.read();
          if (done) break;
          size += value.byteLength;
          if (size > 32 * 1024) {
            await reader.cancel();
            writeJson(response,502,{error:'provider_catalog_too_large'});return;
          }
          chunks.push(Buffer.from(value));
        }
        const document = safeJson(Buffer.concat(chunks,size));
        if (!Array.isArray(document?.providers)
            || !Array.isArray(document?.challenge_providers)) {
          writeJson(response,502,{error:'provider_catalog_invalid'});return;
        }
        const normalize = entries => [...new Set(entries.filter(
          p => typeof p === 'string' && /^[a-z][a-z0-9-]{0,39}$/.test(p)))].slice(0,32);
        writeJson(response,200,{
          providers:normalize(document.providers),
          challenge_providers:normalize(document.challenge_providers),
        },{'cache-control':'public, max-age=180'});
      } catch {
        writeJson(response,502,{error:'provider_catalog_unavailable'});
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/openproof/connections') {
      // The OpenProof account API intentionally has no browser CORS contract.
      // Relay only its fixed, bearer-authenticated identity connections route;
      // never accept a caller-supplied destination or ambient cookie.
      response.setHeader('access-control-allow-origin', '*');
      response.setHeader('access-control-allow-methods', 'GET, OPTIONS');
      response.setHeader('access-control-allow-headers', 'authorization');
      response.setHeader('cache-control', 'no-store');
      response.setHeader('x-content-type-options', 'nosniff');
      if (request.method === 'OPTIONS') { response.writeHead(204); response.end(); return; }
      if (request.method !== 'GET') {
        writeJson(response, 405, { error: 'method_not_allowed' }); return;
      }
      const authorization = String(request.headers.authorization || '');
      if (authorization.length > 8192 ||
          !/^Bearer [A-Za-z0-9._~+\/-]+={0,2}$/.test(authorization)) {
        writeJson(response, 401, { error: 'bearer_token_required' }); return;
      }
      const rate = limiter.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) {
        writeJson(response, 429, { error: 'rate_limited' }); return;
      }
      const delegated = accountHeaders(authorization);
      if (!delegated) {
        writeJson(response, 401, {error:'openproof_unauthorized'});return;
      }
      try {
        const upstream = await runtime.fetchImpl(
          config.openProofIssuer.href.replace(/\/$/, '') + '/account/connections', {
            method: 'GET',
            headers: delegated,
            redirect: 'error',
            signal: AbortSignal.timeout(8_000),
          },
        );
        if (upstream.status === 401 || upstream.status === 403) {
          writeJson(response, 401, { error: 'openproof_unauthorized' }); return;
        }
        if (!upstream.ok || !upstream.body ||
            !String(upstream.headers.get('content-type') || '').startsWith('application/json')) {
          upstream.body?.cancel();
          writeJson(response, 502, { error: 'openproof_unavailable' }); return;
        }
        const reader = upstream.body.getReader();
        const chunks = [];
        let size = 0;
        while (true) {
          const part = await reader.read();
          if (part.done) break;
          size += part.value.byteLength;
          if (size > 256 * 1024) {
            await reader.cancel();
            writeJson(response, 502, { error: 'openproof_reply_too_large' });
            return;
          }
          chunks.push(Buffer.from(part.value));
        }
        const decoded = JSON.parse(Buffer.concat(chunks, size).toString('utf8'));
        if (!decoded || !Array.isArray(decoded.connections)) {
          writeJson(response, 502, { error: 'openproof_invalid_connections' }); return;
        }
        // Only the identity linkage document is returned, never the bearer.
        response.setHeader('content-type', 'application/json; charset=utf-8');
        response.end(JSON.stringify({ connections: decoded.connections }));
      } catch {
        writeJson(response, 502, { error: 'openproof_unavailable' });
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/openproof/connections/handoff'
        || requestUrl.pathname === '/v1/browser/openproof/connections/disconnect') {
      // Fixed-scope browser gateway. The OpenProof issuer itself does not
      // expose a browser CORS contract. Never accept a user-supplied upstream
      // URL, canonical subject or arbitrary callback. Bearer is checked by
      // OpenProof, and account mutations remain bound to that identity.
      response.setHeader('access-control-allow-origin', '*');
      response.setHeader('access-control-allow-methods', 'POST, OPTIONS');
      response.setHeader('access-control-allow-headers', 'authorization,content-type');
      response.setHeader('cache-control', 'no-store');
      response.setHeader('x-content-type-options', 'nosniff');
      if (request.method === 'OPTIONS') { response.writeHead(204); response.end(); return; }
      if (request.method !== 'POST') {
        writeJson(response, 405, {error:'method_not_allowed'}); return;
      }
      const authorization = String(request.headers.authorization || '');
      if (authorization.length > 8192 ||
          !/^Bearer [A-Za-z0-9._~+\/-]+={0,2}$/.test(authorization)) {
        writeJson(response, 401, {error:'bearer_token_required'}); return;
      }
      const rate = limiter.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) {
        writeJson(response, 429, {error:'rate_limited'}); return;
      }
      let payload;
      try { payload = safeJson(await readBody(request, 2048)); }
      catch {
        writeJson(response, 413, {error:'request_too_large'}); return;
      }
      const provider = payload?.provider;
      if (typeof provider !== 'string' || !/^(x|github|google|linkedin|telegram)$/.test(provider)) {
        writeJson(response, 400, {error:'unsupported_provider'}); return;
      }
      const handoff = requestUrl.pathname.endsWith('/handoff');
      if ((handoff && Object.keys(payload).some(key => key !== 'provider'))
          || (!handoff && (typeof payload.subject !== 'string'
              || payload.subject.length < 1 || payload.subject.length > 512
              || Object.keys(payload).some(key => key !== 'provider' && key !== 'subject')))) {
        writeJson(response, 400, {error:'invalid_connection_request'}); return;
      }
      const upstreamPath = handoff ? '/account/connections/handoff'
                                   : '/account/connections/disconnect';
      const requestJson = handoff
        ? {provider, return_to:'/openproof/account/connections/complete'}
        : {provider,subject:payload.subject};
      const delegated = accountHeaders(authorization);
      if (!delegated) {
        writeJson(response,401,{error:'openproof_unauthorized'});return;
      }
      try {
        const upstream = await runtime.fetchImpl(
          config.openProofIssuer.href.replace(/\/$/,'') + upstreamPath, {
            method:'POST',
            headers:{...delegated,'content-type':'application/json'},
            body:JSON.stringify(requestJson),
            redirect:'error',
            signal:AbortSignal.timeout(8_000),
          });
        if (upstream.status === 401 || upstream.status === 403) {
          writeJson(response, 401, {error:'openproof_unauthorized'}); return;
        }
        if (upstream.status === 412) {
          writeJson(response, 412, {error:'last_sign_in_method'}); return;
        }
        if ((handoff && upstream.status !== 201)
            || (!handoff && upstream.status !== 200)
            || !String(upstream.headers.get('content-type') || '')
              .toLowerCase().startsWith('application/json')) {
          writeJson(response, 502, {error:'openproof_unavailable'}); return;
        }
        const contentLength = Number(upstream.headers.get('content-length') || 0);
        if (contentLength > 4096) {
          writeJson(response, 502, {error:'openproof_invalid_reply'}); return;
        }
        if (!upstream.body) {
          writeJson(response,502,{error:'openproof_invalid_reply'});return;
        }
        const reader = upstream.body.getReader();
        const chunks = [];
        let size = 0;
        while (true) {
          const {done,value} = await reader.read();
          if (done) break;
          size += value.byteLength;
          if (size > 4096) {
            await reader.cancel();
            writeJson(response,502,{error:'openproof_invalid_reply'});return;
          }
          chunks.push(Buffer.from(value));
        }
        const raw = Buffer.concat(chunks,size);
        const decoded = safeJson(raw);
        if (handoff) {
          let url;
          try { url = new URL(decoded?.handoff_url,config.openProofIssuer); }
          catch { writeJson(response,502,{error:'openproof_invalid_handoff'});return; }
          if (url.origin !== config.openProofIssuer.origin
              || url.pathname !== '/account/connections/handoff'
              || !url.searchParams.get('ticket') || url.username || url.password
              || url.hash) {
            writeJson(response,502,{error:'openproof_invalid_handoff'});return;
          }
          // Do not redirect the browser to the API issuer. Telegram (and
          // other providers) returns to the registered callback on
          // genyleap.com; __Host- state cookies must be minted on that host.
          // The opaque ticket is validated server-side before any redirect.
          const browserUrl = new URL(config.connectionBrowserBase.href);
          browserUrl.pathname += '/account/connections/handoff';
          browserUrl.searchParams.set('ticket', url.searchParams.get('ticket'));
          writeJson(response,200,{
            handoff_url:browserUrl.href,expires_at:decoded.expires_at,
          });
        } else if (decoded?.disconnected === true) {
          writeJson(response,200,{disconnected:true});
        } else {
          writeJson(response,502,{error:'openproof_invalid_reply'});
        }
      } catch {
        writeJson(response,502,{error:'openproof_unavailable'});
      }
      return;
    }

    if (requestUrl.pathname === '/v1/browser/tokens') {
      const origin = request.headers.origin;
      if (origin === 'https://caster.genyleap.com') {
        response.setHeader('access-control-allow-origin', origin);
        response.setHeader('vary', 'origin');
      }
      response.setHeader('access-control-allow-methods', 'GET, OPTIONS');
      response.setHeader('access-control-allow-headers', 'accept');
      if (request.method === 'OPTIONS') {
        response.writeHead(origin === 'https://caster.genyleap.com' ? 204 : 403);
        response.end();
        return;
      }
      if (request.method !== 'GET') {
        writeJson(response, 405, { error: 'method_not_allowed' }); return;
      }
      const chain = requestUrl.searchParams.get('network') || '';
      const address = requestUrl.searchParams.get('address') || '';
      if (!['base', 'eth'].includes(chain) || !/^0x[0-9a-f]{40}$/i.test(address)
          || [...requestUrl.searchParams.keys()].some(key => !['network', 'address'].includes(key))) {
        writeJson(response, 400, { error: 'invalid_token_reference' }); return;
      }
      const id = chain + ':' + address.toLowerCase();
      const cached = tokenSnapshots.get(id);
      if (cached && cached.expires > runtime.now()) {
        writeJson(response, 200, cached.data, { 'cache-control': 'public, max-age=45' });
        return;
      }
      const rate = limiter.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) { writeJson(response, 429, { error: 'rate_limited' }); return; }
      const resolved = await publicTokenSnapshot(chain, address, runtime.fetchImpl);
      if (resolved.status !== 200) {
        writeJson(response, resolved.status, { error: resolved.error }); return;
      }
      if (tokenSnapshots.size > 250) tokenSnapshots.clear();
      tokenSnapshots.set(id, { data: resolved.data, expires: runtime.now() + 45_000 });
      writeJson(response, 200, resolved.data, { 'cache-control': 'public, max-age=45' });
      return;
    }

    if (requestUrl.pathname === '/v1/browser/images') {
      // Public raster bytes only. Browser clients cannot read many external
      // image CDNs directly because of cross-origin policy.
      response.setHeader('access-control-allow-origin', '*');
      response.setHeader('access-control-allow-methods', 'GET, OPTIONS');
      response.setHeader('access-control-allow-headers', 'accept');
      response.setHeader('cross-origin-resource-policy', 'cross-origin');
      response.setHeader('x-content-type-options', 'nosniff');
      if (request.method === 'OPTIONS') { response.writeHead(204); response.end(); return; }
      if (request.method !== 'GET') {
        writeJson(response, 405, { error: 'method_not_allowed' }); return;
      }
      const requested = requestUrl.searchParams.get('url');
      const declaringApp = requestUrl.searchParams.get('miniapp');
      // Do not let a caller claim that arbitrary media belongs to a Mini
      // App: the gateway verifies the origin's pinned public manifest, then
      // grants ONLY the exact image URLs declared there.
      if ([...requestUrl.searchParams.keys()].some(key=>!['url','miniapp'].includes(key))
          || (declaringApp !== null && (!validMiniAppTarget(declaringApp)
              || declaringApp.length > 2048))) {
        writeJson(response,400,{error:'invalid_image_scope'});return;
      }
      const verifiedArtwork = typeof requested === 'string'
        && ((verifiedMiniAppIcons.get(requested) || 0) > runtime.now()
            || (verifiedMiniAppImages.get(requested) || 0) > runtime.now());
      const limit = verifiedArtwork ? miniAppArtworkLimiter : limiter;
      const rate = limit.check(clientAddress(request, config.trustCloudflare), runtime.now());
      if (!rate.allowed) {
        writeJson(response, 429, { error: 'rate_limited' }, { 'retry-after': '60' });
        return;
      }
      let allowedHosts;
      if (declaringApp && typeof requested === 'string'
          && !validImageUrl(requested)
          && (verifiedMiniAppIcons.get(requested) || 0) <= runtime.now()
          && (verifiedMiniAppImages.get(requested) || 0) <= runtime.now()) {
        const cached = miniAppArtworkScopes.get(declaringApp);
        let verified = cached?.verifiedImageUrls;
        if (!Array.isArray(verified) || cached.expires <= runtime.now()) {
          const resolved = await resolveMiniAppPreview(declaringApp);
          verified = resolved.status === 200 && resolved.data?.resolved === true
            ? resolved.data.verifiedImageUrls : null;
          if (Array.isArray(verified)) {
            // Reuse a short-lived, bounded exact-artwork grant after the
            // first verified manifest read, including cold saved libraries.
            if (miniAppArtworkScopes.size > 180) miniAppArtworkScopes.clear();
            miniAppArtworkScopes.set(declaringApp, {
              verifiedImageUrls:verified.slice(0, 8),
              expires:runtime.now()+120000
            });
          }
        }
        if (Array.isArray(verified) && verified.includes(requested))
          verifiedMiniAppImages.set(requested,runtime.now()+300000);
      }
      // First-party catalogue icons and verified OpenGraph images may live
      // outside the static CDN allowlist. Both must match an exact short-lived
      // server-validated URL, never a user-supplied arbitrary image source. Refresh the
      // verified set lazily after a server restart or browser cache hit.
      if (typeof requested === 'string' && !validImageUrl(requested)) {
        if ((verifiedLinkImages.get(requested) || 0) > runtime.now()
            || (verifiedMiniAppImages.get(requested) || 0) > runtime.now()) {
          try {
            const checked = new URL(requested);
            if (checked.protocol === 'https:' && !checked.username && !checked.password
                && !checked.hash && (!checked.port || checked.port === '443'))
              allowedHosts = [...defaultBrowserImageHosts, checked.hostname];
          } catch {}
        }
        let candidate = null;
        try { candidate = new URL(requested); } catch {}
        if (!allowedHosts && candidate?.protocol === 'https:' && !candidate.username
            && !candidate.password && !candidate.hash && (!candidate.port || candidate.port === '443')) {
          if ((verifiedMiniAppIcons.get(candidate.href) || 0) <= runtime.now())
            await refreshMiniAppIconCatalogue();
          if ((verifiedMiniAppIcons.get(candidate.href) || 0) > runtime.now())
            allowedHosts = [...defaultBrowserImageHosts, candidate.hostname];
        }
        if (!allowedHosts) {
          writeJson(response, 400, { error: 'image_source_not_allowed' });
          return;
        }
      }
      const verifiedMiniAppArtwork = typeof requested === 'string'
        && ((verifiedMiniAppIcons.get(requested) || 0) > runtime.now()
            || (verifiedMiniAppImages.get(requested) || 0) > runtime.now());
      const options = allowedHosts ? { allowedHosts } : {};
      let result;
      const cachedArtwork = verifiedMiniAppArtwork
        ? verifiedArtworkCache.get(requested) : null;
      if (cachedArtwork && cachedArtwork.expires > runtime.now()) {
        result = {status:200,body:cachedArtwork.body,contentType:cachedArtwork.contentType};
      } else if (verifiedMiniAppArtwork) {
        if (cachedArtwork) {
          verifiedArtworkCacheBytes -= cachedArtwork.body.length;
          verifiedArtworkCache.delete(requested);
        }
        let pending = verifiedArtworkPending.get(requested);
        if (!pending) {
          pending = (async()=>{
            try {
              let fetched = await retrieveImage(requested, options);
              // A verified, exact URL can have a transient origin/CDN error.
              // Retry only one time and only for upstream failures, never for
              // denied URLs, invalid raster types or oversized payloads.
              if (fetched.status === 502)
                fetched = await retrieveImage(requested, options);
              if (fetched.status === 200)
                rememberVerifiedArtwork(requested, fetched);
              return fetched;
            } catch {
              return {status:502,error:'image_network_failed'};
            } finally {
              verifiedArtworkPending.delete(requested);
            }
          })();
          verifiedArtworkPending.set(requested,pending);
        }
        result = await pending;
      } else {
        result = await retrieveImage(requested, options);
      }
      if (result.status !== 200) {
        writeJson(response, result.status, { error: result.error });
        return;
      }
      response.writeHead(200, {
        'content-type': result.contentType,
        'content-length': String(result.body.length),
        'cache-control': 'public, max-age=3600',
      });
      response.end(result.body);
      return;
    }

    if (requestUrl.pathname === '/healthz' && request.method === 'GET') {
      writeJson(response, 200, { status: 'ok', storage: 'configured' });
      return;
    }

    if (requestUrl.pathname.startsWith('/m/') && (request.method === 'GET' || request.method === 'HEAD')) {
      let key;
      try {
        key = decodeURIComponent(requestUrl.pathname.slice(3));
      } catch {
        writeJson(response, 404, { error: 'asset_not_found' });
        return;
      }
      if (!validObjectKey(key)) {
        writeJson(response, 404, { error: 'asset_not_found' });
        return;
      }
      try {
        if (request.method === 'HEAD') {
          const object = await storage.headObject(key);
          const headers = {
            'cache-control': 'public, max-age=300',
            'x-content-type-options': 'nosniff',
          };
          if (object.contentType) headers['content-type'] = object.contentType;
          if (Number.isSafeInteger(object.sizeBytes) && object.sizeBytes >= 0) {
            headers['content-length'] = String(object.sizeBytes);
          }
          if (object.etag) headers.etag = object.etag;
          response.writeHead(200, headers);
          response.end();
          return;
        }

        const imageTypes = new Map([
          ['jpg', 'image/jpeg'], ['png', 'image/png'],
          ['webp', 'image/webp'], ['gif', 'image/gif'], ['avif', 'image/avif'],
        ]);
        const extension = key.slice(key.lastIndexOf('.') + 1).toLowerCase();
        const expectedImageType = imageTypes.get(extension);
        if (expectedImageType) {
          // Farcaster's media crawler needs a stable HTTPS URL whose GET
          // returns actual image bytes and a correct MIME type. A 307 to a
          // short-lived, signed QStorage URL is only a link preview to some
          // external clients, even when the CastAdd embed is well formed.
          const bytes = await storage.readImageObject(key, config.maxImageBytes);
          const mime = rasterMimeFromBytes(bytes);
          if (mime !== expectedImageType) {
            writeJson(response, 415, { error: 'invalid_image_payload' });
            return;
          }
          response.writeHead(200, {
            'content-type': mime,
            'content-length': String(bytes.length),
            'cache-control': 'public, max-age=120',
            'access-control-allow-origin': '*',
            'cross-origin-resource-policy': 'cross-origin',
            'x-content-type-options': 'nosniff',
          });
          response.end(bytes);
          return;
        }
        // Larger video media continue using short-lived downloads; unlike
        // static images these require streaming/range support before proxying.
        const location = storage.createDownload({
          key,
          method: 'GET',
          expiresSeconds: config.downloadUrlLifetimeSeconds,
        });
        response.writeHead(307, {
          location,
          'cache-control': 'private, no-store, max-age=0',
          'cdn-cache-control': 'no-store',
          'cloudflare-cdn-cache-control': 'no-store',
          'x-content-type-options': 'nosniff',
        });
        response.end();
      } catch (error) {
        const status = error?.statusCode === 404 ? 404 : 503;
        writeJson(response, status, { error: status === 404
          ? 'asset_not_found' : 'media_gateway_unavailable' });
      }
      return;
    }

    // Uploads require a network-verified Ed25519 Farcaster signer proof.
    // Permit browser CORS only from the first-party GenyCaster app. This is
    // not a public unsigned upload endpoint; all POST/PUT/DELETE operations
    // continue through verifyProof and the replay cache.
    const browserUploadPath = requestUrl.pathname === '/v1/uploads'
      || requestUrl.pathname === '/v1/uploads/complete'
      || /^\/v1\/uploads\/[^/]+\/parts\/[1-9][0-9]{0,4}$/.test(requestUrl.pathname);
    if (browserUploadPath && request.headers.origin === 'https://caster.genyleap.com') {
      response.setHeader('access-control-allow-origin', 'https://caster.genyleap.com');
      response.setHeader('vary', 'origin');
      response.setHeader('access-control-allow-methods', 'POST, PUT, DELETE, OPTIONS');
      response.setHeader('access-control-allow-headers',
        'content-type, x-genycaster-fid, x-genycaster-signer-key, x-genycaster-timestamp, x-genycaster-nonce, x-genycaster-signature');
      response.setHeader('cache-control','no-store');
    }
    if (browserUploadPath && request.method === 'OPTIONS') {
      response.writeHead(request.headers.origin === 'https://caster.genyleap.com' ? 204 : 403);
      response.end();
      return;
    }

    const partMatch = /^\/v1\/uploads\/([^/]+)\/parts\/([1-9][0-9]{0,4})$/.exec(requestUrl.pathname);
    const createRoute = request.method === 'POST' && requestUrl.pathname === '/v1/uploads';
    const completeRoute = request.method === 'POST' && requestUrl.pathname === '/v1/uploads/complete';
    const abortRoute = request.method === 'DELETE' && requestUrl.pathname === '/v1/uploads';
    const deleteAssetRoute = request.method === 'DELETE' && requestUrl.pathname === '/v1/assets';
    const partRoute = request.method === 'PUT' && partMatch !== null;
    if (!createRoute && !completeRoute && !abortRoute && !deleteAssetRoute && !partRoute) {
      writeJson(response, 404, { error: 'route_not_available' });
      return;
    }

    const clientKey = clientAddress(request, config.trustCloudflare);
    const rate = (partRoute ? partLimiter : uploadLimiter).check(clientKey, runtime.now());
    response.setHeader('ratelimit-limit', String(partRoute ? config.partRateLimit : config.rateLimit));
    response.setHeader('ratelimit-remaining', String(rate.remaining));
    if (!rate.allowed) {
      writeJson(response, 429, { error: 'rate_limited' }, { 'retry-after': String(Math.ceil(config.rateWindowMs / 1000)) });
      return;
    }

    try {
      if (partRoute) {
        const nowSeconds = Math.floor(runtime.now() / 1000);
        const ticket = decodeTicket(config, partMatch[1], nowSeconds);
        if (!ticket || ticket.v !== 2 || !ticket.providerUploadId ||
            !Number.isSafeInteger(ticket.sizeBytes) || !Number.isSafeInteger(ticket.partSizeBytes)) {
          writeJson(response, 403, { error: 'invalid_upload_ticket' });
          return;
        }
        const partNumber = Number.parseInt(partMatch[2], 10);
        const expectedBytes = expectedPartBytes(ticket.sizeBytes, ticket.partSizeBytes, partNumber);
        if (!Number.isSafeInteger(expectedBytes) || expectedBytes <= 0) {
          writeJson(response, 422, { error: 'invalid_upload_part' });
          return;
        }
        const body = await readBody(request, expectedBytes);
        if (body.length !== expectedBytes) {
          writeJson(response, 409, {
            error: 'upload_part_size_mismatch',
            message: `Upload part ${partNumber} must contain exactly ${expectedBytes} bytes.`,
          });
          return;
        }
        const authorization = await verifyProof(
          config,
          request,
          body,
          requestUrl.pathname,
          runtime,
          replayCache,
          { verifySignerState: false },
        );
        if (!ticketMatchesAuthorization(ticket, authorization)) {
          writeJson(response, 403, { error: 'invalid_upload_ticket' });
          return;
        }
        const uploaded = await storage.uploadPart({
          key: ticket.key,
          providerUploadId: ticket.providerUploadId,
          partNumber,
          body,
        });
        writeJson(response, 200, {
          partNumber,
          etag: uploaded.etag,
          sizeBytes: body.length,
        });
        return;
      }

      const body = await readBody(request, config.maxJsonBytes);
      const authorization = await verifyProof(
        config,
        request,
        body,
        requestUrl.pathname,
        runtime,
        replayCache,
        { verifySignerState: createRoute || deleteAssetRoute },
      );
      const document = safeJson(body);
      if (!document) {
        writeJson(response, 400, { error: 'invalid_json' });
        return;
      }

      if (deleteAssetRoute) {
        const managed = decodeAssetManagementToken(config, document.managementToken);
        if (!managed || managed.fid !== authorization.fid) {
          writeJson(response, 403, { error: 'invalid_asset_management_token' });
          return;
        }
        await storage.deleteObject(managed.key);
        response.writeHead(204, { 'cache-control': 'no-store' });
        response.end();
        return;
      }

      if (createRoute) {
        const fileName = sanitizeFileName(document.fileName);
        const contentType = String(document.contentType || '').toLowerCase().trim();
        const sizeBytes = Number(document.sizeBytes);
        const definition = Number.isSafeInteger(sizeBytes) ? mediaDefinition(contentType, sizeBytes, config) : null;
        if (!definition) {
          writeJson(response, 422, { error: 'unsupported_media', message: 'Unsupported media type or file size.' });
          return;
        }
        const partCount = multipartPartCount(sizeBytes, config.multipartPartBytes);
        if (partCount < 1 || partCount > 10000) {
          writeJson(response, 422, { error: 'unsupported_media', message: 'Media requires too many upload parts.' });
          return;
        }
        const key = objectKey(definition, runtime.now());
        const stableAssetUrl = assetUrl(config, key);
        const multipart = await storage.beginMultipartUpload({ key, contentType });
        const nowSeconds = Math.floor(runtime.now() / 1000);
        const ticket = encodeTicket(config, {
          v: 2,
          key,
          fileName,
          contentType,
          sizeBytes,
          assetUrl: stableAssetUrl,
          fid: authorization.fid,
          signerKey: authorization.signerKey,
          providerUploadId: multipart.providerUploadId,
          partSizeBytes: config.multipartPartBytes,
          exp: nowSeconds + config.ticketLifetimeSeconds,
        });
        writeJson(response, 201, {
          uploadId: ticket,
          mode: 'gateway-multipart',
          partSizeBytes: config.multipartPartBytes,
          partCount,
          assetUrl: stableAssetUrl,
          expiresIn: config.ticketLifetimeSeconds,
        });
        return;
      }

      const nowSeconds = Math.floor(runtime.now() / 1000);
      const ticket = decodeTicket(config, document.uploadId, nowSeconds);
      if (!ticket || ticket.v !== 2 || !ticket.providerUploadId ||
          !ticketMatchesAuthorization(ticket, authorization)) {
        writeJson(response, 403, { error: 'invalid_upload_ticket' });
        return;
      }

      if (abortRoute) {
        await storage.abortMultipartUpload({
          key: ticket.key,
          providerUploadId: ticket.providerUploadId,
        });
        response.writeHead(204, { 'cache-control': 'no-store' });
        response.end();
        return;
      }

      const expectedCount = multipartPartCount(ticket.sizeBytes, ticket.partSizeBytes);
      const parts = validatedCompletionParts(document.parts, expectedCount);
      if (!parts) {
        writeJson(response, 422, {
          error: 'invalid_upload_parts',
          message: 'Completion must contain one ordered ETag for every uploaded part.',
        });
        return;
      }

      await storage.completeMultipartUpload({
        key: ticket.key,
        providerUploadId: ticket.providerUploadId,
        parts,
      });
      const object = await storage.headObject(ticket.key);
      if (object.sizeBytes !== ticket.sizeBytes) {
        writeJson(response, 409, { error: 'upload_size_mismatch', message: 'Uploaded object size does not match the ticket.' });
        return;
      }
      if (object.contentType && object.contentType.split(';', 1)[0].trim().toLowerCase() !== ticket.contentType) {
        writeJson(response, 409, { error: 'upload_type_mismatch', message: 'Uploaded object content type does not match the ticket.' });
        return;
      }
      writeJson(response, 200, {
        assetUrl: ticket.assetUrl,
        contentType: ticket.contentType,
        fileName: ticket.fileName,
        sizeBytes: ticket.sizeBytes,
        managementToken: encodeAssetManagementToken(config, {
          key: ticket.key,
          fid: ticket.fid,
        }),
      });
    } catch (error) {
      const status = Number.isSafeInteger(error?.statusCode) ? error.statusCode : 500;
      console.error('Genycaster media gateway request failed', {
        method: request.method,
        path: requestUrl.pathname,
        status,
        message: String(error?.message || 'unknown error').slice(0, 500),
      });
      writeJson(response, status, {
        error: status >= 500 ? 'media_gateway_unavailable' : 'media_upload_rejected',
        message: status >= 500 ? 'Media storage is temporarily unavailable.' : String(error.message || 'Media upload rejected.'),
      });
    }
  });
}
