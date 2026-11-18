import {createServer} from 'node:http';
import {timingSafeEqual} from 'node:crypto';
import {fetchFirmQuote} from './quote.mjs';

const WINDOW_MS = 60_000;
const MAX_PER_MINUTE = 12;
const MAX_REQUEST = 2048;

function constantTimeMatch(a, b) {
  const first = Buffer.from(String(a || ''));
  const second = Buffer.from(String(b || ''));
  return first.length === second.length && first.length > 0
    && timingSafeEqual(first, second);
}

export function createQuoteServer({apiKey, accessToken, fetchImpl = fetch} = {}) {
  if (!apiKey || !accessToken || accessToken.length < 32)
    throw new Error('Swap quote broker requires a 0x key and a 32+ character private ingress token');
  const rate = new Map();
  const respond = (res, status, value) => {
    res.writeHead(status, {
      'content-type':'application/json; charset=utf-8',
      'cache-control':'no-store, private', 'x-content-type-options':'nosniff',
      'referrer-policy':'no-referrer',
    });
    res.end(JSON.stringify(value));
  };
  return createServer(async (req, res) => {
    if (req.url === '/healthz' && req.method === 'GET') {
      respond(res, 200, {ok:true, provider:'0x-v2', chainId:8453});
      return;
    }
    if (req.url !== '/v1/quote' || req.method !== 'POST') {
      respond(res, 404, {error:'not_found'});
      return;
    }
    // Never expose the 0x key to a native app, iframe, or a browser. A
    // production gateway must authenticate an OpenProof user then set this
    // server-to-server token. The broker binds to loopback by default.
    if (!constantTimeMatch(req.headers['x-genycaster-broker-token'], accessToken)) {
      respond(res, 401, {error:'unauthorized'});
      return;
    }
    const now = Date.now();
    const client = req.socket.remoteAddress || 'unknown';
    const entry = rate.get(client) || {window:now, count:0};
    if (now - entry.window >= WINDOW_MS) { entry.window=now; entry.count=0; }
    rate.set(client, entry);
    if (++entry.count > MAX_PER_MINUTE) {
      respond(res, 429, {error:'rate_limited'});
      return;
    }
    if (rate.size > 10000) {
      for (const [key, bucket] of rate) {
        if (now - bucket.window > WINDOW_MS * 2) rate.delete(key);
      }
    }
    if (Number(req.headers['content-length'] || 0) > MAX_REQUEST) {
      respond(res, 413, {error:'request_too_large'});
      return;
    }
    let size=0;
    let chunks=[];
    try {
      for await (const chunk of req) {
        size+=chunk.length;
        if (size > MAX_REQUEST) {
          respond(res, 413, {error:'request_too_large'});
          return;
        }
        chunks.push(chunk);
      }
      const parsed = JSON.parse(Buffer.concat(chunks).toString('utf8'));
      const quote = await fetchFirmQuote(parsed, apiKey, {fetch:fetchImpl});
      respond(res, 200, quote);
    } catch(error) {
      const code=String(error?.message || 'quote_failed');
      const invalid = /^(invalid_|unsupported_|unexpected_|identical_|token_must_)/.test(code);
      // Never echo provider payloads, origin details or credentials.
      respond(res, invalid ? 422 : 502, {error: invalid ? code : 'quote_unavailable'});
    }
  });
}

if (process.argv[1] && import.meta.url === new URL('file://' + process.argv[1]).href) {
  const port = Number(process.env.GENYCASTER_SWAP_BROKER_PORT || 18765);
  const app=createQuoteServer({
    apiKey: process.env.GENYCASTER_0X_API_KEY,
    accessToken: process.env.GENYCASTER_SWAP_BROKER_TOKEN,
  });
  if (!Number.isInteger(port) || port < 1024 || port > 65535) throw new Error('Invalid broker port');
  app.listen(port, '127.0.0.1', () => {
    console.log('Swap broker running on loopback port ' + port);
  });
}
