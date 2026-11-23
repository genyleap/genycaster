// Smart Media Pipeline — deterministic, public, bounded delivery contracts.
// No user-supplied resize dimensions or arbitrary file/object locations.
export const VARIANTS = Object.freeze({
  thumbnail: Object.freeze({maxEdge: 320, autoQuality: 76, highQuality: 86}),
  feed: Object.freeze({maxEdge: 960, autoQuality: 83, highQuality: 91}),
  full: Object.freeze({maxEdge: 1600, autoQuality: 85, highQuality: 93}),
});

const OUTPUT_FORMATS = new Set(['auto', 'webp', 'jpeg', 'avif', 'png']);
const MODES = new Set(['auto', 'high']);

export class SmartMediaError extends Error {
  constructor(code, statusCode, message = code) {
    super(message);
    this.name = 'SmartMediaError';
    this.code = code;
    this.statusCode = statusCode;
  }
}

export function parseSmartMediaRequest(searchParams) {
  if (!searchParams.has('variant')) return null;
  // Reject unbounded/inconsistent URLs; omit all source, FID, arbitrary URL,
  // width/height and codec options from the public transform surface.
  const allowed = new Set(['variant', 'format', 'mode']);
  for (const name of searchParams.keys()) {
    if (!allowed.has(name) || searchParams.getAll(name).length !== 1)
      throw new SmartMediaError('invalid_media_variant', 400);
  }
  const variant = searchParams.get('variant');
  if (variant === 'original') {
    if (searchParams.has('format') || searchParams.has('mode'))
      throw new SmartMediaError('invalid_media_variant', 400);
    return Object.freeze({variant: 'original', format: 'original', mode: 'original'});
  }
  if (!Object.hasOwn(VARIANTS, variant))
    throw new SmartMediaError('invalid_media_variant', 400);
  const format = searchParams.get('format') || 'auto';
  const mode = searchParams.get('mode') || 'auto';
  if (!OUTPUT_FORMATS.has(format) || !MODES.has(mode))
    throw new SmartMediaError('invalid_media_variant', 400);
  return Object.freeze({variant, format, mode});
}

export function transformPolicy({sourceMime, metadata, request}) {
  const preset = VARIANTS[request.variant];
  if (!preset) throw new SmartMediaError('invalid_media_variant', 400);
  if (!Number.isSafeInteger(metadata.width) || metadata.width <= 0
      || !Number.isSafeInteger(metadata.height) || metadata.height <= 0)
    throw new SmartMediaError('invalid_image_dimensions', 422);
  const animated = metadata.pages > 1 || metadata.animated === true
    || sourceMime === 'image/gif';
  if (animated) {
    // No first-frame-only conversions: preserve all timing/transparency.
    return Object.freeze({passthrough: true, reason: 'animated'});
  }
  const hasAlpha = Boolean(metadata.hasAlpha);
  // PNG content (especially Stats cards and screenshots) is treated as
  // graphic: default WebP is lossless. JPEG photography is lossy.
  const graphical = sourceMime === 'image/png' || hasAlpha;
  const format = request.format === 'auto' ? 'webp' : request.format;
  if (format === 'jpeg' && hasAlpha)
    throw new SmartMediaError('transparency_requires_alpha_codec', 422);
  const quality = request.mode === 'high' ? preset.highQuality : preset.autoQuality;
  const lossless = request.format === 'auto' && graphical
    && request.mode === 'auto';
  const dimensionsChanged = Math.max(metadata.width, metadata.height) > preset.maxEdge;
  return Object.freeze({
    passthrough: false,
    maxEdge: preset.maxEdge,
    format,
    quality,
    lossless,
    dimensionsChanged,
    allowSourceFallback: request.format === 'auto',
  });
}
