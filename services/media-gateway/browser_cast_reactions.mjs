// Public, read-only positive evidence of a viewer's reactions to casts.
// This is NOT a replacement for authenticated protocol reads. The public
// Farcaster API has bounded reaction pages: absence is unknown, never "false".
// A verified match requires both the exact cast hash (fixed URL) and reactor FID.

const MAX_BATCH = 8;
const MAX_RESULTS = 100;
const MAX_RESPONSE_CHARS = 3 * 1024 * 1024;
const CACHE_TTL_MS = 90_000;
const MAX_CACHE = 256;

function normalizeHash(raw) {
  const value = String(raw || '').trim().toLowerCase();
  const hash = value.startsWith('0x') ? value : '0x' + value;
  return /^0x[0-9a-f]{40}$/.test(hash) ? hash : '';
}

function normalizeFid(raw) {
  const fid = Number(raw);
  return Number.isSafeInteger(fid) && fid > 0 ? fid : 0;
}

export function normalizeReactionBatch(input) {
  if (!input || typeof input !== 'object' || Array.isArray(input)
      || !Array.isArray(input.casts) || input.casts.length < 1
      || input.casts.length > MAX_BATCH) return null;
  const viewerFid = normalizeFid(input.viewerFid);
  if (!viewerFid) return null;
  const deduplicated = new Set();
  const casts = [];
  for (const item of input.casts) {
    const hash = normalizeHash(item);
    if (!hash) return null;
    if (!deduplicated.has(hash)) {
      deduplicated.add(hash);
      casts.push(hash);
    }
  }
  return {viewerFid, casts};
}

export function createBrowserReactionService({
  fetchImpl = globalThis.fetch,
  now = () => Date.now(),
} = {}) {
  const cached = new Map();
  const inFlight = new Map();

  async function queryCast(hash) {
    const current = cached.get(hash);
    if (current && current.expiresAt > now()) return current;
    if (inFlight.has(hash)) return inFlight.get(hash);
    const task = (async () => {
      const url = new URL('https://api.farcaster.xyz/v2/cast-reactions');
      url.searchParams.set('castHash', hash);
      url.searchParams.set('limit', String(MAX_RESULTS));
      const response = await fetchImpl(url, {
        method: 'GET',
        redirect: 'error',
        headers: {accept: 'application/json'},
        signal: AbortSignal.timeout(5_000),
      });
      if (!response.ok || !String(response.headers.get('content-type') || '')
          .toLowerCase().startsWith('application/json')) {
        await response.body?.cancel();
        throw new Error('public reaction lookup unavailable');
      }
      if (Number(response.headers.get('content-length') || 0) > MAX_RESPONSE_CHARS) {
        await response.body?.cancel();
        throw new Error('public reaction lookup too large');
      }
      const text = await response.text();
      if (text.length > MAX_RESPONSE_CHARS)
        throw new Error('public reaction lookup too large');
      const parsed = JSON.parse(text);
      if (!Array.isArray(parsed?.result?.reactions))
        throw new Error('invalid reaction response');
      const positive = new Set();
      for (const item of parsed.result.reactions) {
        const fid = normalizeFid(item?.reactor?.fid);
        if (fid && (item.type === 'like' || item.type === 'recast'))
          positive.add(fid + ':' + item.type);
      }
      const result = {positive, expiresAt: now() + CACHE_TTL_MS};
      if (cached.size >= MAX_CACHE) cached.delete(cached.keys().next().value);
      cached.set(hash, result);
      return result;
    })();
    inFlight.set(hash, task);
    try { return await task; }
    finally { inFlight.delete(hash); }
  }

  return {
    async resolve(payload) {
      const normalized = normalizeReactionBatch(payload);
      if (!normalized) return {status: 400, error: 'invalid_reaction_batch'};
      const reactions = Object.create(null);
      // Exactly two upstream requests in flight at once. Do not stall or
      // flood the upstream API if a feed has many recycled delegates.
      for (let index = 0; index < normalized.casts.length; index += 2) {
        const group = normalized.casts.slice(index, index + 2);
        const observed = await Promise.all(group.map(hash =>
          queryCast(hash).catch(() => null)));
        observed.forEach((result, offset) => {
          if (!result) return;
          const hash = group[offset];
          const liked = result.positive.has(normalized.viewerFid + ':like');
          const recasted = result.positive.has(normalized.viewerFid + ':recast');
          // Only verified positive facts. Non-matches on a truncated public
          // response must NEVER clear local/native authenticated reactions.
          if (liked || recasted)
            reactions[hash] = {
              ...(liked ? {liked: true} : {}),
              ...(recasted ? {recasted: true} : {}),
            };
        });
      }
      return {status: 200, data: {reactions}};
    },
  };
}
