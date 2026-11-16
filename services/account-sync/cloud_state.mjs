// Genycaster-owned cloud state. OpenProof authorizes the principal, but never
// stores these app-specific preferences, drafts, bookmarks or schedules.
import {createHash, randomBytes, timingSafeEqual} from 'node:crypto';
import {createSignerSessionProof} from './signer_session_proof.mjs';
import {serverDeployment, trustedOrigin} from '../deployment_config.mjs';
import {validateFeedback} from './feedback.mjs';
import {validateCastTopicAssignment,TOPIC_SLUG,topicPagination} from './topics.mjs';
import {createGithubCalendarReader} from './github_contributions.mjs';
import {calculateStreaks,eligibleStreakDay,STREAK_START_DAY} from './streaks.mjs';
import {createGenyAssetsPolicy,evaluateGenyAssets,verifiedAccountWallets} from './geny_assets.mjs';
import {createProBillingPolicy,createProRateReader,createProTokenVerifier,createProQuote,
  proPaymentAsset,quoteAsset,formatTokenAmount,
  formatGeny,subscriptionEntitlements,verifyProTransfer} from './pro_billing.mjs';
import {createLeaderboardProfileReader} from './leaderboard_profiles.mjs';

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
  'Professional services','Sports','Trading','Travel & transportation','Other'
]);
const PROFILE_ROLE = new Set([
  'Artist','Author','Creator','Designer','Developer','Engineer',
  'Entrepreneur','Founder','Investor','Journalist','Photographer',
  'Researcher','Student','Teacher','Consultant','Organization','Other',
  'Trader','Analyst','Product Manager','Community Manager','Content Creator'
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
export function verifiedGitHubProfile(value) {
  // Only consume this on a server-to-server OpenProof verified connection
  // response. Never elevate owner-authored portable profile labels.
  if(typeof value!=='string' || value.length>160)return null;
  try {
    const url=new URL(value);
    if(url.protocol!=='https:' || url.hostname!=='github.com' ||
       url.port || url.username || url.password || url.search || url.hash)
      return null;
    const match=/^\/([A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?)\/?$/.exec(url.pathname);
    return match ? {login:match[1],url:'https://github.com/'+match[1]} : null;
  }catch {return null;}
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
    const secondary=item.secondary_categories===undefined?[]:item.secondary_categories;
    if(!Array.isArray(secondary)||secondary.length>3
       || new Set(secondary).size!==secondary.length
       || secondary.some(value=>typeof value!=='string'
          || !PROFILE_CATEGORY.has(value) || value===item.category))return null;
    const roles=item.roles===undefined
      ? (item.profession?[item.profession]:[]) : item.roles;
    if(!Array.isArray(roles)||roles.length>5||new Set(roles).size!==roles.length
      ||roles.some(value=>typeof value!=='string'||!PROFILE_ROLE.has(value))
      ||(roles[0]||'')!==item.profession)return null;
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
    return {category:item.category,profession:item.profession,
      secondary_categories:secondary,roles,links,
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
    const priority=item.priority===undefined?0:item.priority;
    if (!Number.isInteger(priority)||priority<0||priority>2)return null;
    let preview=null;
    if (item.preview!==undefined && item.preview!==null) {
      const p=item.preview;
      if(!p||typeof p!=='object'||Array.isArray(p))return null;
      const fid=Number(p.authorFid||0);
      if(!Number.isSafeInteger(fid)||fid<0||fid>Number.MAX_SAFE_INTEGER)return null;
      for(const key of ['author','username','displayName','avatarUrl','text'])
        if(p[key]!==undefined && typeof p[key]!=='string')return null;
      if(String(p.avatarUrl||'') &&
         (!/^https:\/\//.test(p.avatarUrl)||p.avatarUrl.length>512))return null;
      const stamp=Number(p.createdAtMs||0);
      if(!Number.isSafeInteger(stamp)||stamp<0)return null;
      preview={authorFid:fid,author:String(p.author||'').slice(0,128),
        username:String(p.username||'').slice(0,128),
        displayName:String(p.displayName||'').slice(0,128),
        avatarUrl:String(p.avatarUrl||'').slice(0,512),
        text:String(p.text||'').slice(0,1200),createdAtMs:stamp};
    }
    return {cast_hash: item.cast_hash.toLowerCase(), is_deleted: item.is_deleted === true,
      priority,preview,
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
    // Application-controlled singletons are reserved from the generic API.
    if (['00000000-0000-4000-8000-000000000001',
         '00000000-0000-4000-8000-000000000003'].includes(String(item.id).toLowerCase())
        || (item.source==='genycaster'
            && ['seen_watermark','profile_status','streak_preferences'].includes(item.kind))) return null;
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
export function validateBookmarkCollectionChange(raw) {
  if(!raw || typeof raw!=='object' || Array.isArray(raw)
     || !ID.test(String(raw.id||''))) return null;
  const action=raw.action;
  if(!['create','rename','delete'].includes(action))return null;
  if(action!=='create' && (!/^[1-9][0-9]{0,15}$/.test(String(raw.expected_revision||''))))
    return null;
  let name='';
  if(action!=='delete') {
    if(typeof raw.name!=='string' || raw.name!==raw.name.trim()
       || raw.name!==raw.name.normalize('NFC')
       || Array.from(raw.name).length<1 || Array.from(raw.name).length>80
       || /[\u0000-\u001f\u007f-\u009f]/u.test(raw.name))
      return null;
    name=raw.name;
  }
  return {id:raw.id.toLowerCase(),action,name,
          ...(action==='create'?{}:{expected_revision:String(raw.expected_revision)})};
}
export function validateBookmarkCollectionItem(raw) {
  if(!raw || typeof raw!=='object' || Array.isArray(raw)
     || !ID.test(String(raw.collection_id||''))
     || !HASH.test(String(raw.cast_hash||''))
     || typeof raw.remove!=='boolean') return null;
  return {collection_id:raw.collection_id.toLowerCase(),
          cast_hash:raw.cast_hash.toLowerCase(),remove:raw.remove};
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
export async function verifyCastTopicOwnership(hash,fid,{fetchImpl=globalThis.fetch,
  hypersnapOrigin=serverDeployment().hypersnapOrigin,
  snapchainOrigin=serverDeployment().snapchainOrigin}={}) {
  // Verify a confirmed CastAdd against a protocol node. HyperSnap is the
  // default network, but users may choose Snapchain. Neither a signed-in FID
  // nor a hash supplied by the client proves ownership by itself.
  if(!/^0x[0-9a-f]{40}$/.test(hash) ||
     !/^[1-9][0-9]{0,18}$/.test(String(fid))) return false;
  const castId='?fid='+encodeURIComponent(String(fid))+
    '&hash='+encodeURIComponent(hash);
  for(const base of [
    hypersnapOrigin+'/v1/castById',
    snapchainOrigin+'/v1/castById'
  ]) {
    try {
      const response=await fetchImpl(base+castId,{method:'GET',
        headers:{accept:'application/json'},redirect:'error',
        signal:AbortSignal.timeout(4500)});
      if(response.status!==200 ||
         Number(response.headers.get('content-length')||0)>32768 ||
         !response.body)continue;
      const chunks=[];let size=0;
      for await(const chunk of response.body) {
        size+=chunk.byteLength;
        if(size>32768)break;
        chunks.push(Buffer.from(chunk));
      }
      if(size>32768)continue;
      const payload=JSON.parse(Buffer.concat(chunks).toString('utf8'));
      const record=payload?.message??payload;
      const data=record?.data;
      if(String(data?.fid||'')===String(fid) &&
         String(record?.hash||'').toLowerCase()===hash &&
         !!data?.castAddBody && typeof data.castAddBody==='object')
        return true;
    }catch{}
  }
  // Some HyperSnap nodes expose indexed casts through v2 rather than a v1
  // hub read. Require the exact FID/hash pair in a bounded server response.
  try {
    const url=hypersnapOrigin+'/v2/farcaster/feed/user/casts?fid='+
      encodeURIComponent(String(fid))+'&limit=100';
    const response=await fetchImpl(url,{method:'GET',
      headers:{accept:'application/json'},redirect:'error',
      signal:AbortSignal.timeout(4500)});
    if(response.status!==200 ||
       Number(response.headers.get('content-length')||0)>131072 ||
       !response.body)return false;
    const chunks=[];let size=0;
    for await(const chunk of response.body) {
      size+=chunk.byteLength;
      if(size>131072)return false;
      chunks.push(Buffer.from(chunk));
    }
    const payload=JSON.parse(Buffer.concat(chunks).toString('utf8'));
    if(!Array.isArray(payload?.casts)||payload.casts.length>100)return false;
    return payload.casts.some(cast=>
      String(cast?.author?.fid||'')===String(fid) &&
      String(cast?.hash||'').replace(/^0x/i,'').toLowerCase()===hash.slice(2));
  }catch{return false;}
}
// Explicit support for the two protocol providers. A client-provided cast
// hash is never proof: the provider must independently return the exact FID,
 // hash and a CastAdd payload. Both endpoints are bounded and fail closed.
export async function verifyStreakCastOwnership(hash,fid,{fetchImpl=globalThis.fetch,
  hypersnapOrigin=serverDeployment().hypersnapOrigin,
  snapchainOrigin=serverDeployment().snapchainOrigin}={}) {
  if(!HASH.test(hash)||!/^[1-9][0-9]{0,18}$/.test(String(fid)))return null;
  // HyperSnap's documented feed/user/casts endpoint is the first choice.
  // Match the returned author FID AND hash; only fresh published casts count.
  // Its hash is bare 40-hex (unlike the 0x-prefixed account API hash).
  try {
    const url=hypersnapOrigin+'/v2/farcaster/feed/user/casts'
      +'?fid='+encodeURIComponent(String(fid))+'&limit=100';
    const response=await fetchImpl(url,{method:'GET',redirect:'error',
      headers:{accept:'application/json'},signal:AbortSignal.timeout(4500)});
    if(response.status===200 &&
       Number(response.headers.get('content-length')||0)<=131072 && response.body) {
      const chunks=[];let size=0;
      for await(const chunk of response.body) {
        size+=chunk.byteLength;if(size>131072)break;
        chunks.push(Buffer.from(chunk));
      }
      if(size<=131072) {
        const payload=JSON.parse(Buffer.concat(chunks).toString('utf8'));
        if(Array.isArray(payload?.casts) && payload.casts.length<=100)
          for(const cast of payload.casts) {
            if(String(cast?.author?.fid||'')!==String(fid) ||
               String(cast?.hash||'').replace(/^0x/i,'').toLowerCase()
                 !==hash.slice(2).toLowerCase())continue;
            const stamp=Date.parse(cast.timestamp);
            if(!Number.isFinite(stamp) || stamp>Date.now()+300000 ||
               stamp<Date.now()-172800000)continue;
            return {kind:cast.parent_hash ? 'reply':'cast',
              day:new Date(stamp).toISOString().slice(0,10)};
          }
      }
    }
  }catch{}
  // Compatibility with v1 hub reads on either protocol network.
  const endpoints=[
    hypersnapOrigin+'/v1/castById',
    snapchainOrigin+'/v1/castById'
  ];
  for(const base of endpoints) {
    try {
      const response=await fetchImpl(base+'?fid='+encodeURIComponent(String(fid))
        +'&hash='+encodeURIComponent(hash),{method:'GET',
          headers:{accept:'application/json'},redirect:'error',
          signal:AbortSignal.timeout(4500)});
      if(response.status!==200 || Number(response.headers.get('content-length')||0)>32768
         ||!response.body)continue;
      const chunks=[];let size=0;
      for await(const chunk of response.body) {
        size+=chunk.byteLength;
        if(size>32768)break;
        chunks.push(Buffer.from(chunk));
      }
      if(size>32768)continue;
      const payload=JSON.parse(Buffer.concat(chunks).toString('utf8'));
      const record=payload?.message??payload;
      const data=record?.data;
      if(String(data?.fid)!==String(fid) ||
         String(record?.hash||'').toLowerCase()!==hash.toLowerCase() ||
         !data?.castAddBody || typeof data.castAddBody!=='object')continue;
      // Farcaster protocol timestamps are seconds since 2021-01-01 UTC.
      const timestamp=Number(data.timestamp);
      const published=Date.UTC(2021,0,1)+timestamp*1000;
      if(!Number.isSafeInteger(timestamp) || published>Date.now()+300000
         || published<Date.now()-172800000)continue;
      const body=data.castAddBody;
      return {kind:body.parentCastId ? 'reply':'cast',
        day:new Date(published).toISOString().slice(0,10)};
    }catch {}
  }
  return null;
}
// Recover activity from authoritative signed network indexes, even when a
// just-confirmed Cast was not reported by the client (session refresh, tab
// close, temporary 422 while HyperSnap indexes a newly published message).
// Never credit client-provided days or hashes, and never derive an owner FID
// from a request parameter. Feed rows are validated against the authorized FID.
export async function discoverVerifiedStreakCasts(fid,{
  fetchImpl=globalThis.fetch,now=new Date(),
  hypersnapOrigin=serverDeployment().hypersnapOrigin
}={}) {
  const owner=String(fid||'');
  if(!/^[1-9][0-9]{0,18}$/.test(owner) || !Number.isSafeInteger(Number(owner)))
    return null;
  try {
    const url=hypersnapOrigin+'/v2/farcaster/feed/user/casts'
      +'?fid='+encodeURIComponent(owner)+'&limit=50';
    const response=await fetchImpl(url,{method:'GET',redirect:'error',
      headers:{accept:'application/json'},signal:AbortSignal.timeout(4500)});
    if(response.status!==200 ||
       Number(response.headers.get('content-length')||0)>262144 ||
       !response.body) return null;
    const chunks=[];let bytes=0;
    for await(const chunk of response.body) {
      bytes+=chunk.byteLength;
      if(bytes>262144)return null;
      chunks.push(Buffer.from(chunk));
    }
    const doc=JSON.parse(Buffer.concat(chunks).toString('utf8'));
    if(!Array.isArray(doc?.casts) || doc.casts.length>50)return null;
    const earliest=Math.max(now.getTime()-14*86400000,
      Date.parse(STREAK_START_DAY+'T00:00:00Z'));
    const newest=now.getTime()+300000;
    const days=new Set();
    const events=[];
    for(const cast of doc.casts) {
      if(String(cast?.author?.fid||'')!==owner)continue;
      const hash=String(cast?.hash||'').replace(/^0x/i,'').toLowerCase();
      if(!/^[0-9a-f]{40}$/.test(hash) ||
         (cast.object!=='cast' && cast.object!==undefined))continue;
      const timestamp=Date.parse(cast.timestamp);
      if(!Number.isFinite(timestamp) || timestamp>newest ||
          timestamp<earliest)continue;
      const kind=cast.parent_hash ? 'reply' : 'cast';
      const day=new Date(timestamp).toISOString().slice(0,10);
      // Only one verified immutable Cast per UTC day and activity type is
      // required. Avoid dozens of SQL writes from prolific accounts.
      const dayKind=day+':'+kind;
      if(days.has(dayKind))continue;
      days.add(dayKind);
      events.push({hash:'0x'+hash,day,kind});
    }
    return events;
  }catch {return null;}
}

export function createCloudHandler({store, issuer, fetchImpl=globalThis.fetch,
                                    publicOrigin=serverDeployment().publicOrigin,
                                    browserGateway=serverDeployment().browserGateway,
                                    snapchainOrigin=serverDeployment().snapchainOrigin,
                                    hypersnapOrigin=serverDeployment().hypersnapOrigin,
                                    verifyTopicOwner=verifyCastTopicOwnership,
                                    verifyStreakCast=verifyStreakCastOwnership,
                                    proPolicy=createGenyAssetsPolicy(),
                                    billingPolicy=createProBillingPolicy(),
                                    readGenyRate=createProRateReader({fetchImpl}),
                                    checkProToken=createProTokenVerifier({fetchImpl}),
                                    resolveProPaymentWallets=(owner)=>verifiedAccountWallets(owner,{policy:proPolicy,fetchImpl}),
                                    verifyBillingTransfer=verifyProTransfer,
                                    resolvePro=async(fid,owner)=>{
                                      if(!owner || !store.proBillingReady?.())
                                        return {...subscriptionEntitlements(null,billingPolicy),status:'unavailable',eligible:false};
                                      return subscriptionEntitlements(
                                        await store.getProSubscription(owner),billingPolicy);
                                    },
                                    resolveGenyAssets=(fid)=>evaluateGenyAssets(fid,{policy:proPolicy,fetchImpl}),
                                    discoverStreakCasts=(fid)=>discoverVerifiedStreakCasts(fid,{fetchImpl,hypersnapOrigin})}) {
  // Bounded per-account reconciliation cache; failed network fetches never
  // block a valid historical streak already persisted in PostgreSQL.
  const recentlyCheckedStreaks=new Map();
  async function reconcileVerifiedStreak(owner) {
    const key=String(owner.fid||'');
    if(!/^[1-9][0-9]{0,18}$/.test(key))return;
    const now=Date.now();
    const cached=recentlyCheckedStreaks.get(key);
    if(cached && now-cached.checkedAt<90000)return cached.pending;
    if(recentlyCheckedStreaks.size>=1024)
      recentlyCheckedStreaks.delete(recentlyCheckedStreaks.keys().next().value);
    const pending=(async()=>{
      const casts=await discoverStreakCasts(key);
      if(!Array.isArray(casts))return;
      for(const entry of casts) {
        if(!HASH.test(entry.hash) || !['cast','reply'].includes(entry.kind)
            || !eligibleStreakDay(entry.day))continue;
        // The network reader already verified the author and timestamp; the
        // store enforces idempotency on (issuer, subject, cast_hash).
        await store.recordStreakCast(owner,entry.hash,entry.kind,entry.day);
      }
    })();
    recentlyCheckedStreaks.set(key,{checkedAt:now,pending});
    try {await pending;}catch{} // Read the last durable state if indexer is down.
    const current=recentlyCheckedStreaks.get(key);
    if(current?.pending===pending)recentlyCheckedStreaks.set(key,{checkedAt:now});
  }

  if (!store || !issuer || new URL(issuer).protocol !== 'https:')
    throw new Error('A store and trusted HTTPS OpenProof issuer are required');
  const upstream=trustedOrigin(issuer,'GENYCASTER_OPENPROOF_ISSUER');
  const trustedPublicOrigin=trustedOrigin(publicOrigin,'GENYCASTER_PUBLIC_ORIGIN');
  const trustedBrowserGateway=trustedOrigin(browserGateway,'GENYCASTER_BROWSER_GATEWAY_ORIGIN');
  const trustedSnapchainOrigin=trustedOrigin(snapchainOrigin,'GENYCASTER_SNAPCHAIN_URL');
  const trustedHyperSnapOrigin=trustedOrigin(hypersnapOrigin,'GENYCASTER_HYPERSNAP_ORIGIN');
  const signerProof=createSignerSessionProof({
    fetchImpl,issuer:upstream,snapchainOrigin:trustedSnapchainOrigin,
    hypersnapOrigin:trustedHyperSnapOrigin});
  const readGithubCalendar=createGithubCalendarReader({fetchImpl});
  const leaderboardProfiles=createLeaderboardProfileReader({
    fetchImpl,hypersnapOrigin:trustedHyperSnapOrigin});
  function sameOriginRequest(request) {
    // The browser must be on the official Genycaster host, not another site
    // submitting a form and attempting to plant an authenticated cookie.
    return request.headers.origin===trustedPublicOrigin &&
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
  async function allowProMutation(owner,response) {
    const membership=await resolvePro(owner.fid,owner);
    if(membership.status==='verified' && membership.eligible===true) return true;
    respond(response,403,{error:'geny_pro_required',
      monthly_usd_cents:billingPolicy.monthlyUsdCents,
      verification_status:membership.status});
    return false;
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
      // The signed account proof issues protocol-scoped sessions. HyperSnap
      // native FIDs use their own verified signer registry and issuer; never
      // mistake them for a linked OpenProof/Farcaster identity.
      const match=/^(farcaster|hypersnap):([1-9][0-9]{0,18})$/
        .exec(String(session?.subject||''));
      if(!match || !Number.isSafeInteger(Number(match[2])) ||
          (match[1]==='hypersnap' &&
           (session.issuer!=='hypersnap:native-registry' ||
            Number(match[2])<4294967296)))return null;
      // Browsers automatically attach cookies: non-GET operations additionally
      // require a secret returned only to the same-origin account client.
      if(request.method!=='GET' &&
          !csrfMatches(request.headers['x-genycaster-csrf'],session.csrf_token))
        return null;
      return {issuer:session.issuer,subject:session.subject,
        fid:match[2],cookieAuth:true,
        sessionToken:cookie,csrfToken:session.csrf_token};
    }
    if (!/^Bearer [A-Za-z0-9._~+/-]+={0,2}$/.test(header) || header.length>8192) return null;
    const recovery=header.startsWith('Bearer recovery.');
    // A recovery ticket is verified by the pre-existing Farcaster broker in
    // the Genycaster media gateway. It holds the OpenProof session server-side.
    // No new grant, cookie transfer to browser, or OpenProof modification.
    const connectionEndpoint=recovery
      ? trustedBrowserGateway+'/v1/browser/openproof/connections'
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
      'connections/portable','topics','topic_casts','cast_topics',
      'bookmark_collections','bookmark_collection_items','github_contributions','profile_status',
      'entitlements','geny/assets','pro/plan','pro/quote','pro/verify',
      'streak','streak/cast','streak/preferences','streak/leaderboard','profile_activity',
      'muted_profiles','feedback',
      'session','session/challenge','session/proof'].includes(entity)) {
      respond(response,404,{error:'not_found'});return true;
    }
    // Public price discovery is not an entitlement. Users can see the
    // operator-controlled price before establishing a private cloud session.
    // Checkout and subscription activation remain strictly authenticated.
    if(entity==='pro/plan') {
      if(request.method!=='GET') {
        respond(response,405,{error:'method_not_allowed'});return true;
      }
      respond(response,200,{plan:{
        price_usd_cents:billingPolicy.monthlyUsdCents,
        period_days:billingPolicy.periodDays,
        chain_id:billingPolicy.chainId,
        token_symbol:'GENY',payment_assets:['GENY','USDC'],
        payment_available:Boolean(billingPolicy.enabled && store.proBillingReady?.()),
        billing_mode:'monthly_payment'
      }});
      return true;
    }
    if(entity==='profile_activity' && request.method==='GET') {
      const params=new URL(request.url,'http://localhost').searchParams;
      const fid=params.get('fid');
      if(!fid || params.getAll('fid').length!==1 ||
          !/^[1-9][0-9]{0,18}$/.test(fid) ||
          !Number.isSafeInteger(Number(fid))) {
        respond(response,400,{error:'invalid_fid'});return true;
      }
      try {
        const data=await store.getPublicActivitySource(fid,upstream);
        respond(response,200,{fid,activity_source:data.activity_source,
          github_login:data.github_login});
      }catch {
        respond(response,503,{error:'activity_preference_unavailable'});
      }
      return true;
    }
    if(entity==='profile_status' && request.method==='GET') {
      const params=new URL(request.url,'http://localhost').searchParams;
      const fid=params.get('fid');
      if(fid!==null) {
        if(params.getAll('fid').length!==1 || !/^[1-9][0-9]{0,18}$/.test(fid)
            || !Number.isSafeInteger(Number(fid))) {
          respond(response,400,{error:'invalid_fid'});return true;
        }
        try {respond(response,200,{item:await store.getPublicProfileStatus(fid,upstream)});}
        catch {respond(response,503,{error:'status_unavailable'});}
        return true;
      }
    }
    // Rankings are public read-only. Joining is a private, authenticated
    // preference; viewing never depends on the visitor opting in.
    if(entity==='streak/leaderboard') {
      if(request.method!=='GET') {
        respond(response,405,{error:'method_not_allowed'});return true;
      }
      if(typeof store.streakReady!=='function' || !store.streakReady()) {
        respond(response,503,{error:'streak_unavailable'});return true;
      }
      const params=new URL(request.url,'http://localhost').searchParams;
      const track=params.get('track')||'activity';
      if(params.getAll('track').length>1 ||
         !['activity','creator'].includes(track)) {
        respond(response,400,{error:'invalid_streak_track'});return true;
      }
      try {
        const leaders=await store.listStreakLeaderboard(track);
        respond(response,200,{track,leaders:await leaderboardProfiles.decorate(leaders)});
      }catch {respond(response,503,{error:'leaderboard_unavailable'});}
      return true;
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
      if(entity==='feedback') {
        if(typeof store.feedbackReady!=='function' || !store.feedbackReady()) {
          respond(response,503,{error:'feedback_unavailable'});return true;
        }
        if(request.method==='GET') {
          respond(response,200,await store.getFeedbackOverview(owner));return true;
        }
        if(request.method!=='POST') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        if(!String(request.headers['content-type']||'').startsWith('application/json')) {
          respond(response,415,{error:'unsupported_media_type'});return true;
        }
        const input=await readBody(request,16_384);
        const report=validateFeedback(input);
        if(!report) {respond(response,400,{error:'invalid_feedback'});return true;}
        const saved=await store.submitFeedback(owner,report);
        if(!saved) {respond(response,429,{error:'feedback_limit_reached'});return true;}
        respond(response,201,{report:saved});return true;
      }
      if(entity==='geny/assets') {
        if(request.method!=='GET') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        respond(response,200,{holding:await resolveGenyAssets(owner.fid)});
        return true;
      }
      if(entity==='entitlements') {
        if(request.method!=='GET') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        respond(response,200,{entitlements:await resolvePro(owner.fid,owner)});
        return true;
      }
      if(entity==='pro/quote') {
        if(request.method!=='POST') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        if(!store.proBillingReady?.() || !billingPolicy.enabled) {
          respond(response,503,{error:'billing_not_configured'});return true;
        }
        if(!String(request.headers['content-type']||'').startsWith('application/json')) {
          respond(response,415,{error:'unsupported_media_type'});return true;
        }
        const input=await readBody(request,128);
        if(!input || Array.isArray(input) || Object.keys(input).some(x=>x!=='asset')) {
          respond(response,400,{error:'invalid_quote_request'});return true;
        }
        const asset=input.asset===undefined?'GENY':input.asset;
        if(asset!=='GENY' && asset!=='USDC') {
          respond(response,400,{error:'unsupported_payment_asset'});return true;
        }
        let payerWallets;
        try {
          payerWallets=await resolveProPaymentWallets(owner);
          if(!Array.isArray(payerWallets) || payerWallets.length===0) {
            respond(response,422,{error:'verified_wallet_required'});return true;
          }
        }catch {
          respond(response,503,{error:'wallet_verification_unavailable'});return true;
        }
        let quote;
        try {
          await checkProToken(billingPolicy,asset);
          const rate=asset==='GENY'?await readGenyRate():null;
          quote=createProQuote({policy:billingPolicy,rate,asset});
        }catch {
          respond(response,503,{error:'billing_rate_unavailable'});return true;
        }
        const stored=await store.createProQuote(owner,quote);
        if(!stored) {respond(response,429,{error:'too_many_pending_quotes'});return true;}
        respond(response,201,{quote:{
          id:stored.id,price_usd_cents:stored.price_usd_cents,
          asset:quoteAsset(stored),decimals:proPaymentAsset(quoteAsset(stored)).decimals,
          token:stored.token,treasury:stored.treasury,
          chain_id:stored.chain_id,amount_base_units:stored.amount_base_units,
          amount_geny:formatTokenAmount(BigInt(stored.amount_base_units),
            proPaymentAsset(quoteAsset(stored)).decimals),
          geny_price_usd:stored.geny_price_usd,
          created_at:stored.created_at,expires_at:stored.expires_at,
          period_days:30,verified_payers:payerWallets.slice(0,100)
        }});return true;
      }
      if(entity==='pro/verify') {
        if(request.method!=='POST') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        if(!store.proBillingReady?.() || !billingPolicy.enabled) {
          respond(response,503,{error:'billing_not_configured'});return true;
        }
        if(!String(request.headers['content-type']||'').startsWith('application/json')) {
          respond(response,415,{error:'unsupported_media_type'});return true;
        }
        const input=await readBody(request,512);
        if(!input || Array.isArray(input) || Object.keys(input).sort().join(',')!=='quote_id,tx_hash'
            || typeof input.quote_id!=='string'
            || !/^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-8][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$/.test(input.quote_id)
            || typeof input.tx_hash!=='string'
            || !/^0x[0-9a-fA-F]{64}$/.test(input.tx_hash)) {
          respond(response,400,{error:'invalid_payment_reference'});return true;
        }
        const quote=await store.getProQuote(owner,input.quote_id);
        if(!quote) {respond(response,404,{error:'quote_not_found'});return true;}
        if(quote.status==='paid') {
          if(quote.tx_hash===input.tx_hash.toLowerCase()) {
            respond(response,200,{status:'already_paid',
              entitlements:await resolvePro(owner.fid,owner)});return true;
          }
          respond(response,409,{error:'quote_already_paid'});return true;
        }
        if(!['GENY','USDC'].some(asset=>quote.token===proPaymentAsset(asset).token)
            || quote.chain_id!==billingPolicy.chainId
            || quote.treasury!==billingPolicy.treasury) {
          respond(response,409,{error:'billing_policy_changed'});return true;
        }
        let receipt;
        try {
          receipt=await verifyBillingTransfer({
            owner,quote,txHash:input.tx_hash,policy:billingPolicy,fetchImpl});
        }catch(error) {
          const code=String(error?.message||'');
          const rejected=new Set(['invalid_payment_hash','verified_wallet_required',
            'wrong_billing_chain','payment_failed','payment_not_matched',
            'payment_outside_quote','quote_not_payable']);
          respond(response,code==='payment_pending'?202:rejected.has(code)?422:503,
            {error:code==='payment_pending'?code
              :rejected.has(code)?code:'billing_verification_unavailable'});
          return true;
        }
        const result=await store.confirmProPayment(owner,quote.id,receipt);
        if(result.status==='conflict') {
          respond(response,409,{error:'payment_already_used'});return true;
        }
        if(result.status==='not_found') {
          respond(response,404,{error:'quote_not_found'});return true;
        }
        respond(response,200,{status:result.status,
          entitlements:await resolvePro(owner.fid,owner)});return true;
      }
      // Writes to app-owned Pro features must never trust local status, a
      // profile decoration, or a caller-provided wallet address. Refresh the
      // server-issued entitlement using this verified session's FID.
      if((entity==='connections' || entity==='connections/portable')
          && request.method==='PUT'
          && !await allowProMutation(owner,response)) return true;
      if(entity==='streak/preferences') {
        if(request.method==='GET') {
          respond(response,200,{preferences:await store.getStreakPreferences(owner)});
          return true;
        }
        if(request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const payload=await readBody(request,512);
        if(!payload || Array.isArray(payload) ||
           Object.keys(payload).sort().join(',')!=='activity_source,leaderboard_visible,revision' ||
           !['casts','github'].includes(payload.activity_source) ||
           typeof payload.leaderboard_visible!=='boolean' ||
           !Number.isSafeInteger(payload.revision) || payload.revision<0) {
          respond(response,400,{error:'invalid_streak_preferences'});return true;
        }
        const saved=await store.putStreakPreferences(owner,payload);
        respond(response,saved?200:409,saved
          ?{preferences:saved}:{error:'revision_conflict'});
        return true;
      }
      if(entity==='streak' || entity==='streak/cast') {
        if(typeof store.streakReady!=='function' || !store.streakReady()) {
          respond(response,503,{error:'streak_unavailable'});return true;
        }
        if(entity==='streak' && request.method==='GET') {
          await reconcileVerifiedStreak(owner);
          const items=await store.listStreakEvents(owner);
          respond(response,200,{streak:calculateStreaks(items)});return true;
        }
        if(entity!=='streak/cast' || request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const payload=await readBody(request,512);
        const hash=payload?.cast_hash;
        if(!payload || Object.keys(payload).length!==1 ||
           typeof hash!=='string' || !HASH.test(hash)) {
          respond(response,400,{error:'invalid_cast_hash'});return true;
        }
        // Server verifies the account owner and the network message before
        // storing. A repeat request for the same hash is idempotent.
        const evidence=await verifyStreakCast(hash.toLowerCase(),owner.fid,{fetchImpl,
          hypersnapOrigin:trustedHyperSnapOrigin,snapchainOrigin:trustedSnapchainOrigin});
        if(!evidence || !['cast','reply'].includes(evidence.kind)
           || !eligibleStreakDay(evidence.day)) {
          respond(response,422,{error:'cast_not_confirmed'});return true;
        }
        const saved=await store.recordStreakCast(owner,hash.toLowerCase(),evidence.kind,evidence.day);
        respond(response,200,{recorded:saved.recorded,
          streak:calculateStreaks(await store.listStreakEvents(owner))});
        return true;
      }
      if(entity==='profile_status') {
        if(request.method==='GET') {
          respond(response,200,{item:await store.getOwnProfileStatus(owner)});
          return true;
        }
        if(request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const payload=await readBody(request,2048);
        const choices=['none','available','busy','idle','away','focusing','creating','dnd','custom'];
        if(!payload || typeof payload!=='object' || Array.isArray(payload)
            || !choices.includes(payload.mood)
            || typeof payload.note!=='string' || [...payload.note].length>72
            || /[\u0000-\u001f\u007f]/u.test(payload.note)
            || typeof payload.visible!=='boolean'
            || !Number.isSafeInteger(payload.revision) || payload.revision<0
            || payload.revision>Number.MAX_SAFE_INTEGER-1
            || (payload.mood==='custom' && !payload.note.trim())) {
          respond(response,400,{error:'invalid_status'});return true;
        }
        const saved=await store.putProfileStatus(owner,{
          mood:payload.mood,note:payload.note.trim(),
          visible:payload.visible,revision:payload.revision
        });
        if(!saved){respond(response,409,{error:'revision_conflict'});return true;}
        respond(response,200,{item:saved});return true;
      }
      if(entity==='github_contributions') {
        if(request.method!=='GET') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const params=new URL(request.url,'http://localhost').searchParams;
        const login=params.get('username')||'';
        const yearText=params.get('year')||'';
        const currentYear=new Date().getUTCFullYear();
        if(!/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/.test(login)
            || !/^(20[1-2][0-9])$/.test(yearText)
            || Number(yearText)<2014 || Number(yearText)>currentYear
            || params.getAll('username').length!==1 || params.getAll('year').length!==1) {
          respond(response,400,{error:'invalid_calendar_request'});return true;
        }
        // Require a server-confirmed GitHub connection belonging to the
        // authenticated Farcaster identity. Never trust a supplied username
        // as the link proof; no bearer tokens are exposed to the browser.
        const connections=await store.list('connections',owner);
        const verified=connections.some(row=>{
          if(row.provider!=='github' || !row.last_confirmed_at)return false;
          const preferred=String(row.preferred_username||'').toLowerCase();
          const profile=String(row.provider_profile_url||'');
          return preferred===login.toLowerCase()
            || profile.toLowerCase()==='https://github.com/'+login.toLowerCase()
            || profile.toLowerCase()==='https://github.com/'+login.toLowerCase()+'/';
        });
        if(!verified) {
          respond(response,403,{error:'github_identity_unconfirmed'});return true;
        }
        try {
          respond(response,200,await readGithubCalendar(login,Number(yearText)));
        }catch(error) {
          // Preserve actionable states without exposing upstream HTML, URLs
          // or authorization/session data to the client.
          const reason=String(error?.message||'');
          respond(response,503,{error:reason==='incomplete_github_calendar'
            ? 'github_calendar_incomplete' : 'github_calendar_unavailable'});
        }
        return true;
      }
      if(entity==='topics' || entity==='topic_casts') {
        if(request.method!=='GET') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        if(entity==='topics') {
          respond(response,200,{items:await store.listTopics()});return true;
        }
        const search=new URL(request.url,'http://localhost').searchParams;
        const slug=search.get('topic') || '';
        const page=topicPagination(search);
        const authorFid=search.get('author_fid');
        if(!TOPIC_SLUG.test(slug) || !page
           || search.getAll('author_fid').length>1
           || (authorFid!==null && !/^[1-9][0-9]{0,18}$/.test(authorFid))) {
          respond(response,400,{error:'invalid_topic_filter'});return true;
        }
        respond(response,200,{items:await store.listTopicCasts(
          slug,page.limit,page.offset,authorFid),
          limit:page.limit,offset:page.offset});
        return true;
      }
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
      if(entity==='muted_profiles') {
        // App-owned local moderation never broadcasts a protocol block or
        // mutates a Farcaster Follow relationship. The verified account owner
        // alone may view or update its cross-device mute preferences.
        if(request.method==='GET') {
          respond(response,200,{preferences:await store.getMutedProfiles(owner)});
          return true;
        }
        if(request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const payload=await readBody(request,5000);
        const fids=payload?.muted_fids;
        if(!payload || Array.isArray(payload)
            || Object.keys(payload).sort().join(',')!=='muted_fids,revision'
            || !Number.isSafeInteger(payload.revision)
            || payload.revision<0 || payload.revision>9007199254740990
            || !Array.isArray(fids) || fids.length>256
            || fids.some(fid=>typeof fid!=='string'
              || !/^[1-9][0-9]{0,18}$/.test(fid)
              || !Number.isSafeInteger(Number(fid))
              || fid===String(owner.fid))
            || new Set(fids).size!==fids.length) {
          respond(response,400,{error:'invalid_muted_profiles'});return true;
        }
        const saved=await store.putMutedProfiles(owner,{
          muted_fids:[...fids].sort((a,b)=>Number(a)-Number(b)),
          revision:payload.revision
        });
        respond(response,saved?200:409,saved
          ?{preferences:saved}:{error:'revision_conflict'});
        return true;
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
      if(entity==='bookmark_collections') {
        if(typeof store.bookmarkCollectionsReady==='function'
           && !store.bookmarkCollectionsReady()) {
          respond(response,503,{error:'bookmark_collections_unavailable'});return true;
        }
        if(request.method==='GET') {
          respond(response,200,{items:await store.listBookmarkCollections(owner)});
          return true;
        }
        if(request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const change=validateBookmarkCollectionChange(await readBody(request,2048));
        if(!change) {respond(response,400,{error:'invalid_bookmark_collection'});return true;}
        // Account data remains removable even after the owner loses Pro.
        if(change.action!=='delete' && !await allowProMutation(owner,response)) return true;
        const saved=await store.putBookmarkCollection(owner,change);
        if(!saved) {respond(response,409,{error:'bookmark_collection_conflict'});return true;}
        respond(response,200,{item:saved});return true;
      }
      if(entity==='bookmark_collection_items') {
        if(typeof store.bookmarkCollectionsReady==='function'
           && !store.bookmarkCollectionsReady()) {
          respond(response,503,{error:'bookmark_collections_unavailable'});return true;
        }
        if(request.method==='GET') {
          const search=new URL(request.url,'http://localhost').searchParams;
          const id=search.get('collection_id');
          if(id===null && typeof store.listBookmarkMemberships==='function') {
            respond(response,200,{items:await store.listBookmarkMemberships(owner)});
            return true;
          }
          if(!ID.test(String(id||''))) {
            respond(response,400,{error:'invalid_bookmark_collection'});return true;
          }
          respond(response,200,{items:await store.listBookmarkCollectionItems(owner,id.toLowerCase())});
          return true;
        }
        if(request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const change=validateBookmarkCollectionItem(await readBody(request,2048));
        if(!change) {respond(response,400,{error:'invalid_bookmark_collection_item'});return true;}
        if(change.remove!==true && !await allowProMutation(owner,response)) return true;
        const saved=await store.putBookmarkCollectionItem(owner,change);
        if(!saved) {respond(response,409,{error:'bookmark_collection_item_conflict'});return true;}
        respond(response,200,{item:saved});return true;
      }
      if(entity==='cast_topics') {
        if(request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const assignment=validateCastTopicAssignment(await readBody(request,2048));
        if(!assignment || typeof store.assignCastTopics!=='function') {
          respond(response,400,{error:'invalid_cast_topics'});return true;
        }
        if(!await verifyTopicOwner(assignment.cast_hash,owner.fid,{fetchImpl,
          hypersnapOrigin:trustedHyperSnapOrigin,snapchainOrigin:trustedSnapchainOrigin})) {
          respond(response,422,{error:'cast_not_confirmed_for_owner'});return true;
        }
        const saved=await store.assignCastTopics(owner,
          assignment.cast_hash,assignment.topics);
        if(!saved) {respond(response,409,{error:'topic_conflict'});return true;}
        respond(response,200,{item:saved});return true;
      }
      if(entity==='connections/portable') {
        if(request.method!=='PUT') {
          respond(response,405,{error:'method_not_allowed'});return true;
        }
        const data=await readBody(request,8192);
        if(!data||!Array.isArray(data.items)||data.items.length>4
           ||typeof store.importConnectionLabels!=='function') {
          respond(response,400,{error:'invalid_connections'});return true;
        }
        const allowed=new Set(['x','github','linkedin','telegram']);
        const hosts={
          x:['x.com','twitter.com'],github:['github.com'],
          linkedin:['linkedin.com','www.linkedin.com'],
          telegram:['t.me']
        };
        const unique=new Set(),cleaned=[];
        for(const row of data.items) {
          if(!row || typeof row!=='object'||Array.isArray(row)
             ||!allowed.has(row.provider)||unique.has(row.provider)
             ||typeof row.display_name!=='string'||row.display_name.length>256
             ||typeof row.preferred_username!=='string'
             ||row.preferred_username.length>128
             ||typeof row.provider_profile_url!=='string'
             ||row.provider_profile_url.length>512
             ||(row.manual_profile_url!==undefined &&
                (typeof row.manual_profile_url!=='string' ||
                 (row.manual_profile_url && !validLinkedInUrl(row.manual_profile_url)) ||
                 (row.provider!=='linkedin' && row.manual_profile_url)))) {
            respond(response,400,{error:'invalid_connections'});return true;
          }
          if(row.provider_profile_url) {
            let url;
            try{url=new URL(row.provider_profile_url);}catch{}
            if(!url||url.protocol!=='https:'||!hosts[row.provider].includes(url.hostname)
               ||url.username||url.password||url.port||url.search||url.hash) {
              respond(response,400,{error:'invalid_connections'});return true;
            }
          }
          unique.add(row.provider);
          cleaned.push({...row});
        }
        await store.importConnectionLabels(owner,cleaned);
        respond(response,200,{saved:cleaned.length});return true;
      }
      if (entity==='connections/refresh') {
        if (request.method!=='POST') {respond(response,405,{error:'method_not_allowed'});return true;}
        const reply=await fetchImpl(owner.authorization.startsWith('Bearer recovery.')
          ? trustedBrowserGateway+'/v1/browser/openproof/connections'
          : upstream+'/account/connections',{
          headers:{authorization:owner.authorization,accept:'application/json'},
          redirect:'error',signal:AbortSignal.timeout(6000)});
        if (!reply.ok) {respond(response,502,{error:'connections_unavailable'});return true;}
        const payload=await reply.json();
        if (!Array.isArray(payload.connections) || payload.connections.length>100) {
          respond(response,502,{error:'invalid_connections'});return true;
        }
        const cleaned=payload.connections.filter(c=>c && PROVIDER.test(c.provider||''))
          .map(c=>{
            const confirmedGitHub=c.provider==='github'
              ? verifiedGitHubProfile(c.profile_url) : null;
            const preferred=String(c.preferred_username||'').trim();
            const githubLogin=/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/.test(preferred)
              ? preferred : confirmedGitHub?.login||'';
            return {
              provider:c.provider,
              display_name:String(c.display_name||'').slice(0,256),
              preferred_username:(c.provider==='github' ? githubLogin : preferred).slice(0,128),
              // Verified OpenProof provenance only. Portable/user-entered URLs
              // never satisfy the GitHub contribution ownership check.
              provider_profile_url:c.provider==='github' ? confirmedGitHub?.url||null
                : c.provider==='linkedin' && validLinkedInUrl(c.profile_url)
                  ? c.profile_url : null,
            };
          });
        await store.refreshConnections(owner,cleaned);
        respond(response,200,{refreshed:cleaned.length});return true;
      }
      if (request.method==='GET') {
        if (entity==='bookmarks' && typeof store.listBookmarkChanges==='function') {
          const url=new URL(request.url,'http://localhost');
          if (url.searchParams.get('state')==='1' &&
              typeof store.listBookmarkSnapshot==='function') {
            respond(response,200,{
              items:await store.listBookmarkSnapshot(owner),
              preview_supported:typeof store.bookmarkPreviewsReady==='function'
                ?store.bookmarkPreviewsReady():false,
            });
            return true;
          }
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
