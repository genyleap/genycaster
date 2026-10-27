// Canonical topic IDs are server-managed and independent of parent_url.
export const CAST_HASH = /^0x[0-9a-f]{40}$/i;
export const TOPIC_SLUG = /^[a-z][a-z0-9-]{1,63}$/;
export const MAX_TOPICS_PER_CAST = 3;
export const initialTopics = Object.freeze([
  'art','development','photography','design','technology',
  'music','writing','science','business','gaming'
]);
export function validateCastTopicAssignment(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body)
      || !CAST_HASH.test(body.cast_hash || '')
      || !Array.isArray(body.topics) || body.topics.length > MAX_TOPICS_PER_CAST
      || body.topics.length === 0 || !body.topics.every(t => typeof t === 'string'
        && TOPIC_SLUG.test(t)) || new Set(body.topics).size !== body.topics.length)
    return null;
  return {cast_hash:body.cast_hash.toLowerCase(),topics:[...body.topics]};
}
export function topicPagination(query) {
  const limit=Number(query.get('limit')||30);
  const offset=Number(query.get('offset')||0);
  if (!Number.isSafeInteger(limit) || limit<1 || limit>50
      || !Number.isSafeInteger(offset) || offset<0 || offset>10000) return null;
  return {limit,offset};
}
