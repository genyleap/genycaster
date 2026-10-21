import { createHash, randomBytes } from 'node:crypto';

const relay = 'https://relay.farcaster.xyz';
const attemptLifetimeMs = 5 * 60_000;
const maxAttempts = 256;
const maxResponseBytes = 256 * 1024;

function resultError(code, status = 502) {
  const error = new Error(code);
  error.code = code;
  error.status = status;
  return error;
}

async function boundedJson(response) {
  if (!response?.headers?.get('content-type')?.toLowerCase().startsWith('application/json') ||
      !response.body) throw resultError('auth_invalid_upstream_response');
  const chunks = [];
  let size = 0;
  const reader = response.body.getReader();
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    size += value.byteLength;
    if (size > maxResponseBytes) {
      await reader.cancel();
      throw resultError('auth_upstream_response_too_large');
    }
    chunks.push(Buffer.from(value));
  }
  try { return JSON.parse(Buffer.concat(chunks, size).toString('utf8')); }
  catch { throw resultError('auth_upstream_response_invalid_json'); }
}

function absorbCookies(store, headers) {
  const values = headers.getSetCookie?.() || [];
  for (const raw of values) {
    const matched = /^(__Host-openproof-[A-Za-z0-9-]+)=([A-Za-z0-9._~-]{0,512})(?:;|$)/.exec(raw);
    if (!matched) continue;
    if (!matched[2] || /(?:^|;)\\s*Max-Age=0(?:;|$)/i.test(raw)) {
      store.delete(matched[1]);
    } else {
      store.set(matched[1], matched[2]);
    }
  }
}

function cookieHeader(store) {
  return [...store].map(([name, value]) => name + '=' + value).join('; ');
}

function validateApproval(raw) {
  try {
    const url = new URL(raw);
    return url.protocol === 'https:' && !url.username && !url.password &&
      ['warpcast.com', 'farcaster.xyz'].includes(url.hostname) ? url.href : null;
  } catch { return null; }
}

async function upstreamJson(fetchImpl, url, options, expected = [200]) {
  let response;
  try {
    response = await fetchImpl(url, {
      ...options,
      redirect: 'error',
      signal: AbortSignal.timeout(10_000),
    });
  } catch { throw resultError('auth_upstream_unreachable'); }
  if (!expected.includes(response.status)) throw resultError('auth_upstream_rejected');
  return { response, json: await boundedJson(response) };
}

