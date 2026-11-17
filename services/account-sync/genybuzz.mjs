// GenyBuzz / v1: independent GenyCaster social attention extension.
// Its context is a presentation entry point, not a protocol transport.
// Future Direct Messages can call the same operation with context='dm'.
const FID=/^[1-9][0-9]{0,18}$/;
const UUID=/^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
const MAX_SAFE_FID=Number.MAX_SAFE_INTEGER;
const NATIVE_FID_START=4294967296;

export function validateBuzzTarget(input, issuer) {
  if (!input || typeof input!=='object' || Array.isArray(input)
      || Object.keys(input).sort().join(',')!=='context,target_fid,target_kind'
      || typeof input.target_fid!=='string' || !FID.test(input.target_fid)
      || !Number.isSafeInteger(Number(input.target_fid))
      || Number(input.target_fid)>MAX_SAFE_FID
      || !['farcaster','hypersnap'].includes(input.target_kind)
      // v1 is profile-only. A forged "dm" context must not bypass
      // future conversation membership authorization.
      || input.context!=='profile'
      || (input.target_kind==='hypersnap'
          && Number(input.target_fid)<NATIVE_FID_START))return null;
  // Identity is an application-account key. We never publish this as a
  // HyperSnap/Snapchain message or infer signing authority from the FID.
  return {
    issuer:input.target_kind==='hypersnap'?'hypersnap:native-registry':issuer,
    subject:input.target_kind+':'+input.target_fid,
    context:input.context
  };
}

export async function handleGenyBuzz({
  entity,request,response,owner,issuer,store,readBody,respond
}) {
  if (!['buzz','buzz/preferences','buzz/inbox','buzz/ack'].includes(entity))
    return false;
  if (typeof store.buzzReady!=='function' || !store.buzzReady()) {
    respond(response,503,{error:'buzz_unavailable'});return true;
  }
  // No simple cross-site form encoding can reach an account mutation.
  // A valid cookie-bound request already needs its separate CSRF token.
  if (request.method!=='GET' && request.method!=='HEAD' &&
      !String(request.headers['content-type']||'').startsWith('application/json')) {
    respond(response,415,{error:'unsupported_media_type'});return true;
  }
  if (entity==='buzz/preferences') {
    if (request.method==='GET') {
      respond(response,200,{preferences:await store.getBuzzPreferences(owner)});
      return true;
    }
    if (request.method!=='PUT') {
      respond(response,405,{error:'method_not_allowed'});return true;
    }
    const input=await readBody(request,256);
    if (!input || typeof input!=='object' || Array.isArray(input)
        || Object.keys(input).sort().join(',')!=='enabled,revision'
        || typeof input.enabled!=='boolean'
        || !Number.isSafeInteger(input.revision)
        || input.revision<0 || input.revision>=Number.MAX_SAFE_INTEGER) {
      respond(response,400,{error:'invalid_buzz_preferences'});return true;
    }
    const value=await store.putBuzzPreferences(owner,input);
    respond(response,value?200:409,value
      ?{preferences:value}:{error:'revision_conflict'});
    return true;
  }
  if (entity==='buzz/inbox') {
    if (request.method!=='GET') {
      respond(response,405,{error:'method_not_allowed'});return true;
    }
    const params=new URL(request.url,'http://localhost').searchParams;
    if ([...params.keys()].length) {
      respond(response,400,{error:'invalid_buzz_query'});return true;
    }
    respond(response,200,{items:await store.listBuzzInbox(owner)});
    return true;
  }
  if (entity==='buzz/ack') {
    if (request.method!=='POST') {
      respond(response,405,{error:'method_not_allowed'});return true;
    }
    const input=await readBody(request,256);
    if (!input || typeof input!=='object' || Array.isArray(input)
        || Object.keys(input).join(',')!=='id'
        || typeof input.id!=='string' || !UUID.test(input.id)) {
      respond(response,400,{error:'invalid_buzz_id'});return true;
    }
    // Acknowledgment only acts on the verified recipient's inbox.
    await store.ackBuzz(owner,input.id.toLowerCase());
    respond(response,200,{acknowledged:true});return true;
  }
  if (request.method!=='POST') {
    respond(response,405,{error:'method_not_allowed'});return true;
  }
  // Emergency server-side stop: only the operator can change this environment
  // setting. Existing preferences and inbox/ack remain usable while disabled.
  if (process.env.GENYCASTER_BUZZ_SEND_DISABLED==='1') {
    respond(response,503,{error:'buzz_unavailable'});return true;
  }
  const target=validateBuzzTarget(await readBody(request,512),issuer);
  if (!target) {
    respond(response,400,{error:'invalid_buzz_target'});return true;
  }
  if (owner.issuer===target.issuer && owner.subject===target.subject) {
    respond(response,400,{error:'cannot_buzz_self'});return true;
  }
  const result=await store.sendBuzz(owner,target);
  if (result.status==='rate_limited') {
    response.setHeader('Retry-After','60');
    respond(response,429,{error:'buzz_rate_limited'});return true;
  }
  if (result.status!=='sent') {
    // Deliberately do not disclose whether the account is registered,
    // opted in, muted or blocked.
    respond(response,403,{error:'buzz_not_available'});return true;
  }
  respond(response,201,{item:{id:result.id,context:target.context}});
  return true;
}
