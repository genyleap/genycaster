// Account-scoped feedback. Users report issues; moderation and reward points
// remain server-owned. Never trust a client-provided FID, status or score.
export const FEEDBACK_CATEGORIES = Object.freeze([
  'bug','performance','improvement','idea'
]);

export function validateFeedback(raw) {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) return null;
  if (!FEEDBACK_CATEGORIES.includes(raw.category)
      || !Number.isInteger(raw.sentiment) || raw.sentiment < 1 || raw.sentiment > 5
      || typeof raw.message !== 'string') return null;
  const message = raw.message.trim().normalize('NFC');
  if (Array.from(message).length < 10 || Array.from(message).length > 4000
      || /[\u0000-\u0008\u000b\u000c\u000e-\u001f]/u.test(message)) return null;
  const safeContext = (value, limit) => {
    if (value === undefined) return '';
    if (typeof value !== 'string' || value.length > limit
        || /[\u0000-\u001f\u007f]/u.test(value)) return null;
    return value.trim();
  };
  const platform = safeContext(raw.platform, 32);
  const appVersion = safeContext(raw.appVersion, 40);
  const route = safeContext(raw.route, 96);
  if (platform === null || appVersion === null || route === null) return null;
  // No client-supplied author, review status or reward parameters.
  if (['fid','issuer','subject','status','pointsAwarded','reward'].some(
      key => Object.hasOwn(raw,key))) return null;
  return {category:raw.category,sentiment:raw.sentiment,message,
    platform,appVersion,route};
}
