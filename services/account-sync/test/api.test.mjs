import assert from 'node:assert/strict';
import test from 'node:test';
import {createAccountApi, normalizeCatalog} from '../server.mjs';

const upstream = 'https://identity.example.test/auth/providers';
const response = data => new Response(JSON.stringify(data), {
  status:200,headers:{'content-type':'application/json'}
});
const fetchApi = async(server,path,options={}) => {
  const address=server.address();
  return fetch('http://127.0.0.1:'+address.port+path,options);
};

test('catalog strips login-only mechanisms and invalid IDs without copying linked identities',()=>{
  assert.deepEqual(normalizeCatalog({
    providers:['x','github','x','bad id','GOOGLE','google'],
    challenge_providers:['farcaster','passkey','ethereum-wallet','local']
  }),{providers:['ethereum-wallet','farcaster','github','google','x']});
  assert.throws(()=>normalizeCatalog({connections:[]}),/invalid-provider-catalog/);
});

test('API returns public catalog, caches and deduplicates concurrent refreshes',async()=>{
  let calls=0;let clock=0;
  const server=createAccountApi({catalogUrl:upstream,now:()=>clock,fetchImpl:async()=>{
    calls++;await new Promise(resolve=>setTimeout(resolve,20));
    return response({providers:['x','github'],challenge_providers:['local','farcaster']});
  }});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try {
    const [a,b]=await Promise.all([fetchApi(server,'/api/v1/connections/providers'),
      fetchApi(server,'/api/v1/connections/providers')]);
    assert.equal(a.status,200);assert.equal(b.status,200);
    assert.deepEqual(await a.json(),{providers:['farcaster','github','x']});
    assert.equal(calls,1);
    await fetchApi(server,'/api/v1/connections/providers');
    assert.equal(calls,1);
    clock+=301_000;
    await fetchApi(server,'/api/v1/connections/providers');
    assert.equal(calls,2);
    assert.equal((await fetchApi(server,'/healthz')).status,200);
    assert.equal((await fetchApi(server,'/api/v1/connections/providers',{method:'POST'})).status,405);
    assert.equal((await fetchApi(server,'/api/v1/bookmarks')).status,404);
  } finally {server.close();}
});

test('API does not reveal fetch failures or accept insecure provider origins',async()=>{
  assert.throws(()=>createAccountApi({catalogUrl:'http://example.test'}),/HTTPS/);
  assert.throws(()=>createAccountApi({catalogUrl:'https://user:pass@example.test'}),/HTTPS/);
  const server=createAccountApi({catalogUrl:upstream,fetchImpl:async()=>{throw new Error('SECRET');}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try {
    const r=await fetchApi(server,'/api/v1/connections/providers');
    assert.equal(r.status,503);
    assert.equal((await r.text()).includes('SECRET'),false);
  }finally{server.close();}
});

test('public health bridge exposes checked service state without authentication',async()=>{
  let status=200;
  const healthFetchImpl=async(url, options)=>{
    assert.equal(url,'http://127.0.0.1:8798/api/health');
    assert.ok(options.method==='GET'||options.method==='HEAD');
    return new Response(options.method==='HEAD'?null:JSON.stringify({
      service:'genycaster',
      status:status===200?'operational':'unavailable',
      checks:{web:'operational',backend:'operational',snapchain:'operational',hypersnap:'operational'},
    }),{status,headers:{'content-type':'application/json'}});
  };
  const server=createAccountApi({catalogUrl:upstream,healthFetchImpl});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try{
    const ready=await fetchApi(server,'/api/v1/cloud/health');
    assert.equal(ready.status,200);
    assert.equal((await ready.json()).status,'operational');
    assert.equal((await fetchApi(server,'/api/v1/cloud/health',{method:'HEAD'})).status,200);
    assert.equal((await fetchApi(server,'/api/v1/cloud/health',{method:'POST'})).status,405);
    status=503;
    const unavailable=await fetchApi(server,'/api/v1/cloud/health');
    assert.equal(unavailable.status,503);
    assert.equal((await unavailable.json()).status,'unavailable');
    assert.equal(unavailable.headers.get('retry-after'),'30');
  }finally{server.close();}
});

test('public health bridge fails closed when its worker is unreachable',async()=>{
  const server=createAccountApi({catalogUrl:upstream,healthFetchImpl:async()=>{throw Error('private_detail');}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try{
    const response=await fetchApi(server,'/api/v1/cloud/health');
    assert.equal(response.status,503);
    const body=await response.text();
    assert.equal(body.includes('private_detail'),false);
    assert.equal(JSON.parse(body).status,'unavailable');
  }finally{server.close();}
});
