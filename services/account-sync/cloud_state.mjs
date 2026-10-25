// Genycaster-owned cloud state. OpenProof authorizes the principal, but never
// stores these app-specific preferences, drafts, bookmarks or schedules.
const ID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const PROVIDER = /^[a-z][a-z0-9-]{0,39}$/;
const HASH = /^0x[0-9a-f]{40}$/i;
const KINDS = Object.freeze({
  drafts: ['cast', 'article', 'thread'],
  tasks: ['scheduled_cast', 'scheduled_article', 'reminder'],
});
export function validLinkedInUrl(value) {
  if (typeof value !== 'string' || value.length > 2048) return false;
  try {
    const url = new URL(value);
    return url.protocol === 'https:' && url.hostname === 'www.linkedin.com' &&
      url.port === '' && !url.username && !url.password && !url.search && !url.hash &&
      /^\/in\/[A-Za-z0-9_-]{3,100}\/?$/.test(url.pathname);
  } catch { return false; }
}
export function validateCloudItem(entity, item) {
  if (!item || typeof item !== 'object' || Array.isArray(item)) return null;
  if (entity === 'connections') {
    if (!PROVIDER.test(item.provider || '') || item.provider !== 'linkedin') return null;
    if (!validLinkedInUrl(item.manual_profile_url)) return null;
    return {provider: item.provider, manual_profile_url: item.manual_profile_url,
      publicly_visible: item.publicly_visible === true};
  }
  if (entity === 'bookmarks') {
    if (typeof item.cast_hash !== 'string' || !HASH.test(item.cast_hash)) return null;
    return {cast_hash: item.cast_hash.toLowerCase(), is_deleted: item.is_deleted === true};
  }
  if (!ID.test(String(item.id || ''))) return null;
  if (entity === 'drafts') {
    if (!KINDS.drafts.includes(item.kind) || typeof item.title !== 'string' ||
        item.title.length > 256 || !item.content || typeof item.content !== 'object' ||
        Array.isArray(item.content)) return null;
    return {id:item.id.toLowerCase(), kind:item.kind, title:item.title, content:item.content,
      revision:item.revision};
  }
  if (entity === 'tasks') {
    if (!KINDS.tasks.includes(item.kind) || !['draft', 'scheduled', 'cancelled'].includes(item.state) ||
        typeof item.timezone !== 'string' || !/^[A-Za-z_]+\/[A-Za-z_+/-]{1,80}$|^UTC$/.test(item.timezone) ||
        typeof item.schedule_at !== 'string' || !Number.isFinite(Date.parse(item.schedule_at)) || !item.payload || typeof item.payload !== 'object' ||
        Array.isArray(item.payload) || !ID.test(String(item.idempotency_key || ''))) return null;
    return {id:item.id.toLowerCase(), kind:item.kind, state:item.state,
      schedule_at:new Date(item.schedule_at).toISOString(), timezone:item.timezone,
      payload:item.payload, idempotency_key:item.idempotency_key.toLowerCase(),
      revision:item.revision};
  }
  if (entity === 'notifications') {
    if (!/^[a-z][a-z0-9_-]{0,39}$/.test(item.source || '') ||
        typeof item.kind !== 'string' || item.kind.length > 64 || !item.kind ||
        typeof item.occurred_at !== 'string' ||
        !Number.isFinite(Date.parse(item.occurred_at)) ||
        (item.read_at != null && (typeof item.read_at !== 'string' ||
          !Number.isFinite(Date.parse(item.read_at)))) ||
        !item.payload || typeof item.payload !== 'object' || Array.isArray(item.payload)) return null;
    return {id:item.id.toLowerCase(), source:item.source,kind:item.kind,payload:item.payload,
      occurred_at:new Date(item.occurred_at).toISOString(),
      read_at:item.read_at ? new Date(item.read_at).toISOString() : null,
      revision:item.revision};
  }
  return null;
}
async function readBody(request, limit=24576) {
  let size=0;const chunks=[];
  for await (const chunk of request) {
    size+=chunk.length;
    if (size>limit) {const error=new Error('too_large');error.status=413;throw error;}
    chunks.push(chunk);
  }
  try { return JSON.parse(Buffer.concat(chunks,size).toString('utf8')); }
  catch {const error=new Error('invalid_json');error.status=400;throw error;}
}
function respond(response,status,value) {
  response.writeHead(status, {'content-type':'application/json; charset=utf-8',
    'cache-control':'no-store','x-content-type-options':'nosniff'});
  response.end(JSON.stringify(value));
}
export function createCloudHandler({store, issuer, fetchImpl=globalThis.fetch}) {
  if (!store || !issuer || new URL(issuer).protocol !== 'https:')
    throw new Error('A store and trusted HTTPS OpenProof issuer are required');
  const upstream=issuer.replace(/\/$/,'');
  async function authorize(request) {
    const header=String(request.headers.authorization||'');
    if (!/^Bearer [A-Za-z0-9._~+/-]+={0,2}$/.test(header) || header.length>8192 ||
        header.startsWith('Bearer recovery.')) return null;
    const response=await fetchImpl(upstream+'/account/profile',{headers:{authorization:header,
      accept:'application/json'},redirect:'error',signal:AbortSignal.timeout(6000)});
    if (response.status!==200) return null;
    const profile=await response.json();
    if (!profile || typeof profile.identity_id !== 'string' ||
        !/^[A-Za-z0-9:_-]{1,200}$/.test(profile.identity_id)) return null;
    return {issuer:upstream,subject:profile.identity_id,authorization:header};
  }
  return async function handleCloud(request,response,path) {
    if (!path.startsWith('/api/v1/cloud/')) return false;
    const entity=path.slice('/api/v1/cloud/'.length);
    if (!['connections','bookmarks','notifications','app_updates','tasks','drafts',
      'connections/refresh'].includes(entity)) {
      respond(response,404,{error:'not_found'});return true;
    }
    if (entity==='app_updates') {
      if (request.method!=='GET') respond(response,405,{error:'method_not_allowed'});
      else try {respond(response,200,{items:await store.listUpdates()});}
        catch {respond(response,503,{error:'cloud_unavailable'});}
      return true;
    }
    let owner;
    try {owner=await authorize(request);}catch{}
    if (!owner) {respond(response,401,{error:'authentication_required'});return true;}
    try {
      if (entity==='connections/refresh') {
        if (request.method!=='POST') {respond(response,405,{error:'method_not_allowed'});return true;}
        const reply=await fetchImpl(upstream+'/account/connections',{
          headers:{authorization:owner.authorization,accept:'application/json'},
          redirect:'error',signal:AbortSignal.timeout(6000)});
        if (!reply.ok) {respond(response,502,{error:'connections_unavailable'});return true;}
        const payload=await reply.json();
        if (!Array.isArray(payload.connections) || payload.connections.length>100) {
          respond(response,502,{error:'invalid_connections'});return true;
        }
        const cleaned=payload.connections.filter(c=>c && PROVIDER.test(c.provider||''))
          .map(c=>({
            provider:c.provider,
            display_name:String(c.display_name||'').slice(0,256),
            preferred_username:String(c.preferred_username||'').slice(0,128),
            provider_profile_url:c.provider==='linkedin' && validLinkedInUrl(c.profile_url)
              ? c.profile_url : null,
          }));
        await store.refreshConnections(owner,cleaned);
        respond(response,200,{refreshed:cleaned.length});return true;
      }
      if (request.method==='GET') {
        respond(response,200,{items:await store.list(entity,owner)});return true;
      }
      if (request.method!=='PUT') {respond(response,405,{error:'method_not_allowed'});return true;}
      const raw=await readBody(request);
      const item=validateCloudItem(entity,raw);
      if (!item) {respond(response,400,{error:'invalid_item'});return true;}
      if (JSON.stringify(item).length>20000) {respond(response,413,{error:'item_too_large'});return true;}
      const saved=await store.put(entity,owner,item);
      if (!saved) {respond(response,409,{error:'revision_conflict'});return true;}
      respond(response,200,{item:saved});return true;
    } catch(error) {
      respond(response,error?.status||503,{error:error?.status===413?'item_too_large'
        :error?.status===400?'invalid_json':'cloud_unavailable'});
      return true;
    }
  };
}
