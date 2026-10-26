// Genycaster-owned cloud state. OpenProof authorizes the principal, but never
// stores these app-specific preferences, drafts, bookmarks or schedules.
import {createHash, randomBytes, timingSafeEqual} from 'node:crypto';
import {createSignerSessionProof} from './signer_session_proof.mjs';

const SESSION_COOKIE='__Host-genycaster-account';
const SESSION_LIFETIME=30*24*60*60;
function sessionToken(request) {
  const cookies=String(request.headers.cookie||'').split(';');
  const match=cookies.map(value=>value.trim()).find(value=>
    value.startsWith(SESSION_COOKIE+'='));
  const value=match?.slice(SESSION_COOKIE.length+1)||'';
  return /^[A-Za-z0-9_-]{43}$/.test(value) ? value : null;
}
function sessionDigest(value) {
  return createHash('sha256').update(value).digest('hex');
}
function csrfMatches(header,expected) {
  if(typeof header!=='string' || typeof expected!=='string' ||
     header.length!==43 || expected.length!==43)return false;
  return timingSafeEqual(Buffer.from(header),Buffer.from(expected));
}
const ID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const PROVIDER = /^[a-z][a-z0-9-]{0,39}$/;
const HASH = /^0x[0-9a-f]{40}$/i;
const PROFILE_CATEGORY = new Set([
  'Arts & entertainment','Business','Education','Finance','Food & beverage',
  'Health & wellness','Media & news','Science & technology','Technology',
  'Professional services','Sports','Travel & transportation','Other'
]);
const PROFILE_ROLE = new Set([
  'Artist','Author','Creator','Designer','Developer','Engineer',
  'Entrepreneur','Founder','Investor','Journalist','Photographer',
  'Researcher','Student','Teacher','Consultant','Organization','Other'
]);
const KINDS = Object.freeze({
  drafts: ['cast', 'article', 'thread'],
  tasks: ['scheduled_cast', 'scheduled_article', 'reminder'],
});
export function validPersonalLink(value) {
  if (typeof value !== 'string' || value.length > 512) return false;
  try {
    const url = new URL(value);
    return url.protocol === 'https:' && url.username === '' && url.password === ''
      && url.port === '' && url.hostname.length <= 253
      && /^(?=.{1,253}$)(?:[a-z0-9-]+\.)+[a-z]{2,63}$/i.test(url.hostname)
      && !/\s/.test(value) && url.hash.length < 128 && url.search.length < 256;
  } catch {return false;}
}
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
  if (entity === 'profile_extras') {
    if (!Number.isSafeInteger(item.revision) || item.revision < 0
        || !Array.isArray(item.links) || item.links.length > 5
        || typeof item.category !== 'string' || item.category.length > 80
        || typeof item.profession !== 'string' || item.profession.length > 80
        || (item.category !== '' && !PROFILE_CATEGORY.has(item.category))
        // Previously saved freeform professions require explicit selection on
        // upgrade; arbitrary user strings cannot masquerade as a classified role.
        || (item.profession !== '' && !PROFILE_ROLE.has(item.profession))) return null;
    const seen = new Set();
    const links = [];
    for (const value of item.links) {
      if (!value || typeof value !== 'object' || Array.isArray(value)
          || typeof value.label !== 'string' || value.label.length > 32
          || typeof value.url !== 'string' || value.url.length > 512
          || !validPersonalLink(value.url) || seen.has(value.url)) return null;
      seen.add(value.url);
      links.push({label:value.label,url:value.url});
    }
    return {category:item.category,profession:item.profession,links,
      revision:item.revision,publicly_visible:false};
  }
  if (entity === 'connections') {
    if (!PROVIDER.test(item.provider || '') || item.provider !== 'linkedin') return null;
    if (item.manual_profile_url !== null && !validLinkedInUrl(item.manual_profile_url)) return null;
    return {provider: item.provider, manual_profile_url: item.manual_profile_url,
      publicly_visible: item.publicly_visible === true && item.manual_profile_url !== null};
  }
  if (entity === 'bookmarks') {
    if (typeof item.cast_hash !== 'string' || !HASH.test(item.cast_hash)) return null;
    if (item.mutation_id !== undefined && !ID.test(String(item.mutation_id))) return null;
    return {cast_hash: item.cast_hash.toLowerCase(), is_deleted: item.is_deleted === true,
      mutation_id:item.mutation_id ? String(item.mutation_id).toLowerCase() : undefined};
  }
  if (!ID.test(String(item.id || ''))) return null;
  if (entity === 'drafts') {
    if (!KINDS.drafts.includes(item.kind) || typeof item.title !== 'string' ||
        item.title.length > 256 || !item.content || typeof item.content !== 'object' ||
        Array.isArray(item.content) || !Number.isSafeInteger(item.revision) ||
        item.revision < 0 || (item.is_deleted !== undefined &&
          typeof item.is_deleted !== 'boolean')) return null;
    if (item.is_deleted === true &&
        (item.title !== '' || Object.keys(item.content).length !== 0)) return null;
    return {id:item.id.toLowerCase(), kind:item.kind, title:item.title, content:item.content,
      revision:item.revision,is_deleted:item.is_deleted===true};
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
    // Reserved for the app-owned read watermark, never a user-supplied event.
    if (String(item.id).toLowerCase()==='00000000-0000-4000-8000-000000000001'
        || (item.source==='genycaster' && item.kind==='seen_watermark')) return null;
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
  const signerProof=createSignerSessionProof({fetchImpl});
  function sameOriginRequest(request) {
    // The browser must be on the official Genycaster host, not another site
    // submitting a form and attempting to plant an authenticated cookie.
    return request.headers.origin==='https://caster.genyleap.com' &&
      (!request.headers['sec-fetch-site'] ||
       request.headers['sec-fetch-site']==='same-origin');
  }
  async function issueSession(owner,response) {
    if(typeof store.createSession!=='function')
      throw Object.assign(new Error('session_unavailable'),{status:503});
    const token=randomBytes(32).toString('base64url');
    const csrf=randomBytes(32).toString('base64url');
    await store.createSession(owner,sessionDigest(token),csrf);
    response.setHeader('Set-Cookie',SESSION_COOKIE+'='+token+
      '; Path=/; Max-Age='+SESSION_LIFETIME+
      '; Secure; HttpOnly; SameSite=Strict');
    respond(response,200,{fid:owner.fid,csrf_token:csrf});
  }
  // Genycaster account data is scoped to the *verified Farcaster FID*.
  // OAuth and verified Farcaster recovery must resolve to exactly the same
  // stable owner; caller-supplied identity/FID is never accepted.
  async function authorize(request) {
    const header=String(request.headers.authorization||'');
    if (!header) {
      // Browser continuity is backed by an independently verified,
      // server-issued HttpOnly cookie. A bare remembered FID is never proof.
      const cookie=sessionToken(request);
      if(!cookie || typeof store.getSession!=='function') return null;
      const session=await store.getSession(sessionDigest(cookie));
      if(!session || !/^farcaster:[1-9][0-9]{0,18}$/.test(session.subject))
        return null;
      // Browsers automatically attach cookies: non-GET operations additionally
      // require a secret returned only to the same-origin account client.
      if(request.method!=='GET' &&
          !csrfMatches(request.headers['x-genycaster-csrf'],session.csrf_token))
        return null;
      return {issuer:session.issuer,subject:session.subject,
        fid:session.subject.slice('farcaster:'.length),
        cookieAuth:true,sessionToken:cookie,csrfToken:session.csrf_token};
    }
    if (!/^Bearer [A-Za-z0-9._~+/-]+={0,2}$/.test(header) || header.length>8192) return null;
    const recovery=header.startsWith('Bearer recovery.');
    // A recovery ticket is verified by the pre-existing Farcaster broker in
    // the Genycaster media gateway. It holds the OpenProof session server-side.
    // No new grant, cookie transfer to browser, or OpenProof modification.
    const connectionEndpoint=recovery
      ? 'https://storage-farcaster.genyleap.com/v1/browser/openproof/connections'
      : upstream+'/account/connections';
    if (!recovery) {
      const profileResponse=await fetchImpl(upstream+'/account/profile',{
        headers:{authorization:header,accept:'application/json'},
        redirect:'error',signal:AbortSignal.timeout(6000)});
      if (profileResponse.status!==200) return null;
      const profile=await profileResponse.json();
      if (!profile || typeof profile.identity_id!=='string'
          || !/^[A-Za-z0-9:_-]{1,200}$/.test(profile.identity_id)) return null;
    }
    const response=await fetchImpl(connectionEndpoint,{
      headers:{authorization:header,accept:'application/json'},
      redirect:'error',signal:AbortSignal.timeout(6000)});
    if (response.status!==200) return null;
    const payload=await response.json();
    if (!Array.isArray(payload.connections) || payload.connections.length>100) return null;
    const linked=payload.connections.filter(c=>c?.provider==='farcaster');
    if (linked.length!==1) return null;
    const fid=String(linked[0].fid||'');
    if (!/^[1-9][0-9]{0,18}$/.test(fid) || !Number.isSafeInteger(Number(fid))) return null;
    return {issuer:upstream,subject:'farcaster:'+fid,authorization:header,fid};
  }
  return async function handleCloud(request,response,path) {
    if (!path.startsWith('/api/v1/cloud/')) return false;
    const entity=path.slice('/api/v1/cloud/'.length);
    if (!['connections','bookmarks','notifications','notification_seen',
      'app_updates','tasks','drafts','profile_extras','connections/refresh',
      'session','session/challenge','session/proof'].includes(entity)) {
      respond(response,404,{error:'not_found'});return true;
    }
    if (entity==='app_updates') {
      if (request.method!=='GET') respond(response,405,{error:'method_not_allowed'});
      else try {respond(response,200,{items:await store.listUpdates()});}
        catch {respond(response,503,{error:'cloud_unavailable'});}
      return true;
    }
    if(entity==='session/challenge' || entity==='session/proof') {
      if(request.method!=='POST') {
        respond(response,405,{error:'method_not_allowed'});return true;
      }
      if(!sameOriginRequest(request)) {
        respond(response,403,{error:'origin_not_allowed'});return true;
      }
      try {
        if(!String(request.headers['content-type']||'').startsWith('application/json')) {
          respond(response,415,{error:'unsupported_media_type'});return true;
        }
        const data=await readBody(request,1024);
        if(entity==='session/challenge') {
          respond(response,200,signerProof.issue(data?.fid));return true;
        }
        const owner=await signerProof.prove(data);
        await issueSession(owner,response);
        return true;
      }catch(error) {
        const code=String(error?.message||'');
        const allowed=new Set(['invalid_fid','challenge_limit',
          'invalid_account_proof','unregistered_signer',
          'signer_registry_unavailable','session_unavailable',
          'account_sessions_not_ready','too_large','invalid_json']);
        respond(response,[400,401,413,429,503].includes(error?.status)
          ?error.status:503,{error:allowed.has(code)?code:'session_unavailable'});
        return true;
      }
    }
    let owner;
    try {owner=await authorize(request);}catch{}
    if (!owner) {respond(response,401,{error:'authentication_required'});return true;}
    try {
      if(entity==='session') {
        if(request.method==='POST') {
          // A verified, fresh OpenProof/Farcaster grant is the only bootstrap
          // path. An existing cookie cannot issue an unlimited new cookie.
          if(owner.cookieAuth || typeof store.createSession!=='function') {
            respond(response,401,{error:'authentication_required'});return true;
          }
          await issueSession(owner,response);
          return true;
        }
        if(request.method==='GET' && owner.cookieAuth) {
          respond(response,200,{fid:owner.fid,csrf_token:owner.csrfToken});
          return true;
        }
        if(request.method==='DELETE' && owner.cookieAuth
           && typeof store.revokeSession==='function') {
          await store.revokeSession(sessionDigest(owner.sessionToken));
          response.setHeader('Set-Cookie',SESSION_COOKIE+
            '=; Path=/; Max-Age=0; Secure; HttpOnly; SameSite=Strict');
          respond(response,200,{revoked:true});return true;
        }
        respond(response,405,{error:'method_not_allowed'});return true;
      }
      if (entity==='notification_seen') {
        // A private account-wide read watermark, not a protocol notification.
        // The server resolves the owner from the verified bearer, never a FID
        // supplied by the caller. Milliseconds travel as decimal strings to
        // avoid losing precision in JavaScript/JSON transports.
        if (request.method==='GET') {
          respond(response,200,{seen_at_ms:await store.getNotificationSeen(owner)});
          return true;
        }
        if (request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const raw=await readBody(request);
        const value=raw?.seen_at_ms;
        if (typeof value!=='string' || !/^(0|[1-9][0-9]{0,15})$/.test(value)
            || !Number.isSafeInteger(Number(value))
            || Number(value)>Date.now()+300_000) {
          respond(response,400,{error:'invalid_seen_at'});return true;
        }
        respond(response,200,{seen_at_ms:await store.putNotificationSeen(owner,value)});
        return true;
      }
      if (entity==='connections/refresh') {
        if (request.method!=='POST') {respond(response,405,{error:'method_not_allowed'});return true;}
        const reply=await fetchImpl(owner.authorization.startsWith('Bearer recovery.')
          ? 'https://storage-farcaster.genyleap.com/v1/browser/openproof/connections'
          : upstream+'/account/connections',{
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
        if (entity==='bookmarks' && typeof store.listBookmarkChanges==='function') {
          const url=new URL(request.url,'http://localhost');
          const value=url.searchParams.get('since') || '0';
          if (!/^(0|[1-9][0-9]{0,17})$/.test(value)) {
            respond(response,400,{error:'invalid_cursor'});return true;
          }
          const items=await store.listBookmarkChanges(owner,value);
          const cursor=items.length ? String(items.at(-1).sequence) : value;
          respond(response,200,{items,next_cursor:cursor});return true;
        }
        if (entity==='profile_extras') {
          respond(response,200,{item:await store.getProfileExtras(owner)});return true;
        }
        if (entity==='drafts') {
          const id=new URL(request.url,'http://localhost').searchParams.get('id');
          if (id!==null) {
            if (!ID.test(id)) {respond(response,400,{error:'invalid_draft_id'});return true;}
            const row=await store.getDraft(owner,id.toLowerCase());
            respond(response,200,{items:row?[row]:[]});return true;
          }
        }
        respond(response,200,{items:await store.list(entity,owner)});return true;
      }
      if (request.method!=='PUT') {respond(response,405,{error:'method_not_allowed'});return true;}
      // Rich drafts store the entire editable document, not only cast text.
      // Keep them bounded separately from short profile and preference writes.
      const itemLimit=entity==='drafts'?196_608:20_000;
      const raw=await readBody(request,entity==='drafts'?208_896:24_576);
      const item=validateCloudItem(entity,raw);
      if (!item) {respond(response,400,{error:'invalid_item'});return true;}
      if (Buffer.byteLength(JSON.stringify(item),'utf8')>itemLimit) {
        respond(response,413,{error:'item_too_large'});return true;
      }
      const saved=entity==='profile_extras'
        ? await store.putProfileExtras(owner,item)
        : await store.put(entity,owner,item);
      if (!saved) {respond(response,409,{error:'revision_conflict'});return true;}
      respond(response,200,{item:saved});return true;
    } catch(error) {
      respond(response,error?.status||503,{error:error?.status===413?'item_too_large'
        :error?.status===400?'invalid_json':'cloud_unavailable'});
      return true;
    }
  };
}
