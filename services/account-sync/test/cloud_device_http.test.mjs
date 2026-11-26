import test from 'node:test';
import assert from 'node:assert/strict';
import {generateKeyPairSync,sign,createHash} from 'node:crypto';
import {createAccountApi} from '../server.mjs';
import {cloudDeviceMessage,REVOCATION_NONCE} from '../cloud_device_proof.mjs';

const issuer='https://identity.test', origin='https://genycaster.xyz', fid='42';
const key=()=>generateKeyPairSync('ed25519');
const pub=k=>k.publicKey.export({format:'der',type:'spki'}).subarray(-32).toString('hex');
const signed=(challenge,k)=>({...challenge,signature:sign(null,Buffer.from(cloudDeviceMessage(challenge)),k.privateKey).toString('hex')});
const hash=value=>createHash('sha256').update(value).digest('hex');
function fixture(){
  const sessions=new Map(),devices=new Map();
  const scope=o=>o.issuer+'|'+o.subject;
  const id=(o,k)=>scope(o)+'|'+k;
  const store={
    cloudDevicesReady:()=>true,
    async hasCloudDeviceHistory(o){return [...devices.values()].some(d=>d.scope===scope(o));},
    async enrollCloudDevice(o,k){
      const name=id(o,k),prior=devices.get(name);
      if(prior?.revoked)throw Object.assign(Error('device_revoked'),{status:401});
      devices.set(name,{scope:scope(o),revoked:false});
    },
    async touchCloudDevice(o,k){const d=devices.get(id(o,k));return !!d&&!d.revoked;},
    async revokeCloudDevice(o,k){
      const d=devices.get(id(o,k));if(!d)return false;
      d.revoked=true;
      for(const [token,s] of sessions)if(scope(s)===scope(o)&&s.device_key===k)sessions.delete(token);
      return true;
    },
    async createSession(o,token,csrf){
      if(o.deviceKey){
        assert(await this.touchCloudDevice(o,o.deviceKey));
        for(const [old,s] of sessions)if(scope(s)===scope(o)&&s.device_key===o.deviceKey)sessions.delete(old);
      }
      sessions.set(token,{issuer:o.issuer,subject:o.subject,csrf_token:csrf,auth_source:o.authSource||'legacy',device_key:o.deviceKey});
    },
    async getSession(token){return sessions.get(token)||null;},
    async revokeSession(token,o){if(o?.deviceKey)await this.revokeCloudDevice(o,o.deviceKey);sessions.delete(token);},
    async list(){return [];},
  };
  return {store,sessions,devices};
}
async function setup(strict=false){
  const state=fixture(),social=key();
  const fetchImpl=async(url,options)=>{
    const target=new URL(url);
    if(target.origin===issuer){
      const identity=options.headers.authorization==='Bearer verified'?'42':
        options.headers.authorization==='Bearer other'?'43':null;
      if(!identity)return new Response('{}',{status:401});
      if(target.pathname==='/account/profile')return Response.json({identity_id:'id-'+identity});
      if(target.pathname==='/account/connections')return Response.json({connections:[{provider:'farcaster',fid:identity}]});
    }
    if(target.pathname==='/v1/signersByFid')return Response.json({signers:[{
      fid:target.searchParams.get('fid'),key:pub(social),keyType:1,source:'SIGNER_SOURCE_ONCHAIN',
      scopes:['MESSAGE_TYPE_CAST_ADD'],
    }]});
    throw Error('Unexpected upstream');
  };
  const server=createAccountApi({catalogUrl:issuer+'/catalog',issuer,store:state.store,fetchImpl,
    publicOrigin:origin,allowLegacySignerSessions:!strict});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
  const request=(path,method='GET',body,headers={})=>fetch(base+path,{
    method,headers:{origin,'content-type':'application/json',...headers},
    ...(body===undefined?{}:{body:JSON.stringify(body)}),
  });
  async function prove(k,purpose='restore',headers={},identity=fid){
    const challenge=await request('session/device/challenge','POST',{
      purpose,network:'farcaster',fid:identity,public_key:pub(k)});
    assert.equal(challenge.status,200);
    return request('session/device/proof','POST',signed(await challenge.json(),k),headers);
  }
  async function legacy(){
    const response=await request('session/challenge','POST',{fid});
    if(response.status!==200)return response;
    const {nonce}=await response.json();
    const signature=sign(null,Buffer.from('genycaster-account-session-v1\n'+fid+'\n'+nonce),social.privateKey).toString('hex');
    return request('session/proof','POST',{fid,nonce,public_key:pub(social),signature});
  }
  return {...state,request,prove,legacy,close:async()=>{
    server.closeAllConnections();await new Promise(resolve=>server.close(resolve));
  }};
}
const cookie=r=>r.headers.get('set-cookie').split(';')[0];
const bearer={authorization:'Bearer verified'};

