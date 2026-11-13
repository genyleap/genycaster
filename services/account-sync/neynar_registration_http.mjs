// Optional, same-origin, paid-voucher-gated Farcaster registration API.
// No API key, wallet identifier or private custody key is ever returned.
const PATHS=Object.freeze({
  '/api/v1/registration/farcaster/reserve':'reserve',
  '/api/v1/registration/farcaster/submit':'submit',
  '/api/v1/registration/farcaster/status':'status'
});
const PARAMETERS=Object.freeze({
  reserve:['custody_address','idempotency_key'],
  submit:['custody_address','reservation_id','deadline','signature','fname'],
  status:['custody_address','reservation_id']
});
async function readJson(request,max=3072) {
  if(!/^application\/json(?:;\s*charset=utf-8)?$/i.test(
    String(request.headers['content-type']||'')))
    throw new Error('invalid_json_content_type');
  if(Number(request.headers['content-length']||0)>max)
    throw new Error('request_too_large');
  let size=0;const chunks=[];
  for await(const chunk of request) {
    size+=chunk.length;
    if(size>max)throw new Error('request_too_large');
    chunks.push(chunk);
  }
  try{
    const object=JSON.parse(Buffer.concat(chunks).toString('utf8'));
    if(!object||Array.isArray(object)||typeof object!=='object')
      throw new Error('bad');
    return object;
  }catch{throw new Error('invalid_json');}
}
const reply=(res,code,data)=>{
  res.writeHead(code,{'Cache-Control':'no-store',
    'X-Content-Type-Options':'nosniff','Content-Type':'application/json'});
  res.end(JSON.stringify(data));
};
export function createNeynarRegistrationHttpHandler({relay,publicOrigin}) {
  if(!relay||typeof relay.reserve!=='function'
     ||typeof relay.submit!=='function'||typeof relay.status!=='function')
    throw new Error('complete Neynar relay required');
  const origin=new URL(publicOrigin);
  if(origin.protocol!=='https:'||origin.username||origin.password
     ||origin.pathname!=='/'||origin.search||origin.hash)
    throw new Error('canonical HTTPS origin required');
  return async(request,response,path)=>{
    const action=PATHS[path];
    if(!action)return false;
    if(request.method!=='POST') {
      reply(response,405,{error:'method_not_allowed'});return true;
    }
    // Reject credential-bearing cross-site browser requests. A native Qt
    // application does not send Origin; it still needs a paid bearer voucher.
    const requestOrigin=request.headers.origin;
    if((requestOrigin&&requestOrigin!==origin.origin)
       ||(request.headers['sec-fetch-site']
          &&request.headers['sec-fetch-site']!=='same-origin')) {
      reply(response,403,{error:'origin_not_allowed'});return true;
    }
    const enrollment=request.headers['x-genycaster-paid-enrollment'];
    if(typeof enrollment!=='string'||enrollment.length>1100
       ||enrollment.length<60) {
      reply(response,401,{error:'paid_enrollment_required'});return true;
    }
    try {
      const body=await readJson(request);
      const fields=Object.keys(body).sort();
      if(fields.join(',')!==PARAMETERS[action].slice().sort().join(',')
          ||fields.some(key=>typeof body[key]!=='string')) {
        reply(response,400,{error:'invalid_registration_request'});return true;
      }
      const args={
        enrollment,custodyAddress:body.custody_address,
        ...(action==='reserve'?{idempotencyKey:body.idempotency_key}
          :{reservationId:body.reservation_id}),
        ...(action==='submit'?{
          deadline:body.deadline,signature:body.signature,fname:body.fname}:{})
      };
      const result=await relay[action](args);
      reply(response,200,{registration:result});
    }catch(error) {
      const reason=String(error?.message||'');
      const forbidden=reason==='enrollment_not_authorized'
        ||reason==='reservation_not_found';
      const invalid=/^(invalid_|reservation_expired|custody_already_registered)/.test(reason);
      const conflicting=/^(reservation_conflict|lease_reservation_conflict)/.test(reason);
      reply(response,forbidden?403:invalid?422:conflicting?409:503,{
        error:forbidden?'registration_not_authorized':
          invalid?'invalid_registration_proof':
          conflicting?'registration_conflict':'registration_unavailable'
      });
    }
    return true;
  };
}
