import { lookup } from 'node:dns/promises';
import https from 'node:https';
import { isIP } from 'node:net';

// Fixed first-party policy: requests for arbitrary URLs must never become a
// general-purpose internet proxy or a route to internal services.
const defaultHosts = [
  'imagedelivery.net', 'cdn.warpcast.com', 'wrpcd.net', 'i.imgur.com',
  'pbs.twimg.com', 'res.cloudinary.com', 'ipfs.io', 'gateway.pinata.cloud',
  'arweave.net', 'media-farcaster.genyleap.com', 'images.unsplash.com',
  'cdn.farcaster.xyz', 'i.seadn.io', 'media.discordapp.net', 'cdn.discordapp.com',
];
const mimeTypes = new Set(['image/jpeg', 'image/png', 'image/webp', 'image/gif']);
export const maximumBrowserImageBytes = 8 * 1024 * 1024;

export function validImageUrl(rawUrl, allowedHosts = defaultHosts) {
  if (typeof rawUrl !== 'string' || rawUrl.length > 4096) return null;
  let parsed;
  try { parsed = new URL(rawUrl); } catch { return null; }
  if (parsed.protocol !== 'https:' || parsed.port && parsed.port !== '443'
      || parsed.username || parsed.password || parsed.hash || isIP(parsed.hostname)) return null;
  const hostname = parsed.hostname.toLowerCase().replace(/\.$/, '');
  if (!allowedHosts.some(host => hostname === host || hostname.endsWith('.' + host))) return null;
  parsed.hash = '';
  return parsed;
}

export function isPublicAddress(address, family) {
  if (family === 4) {
    const bytes = address.split('.').map(Number);
    if (bytes.length !== 4 || bytes.some(v => !Number.isInteger(v) || v < 0 || v > 255)) return false;
    const [a, b, c] = bytes;
    return !(a === 0 || a === 10 || a === 127 || a >= 224 ||
      a === 169 && b === 254 || a === 172 && b >= 16 && b <= 31 ||
      a === 192 && (b === 168 || b === 0) || a === 100 && b >= 64 && b <= 127 ||
      a === 198 && (b === 18 || b === 19) || a === 192 && b === 88 && c === 99 ||
      a === 169 && b === 0 || a === 255);
  }
  if (family === 6 && isIP(address) === 6) {
    // Only routable unicast IPv6, not loopback, unique-local, link-local,
    // mapped IPv4, documentation or multicast addresses.
    const first = Number.parseInt(address.split(':', 1)[0], 16);
    return first >= 0x2000 && first <= 0x3fff && !address.startsWith('2001:db8:');
  }
  return false;
}

function recognizedRaster(bytes, mime) {
  if (mime === 'image/png') return bytes.length >= 8 &&
    bytes.subarray(0, 8).equals(Buffer.from([137,80,78,71,13,10,26,10]));
  if (mime === 'image/jpeg') return bytes.length >= 3 && bytes[0] === 255 && bytes[1] === 216 && bytes[2] === 255;
  if (mime === 'image/gif') return bytes.length >= 6 &&
    (bytes.subarray(0, 6).toString('ascii') === 'GIF87a' || bytes.subarray(0, 6).toString('ascii') === 'GIF89a');
  if (mime === 'image/webp') return bytes.length >= 12 &&
    bytes.toString('ascii', 0, 4) === 'RIFF' && bytes.toString('ascii', 8, 12) === 'WEBP';
  return false;
}

export async function retrieveBrowserImage(rawUrl, {
  lookupHost = lookup,
  allowedHosts = defaultHosts,
  requester = https.request,
  redirects = 0,
} = {}) {
  const url = validImageUrl(rawUrl, allowedHosts);
  if (!url) return { status: 400, error: 'image_source_not_allowed' };
  let addresses;
  try { addresses = await lookupHost(url.hostname, { all: true, verbatim: true }); }
  catch { return { status: 502, error: 'image_source_dns_failed' }; }
  if (!Array.isArray(addresses) || addresses.length === 0 ||
      addresses.some(value => !isPublicAddress(value.address, value.family))) {
    return { status: 403, error: 'image_source_not_public' };
  }
  // Prefer IPv4 when the host has no working IPv6 egress; still pin DNS.
  const selected = addresses.find(entry => entry.family === 4) ?? addresses[0];
  return await new Promise(resolve => {
    let finished = false;
    const finish = value => { if (!finished) { finished = true; resolve(value); } };
    const request = requester(url, {
      method: 'GET',
      timeout: 8_000,
      headers: { accept: 'image/jpeg,image/png,image/webp,image/gif' },
      autoSelectFamily: false,
      lookup: (_hostname, options, callback) => options.all
        ? callback(null, [selected])
        : callback(null, selected.address, selected.family),
    }, async response => {
      const status = response.statusCode || 502;
      if ([301,302,303,307,308].includes(status)) {
        response.resume();
        if (redirects >= 2) { finish({ status: 502, error: 'image_redirect_limit' }); return; }
        const location = response.headers.location;
        if (!location) { finish({ status: 502, error: 'image_redirect_missing' }); return; }
        try {
          finish(await retrieveBrowserImage(new URL(location, url).href, {
            lookupHost, allowedHosts, requester, redirects: redirects + 1,
          }));
        } catch { finish({ status: 502, error: 'image_redirect_failed' }); }
        return;
      }
      if (status !== 200) { response.resume(); finish({ status: 502, error: 'image_upstream_rejected' }); return; }
      const type = String(response.headers['content-type'] || '').split(';')[0].trim().toLowerCase();
      if (!mimeTypes.has(type)) { response.resume(); finish({ status: 415, error: 'image_unsupported_type' }); return; }
      const reported = Number(response.headers['content-length'] || 0);
      if (!Number.isFinite(reported) || reported > maximumBrowserImageBytes) {
        response.destroy(); finish({ status: 413, error: 'image_too_large' }); return;
      }
      let total = 0;
      const chunks = [];
      response.on('data', chunk => {
        total += chunk.length;
        if (total > maximumBrowserImageBytes) {
          response.destroy();
          finish({ status: 413, error: 'image_too_large' });
          return;
        }
        chunks.push(chunk);
      });
      response.on('end', () => {
        if (finished) return;
        const data = Buffer.concat(chunks, total);
        finish(recognizedRaster(data, type)
          ? { status: 200, body: data, contentType: type }
          : { status: 415, error: 'image_invalid_payload' });
      });
      response.on('error', () => finish({ status: 502, error: 'image_stream_failed' }));
    });
    request.on('timeout', () => request.destroy(new Error('image timeout')));
    request.on('error', () => finish({ status: 502, error: 'image_network_failed' }));
    request.end();
  });
}
