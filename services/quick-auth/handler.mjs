import { createHmac, timingSafeEqual } from 'node:crypto';

function json(response, status, data) {
  const body = JSON.stringify(data);
  response.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
    'Content-Length': Buffer.byteLength(body),
  });
  response.end(body);
}

function matchesSecret(expected, actual) {
  // Hash before comparison so differing-length inputs take the same code path.
  const digest = (value) => createHmac('sha256', 'genycaster-quick-auth-internal').update(value).digest();
  return timingSafeEqual(digest(expected), digest(actual));
}

/**
 * Internal-only protocol proof verification. NEVER generates an app account session.
 * The caller must validate the returned FID against the linked OpenProof identity.
 */
export function createHandler({ verifyToken, domain, internalSecret, hubErrorClass, invalidTokenClass }) {
  if (!domain || !internalSecret || typeof verifyToken !== 'function') {
    throw new Error('Verification domain, internal secret and verifier are mandatory');
  }

  return async (request, response) => {
    if (request.method !== 'POST' || request.url !== '/v1/protocol-auth/verify') {
      return json(response, 404, { error: 'not_found' });
    }
    const submittedSecret = request.headers['x-genycaster-internal-secret'];
    if (typeof submittedSecret !== 'string'
        || submittedSecret.length > 1024
        || !matchesSecret(internalSecret, submittedSecret)) {
      return json(response, 403, { error: 'forbidden' });
    }

    const bearer = request.headers.authorization;
    if (typeof bearer !== 'string' || !/^Bearer [^\s]{1,8192}$/.test(bearer)) {
      return json(response, 401, { error: 'invalid_token' });
    }
    try {
      const proof = await verifyToken({ token: bearer.slice(7), domain });
      const fid = Number(proof?.sub);
      if (!Number.isSafeInteger(fid) || fid <= 0
          || !['custody', 'authAddress', 'verified', 'quickAuthServer'].includes(proof?.method)) {
        return json(response, 401, { error: 'invalid_token' });
      }
      const address = proof.method === 'quickAuthServer' ? null : proof.address;
      if (address !== null && (typeof address !== 'string' || !/^0x[a-fA-F0-9]{40}$/.test(address))) {
        return json(response, 401, { error: 'invalid_token' });
      }
      return json(response, 200, { fid, method: proof.method, address });
    } catch (error) {
      if (invalidTokenClass && error instanceof invalidTokenClass) {
        return json(response, 401, { error: 'invalid_token' });
      }
      // Unknown hub/transport failures are never converted to successful authentication.
      if (hubErrorClass && error instanceof hubErrorClass) {
        return json(response, 503, { error: 'verification_unavailable' });
      }
      return json(response, 503, { error: 'verification_unavailable' });
    }
  };
}
