import http from 'node:http';

const allowedRoutes = new Map([
  ['/v1/signersByFid', new Set(['GET'])],
  ['/v1/submitMessage', new Set(['POST'])],
  ['/v1/castById', new Set(['GET'])],
  ['/v1/reactionById', new Set(['GET'])],
  ['/v1/linkById', new Set(['GET'])],
]);

const responseHeadersToForward = new Set([
  'content-type',
  'retry-after',
  'ratelimit-limit',
  'ratelimit-remaining',
  'ratelimit-reset',
  'x-ratelimit-limit',
  'x-ratelimit-remaining',
  'x-ratelimit-reset',
]);

function positiveInteger(value, fallback, name) {
  if (value === undefined || value === '') return fallback;
  const parsed = Number.parseInt(value, 10);
  if (!Number.isSafeInteger(parsed) || parsed <= 0) {
    throw new Error(`${name} must be a positive integer.`);
  }
  return parsed;
}

function normalizeBaseUrl(value) {
  const url = new URL(value);
  if (url.protocol !== 'https:' && url.protocol !== 'http:') {
    throw new Error('GENYLEAP_SNAPCHAIN_UPSTREAM_BASE_URL must use http or https.');
  }
  if (url.username || url.password || url.search || url.hash) {
    throw new Error('GENYLEAP_SNAPCHAIN_UPSTREAM_BASE_URL must be a credential-free root URL.');
  }
  url.pathname = url.pathname.replace(/\/+$/, '') || '/';
  return url;
}

function validHeaderName(value) {
  return /^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/.test(value);
}

export function loadConfig(env = process.env) {
  const upstreamRaw = env.GENYLEAP_SNAPCHAIN_UPSTREAM_BASE_URL?.trim();
  if (!upstreamRaw) {
    throw new Error('GENYLEAP_SNAPCHAIN_UPSTREAM_BASE_URL is required.');
  }

  const authHeader = env.GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_HEADER?.trim() ?? '';
  const authValue = env.GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_VALUE ?? '';
  if (Boolean(authHeader) !== Boolean(authValue)) {
    throw new Error('GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_HEADER and AUTH_VALUE must be configured together.');
  }
  if (authHeader && !validHeaderName(authHeader)) {
    throw new Error('GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_HEADER contains invalid HTTP token characters.');
  }
  if (authValue.includes('\r') || authValue.includes('\n')) {
    throw new Error('GENYLEAP_SNAPCHAIN_UPSTREAM_AUTH_VALUE must not contain line breaks.');
  }

  return Object.freeze({
    upstreamBaseUrl: normalizeBaseUrl(upstreamRaw),
    upstreamAuthHeader: authHeader,
    upstreamAuthValue: authValue,
    host: env.GENYLEAP_SNAPCHAIN_HOST?.trim() || '127.0.0.1',
    port: positiveInteger(env.GENYLEAP_SNAPCHAIN_PORT, 8787, 'GENYLEAP_SNAPCHAIN_PORT'),
    timeoutMs: positiveInteger(
      env.GENYLEAP_SNAPCHAIN_REQUEST_TIMEOUT_MS,
      20_000,
      'GENYLEAP_SNAPCHAIN_REQUEST_TIMEOUT_MS',
    ),
    maxRequestBytes: positiveInteger(
      env.GENYLEAP_SNAPCHAIN_MAX_REQUEST_BYTES,
      128 * 1024,
      'GENYLEAP_SNAPCHAIN_MAX_REQUEST_BYTES',
    ),
    maxResponseBytes: positiveInteger(
      env.GENYLEAP_SNAPCHAIN_MAX_RESPONSE_BYTES,
      8 * 1024 * 1024,
      'GENYLEAP_SNAPCHAIN_MAX_RESPONSE_BYTES',
    ),
    rateWindowMs: positiveInteger(
      env.GENYLEAP_SNAPCHAIN_RATE_WINDOW_MS,
      10_000,
      'GENYLEAP_SNAPCHAIN_RATE_WINDOW_MS',
    ),
    getRateLimit: positiveInteger(
      env.GENYLEAP_SNAPCHAIN_GET_RATE_LIMIT,
      60,
      'GENYLEAP_SNAPCHAIN_GET_RATE_LIMIT',
    ),
    postRateLimit: positiveInteger(
      env.GENYLEAP_SNAPCHAIN_POST_RATE_LIMIT,
      10,
      'GENYLEAP_SNAPCHAIN_POST_RATE_LIMIT',
    ),
    trustCloudflare: env.GENYLEAP_SNAPCHAIN_TRUST_CLOUDFLARE === '1',
  });
}

class FixedWindowRateLimiter {
  constructor(windowMs, getLimit, postLimit) {
    this.windowMs = windowMs;
    this.getLimit = getLimit;
    this.postLimit = postLimit;
    this.entries = new Map();
  }

  check(key, method, now = Date.now()) {
    const limit = method === 'POST' ? this.postLimit : this.getLimit;
    const bucketKey = `${method}:${key}`;
    let entry = this.entries.get(bucketKey);
    if (!entry || now >= entry.resetAt) {
      entry = { count: 0, resetAt: now + this.windowMs };
      this.entries.set(bucketKey, entry);
    }
    entry.count += 1;
    const retryAfterSeconds = Math.max(1, Math.ceil((entry.resetAt - now) / 1000));
    return {
      allowed: entry.count <= limit,
      limit,
      remaining: Math.max(0, limit - entry.count),
      retryAfterSeconds,
    };
  }

  prune(now = Date.now()) {
    for (const [key, entry] of this.entries) {
      if (now >= entry.resetAt + this.windowMs) this.entries.delete(key);
    }
  }
}

