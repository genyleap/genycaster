// Public, read-only positive evidence of a viewer's reactions to casts.
// This is NOT a replacement for authenticated protocol reads. The public
// Farcaster API has bounded reaction pages: absence is unknown, never "false".
// A verified match requires both the exact cast hash (fixed URL) and reactor FID.

const MAX_BATCH = 8;
const MAX_RESULTS = 100;
const MAX_RESPONSE_CHARS = 3 * 1024 * 1024;
const CACHE_TTL_MS = 90_000;
const MAX_CACHE = 256;
const MAX_UPSTREAM_ACTIVE = 2;
const MAX_UPSTREAM_WAITING = 32;
const TOTAL_BUDGET_MS = 6_000;
const UNAVAILABLE_RETRY_MS = 5_000;

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
  workBudgetMs = TOTAL_BUDGET_MS,
} = {}) {
  const cached = new Map();
  const inFlight = new Map();
  let activeUpstream = 0;
  const waitingUpstream = [];

  // Provider slots belong to the service, not individual browser requests.
  // A burst of concurrent callers may wait for slots, but cannot multiply
  // the global upstream fan-out by two per HTTP request.
  function drainUpstream() {
    while (activeUpstream < MAX_UPSTREAM_ACTIVE && waitingUpstream.length) {
      const next = waitingUpstream.shift();
      if (next.signal.aborted) { next.resolve(null); continue; }
      next.launch();
    }
  }

  function withUpstreamSlot(run, signal) {
    return new Promise(resolve => {
      if (signal.aborted || (activeUpstream >= MAX_UPSTREAM_ACTIVE
          && waitingUpstream.length >= MAX_UPSTREAM_WAITING)) {
        resolve(null);
        return;
      }
      const launch = async () => {
        if (signal.aborted) { resolve(null); drainUpstream(); return; }
        ++activeUpstream;
        try { resolve(await run()); }
        catch { resolve(null); }
        finally { --activeUpstream; drainUpstream(); }
      };
      if (activeUpstream < MAX_UPSTREAM_ACTIVE) launch();
      else waitingUpstream.push({launch, resolve, signal});
    });
  }

  async function withinDeadline(promise, remainingMs) {
    // A real timer keeps the logical response budget observable even if every
    // mock/transport promise remains pending. AbortSignal.timeout uses an
    // unref'd timer in Node and cannot by itself keep an HTTP task alive.
    if (remainingMs <= 0) return null;
    let timer;
    try {
      return await Promise.race([
        promise,
        new Promise(resolve => { timer = setTimeout(() => resolve(null), remainingMs); }),
      ]);
    } finally {
      clearTimeout(timer);
    }
  }

  async function queryCast(hash) {
    const current = cached.get(hash);
    if (current && current.expiresAt > now()) return current.unavailable ? null : current;
    if (inFlight.has(hash)) return inFlight.get(hash);
    // This signal is owned by the shared cast lookup, not by its first viewer.
    // A caller deadline must never cancel another viewer's active lookup.
    const sharedSignal = AbortSignal.timeout(workBudgetMs);
    const task = withUpstreamSlot(async () => {
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
      if (!response.body) throw new Error('public reaction lookup has no body');
      const reader = response.body.getReader();
      const chunks = [];
      let bytes = 0;
      try {
        for (;;) {
          const {done, value} = await reader.read();
          if (done) break;
          bytes += value.byteLength;
          if (bytes > MAX_RESPONSE_CHARS) {
            await reader.cancel();
            throw new Error('public reaction lookup too large');
          }
          chunks.push(Buffer.from(value));
        }
      } finally {
        reader.releaseLock();
      }
      const parsed = JSON.parse(Buffer.concat(chunks, bytes).toString('utf8'));
      if (!Array.isArray(parsed?.result?.reactions))
        throw new Error('invalid reaction response');
      const positive = new Set();
      for (const item of parsed.result.reactions.slice(0, MAX_RESULTS)) {
        const fid = normalizeFid(item?.reactor?.fid);
        if (fid && (item.type === 'like' || item.type === 'recast'))
          positive.add(fid + ':' + item.type);
      }
      const result = {positive, expiresAt: now() + CACHE_TTL_MS};
      if (cached.size >= MAX_CACHE) cached.delete(cached.keys().next().value);
      cached.set(hash, result);
      return result;
    }, sharedSignal).then(result => {
      if (result === null) {
        // Unavailable is distinct from zero, but it still needs bounded
        // backoff to avoid retry storms from recycled visible delegates.
        cached.set(hash, {unavailable: true, expiresAt: now() + UNAVAILABLE_RETRY_MS});
        while (cached.size > MAX_CACHE) cached.delete(cached.keys().next().value);
      }
      return result;
    });
    inFlight.set(hash, task);
    try { return await task; }
    finally { inFlight.delete(hash); }
  }

  return {
    async resolve(payload) {
      const normalized = normalizeReactionBatch(payload);
      if (!normalized) return {status: 400, error: 'invalid_reaction_batch'};
      const reactions = Object.create(null);
      const deadlineAt = Date.now() + workBudgetMs;
      // At most two shared upstream requests at once across every concurrent
      // client, with one bounded logical-response deadline and partial results.
      for (let index = 0; index < normalized.casts.length; index += 2) {
        const group = normalized.casts.slice(index, index + 2);
        const pending = group.map(async hash => {
          const result = await queryCast(hash).catch(() => null);
          if (!result) return;
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
        const completed = await withinDeadline(Promise.all(pending),
          Math.max(0, deadlineAt - Date.now()));
        if (completed === null) break;
      }
      // Prevent late shared upstream work from mutating an already-returned
      // partial HTTP response.
      return {status: 200, data: {reactions: {...reactions}}};
    },
  };
}
