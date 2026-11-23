// Smart Media Pipeline
// Image variants live behind the existing stable media URL, and never mutate
// the uploaded original. Immutable upload keys + fixed variant parameters
// provide a portable way to request small display images. No private
// credentials, arbitrary URLs, local filesystem or user-derived paths.
import {rasterMimeFromBytes} from '../browser_image_proxy.mjs';
import {createSharpCodec} from './sharp_codec.mjs';
import {SmartMediaError, transformPolicy} from './policy.mjs';

const allowedMime = new Set([
  'image/jpeg', 'image/png', 'image/webp', 'image/gif', 'image/avif',
]);

// Forbid silent collapse of animated PNG/WebP to a single frame, even when
// the codec's metadata treats the container as a single-page image.
function isAnimatedRaster(bytes, mime) {
  if (mime === 'image/gif') return true;
  if (mime === 'image/webp') {
    return bytes.length > 20 && bytes.toString('ascii', 12, 16) === 'VP8X'
      && (bytes[20] & 0x02) !== 0;
  }
  if (mime !== 'image/png') return false;
  let offset = 8;
  // Read the APNG acTL chunk only before the first IDAT; no pixel decoding.
  while (offset + 12 <= bytes.length && offset < 1024 * 1024) {
    const size = bytes.readUInt32BE(offset);
    if (size > bytes.length - offset - 12) return false;
    const type = bytes.toString('ascii', offset + 4, offset + 8);
    if (type === 'acTL') return true;
    if (type === 'IDAT' || type === 'IEND') return false;
    offset += size + 12;
  }
  return false;
}

