import assert from 'node:assert/strict';
import test from 'node:test';
import {createAccountApi, createBaseSwapBackend} from '../server.mjs';
import {request as httpRequest} from 'node:http';
import {validLinkedInUrl,validateCloudItem} from '../cloud_state.mjs';

const principal='farcaster:42';
const recovered='Bearer recovery.'+'R'.repeat(43);
const upstream='https://identity.example.test/auth/providers';
const calls=[];
const store={
  async list(entity,owner){calls.push(['list',entity,owner.subject]);return [];},
  async put(entity,owner,item){calls.push(['put',entity,owner.subject,item]);return item;},
  async listUpdates(){return [{release_id:'1.0.0',title:'Launch'}];},
  async getProfileExtras(owner){calls.push(['get-profile',owner.subject]);return null;},
  async putProfileExtras(owner,item){calls.push(['put-profile',owner.subject,item]);return {...item,revision:1};},
  async refreshConnections(owner,items){calls.push(['refresh',owner.subject,items]);},
  async listBookmarkChanges(owner,since){
    calls.push(['bookmark-changes',owner.subject,since]);
    return [{sequence:'42',cast_hash:'0x'+'ab'.repeat(20),is_deleted:true}];
  },
  bookmarkPreviewsReady:()=>true,
  async listBookmarkSnapshot(owner){
    calls.push(['bookmark-snapshot',owner.subject]);
    return [{cast_hash:'0x'+'ab'.repeat(20),is_deleted:false,
      priority:1,preview:{authorFid:42,text:'Public Cast'}}];
  },
  async importConnectionLabels(owner,items){
    calls.push(['portable-links',owner.subject,items]);
  },
};
const fetchImpl=async(url,opts)=>{
  if(url==='https://storage-farcaster.genyleap.com/v1/browser/openproof/connections') {
    if(opts.headers.authorization!==recovered) return new Response('{}',{status:401});
    return new Response(JSON.stringify({connections:[
      {provider:'farcaster',fid:'42'},{provider:'linkedin',display_name:'Linked'}
    ]}),{status:200});
  }
  if(url.endsWith('/account/profile')) {
    if(opts.headers.authorization!=='Bearer good') return new Response('{}',{status:401});
    return new Response(JSON.stringify({identity_id:principal}),{status:200});
  }
  if(url.endsWith('/account/connections')) return new Response(JSON.stringify({
    connections:[{provider:'farcaster',fid:'42'},
      {provider:'linkedin',display_name:'Kambiz Asadzadeh',
      preferred_username:'',subject:'private-provider-subject',profile_url:'https://www.linkedin.com/in/kambizasadzadeh'}]
  }),{status:200});
  return new Response(JSON.stringify({providers:[],challenge_providers:[]}),{status:200});
};
async function withServer(action, options={}) {
  const server=createAccountApi({catalogUrl:upstream,fetchImpl,store,issuer:'https://identity.example.test',
    proResolver:async()=>({status:'verified',eligible:true}),...options});
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
    assert.ok(calls.every(c=>c[c[0]==='bookmark-changes'?1:2]===principal));
  });
});
test('validated Farcaster recovery and standard OpenProof sessions map to one private account',async()=>{
  calls.length=0;
  await withServer(async request=>{
    const standard=await request('profile_extras');
    assert.equal(standard.status,200);
    const recoveredRead=await request('profile_extras','GET',undefined,recovered);
    assert.equal(recoveredRead.status,200);
    assert.equal((await request('profile_extras','GET',undefined,
      'Bearer recovery.'+'Z'.repeat(43))).status,401);
    assert.ok(calls.filter(value=>value[0]==='get-profile')
      .every(value=>value[1]==='farcaster:42'));
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
    assert.equal(calls.at(-1)[2][1].subject,undefined);
    assert.equal(calls.at(-1)[2][1].provider_profile_url,manual.manual_profile_url);
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
test('Bookmarks carry stable idempotency keys and account-scoped change cursors',async()=>{
  const id='36e3e644-379e-40ee-b09d-9ed30feb1d2c';
  const item={cast_hash:'0x'+'ab'.repeat(20),is_deleted:false,mutation_id:id};
  assert.equal(validateCloudItem('bookmarks',item).mutation_id,id);
  assert.equal(validateCloudItem('bookmarks',{...item,mutation_id:'invalid'}),null);
  await withServer(async request=>{
    const fetched=await request('bookmarks?since=40');
    assert.equal(fetched.status,200);
    assert.deepEqual((await fetched.json()).next_cursor,'42');
    assert.ok(calls.some(value=>value[0]==='bookmark-changes'
      && value[1]===principal && value[2]==='40'));
    assert.equal((await request('bookmarks?since=bad')).status,400);
    assert.equal((await request('bookmarks','PUT',item)).status,200);
  });
});
test('Explicit unlink of manually added LinkedIn URL persists as a tombstone',()=>{
  assert.deepEqual(validateCloudItem('connections',{
    provider:'linkedin',manual_profile_url:null,publicly_visible:true
  }),{provider:'linkedin',manual_profile_url:null,publicly_visible:false});
});
test('User-owned professional category and labelled links have strict validation and server-scoped identity',async()=>{
  calls.length=0;
  const draft={category:'Technology',profession:'Developer',
    links:[{label:'Personal',url:'https://compez.art'},
           {label:'Company',url:'https://genyleap.com'}],
    revision:0,publicly_visible:true,subject:'attacker'};
  const valid=validateCloudItem('profile_extras',draft);
  assert.equal(valid.publicly_visible,false);
  assert.equal(valid.subject,undefined);
  assert.deepEqual(validateCloudItem('profile_extras',{
    ...draft,category:'Technology',secondary_categories:['Trading','Business','Finance']
  }).secondary_categories,['Trading','Business','Finance']);
  assert.deepEqual(validateCloudItem('profile_extras',draft).secondary_categories,[]);
  assert.deepEqual(validateCloudItem('profile_extras',draft).roles,['Developer']);
  assert.deepEqual(validateCloudItem('profile_extras',{
    ...draft,profession:'Founder',
    roles:['Founder','Trader','Analyst','Product Manager','Community Manager']
  }).roles,['Founder','Trader','Analyst','Product Manager','Community Manager']);
  for(const roles of [
    ['Founder','Founder'],['Founder','Nonexistent Role'],
    ['Founder','Trader','Analyst','Product Manager','Community Manager','Content Creator'],
    ['Trader'],[8]
  ]) assert.equal(validateCloudItem('profile_extras',{
    ...draft,profession:'Founder',roles
  }),null);
  for(const secondary_categories of [
    ['Trading','Trading'],['Technology'],['Trading','Business','Finance','Education'],
    ['Fake category'],[7]
  ]) assert.equal(validateCloudItem('profile_extras',{
    ...draft,category:'Technology',secondary_categories
  }),null);
  for (const bad of [
    {...draft,category:'Untrusted category'},
    {...draft,profession:'CEO of the world'},
    {...draft,links:[{label:'Bad',url:'http://insecure.test'}]},
    {...draft,links:[{label:'Bad',url:'https://localhost/private'}]},
    {...draft,links:Array(6).fill({label:'Test',url:'https://example.com'})},
    {...draft,revision:-1},
  ]) assert.equal(validateCloudItem('profile_extras',bad),null);
  await withServer(async request=>{
    assert.equal((await request('profile_extras')).status,200);
    assert.equal((await request('profile_extras','PUT',draft)).status,200);
    const copy=calls.at(-1);
    assert.equal(copy[0],'put-profile');
    assert.equal(copy[1],principal);
    assert.equal(copy[2].publicly_visible,false);
    assert.equal(copy[2].subject,undefined);
    assert.equal((await request('profile_extras','PUT',
      {...draft,links:[{label:'Unsafe',url:'https://127.0.0.1/admin'}]})).status,400);
    assert.equal((await request('profile_extras','GET',undefined,'Bearer invalid')).status,401);
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

test('bookmarks preserve verified owner-scoped priority, public Cast preview and canonical snapshot',async()=>{
  const id='36e3e644-379e-40ee-b09d-9ed30feb1d2c';
  const valid={cast_hash:'0x'+'a2'.repeat(20),is_deleted:false,priority:1,
    mutation_id:id,preview:{authorFid:42,displayName:'COMPEZ',
      text:'Your client. Your node. Your rules.',avatarUrl:'https://example.test/avatar.png'}};
  const parsed=validateCloudItem('bookmarks',valid);
  assert.equal(parsed.priority,1);
  assert.equal(parsed.preview.text,valid.preview.text);
  assert.equal(validateCloudItem('bookmarks',{...valid,priority:3}),null);
  assert.equal(validateCloudItem('bookmarks',{...valid,
    preview:{...valid.preview,avatarUrl:'javascript:alert(1)'}}),null);
  await withServer(async request=>{
    const snapshot=await request('bookmarks?state=1');
    assert.equal(snapshot.status,200);
    const data=await snapshot.json();
    assert.equal(data.preview_supported,true);
    assert.equal(data.items[0].priority,1);
    assert.ok(calls.some(row=>row[0]==='bookmark-snapshot'&&row[1]===principal));
    assert.equal((await request('bookmarks','PUT',valid)).status,200);
  });
});
test('portable connection labels cannot fabricate verified OpenProof authority',async()=>{
  calls.length=0;
  await withServer(async request=>{
    const item={items:[{provider:'github',display_name:'Dev',
      preferred_username:'compez',provider_profile_url:'https://github.com/compez'}]};
    assert.equal((await request('connections/portable','PUT',item)).status,200);
    assert.equal(calls.at(-1)[0],'portable-links');
    assert.equal(calls.at(-1)[1],principal);
    assert.equal(calls.at(-1)[2][0].provider,'github');
    assert.equal((await request('connections/portable','PUT',{
      items:[{...item.items[0],provider_profile_url:'https://evil.example/compez'}]
    })).status,400);
    assert.equal((await request('connections/portable','PUT',{
      items:[item.items[0],item.items[0]]
    })).status,400);
    assert.equal((await request('connections/portable','GET')).status,405);
    assert.equal((await request('connections/portable','PUT',item,'Bearer bad')).status,401);
  });
});

test('swap quote requires authenticated owner, POST and a configured provider',async()=>{
  const quote={chainId:8453,requiresApproval:true,transaction:{to:'0x4752ba5dbc23f44d87826276bf6fd6b1c372ad24'}};
  const calls=[];
  await withServer(async request=>{
    assert.equal((await request('swap/quote','GET')).status,405);
    assert.equal((await request('swap/quote','POST',{sellAmount:'1000000'},'Bearer bad')).status,401);
    assert.equal((await request('swap/quote','POST',{sellAmount:'1000000'})).status,200);
    assert.equal(calls.length,1);
    assert.equal(calls[0].owner.subject,principal);
    const response=await request('swap/quote','POST',{sellAmount:'1000000'});
    assert.equal((await response.json()).transaction.to,quote.transaction.to);
  },{swapQuoteProvider:async(input,owner)=>{calls.push({input,owner});return quote;}});
  await withServer(async request=>{
    const response=await request('swap/quote','POST',{sellAmount:'1000000'});
    assert.equal(response.status,503);
    assert.equal((await response.json()).error,'swap_quote_not_configured');
  });
});

test('native session cookie with CSRF may quote without Origin; browser cross-origin POST is rejected',async()=>{
  const cookie='c'.repeat(43);
  const csrf='d'.repeat(43);
  let storedIssuer='https://identity.example.test';
  let storedSubject=principal;
  const cloudStore={...store,async getSession(){return {
    subject:storedSubject,issuer:storedIssuer,csrf_token:csrf,
  }}};
  const server=createAccountApi({
    catalogUrl:upstream,fetchImpl,store:cloudStore,issuer:'https://identity.example.test',
    swapQuoteProvider:async()=>({chainId:8453,quote:'safe'}),
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const path='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/swap/quote';
  const send=(headers={})=>fetch(path,{method:'POST',
    headers:{cookie:'__Host-genycaster-account='+cookie,
      'x-genycaster-csrf':csrf,'content-type':'application/json',...headers},
    body:JSON.stringify({sellToken:'eip155:8453/native'}),
  });
  // Node fetch replaces Host with the transport hostname; use the raw
  // HTTP client to exercise the Nginx-forwarded public Host/Origin tuple.
  const sendPublicHost=(headers={})=>new Promise((resolve,reject)=>{
    const url=new URL(path);
    const req=httpRequest({hostname:url.hostname,port:url.port,path:url.pathname,
      method:'POST',headers:{host:'genycaster.xyz',
        cookie:'__Host-genycaster-account='+cookie,
        'x-genycaster-csrf':csrf,'content-type':'application/json',
        origin:'https://genycaster.xyz','sec-fetch-site':'same-origin',
        ...headers}},
      response=>{response.resume();response.on('end',()=>resolve(response.statusCode));});
    req.on('error',reject);
    req.end(JSON.stringify({sellToken:'eip155:8453/native'}));
  });
  try{
    const valid=await send();
    assert.equal(valid.status,200,'Qt native cookie+CSRF is sufficient without browser Origin');
    assert.equal((await valid.json()).chainId,8453);
    // A session with a canonical Farcaster subject but a forged/foreign
    // issuer must NEVER be treated as a valid owner, even with the correct
    // signed cookie, CSRF secret and a request from the trusted origin.
    for(const issuer of ['hypersnap:native-registry','https://attacker.invalid','']) {
      storedIssuer=issuer;
      const forged=await send();
      assert.equal(forged.status,401,'foreign issuer cannot access private quote');
      await forged.arrayBuffer();
    }
    storedIssuer='https://identity.example.test';
    storedSubject='hypersnap:42'; // beneath HyperSnap's native FID floor
    const wrongSubject=await send();
    assert.equal(wrongSubject.status,401);
    await wrongSubject.arrayBuffer();
    storedSubject=principal;
    // The canonical WASM hostname and account API share one host-only cookie.
    assert.equal(await sendPublicHost(),200);
    assert.equal(await sendPublicHost({
      host:'app.genycaster.xyz',origin:'https://app.genycaster.xyz'}),403);
    assert.equal(await sendPublicHost({
      origin:'https://app.genycaster.xyz.evil.invalid'}),403);
    assert.equal(await sendPublicHost({'sec-fetch-site':'cross-site'}),403);
    assert.equal((await send({'x-genycaster-csrf':'x'.repeat(43)})).status,401);
    assert.equal((await send({origin:'https://evil.invalid',
      'sec-fetch-site':'cross-site'})).status,403);
    assert.equal((await send({'sec-fetch-site':'cross-site'})).status,403);
  }finally{server.close();}
});

test('swap readiness is a public boolean backed by both authenticated cloud and a configured quote provider',async()=>{
  await withServer(async request=>{
    const status=await request('swap/status','GET',undefined,'Bearer bad');
    assert.equal(status.status,200);
    assert.deepEqual(await status.json(),{available:true,chainId:8453});
    assert.equal((await request('swap/status','POST',{},'Bearer bad')).status,405);
  },{swapQuoteProvider:async()=>({})});
  await withServer(async request=>{
    const status=await request('swap/status','GET',undefined,'Bearer bad');
    assert.equal(status.status,200);
    assert.deepEqual(await status.json(),{available:false,chainId:8453});
  });
});

test('swap capability fails closed when on-chain Base RPC or router is unreachable',async()=>{
  let checks=0;
  await withServer(async request=>{
    const first=await request('swap/status','GET',undefined,'Bearer bad');
    assert.equal(first.status,200);
    assert.deepEqual(await first.json(),{available:false,chainId:8453});
    const second=await request('swap/status','GET',undefined,'Bearer bad');
    assert.deepEqual(await second.json(),{available:false,chainId:8453});
    assert.equal(checks,1,'health check result is cached to protect public RPC');
  },{swapQuoteProvider:async()=>({}),
     swapQuoteStatus:async()=>{checks++;throw new Error('wrong_rpc_or_router_missing');}});
});

test('production swap composition uses direct Base JSON-RPC and verifies deployed Router',async()=>{
  const seen=[];
  const transport=async(_url,request)=>{
    const body=JSON.parse(request.body);
    seen.push({method:body.method,params:body.params});
    const result=body.method==='eth_chainId'?'0x2105'
      :body.method==='eth_getCode'?'0x'+'12'.repeat(64):'0x0';
    return new Response(JSON.stringify({jsonrpc:'2.0',id:body.id,result}),{
      headers:{'Content-Type':'application/json'}});
  };
  const provider=createBaseSwapBackend('https://example.test',transport);
  assert.equal(await provider.swapQuoteStatus(),true);
  assert.deepEqual(seen.map(r=>r.method),['eth_chainId','eth_getCode']);
  assert.equal(seen[1].params[0],'0x4752ba5dbc23f44d87826276bf6fd6b1c372ad24');
  const unavailable=createBaseSwapBackend('https://example.test',async(_url,request)=>{
    const body=JSON.parse(request.body);
    return new Response(JSON.stringify({
      jsonrpc:'2.0',id:body.id,
      result:body.method==='eth_chainId'?'0x1':'0x'
    }));
  });
  assert.equal(await unavailable.swapQuoteStatus(),false);
});
