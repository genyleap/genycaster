import { lookup } from 'node:dns/promises';
import https from 'node:https';
import { randomUUID } from 'node:crypto';
import { isIP } from 'node:net';
import sharp from 'sharp';

// Fixed first-party policy: requests for arbitrary URLs must never become a
// general-purpose internet proxy or a route to internal services.
export const defaultBrowserImageHosts = [
  'imagedelivery.net', 'cdn.warpcast.com', 'wrpcd.net', 'i.imgur.com',
  'pbs.twimg.com', 'res.cloudinary.com', 'ipfs.io', 'gateway.pinata.cloud',
  'arweave.net', 'media-farcaster.genyleap.com', 'images.unsplash.com',
  'cdn.farcaster.xyz', 'i.seadn.io', 'media.discordapp.net', 'cdn.discordapp.com',
  // Warpcast's verified channel directory publishes immutable IPFS artwork
  // through this gateway. Keep it confined to an immutable /ipfs/CID path.
  'ipfs.decentralized-content.com',
  // Keep the browser's bounded raster-only retrieval compatible with the
  // image hosts recognized by the C++ Cast embed classifier.
  'media.tenor.com', 'media1.tenor.com', 'media.giphy.com', 'lh3.googleusercontent.com', 'imagekit.io',
  'assets.geckoterminal.com',
  // Public Terminus Cast media uses content-addressed raster URLs.
  // The stricter per-host path check below disallows arbitrary site paths.
  'media.onterminus.com',
  // Public cast image CDN; accept only opaque /m/<id> raster assets below.
  'quilcast.com',
  // Recaster's public immutable cast raster assets only.
  'cdn.recaster.org',
  // Fixed public poster routes emitted by the first-party preview resolver.
  'i.ytimg.com', 'opengraph.githubassets.com', 'github.com', 'raw.githubusercontent.com',
  // Cloudflare Stream exposes a deterministic raster poster for public video
  // manifests. Only the /thumbnails/thumbnail.jpg asset is admissible here.
  'videodelivery.net', 'cloudflarestream.com',
];
export const maximumBrowserImageBytes = 8 * 1024 * 1024;

// The public Arweave gateway redirects immutable /TXID images to the
// base32-encoded TXID subdomain. Verify the exact content address and path:
// never allow arbitrary cross-origin redirects from a browser image request.
function isContentAddressedArweaveRedirect(source, next) {
  if (source.hostname !== 'arweave.net' || source.search || source.hash
      || next.protocol !== 'https:' || next.port || next.username
      || next.password || next.hash || next.search
      || next.pathname !== source.pathname) return false;
  const identifier = source.pathname.startsWith('/') ? source.pathname.slice(1) : '';
  if (!/^[A-Za-z0-9_-]{43}$/.test(identifier)) return false;
  const bytes = Buffer.from(identifier, 'base64url');
  if (bytes.length !== 32 || bytes.toString('base64url') !== identifier) return false;
  const alphabet = 'abcdefghijklmnopqrstuvwxyz234567';
  let bits = 0, buffer = 0, label = '';
  for (const byte of bytes) {
    buffer = (buffer << 8) | byte;
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      label += alphabet[(buffer >> bits) & 31];
    }
  }
  if (bits) label += alphabet[(buffer << (5 - bits)) & 31];
  return next.hostname === label + '.arweave.net';
}

// Public website icons have a narrowly defined exception to the static CDN
// allowlist. This does NOT grant access to arbitrary pages or paths: the DNS-
// pinned raster fetch below still rejects private hosts, HTML and SVG content.
export function validPublicFaviconUrl(rawUrl) {
  if (typeof rawUrl !== 'string' || rawUrl.length > 2048) return null;
  let url;
  try { url = new URL(rawUrl); } catch { return null; }
  if (url.protocol !== 'https:' || url.port || url.username || url.password
      || url.hash || url.search || isIP(url.hostname)) return null;
  const domain = url.hostname.toLowerCase().replace(/\.$/,'');
  if (!domain.includes('.') || domain === 'localhost'
      || /\.(?:localhost|local|internal|test|invalid)$/.test(domain))
    return null;
  if (!['/favicon.ico','/favicon.png','/apple-touch-icon.png'].includes(url.pathname))
    return null;
  return url;
}