export function createSmartMediaPipeline({
  codec = createSharpCodec(),
  maxPixels = 32_000_000,
  maxOutputBytes = 8 * 1024 * 1024,
  maxConcurrent = 1,
  maxWaiting = 6,
  cacheBudgetBytes = 24 * 1024 * 1024,
  cacheEntryBytes = 2 * 1024 * 1024,
  cacheTtlMs = 120_000,
  now = () => Date.now(),
} = {}) {
  let active = 0;
  let cachedBytes = 0;
  const waiting = [];
  const cache = new Map();
  const pending = new Map();
  const generation = new Map();

  function invalidate(key) {
    const prefix = key + '|';
    generation.set(key, (generation.get(key) || 0) + 1);
    for (const [id, entry] of cache) {
      if (!id.startsWith(prefix)) continue;
      cachedBytes -= entry.bytes.length;
      cache.delete(id);
    }
    // An active request may still complete, but its stale result cannot
    // repopulate the cache because the generation number has changed.
  }

  function enqueue(task) {
    if (active >= maxConcurrent && waiting.length >= maxWaiting)
      return Promise.reject(new SmartMediaError('media_pipeline_busy', 503));
    return new Promise((resolve, reject) => {
      const run = async () => {
        active += 1;
        try { resolve(await task()); } catch (error) { reject(error); }
        finally {
          active -= 1;
          waiting.shift()?.();
        }
      };
      if (active < maxConcurrent) void run();
      else waiting.push(run);
    });
  }

  function putCache(id, result) {
    if (result.bytes.length > cacheEntryBytes || cacheBudgetBytes === 0)
      return;
    while (cache.size && (cachedBytes + result.bytes.length > cacheBudgetBytes
        || cache.size >= 64)) {
      const oldest = cache.keys().next().value;
      cachedBytes -= cache.get(oldest).bytes.length;
      cache.delete(oldest);
    }
    if (cachedBytes + result.bytes.length <= cacheBudgetBytes) {
      cache.set(id, {result, expires: now() + cacheTtlMs, bytes: result.bytes});
      cachedBytes += result.bytes.length;
    }
  }

  async function convert({sourceMime, request, readOriginal,
                          outputLimitBytes = maxOutputBytes}) {
    const original = await readOriginal();
    if (!Buffer.isBuffer(original) || original.length === 0)
      throw new SmartMediaError('invalid_image_payload', 415);
    if (rasterMimeFromBytes(original) !== sourceMime || !allowedMime.has(sourceMime))
      throw new SmartMediaError('invalid_image_payload', 415);
    const passthrough = reason => Object.freeze({
      bytes: original, contentType: sourceMime, optimized: false,
      reason, originalBytes: original.length,
    });
    if (isAnimatedRaster(original, sourceMime)) {
      const animatedMeta = await codec.inspect(original);
      if (animatedMeta.width * animatedMeta.height > maxPixels
          || animatedMeta.pages > 300)
        throw new SmartMediaError('image_pixel_limit_exceeded', 413);
      return passthrough('animated');
    }

    const metadata = await codec.inspect(original);
    const actualFormat = {
      jpeg:'image/jpeg', png:'image/png', webp:'image/webp',
      gif:'image/gif', avif:'image/avif',
    }[metadata.format];
    if (actualFormat !== sourceMime)
      throw new SmartMediaError('invalid_image_payload', 415);
    if (metadata.width * metadata.height > maxPixels)
      throw new SmartMediaError('image_pixel_limit_exceeded', 413);
    const policy = transformPolicy({sourceMime, metadata, request});
    if (policy.passthrough) return passthrough(policy.reason);
    const converted = await codec.transform(original, policy);
    if (!converted || !Buffer.isBuffer(converted.bytes)
        || converted.bytes.length === 0
        || rasterMimeFromBytes(converted.bytes) !== converted.contentType
        || converted.contentType !== {
          jpeg:'image/jpeg',png:'image/png',webp:'image/webp',avif:'image/avif',
        }[policy.format])
      throw new SmartMediaError('image_conversion_failed', 422);
    if (converted.bytes.length > outputLimitBytes) {
      if (policy.allowSourceFallback) return passthrough('output_limit');
      throw new SmartMediaError('media_variant_too_large', 422);
    }
    if (policy.allowSourceFallback && !policy.dimensionsChanged
        && converted.bytes.length >= original.length * 0.97)
      return passthrough('not_smaller');

    return Object.freeze({
      bytes: converted.bytes, contentType: converted.contentType,
      optimized: true, originalBytes: original.length,
    });
  }

  async function getVariant({key, sourceMime, request, readOriginal}) {
    if (request.variant === 'original')
      throw new SmartMediaError('invalid_media_variant', 400);
    if (typeof key !== 'string' || key.length > 256
        || !allowedMime.has(sourceMime) || typeof readOriginal !== 'function')
      throw new SmartMediaError('invalid_media_variant', 400);

    const id = [key, request.variant, request.format, request.mode].join('|');
    const cached = cache.get(id);
    if (cached) {
      cache.delete(id);
      cachedBytes -= cached.bytes.length;
      if (cached.expires > now()) {
        cache.set(id, cached);
        cachedBytes += cached.bytes.length;
        return cached.result;
      }
    }
    if (pending.has(id)) return pending.get(id);
    const version = generation.get(key) || 0;
    const task = enqueue(() => convert({sourceMime, request, readOriginal}));
    pending.set(id, task);
    try {
      const result = await task;
      if (version === (generation.get(key) || 0)) putCache(id, result);
      return result;
    } finally {
      if (pending.get(id) === task) pending.delete(id);
    }
  }

  // Backend ingress policy. Never retain the uncompressed original in
  // QStorage when the caller explicitly opts into optimized upload.
  // Output codec is kept equal to the source codec, preserving legacy URL
  // extension, crawler MIME, and already-shipped upload-client contracts.
  // Original mode uses the old direct QStorage path without invoking this.
  async function optimizeForStorage(bytes, sourceMime, mode = 'auto') {
    if (!Buffer.isBuffer(bytes) || bytes.length === 0
        || bytes.length > 25 * 1024 * 1024)
      throw new SmartMediaError('image_size_limit_exceeded', 413);
    const format = {
      'image/jpeg':'jpeg', 'image/png':'png',
      'image/webp':'webp', 'image/avif':'avif',
      'image/gif':'gif',
    }[sourceMime];
    if (!format) throw new SmartMediaError('unsupported_media', 415);
    if (mode !== 'auto' && mode !== 'high')
      throw new SmartMediaError('invalid_optimization_mode', 400);
    if (format === 'gif') {
      if (rasterMimeFromBytes(bytes) !== sourceMime)
        throw new SmartMediaError('invalid_image_payload', 415);
      const metadata = await codec.inspect(bytes);
      if (metadata.width * metadata.height > maxPixels || metadata.pages > 300)
        throw new SmartMediaError('image_pixel_limit_exceeded', 413);
      return {bytes, contentType:sourceMime, optimized:false,
        originalBytes:bytes.length, reason:'animated_or_gif'};
    }
    return enqueue(() => convert({
      sourceMime,
      request:{variant:'full',format,mode},
      readOriginal:async()=>bytes,
      // Durable canonical objects use the gateway's full 25 MiB source cap;
      // the 8 MiB limit is only for transient delivery variants.
      outputLimitBytes:25 * 1024 * 1024,
    })).then(result => {
      if (result.contentType !== sourceMime)
        throw new SmartMediaError('image_conversion_failed', 422);
      // Storing a larger encoded payload defeats the cost goal, even if it
      // has fewer pixels. Prefer unchanged original in that unusual case.
      if (result.bytes.length >= bytes.length * 0.97) {
        return {
          bytes, contentType:sourceMime, optimized:false,
          originalBytes:bytes.length, reason:'not_smaller',
        };
      }
      return result;
    });
  }

  return Object.freeze({
    getVariant, optimizeForStorage, invalidate,
    // Safe for operational diagnostics, contains no keys or media.
    status() {
      return Object.freeze({
        active, waiting: waiting.length, pending: pending.size,
        cachedEntries: cache.size, cachedBytes,
      });
    },
  });
}
