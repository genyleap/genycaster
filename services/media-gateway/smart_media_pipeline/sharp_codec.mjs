// Isolated Sharp/libvips adapter. The gateway itself does not need to load
// the native codec to serve unchanged, permanent protocol Embed URLs.
import {SmartMediaError} from './policy.mjs';

const CODECS = Object.freeze({
  jpeg: 'image/jpeg', png: 'image/png', webp: 'image/webp',
  gif: 'image/gif', avif: 'image/avif',
});

export function createSharpCodec({loadSharp = () => import('sharp'), maxPixels = 32_000_000} = {}) {
  let pendingModule;
  async function sharpFactory() {
    pendingModule ??= Promise.resolve().then(loadSharp).then(mod => {
      const sharp=mod.default || mod;
      // Keep libvips' own cache/thread usage proportional to the VPS limits.
      sharp.cache?.({memory:32,items:0,files:0});
      sharp.concurrency?.(1);
      return sharp;
    }).catch(error => {
        pendingModule = null;
        throw new SmartMediaError('image_codec_unavailable', 503,
          'Smart Media Pipeline codec is unavailable');
      });
    return pendingModule;
  }
  function open(sharp, bytes) {
    return sharp(bytes, {limitInputPixels: maxPixels, failOn: 'error'});
  }
  return Object.freeze({
    async inspect(bytes) {
      const sharp = await sharpFactory();
      let meta;
      try { meta = await open(sharp, bytes).metadata(); }
      catch { throw new SmartMediaError('invalid_image_payload', 415); }
      if (!CODECS[meta.format])
        throw new SmartMediaError('unsupported_image_format', 415);
      const width = meta.autoOrient?.width || meta.width;
      const height = meta.autoOrient?.height || meta.height;
      if (!Number.isSafeInteger(width) || !Number.isSafeInteger(height)
          || width <= 0 || height <= 0 || width * height > maxPixels)
        throw new SmartMediaError('image_pixel_limit_exceeded', 413);
      return {
        width, height, pages: meta.pages || 1,
        hasAlpha: Boolean(meta.hasAlpha),
        format: meta.format,
      };
    },
    async transform(bytes, policy) {
      const sharp = await sharpFactory();
      try {
        let operation = open(sharp, bytes)
          .rotate() // honor orientation, strip EXIF and GPS on derived copies
          .resize({
            width: policy.maxEdge, height: policy.maxEdge,
            fit: 'inside', withoutEnlargement: true,
          })
          .toColourspace('srgb')
          .timeout({seconds: 12});
        switch (policy.format) {
          case 'webp':
            operation = operation.webp({
              ...(policy.lossless ? {lossless: true} : {quality: policy.quality}),
              effort: 4,
            });
            break;
          case 'jpeg':
            operation = operation.jpeg({
              quality: policy.quality,
              mozjpeg: true,
              chromaSubsampling: policy.quality >= 90 ? '4:4:4' : '4:2:0',
            });
            break;
          case 'avif':
            operation = operation.avif({quality: policy.quality, effort: 4});
            break;
          case 'png':
            operation = operation.png({compressionLevel: 9});
            break;
          default:
            throw new SmartMediaError('invalid_media_variant', 400);
        }
        const result = await operation.toBuffer();
        return {bytes: result, contentType: CODECS[policy.format]};
      } catch (error) {
        if (error instanceof SmartMediaError) throw error;
        throw new SmartMediaError('image_conversion_failed', 422);
      }
    },
  });
}
