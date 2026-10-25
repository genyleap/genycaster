import assert from 'node:assert/strict';
import test from 'node:test';
import {createAccountApi} from '../server.mjs';
import {validLinkedInUrl,validateCloudItem} from '../cloud_state.mjs';

const principal='owner-verified-by-openproof';
const upstream='https://identity.example.test/auth/providers';
const calls=[];
const store={
  async list(entity,owner){calls.push(['list',entity,owner.subject]);return [];},
  async put(entity,owner,item){calls.push(['put',entity,owner.subject,item]);return item;},
  async listUpdates(){return [{release_id:'1.0.0',title:'Launch'}];},
  async refreshConnections(owner,items){calls.push(['refresh',owner.subject,items]);},
};
const fetchImpl=async(url,opts)=>{
  if(url.endsWith('/account/profile')) {
    if(opts.headers.authorization!=='Bearer good') return new Response('{}',{status:401});
    return new Response(JSON.stringify({identity_id:principal}),{status:200});
  }
  if(url.endsWith('/account/connections')) return new Response(JSON.stringify({
    connections:[{provider:'linkedin',display_name:'Kambiz Asadzadeh',
      preferred_username:'',subject:'private-provider-subject',profile_url:'https://www.linkedin.com/in/kambizasadzadeh'}]
  }),{status:200});
  return new Response(JSON.stringify({providers:[],challenge_providers:[]}),{status:200});
};
async function withServer(action) {
  const server=createAccountApi({catalogUrl:upstream,fetchImpl,store,issuer:'https://identity.example.test'});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const url='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
  const request=(path,method='GET',body,token='Bearer good')=>
    fetch(url+path,{method,headers:{authorization:token,'content-type':'application/json'},
      ...(body===undefined?{}:{body:JSON.stringify(body)})});
  try{await action(request);}finally{server.close();}
}
test('LinkedIn manual vanity URLs are strict, self-asserted and not inferred from OIDC sub',()=>{
  assert.ok(validLinkedInUrl('https://www.linkedin.com/in/kambizasadzadeh'));
  for(const url of ['https://www.linkedin.com/in/a','https://linkedin.com/in/username',
    'https://www.linkedin.com/search/results/people/?keywords=person',
    'https://www.linkedin.com/in/a%2fb','https://www.linkedin.com.evil.test/in/name',
    'http://www.linkedin.com/in/kambiz']) assert.equal(validLinkedInUrl(url),false,url);
  assert.equal(validateCloudItem('connections',{provider:'linkedin',manual_profile_url:'test'}),null);
  assert.equal(validateCloudItem('connections',{provider:'github',manual_profile_url:'https://www.linkedin.com/in/kambizasadzadeh'}),null);
});
test('Cloud endpoints reject anonymous and client-asserted identities; only OpenProof account profile binds owner',async()=>{
  calls.length=0;
  await withServer(async request=>{
    assert.equal((await request('drafts','GET',undefined,'Bearer invalid')).status,401);
    assert.equal((await request('drafts','GET',undefined,'Bearer recovery.fake')).status,401);
    assert.equal((await request('drafts','GET')).status,200);
    assert.equal((await request('bookmarks','GET')).status,200);
    assert.ok(calls.every(c=>c[2]===principal));
  });
});
test('Manual LinkedIn URL is distinct from provider-verified URL and never mirrors upstream subject',async()=>{
  calls.length=0;
  await withServer(async request=>{
    const manual={provider:'linkedin',manual_profile_url:'https://www.linkedin.com/in/kambizasadzadeh',
      publicly_visible:true, subject:'attacker-controlled'};
    const put=await request('connections','PUT',manual);
    assert.equal(put.status,200);
    assert.equal(calls.at(-1)[3].subject,undefined);
    const refresh=await request('connections/refresh','POST',{});
    assert.equal(refresh.status,200);
    assert.equal(calls.at(-1)[0],'refresh');
    assert.equal(calls.at(-1)[2][0].subject,undefined);
    assert.equal(calls.at(-1)[2][0].provider_profile_url,manual.manual_profile_url);
  });
});
test('Bookmarks, drafts, task schedules and notifications are scoped to a user and validated before writes',async()=>{
  await withServer(async request=>{
    assert.equal((await request('bookmarks','PUT',{cast_hash:'not-a-cast'})).status,400);
    assert.equal((await request('bookmarks','PUT',{cast_hash:'0x'+'ab'.repeat(20),is_deleted:false})).status,200);
    const id='09fac165-b314-42c9-9ec9-10141b2dc805';
    assert.equal((await request('drafts','PUT',{id,kind:'article',title:'My article',content:{body:'Draft'},revision:0})).status,200);
    assert.equal((await request('tasks','PUT',{id,kind:'scheduled_cast',state:'scheduled',
      timezone:'UTC',schedule_at:'2027-01-05T13:00:00Z',
      idempotency_key:'21647bd8-c541-46aa-ac57-be5632bc157a',payload:{text:'hello'}})).status,200);
    assert.equal((await request('notifications','PUT',{id,kind:'mention',source:'farcaster',
      occurred_at:'2026-10-24T11:00:00Z',payload:{cast:'hello'}})).status,200);
  });
});
test('App updates are public read-only, and cloud storage is disabled when unconfigured',async()=>{
  await withServer(async request=>{
    assert.equal((await request('app_updates')).status,200);
    assert.equal((await request('app_updates','PUT',{})).status,405);
    assert.equal((await request('unknown','GET')).status,404);
  });
  const server=createAccountApi({catalogUrl:upstream,fetchImpl});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try {
    const r=await fetch('http://127.0.0.1:'+server.address().port+'/api/v1/cloud/drafts');
    assert.equal(r.status,503);
    assert.deepEqual(await r.json(),{error:'cloud_not_configured'});
  }finally{server.close();}
});