export function createBrowserFarcasterBroker({ fetchImpl = fetch, now = Date.now,
  issuerOrigin = 'https://openproof.genyleap.com' } = {}) {
  const issuerUrl = new URL(issuerOrigin);
  if (issuerUrl.protocol !== 'https:' || issuerUrl.username || issuerUrl.password ||
      issuerUrl.search || issuerUrl.hash || issuerUrl.pathname !== '/' ||
      issuerUrl.port && issuerUrl.port !== '443')
    throw resultError('auth_invalid_issuer_configuration',500);
  const issuer = issuerUrl.origin;
  const attempts = new Map();
  // Ephemeral, server-held OpenProof session for the SAME locally verified
  // recovery proof. Only an opaque 45-minute capability crosses into the
  // Genycaster C++ adapter; no OpenProof cookie or account token reaches QML.
  const accountSessions = new Map();
  const accountLifetimeMs = 45 * 60_000;
  function cleanup() {
    for (const [key, attempt] of attempts) {
      if (now() >= attempt.expiresAt) attempts.delete(key);
    }
    for (const [key, session] of accountSessions) {
      if (now() >= session.expiresAt) {
        session.cookies.clear();
        accountSessions.delete(key);
      }
    }
  }
  async function start() {
    cleanup();
    if (attempts.size >= maxAttempts) throw resultError('auth_too_many_pending', 429);
    const cookies = new Map();
    const started = await upstreamJson(fetchImpl, issuer + '/auth/web3/start', {
      method: 'POST',
      headers: { accept: 'application/json', 'content-type': 'application/json' },
      body: JSON.stringify({ provider: 'farcaster' }),
    });
    absorbCookies(cookies, started.response.headers);
    const challenge = started.json;
    if (!challenge?.nonce || !challenge?.domain || !challenge?.uri ||
        !challenge?.expires_at || cookies.size < 4 ||
        challenge.domain !== 'genyleap.com') {
      throw resultError('auth_invalid_openproof_challenge');
    }
    const opened = await upstreamJson(fetchImpl, relay + '/v1/channel', {
      method: 'POST',
      headers: { accept: 'application/json', 'content-type': 'application/json' },
      body: JSON.stringify({
        siweUri: challenge.uri,
        domain: challenge.domain,
        nonce: challenge.nonce,
        expirationTime: challenge.expires_at,
        acceptAuthAddress: true,
      }),
    }, [200, 201]);
    const channel = opened.json;
    const approvalUrl = validateApproval(channel?.url);
    if (!approvalUrl || !channel.channelToken ||
        channel.nonce !== challenge.nonce ||
        channel.channelToken.length > 2048) {
      throw resultError('auth_invalid_farcaster_channel');
    }
    const ticket = randomBytes(32).toString('base64url');
    attempts.set(createHash('sha256').update(ticket).digest('hex'), {
      cookies,
      channelToken: channel.channelToken,
      nonce: challenge.nonce,
      expiresAt: now() + attemptLifetimeMs,
      lastPollAt: 0,
      busy: false,
    });
    return { ticket, approval_url: approvalUrl, expires_in: attemptLifetimeMs / 1000 };
  }

  // One-shot direct recovery proof: the browser keeps the BIP39 phrase and
  // custody key, while this broker alone owns cookie-bound OpenProof state.
  // The challenge is bound to an exact Farcaster FID and custody address.
  async function startDirect(fid, address) {
    if (typeof fid !== 'string' || !/^[1-9][0-9]{0,19}$/.test(fid) ||
        BigInt(fid) > 0xffffffffffffffffn ||
        typeof address !== 'string' || !/^0x[0-9a-f]{40}$/i.test(address))
      throw resultError('auth_invalid_recovery_identity',400);
    cleanup();
    if (attempts.size >= maxAttempts) throw resultError('auth_too_many_pending',429);
    const cookies = new Map();
    const challenge = await upstreamJson(fetchImpl, issuer + '/auth/web3/start', {
      method:'POST',
      headers:{accept:'application/json','content-type':'application/json'},
      body:JSON.stringify({provider:'farcaster',fid,address}),
    });
    absorbCookies(cookies, challenge.response.headers);
    const body = challenge.json;
    if (cookies.size < 4 || body?.provider !== 'farcaster'
        || String(body.fid) !== fid || body.address?.toLowerCase() !== address.toLowerCase()
        || body.domain !== 'genyleap.com' || body.statement !== 'Farcaster Auth'
        || body.resource_prefix !== 'farcaster://fid/'
        || typeof body.message !== 'string' || body.message.length < 40
        || body.message.length > 8192 || !body.message.includes('farcaster://fid/'+fid)
        || !/^[A-Za-z0-9]{8,64}$/.test(body.nonce || '')
        || !body.expires_at) {
      throw resultError('auth_invalid_openproof_challenge');
    }
    const ticket = randomBytes(32).toString('base64url');
    attempts.set(createHash('sha256').update(ticket).digest('hex'),{
      cookies, direct:true, fid, message:body.message,
      expiresAt:now()+Math.min(attemptLifetimeMs, 120_000),busy:false,
    });
    return {ticket,message:body.message,fid,expires_in:120};
  }

  async function completeDirect(ticket, signature) {
    if (typeof ticket !== 'string' || !/^[A-Za-z0-9_-]{43}$/.test(ticket)
        || typeof signature !== 'string' || !/^0x[0-9a-f]{130}$/i.test(signature))
      throw resultError('auth_invalid_direct_proof',400);
    cleanup();
    const key = createHash('sha256').update(ticket).digest('hex');
    const attempt = attempts.get(key);
    if (!attempt || !attempt.direct || attempt.busy)
      throw resultError('auth_expired_or_unknown',404);
    // Consume before awaiting any network operation: concurrent calls cannot
    // replay or race a cookie-bound OpenProof completion.
    attempts.delete(key);
    attempt.busy = true;
    try {
      const completed = await upstreamJson(fetchImpl,issuer+'/auth/web3/complete',{
        method:'POST',
        headers:{accept:'application/json','content-type':'application/json',
                 cookie:cookieHeader(attempt.cookies)},
        body:JSON.stringify({message:attempt.message,signature}),
      });
      if (completed.json.authenticated !== true)
        throw resultError('auth_openproof_not_verified',401);
      absorbCookies(attempt.cookies,completed.response.headers);
      const headers={accept:'application/json',cookie:cookieHeader(attempt.cookies)};
      const profile=await upstreamJson(fetchImpl,issuer+'/account/profile',{
        method:'GET',headers,
      });
      const linked=await upstreamJson(fetchImpl,issuer+'/account/connections',{
        method:'GET',headers,
      });
      const subject=profile.json?.identity_id;
      const fc=linked.json?.connections?.filter(c=>c.provider==='farcaster')||[];
      if (typeof subject !== 'string' || subject.length<1 || subject.length>512
          || fc.length!==1 || String(fc[0].fid)!==attempt.fid) {
        throw resultError('auth_verified_identity_mismatch',401);
      }
      cleanup();
      if (accountSessions.size >= maxAttempts)
        throw resultError('auth_too_many_pending',429);
      const accountTicket='recovery.'+randomBytes(32).toString('base64url');
      accountSessions.set(createHash('sha256').update(accountTicket).digest('hex'),{
        fid:attempt.fid,subject,cookies:new Map(attempt.cookies),
        expiresAt:now()+accountLifetimeMs,
      });
      return {authenticated:true,issuer,subject,fid:attempt.fid,
        account_ticket:accountTicket,expires_in:Math.floor(accountLifetimeMs/1000)};
    } finally {
      attempt.cookies.clear();
    }
  }

  async function poll(ticket) {
    if (typeof ticket !== 'string' || !/^[A-Za-z0-9_-]{43}$/.test(ticket))
      throw resultError('auth_invalid_ticket', 400);
    cleanup();
    const key = createHash('sha256').update(ticket).digest('hex');
    const attempt = attempts.get(key);
    if (!attempt || attempt.direct) throw resultError('auth_expired_or_unknown', 404);
    if (attempt.busy || now() - attempt.lastPollAt < 1200)
      throw resultError('auth_poll_too_fast', 429);
    attempt.busy = true;
    attempt.lastPollAt = now();
    try {
      const relayResponse = await upstreamJson(fetchImpl, relay + '/v1/channel/status', {
        method: 'GET',
        headers: { accept: 'application/json',
          authorization: 'Bearer ' + attempt.channelToken },
      }, [200, 202]);
      const proof = relayResponse.json;
      if (proof.state === 'pending') return { pending: true };
      if (proof.state !== 'completed' || proof.nonce !== attempt.nonce ||
          typeof proof.message !== 'string' || proof.message.length > 8192 ||
          typeof proof.signature !== 'string' || proof.signature.length > 8192 ||
          !/^[1-9][0-9]{0,18}$/.test(String(proof.fid)) ||
          (typeof proof.fid === 'number' && !Number.isSafeInteger(proof.fid))) {
        throw resultError('auth_invalid_farcaster_proof');
      }
      const completed = await upstreamJson(fetchImpl, issuer + '/auth/web3/complete', {
        method: 'POST',
        headers: {
          accept: 'application/json',
          'content-type': 'application/json',
          cookie: cookieHeader(attempt.cookies),
        },
        body: JSON.stringify({ message: proof.message, signature: proof.signature }),
      });
      if (completed.json.authenticated !== true) {
        throw resultError('auth_openproof_not_verified');
      }
      absorbCookies(attempt.cookies, completed.response.headers);
      const verifiedCookies = cookieHeader(attempt.cookies);
      const headers = { accept: 'application/json', cookie: verifiedCookies };
      const profile = await upstreamJson(fetchImpl, issuer + '/account/profile', {
        method: 'GET', headers,
      });
      const linked = await upstreamJson(fetchImpl, issuer + '/account/connections', {
        method: 'GET', headers,
      });
      const subject = profile.json.identity_id;
      const farcaster = linked.json.connections?.filter(c => c.provider === 'farcaster') || [];
      if (typeof subject !== 'string' || subject.length < 1 || subject.length > 512 ||
          farcaster.length !== 1 || String(farcaster[0].fid) !== String(proof.fid)) {
        throw resultError('auth_verified_identity_mismatch', 401);
      }
      return {
        authenticated: true,
        subject,
        fid: String(proof.fid),
        username: String(farcaster[0].preferred_username || proof.username || '').slice(0, 128),
        display_name: String(farcaster[0].display_name || proof.displayName || '').slice(0, 256),
        avatar_url: String(farcaster[0].picture_url || proof.pfpUrl || '').slice(0, 2048),
        expires_in: 3600,
      };
    } finally {
      // Completed or failed terminal proofs cannot be replayed. Pending
      // challenges remain available until expiry.
      attempt.busy = false;
    }
  }

  function authorizeRecoveryAccount(authorization) {
    cleanup();
    if (typeof authorization !== 'string' ||
        !/^Bearer recovery\.[A-Za-z0-9_-]{43}$/.test(authorization)) return null;
    const ticket=authorization.slice('Bearer '.length);
    const session=accountSessions.get(createHash('sha256').update(ticket).digest('hex'));
    if (!session || now() >= session.expiresAt) return null;
    return {fid:session.fid,subject:session.subject,
      cookie:cookieHeader(session.cookies)};
  }

  function consume(ticket) {
    if (typeof ticket === 'string')
      attempts.delete(createHash('sha256').update(ticket).digest('hex'));
  }
  return { start, poll, startDirect, completeDirect, authorizeRecoveryAccount, consume };
}