test('HTTP migration rejects legacy cookies and proofs after independent enrollment, preserves OpenProof',async()=>{
  const ctx=await setup(),device=key();
  try{
    const old=await ctx.legacy();assert.equal(old.status,200);const legacyCookie=cookie(old);
    assert.equal((await ctx.request('session','GET',undefined,{cookie:legacyCookie})).status,200);
    assert.equal((await ctx.prove(device,'enroll',{cookie:legacyCookie})).status,401);
    assert.equal((await ctx.prove(device,'enroll',{authorization:'Bearer other'})).status,401);
    assert.equal(ctx.devices.size,0);
    const enrolled=await ctx.prove(device,'enroll',bearer);assert.equal(enrolled.status,200);
    const session=await enrolled.json(),firstCookie=cookie(enrolled);
    assert.equal(session.auth_source,'device');
    assert.match(enrolled.headers.get('set-cookie'),/Secure; HttpOnly; SameSite=Strict/);
    assert.equal((await ctx.request('session','GET',undefined,{cookie:legacyCookie})).status,401);
    assert.equal((await ctx.legacy()).status,410);
    const restored=await ctx.prove(device);assert.equal(restored.status,200);
    const nextCookie=cookie(restored);
    assert.notEqual(nextCookie,firstCookie);
    assert.equal((await ctx.request('session','GET',undefined,{cookie:firstCookie})).status,401);
    assert.equal((await ctx.request('session','GET',undefined,{cookie:nextCookie})).status,200);
    assert.equal((await ctx.request('session','DELETE',undefined,{cookie:nextCookie})).status,401);
    const receipt=signed({purpose:'revoke',network:'farcaster',fid,public_key:pub(device),nonce:REVOCATION_NONCE},device);
    const stranger=key();
    const unknown=signed({...receipt,fid:'999',public_key:pub(stranger)},stranger);
    assert.equal((await ctx.request('session/device/revoke','POST',unknown)).status,409);
    assert.equal(ctx.devices.size,1);
    assert.equal((await ctx.request('session/device/revoke','POST',{...receipt,fid:'43'})).status,401);
    for(let i=0;i<2;i++){
      const revoked=await ctx.request('session/device/revoke','POST',receipt);
      assert.equal(revoked.status,200);assert.deepEqual(await revoked.json(),{revoked:true});
      assert.equal(revoked.headers.get('set-cookie'),null);
    }
    assert.equal((await ctx.request('session','GET',undefined,{cookie:nextCookie})).status,401);
    assert.equal((await ctx.prove(device)).status,401);
    assert.equal((await ctx.prove(device,'enroll',bearer)).status,401);
    assert.equal((await ctx.legacy()).status,410);
    assert.equal((await ctx.prove(key(),'enroll',bearer)).status,200);
    assert.equal((await ctx.request('session','POST',undefined,bearer)).status,200);
  }finally{await ctx.close();}
});

test('strict API denies legacy proof globally but allows fresh independently verified OpenProof',async()=>{
  const ctx=await setup(true);
  try{
    assert.equal((await ctx.legacy()).status,410);
    assert.equal((await ctx.request('session','POST',undefined,bearer)).status,200);
    assert.equal((await ctx.prove(key(),'enroll',bearer)).status,200);
    assert.equal((await ctx.request('session/device/challenge','POST',{
      purpose:'restore',network:'farcaster',fid,public_key:pub(key())},{origin:'https://untrusted.example'})).status,403);
  }finally{await ctx.close();}
});