function clientAddress(request, trustCloudflare) {
  if (trustCloudflare) {
    const address = request.headers['cf-connecting-ip'];
    if (typeof address === 'string' && address.trim()) return address.trim();
  }
  return request.socket.remoteAddress || 'unknown';
}

function writeJson(response, statusCode, payload, extraHeaders = {}) {
  const body = Buffer.from(JSON.stringify(payload));
  response.writeHead(statusCode, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': String(body.length),
    'cache-control': 'no-store',
    'x-content-type-options': 'nosniff',
    ...extraHeaders,
  });
  response.end(body);
}

async function readRequestBody(request, maxBytes) {
  const chunks = [];
  let total = 0;
  for await (const chunk of request) {
    total += chunk.length;
    if (total > maxBytes) {
      const error = new Error('request body too large');
      error.code = 'REQUEST_TOO_LARGE';
      throw error;
    }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks, total);
}

async function readResponseBody(upstreamResponse, maxBytes) {
  if (!upstreamResponse.body) return Buffer.alloc(0);
  const reader = upstreamResponse.body.getReader();
  const chunks = [];
  let total = 0;
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    total += value.byteLength;
    if (total > maxBytes) {
      await reader.cancel();
      const error = new Error('upstream response too large');
      error.code = 'RESPONSE_TOO_LARGE';
      throw error;
    }
    chunks.push(Buffer.from(value));
  }
  return Buffer.concat(chunks, total);
}

function upstreamHeaders(request, config) {
  const headers = new Headers();
  const accept = request.headers.accept;
  const contentType = request.headers['content-type'];
  if (typeof accept === 'string' && accept) headers.set('accept', accept);
  if (typeof contentType === 'string' && contentType) headers.set('content-type', contentType);
  headers.set('user-agent', 'Genyleap-Snapchain-Gateway/1');
  if (config.upstreamAuthHeader) {
    headers.set(config.upstreamAuthHeader, config.upstreamAuthValue);
  }
  return headers;
}

function forwardResponseHeaders(upstreamResponse) {
  const headers = {
    'cache-control': 'no-store',
    'x-content-type-options': 'nosniff',
  };
  for (const [name, value] of upstreamResponse.headers) {
    if (responseHeadersToForward.has(name.toLowerCase())) headers[name] = value;
  }
  return headers;
}

export function createGateway(config, dependencies = {}) {
  const fetchImpl = dependencies.fetchImpl ?? globalThis.fetch;
  const limiter = dependencies.limiter ?? new FixedWindowRateLimiter(
    config.rateWindowMs,
    config.getRateLimit,
    config.postRateLimit,
  );
  const pruneTimer = setInterval(() => limiter.prune(), Math.max(config.rateWindowMs, 10_000));
  pruneTimer.unref();

  const server = http.createServer(async (request, response) => {
    response.setHeader('server', '');
    let requestUrl;
    try {
      requestUrl = new URL(request.url ?? '/', 'http://gateway.invalid');
    } catch {
      writeJson(response, 400, { error: 'invalid_request_url' });
      return;
    }

    if (requestUrl.pathname === '/healthz' && request.method === 'GET') {
      writeJson(response, 200, { status: 'ok' });
      return;
    }

    const methods = allowedRoutes.get(requestUrl.pathname);
    if (!methods) {
      writeJson(response, 404, { error: 'route_not_available' });
      return;
    }
    if (!methods.has(request.method ?? '')) {
      response.setHeader('allow', [...methods].join(', '));
      writeJson(response, 405, { error: 'method_not_allowed' });
      return;
    }

    const rate = limiter.check(clientAddress(request, config.trustCloudflare), request.method);
    response.setHeader('ratelimit-limit', String(rate.limit));
    response.setHeader('ratelimit-remaining', String(rate.remaining));
    if (!rate.allowed) {
      writeJson(
        response,
        429,
        { error: 'rate_limited' },
        { 'retry-after': String(rate.retryAfterSeconds) },
      );
      return;
    }

    let body;
    try {
      body = request.method === 'POST'
        ? await readRequestBody(request, config.maxRequestBytes)
        : undefined;
    } catch (error) {
      if (error?.code === 'REQUEST_TOO_LARGE') {
        writeJson(response, 413, { error: 'request_too_large' });
        return;
      }
      writeJson(response, 400, { error: 'invalid_request_body' });
      return;
    }

    const upstreamUrl = new URL(config.upstreamBaseUrl);
    const prefix = upstreamUrl.pathname === '/' ? '' : upstreamUrl.pathname.replace(/\/$/, '');
    upstreamUrl.pathname = `${prefix}${requestUrl.pathname}`;
    upstreamUrl.search = requestUrl.search;

    try {
      const upstreamResponse = await fetchImpl(upstreamUrl, {
        method: request.method,
        headers: upstreamHeaders(request, config),
        body,
        redirect: 'manual',
        signal: AbortSignal.timeout(config.timeoutMs),
      });

      if (upstreamResponse.status >= 300 && upstreamResponse.status < 400) {
        writeJson(response, 502, { error: 'upstream_redirect_rejected' });
        return;
      }

      const responseBody = await readResponseBody(upstreamResponse, config.maxResponseBytes);
      response.writeHead(upstreamResponse.status, {
        ...forwardResponseHeaders(upstreamResponse),
        'content-length': String(responseBody.length),
      });
      response.end(responseBody);
    } catch (error) {
      if (error?.code === 'RESPONSE_TOO_LARGE') {
        writeJson(response, 502, { error: 'upstream_response_too_large' });
        return;
      }
      const timeout = error?.name === 'TimeoutError' || error?.name === 'AbortError';
      writeJson(response, timeout ? 504 : 502, {
        error: timeout ? 'upstream_timeout' : 'upstream_unavailable',
      });
    }
  });

  server.on('close', () => clearInterval(pruneTimer));
  return server;
}