export function validImageUrl(rawUrl, allowedHosts = defaultBrowserImageHosts,
  managedMediaHost = 'media-farcaster.genyleap.com') {
  if (typeof rawUrl !== 'string' || rawUrl.length > 4096) return null;
  let parsed;
  try { parsed = new URL(rawUrl); } catch { return null; }
  if (parsed.protocol !== 'https:' || parsed.port && parsed.port !== '443'
      || parsed.username || parsed.password || parsed.hash || isIP(parsed.hostname)) return null;
  const hostname = parsed.hostname.toLowerCase().replace(/\.$/, '');
  // First-party managed-media hostnames are exact authorities, never wildcard
  // grants for attacker-controlled subdomains.
  // Pinata uses account-specific public IPFS subdomains. Permit only a
  // content-addressed /ipfs/CID/file raster asset, not arbitrary endpoints,
  // and retain the existing DNS pinning, private-IP, size and MIME checks.
  const pinataSubdomain = /^[a-z0-9-]{1,100}\.mypinata\.cloud$/.test(hostname);
  // Dedicated Pinata gateways serve both a CID directly and a named image
  // under that CID. Reject directories, traversal and arbitrary publisher
  // paths; the downloaded bytes still undergo public-DNS, size and raster
  // signature validation.
  const safePinataPath = /^\/ipfs\/(?:b[a-z2-7]{30,100}|Qm[1-9A-HJ-NP-Za-km-z]{44})(?:\/(?:[A-Za-z0-9._~-]|%20){1,100}\.(?:png|jpe?g|webp|gif|avif))?$/i.test(parsed.pathname);
  // Restricted gateway tokens are intentionally embedded in some public
  // Cast image URLs. Accept ONLY the named Pinata query on an exact CID
  // raster path, not general query strings or application API endpoints.
  const pinataParameters = [...parsed.searchParams.entries()];
  const safePinataQuery = pinataParameters.length === 0
      || (pinataParameters.length === 1
          && pinataParameters[0][0] === 'pinataGatewayToken'
          && /^[A-Za-z0-9_-]{16,256}$/.test(pinataParameters[0][1]));
  if (pinataSubdomain && (!safePinataPath || !safePinataQuery)) return null;
  if (!(pinataSubdomain && safePinataPath && safePinataQuery)
      && !allowedHosts.some(host => hostname === host
          || (host !== managedMediaHost && host !== 'media-farcaster.genyleap.com'
              && host !== 'media1.tenor.com' && host !== 'quilcast.com' && host !== 'cdn.recaster.org'
              && host !== 'i.ytimg.com' && host !== 'opengraph.githubassets.com'
              && host !== 'github.com' && host !== 'raw.githubusercontent.com'
              && hostname.endsWith('.' + host)))) return null;
  // Public GitHub blob artwork only; no arbitrary repository pages or API.
  if (hostname === 'github.com' &&
      (!/^\/[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+\/blob\/[A-Za-z0-9_./-]+\.(?:png|jpe?g|webp|gif|avif)$/i.test(parsed.pathname)||parsed.search!=='?raw=true')) return null;
  if (hostname === 'raw.githubusercontent.com' &&
      (!/^\/[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+\/[A-Za-z0-9_./-]+\.(?:png|jpe?g|webp|gif|avif)$/i.test(parsed.pathname)||parsed.search)) return null;
  if (hostname === 'i.ytimg.com' &&
      (!/^\/vi\/[A-Za-z0-9_-]{11}\/(?:hqdefault|mqdefault|sddefault|maxresdefault|default)\.jpg$/.test(parsed.pathname)||parsed.search)) return null;
  if (hostname === 'opengraph.githubassets.com' &&
      (!/^\/(?:1|[a-f0-9]{32,64})\/[A-Za-z0-9_.-]{1,100}\/[A-Za-z0-9_.-]{1,100}$/.test(parsed.pathname)||parsed.search)) return null;
  if (hostname === 'cdn.recaster.org'
      && (!/^\/cast\/[A-Za-z0-9_-]{16,128}\.(?:png|jpe?g|webp)$/i.test(parsed.pathname)
          || parsed.search)) return null;
  if (hostname === 'quilcast.com'
      && (!/^\/m\/[a-f0-9]{16,80}\.(?:png|jpe?g|webp|gif)$/i.test(parsed.pathname)
          || parsed.search)) return null;
  // Terminus images are only immutable 32-hex-byte-named raster files.
  // Preserve the gateway's existing DNS pinning, MIME and byte-size limits.
  if (hostname === 'media.onterminus.com'
      && (!/^\/[a-f0-9]{32}\.(?:png|jpe?g|webp|gif|avif)$/i.test(parsed.pathname)
          || parsed.search)) return null;
  // Tenor's media1 delivery host returns HTTP 404 to HEAD but valid animated
  // GIF bytes to GET. Proxy just its opaque public GIF assets; do not turn
  // the entire tenor.com namespace into a general-purpose fetch surface.
  if (hostname === 'media1.tenor.com'
      && (!/^\/m\/[A-Za-z0-9_-]{8,64}\/[A-Za-z0-9-]{1,128}\.gif$/.test(parsed.pathname)
          || parsed.search)) return null;
  // First-party media must be a published asset, not a recursive gateway URL.
  if ((hostname === managedMediaHost || hostname === 'media-farcaster.genyleap.com')
      && !parsed.pathname.startsWith('/m/')) return null;
  if (hostname === 'ipfs.decentralized-content.com'
      && !/^\/ipfs\/(?:b[a-z2-7]{30,100}|Qm[1-9A-HJ-NP-Za-km-z]{44})$/.test(parsed.pathname))
    return null;
  if ((hostname === 'videodelivery.net' || hostname.endsWith('.videodelivery.net')
       || hostname.endsWith('.cloudflarestream.com'))
      && !/^\/[a-zA-Z0-9_-]{20,128}\/thumbnails\/thumbnail\.jpg$/.test(parsed.pathname)) return null;
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

// Third-party Mini App hosts sometimes mislabel a valid PNG as XML or
// octet-stream. Decide exclusively from actual raster signatures, never trust
// a claimed MIME type; HTML, XML bodies, scripts and SVG remain rejected.
export function rasterMimeFromBytes(bytes) {
  if (bytes.length >= 8 &&
      bytes.subarray(0, 8).equals(Buffer.from([137,80,78,71,13,10,26,10])))
    return 'image/png';
  if (bytes.length >= 3 && bytes[0] === 255 && bytes[1] === 216 && bytes[2] === 255)
    return 'image/jpeg';
  if (bytes.length >= 6 &&
      (bytes.toString('ascii', 0, 6) === 'GIF87a' || bytes.toString('ascii', 0, 6) === 'GIF89a'))
    return 'image/gif';
  if (bytes.length >= 12 && bytes.toString('ascii', 0, 4) === 'RIFF'
      && bytes.toString('ascii', 8, 12) === 'WEBP')
    return 'image/webp';
  if (bytes.length >= 12 && bytes.toString('ascii', 4, 8) === 'ftyp'
      && ['avif', 'avis'].includes(bytes.toString('ascii', 8, 12)))
    return 'image/avif';
  return null;
}

// Negotiation alone does not guarantee compatibility: immutable WebP/AVIF
// sources may ignore Accept. Convert only when the caller requests the
// conservative Qt raster representation; native WebP/GIF paths stay intact.
export async function compatibleBrowserRaster(result, preferJpeg) {
  if (!preferJpeg || result.status !== 200
      || !['image/webp','image/avif'].includes(result.contentType)) return result;
  try {
    const input = sharp(result.body, {limitInputPixels:20 * 1024 * 1024, failOn:'error'});
    const metadata = await input.metadata();
    const resized = input.rotate().resize({width:2048,height:2048,fit:'inside',withoutEnlargement:true});
    const body = await (metadata.hasAlpha
      ? resized.png({compressionLevel:6})
      : resized.jpeg({quality:85})).toBuffer();
    if (body.length > maximumBrowserImageBytes)
      return {status:413,error:'image_too_large'};
    return {status:200,body,contentType:metadata.hasAlpha?'image/png':'image/jpeg'};
  } catch {
    return {status:415,error:'image_invalid_payload'};
  }
}

export async function retrieveBrowserImage(rawUrl, {
  lookupHost = lookup,
  allowedHosts = defaultBrowserImageHosts,
  managedMediaHost = 'media-farcaster.genyleap.com',
  requester = https.request,
  preferJpeg = false,
  redirects = 0,
  trustedStorageRedirect = false,
} = {}) {
  // Only a first-party /m/ redirect may reach a signed object. The external
  // image API itself must continue rejecting direct private-storage URLs.
  const url = trustedStorageRedirect
    ? validImageUrl(rawUrl, ['qstorage.quilibrium.com'])
    : validImageUrl(rawUrl, allowedHosts, managedMediaHost);
  if (!url || (trustedStorageRedirect &&
      (url.hostname !== 'qstorage.quilibrium.com'
       || !url.pathname.startsWith('/genycaster/v1/')
       || url.searchParams.get('X-Amz-Algorithm') !== 'AWS4-HMAC-SHA256'
       || url.searchParams.get('X-Amz-SignedHeaders') !== 'host'
       || !/^[0-9]{1,4}$/.test(url.searchParams.get('X-Amz-Expires') || '')
       || Number(url.searchParams.get('X-Amz-Expires')) > 900
       || !/^[0-9a-f]{64}$/i.test(url.searchParams.get('X-Amz-Signature') || ''))))
    return { status: 400, error: 'image_source_not_allowed' };
  let addresses;
  try { addresses = await lookupHost(url.hostname, { all: true, verbatim: true }); }
  catch { return { status: 502, error: 'image_source_dns_failed' }; }
  if (!Array.isArray(addresses) || addresses.length === 0 ||
      addresses.some(value => !isPublicAddress(value.address, value.family))) {
    return { status: 403, error: 'image_source_not_public' };
  }
  // Prefer IPv4 when the host has no working IPv6 egress; still pin DNS.
  const selected = addresses.find(entry => entry.family === 4) ?? addresses[0];
  const result = await new Promise(resolve => {
    let finished = false;
    const finish = value => { if (!finished) { finished = true; resolve(value); } };
    // Bust caches that might retain the expired QStorage 307 even longer
    // than the storage signature's lifetime. Never alter Cast embed URLs.
    const target = !trustedStorageRedirect && (url.hostname === managedMediaHost
      || url.hostname === 'media-farcaster.genyleap.com')
      && url.pathname.startsWith('/m/') ? new URL(url.href) : url;
    if (target !== url) target.searchParams.set('gc_image_request', randomUUID());
    const request = requester(target, {
      method: 'GET',
      timeout: 8_000,
      // Never forward arbitrary browser headers to public CDNs. The Qt WASM
      // compatibility retry sets JPEG first; preserve that preference so a
      // CDN can serve an image that the linked Qt raster readers can decode.
      headers: { accept: preferJpeg
        ? 'image/jpeg,image/png;q=0.95'
        : 'image/webp,image/png,image/jpeg,image/gif' },
      // Never use a fresh DNS lookup after validation. Race only the
      // validated public answers to survive intermittent CDN edge failures.
      autoSelectFamily: true,
      autoSelectFamilyAttemptTimeout: 250,
      lookup: (_hostname, options, callback) => options.all
        ? callback(null, addresses)
        : callback(null, selected.address, selected.family),
    }, async response => {
      const status = response.statusCode || 502;
      if ([301,302,303,307,308].includes(status)) {
        response.resume();
        if (redirects >= 2) { finish({ status: 502, error: 'image_redirect_limit' }); return; }
        const location = response.headers.location;
        if (!location) { finish({ status: 502, error: 'image_redirect_missing' }); return; }
        try {
          const next = new URL(location, target);
          const signedStorage = !trustedStorageRedirect && redirects === 0
              && (url.hostname === managedMediaHost
                  || url.hostname === 'media-farcaster.genyleap.com')
              && url.pathname.startsWith('/m/')
              && next.hostname === 'qstorage.quilibrium.com';
          // Mini App OpenGraph endpoints frequently issue a 302 to a GIF
          // or poster under the *same* origin. Follow bounded same-origin
          // redirects only; redo DNS/public-IP pinning at every hop.
          const sameOriginRaster = !trustedStorageRedirect
            && next.origin === url.origin
            && validImageUrl(next.href, allowedHosts, managedMediaHost) !== null;
          const arweaveRaster = !trustedStorageRedirect
            && isContentAddressedArweaveRedirect(url, next)
            && validImageUrl(next.href, allowedHosts, managedMediaHost) !== null;
          const githubRawRaster = !trustedStorageRedirect && url.hostname === 'github.com'
            && next.hostname === 'raw.githubusercontent.com'
            && validImageUrl(next.href, allowedHosts, managedMediaHost) !== null;
          if (!signedStorage && !sameOriginRaster && !arweaveRaster && !githubRawRaster) {
            finish({ status: 502, error: 'image_redirect_not_allowed' });
            return;
          }
          finish(await retrieveBrowserImage(next.href, {
            lookupHost, allowedHosts, managedMediaHost, requester, preferJpeg,
            redirects: redirects + 1,
            trustedStorageRedirect: signedStorage,
          }));
        } catch { finish({ status: 502, error: 'image_redirect_failed' }); }
        return;
      }
      if (status !== 200) { response.resume(); finish({ status: 502, error: 'image_upstream_rejected' }); return; }
      // Do not trust Content-Type from Mini App publishers: verify the final
      // bytes against safe raster magic and emit our own correct MIME header.
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
        // Reject truncated payloads even when the leading raster signature
        // is valid, so incomplete JPEG/WebP never receives a cacheable 200.
        if ((reported > 0 && total !== reported)
            || (typeof response.complete === 'boolean' && !response.complete)) {
          finish({ status: 502, error: 'image_stream_incomplete' });
          return;
        }
        const data = Buffer.concat(chunks, total);
        const rasterType = rasterMimeFromBytes(data);
        finish(rasterType
          ? { status: 200, body: data, contentType: rasterType }
          : { status: 415, error: 'image_invalid_payload' });
      });
      response.on('aborted', () => finish({ status: 502, error: 'image_stream_failed' }));
      response.on('close', () => {
        if (!finished && typeof response.complete === 'boolean' && !response.complete)
          finish({ status: 502, error: 'image_stream_incomplete' });
      });
      response.on('error', () => finish({ status: 502, error: 'image_stream_failed' }));
    });
    request.on('timeout', () => request.destroy(new Error('image timeout')));
    request.on('error', () => finish({ status: 502, error: 'image_network_failed' }));
    request.end();
  });
  return compatibleBrowserRaster(result, preferJpeg);
}
