// Server-owned sticker pack IDs and entitlement classification. Never accept
// a caller-submitted "premium" or "owner" boolean as authority.
export const STICKER_PACKS=Object.freeze({
  'njal':Object.freeze({id:'njal',tier:'free',sticker_count:68}),
  'twemoji-pro':Object.freeze({id:'twemoji-pro',tier:'pro',sticker_count:8})
});
export function validStickerPackIds(ids) {
  return Array.isArray(ids) && ids.length<=12
    && ids.every(id=>typeof id==='string' && Object.hasOwn(STICKER_PACKS,id))
    && new Set(ids).size===ids.length;
}
export function addedPremiumPack(ids,previous) {
  return ids.some(id=>STICKER_PACKS[id]?.tier==='pro'
    && !previous.includes(id));
}
export function canUseStickerPack(id,installed,verifiedPro) {
  return Object.hasOwn(STICKER_PACKS,id)
    && installed.includes(id)
    && (STICKER_PACKS[id].tier==='free' || verifiedPro===true);
}
