import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {createAccountApi} from '../server.mjs';

const issuer='https://identity.test';
const active='Bearer verified-owner';
const other='Bearer verified-other';
const key=hash=>createHash('sha256').update(hash).digest('hex');
function fakeStore() {
  const sessions=new Map(),profiles=new Map(),drafts=new Map();
  const ownerKey=owner=>owner.issuer+'|'+owner.subject;
  return {
    sessions,
    async listUpdates(){return [];},
    async createSession(owner,hash,csrf) {
      sessions.set(hash,{...owner,csrf_token:csrf});
    },
    async getSession(hash){return sessions.get(hash)||null;},
    async revokeSession(hash){sessions.delete(hash);},
    async getProfileExtras(owner){return profiles.get(ownerKey(owner))||null;},
    async putProfileExtras(owner,item) {
      const k=ownerKey(owner),prior=profiles.get(k);
      if((prior?.revision||0)!==item.revision)return null;
      const next={...item,revision:item.revision+1};
      profiles.set(k,next);return next;
    },
    async getDraft(owner,id){return drafts.get(ownerKey(owner)+'|'+id)||null;},
    async list(){return [];},
    async put(entity,owner,item) {
      if(entity!=='drafts')throw Error('unexpected write');
      const k=ownerKey(owner)+'|'+item.id, prior=drafts.get(k);
      if((prior?.revision||0)!==item.revision)return null;
      const next={...item,revision:item.revision+1,deleted_at:null};
      drafts.set(k,next);return next;
    }
  };
}
function verifier(url,options) {
  const auth=options.headers.authorization;
  const fid=auth===active?'123':auth===other?'999':null;
  if(url.endsWith('/account/profile'))return Promise.resolve(
    new Response(fid?JSON.stringify({identity_id:'user-'+fid}):'{}',
      {status:fid?200:401}));
  if(url.endsWith('/account/connections'))return Promise.resolve(
    new Response(JSON.stringify({connections:fid?[{provider:'farcaster',fid}]:[]}),
      {status:fid?200:401}));
  throw Error('untrusted verification URL');
}
test('session continuity needs verified enrollment, HttpOnly cookie and CSRF',async()=>{
  const store=fakeStore();
  const server=createAccountApi({
    catalogUrl:issuer+'/catalog',issuer,store,fetchImpl:verifier});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
  const request=(url,method='GET',headers={},data)=>fetch(base+url,{
    method,headers:{accept:'application/json',...headers},
    ...(data?{body:JSON.stringify(data)}:{})
  });
  try {
    assert.equal((await request('session')).status,401);
    assert.equal((await request('session','POST')).status,401);
    const registered=await request('session','POST',{authorization:active});
    assert.equal(registered.status,200);
    const cookie=registered.headers.get('set-cookie');
    assert.match(cookie,/__Host-genycaster-account=[A-Za-z0-9_-]{43}/);
    assert.match(cookie,/Secure; HttpOnly; SameSite=Strict/);
    const session=await registered.json();
    assert.equal(session.fid,'123');
    assert.match(session.csrf_token,/^[A-Za-z0-9_-]{43}$/);
    const name=cookie.split(';')[0];
    const secret=name.split('=')[1];
    assert.equal(store.sessions.has(key(secret)),true);
    const own={cookie:name};
    assert.equal((await request('session','GET',own)).status,200);
    const profile={category:'Technology',profession:'Engineer',links:[],revision:0};
    assert.equal((await request('profile_extras','PUT',own,profile)).status,401);
    const write={...own,'x-genycaster-csrf':session.csrf_token};
    const saved=await request('profile_extras','PUT',write,profile);
    assert.equal(saved.status,200);
    assert.equal((await saved.json()).item.revision,1);
    const same=await request('profile_extras','GET',own);
    assert.equal((await same.json()).item.profession,'Engineer');
    assert.equal((await request('session','POST',own)).status,401);
    const draft={id:'00000000-0000-4000-8000-000000000002',
      kind:'cast',title:'',content:{document:{blocks:[]}},revision:0};
    assert.equal((await request('drafts','PUT',write,draft)).status,200);
    assert.equal((await request('drafts?id='+draft.id,'GET',own)).status,200);
    const stranger=await request('profile_extras','GET',{authorization:other});
    assert.deepEqual(await stranger.json(),{item:null});
    const logout=await request('session','DELETE',write);
    assert.equal(logout.status,200);
    assert.match(logout.headers.get('set-cookie'),/Max-Age=0/);
    assert.equal((await request('profile_extras','GET',own)).status,401);
  } finally {await new Promise(resolve=>server.close(resolve));}
});
