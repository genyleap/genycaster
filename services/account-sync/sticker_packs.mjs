// Server-owned sticker pack IDs and entitlement classification. Never accept
// a caller-submitted "premium" or "owner" boolean as authority.
export const STICKER_PACKS=Object.freeze({
  'njal':Object.freeze({id:'njal',tier:'free',sticker_count:68}),
  'cream-cat-pro':Object.freeze({id:'cream-cat-pro',tier:'pro',sticker_count:208})
});
// Accounts which already installed the previous Pro pack keep their verified
// library membership when the displayed catalog switches to Cream Cat.
// Migration is read-time only until the next atomic account update.
export function normalizeStickerLibrary(library) {
  if (!library || !Array.isArray(library.pack_ids)) return library;
  const result=[];
  for(const id of library.pack_ids) {
    const canonical=id==='twemoji-pro'?'cream-cat-pro':id;
    if(Object.hasOwn(STICKER_PACKS,canonical) && !result.includes(canonical))
      result.push(canonical);
  }
  return {...library,pack_ids:result};
}
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
