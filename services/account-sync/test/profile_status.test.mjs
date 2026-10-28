import test from 'node:test';
import assert from 'node:assert/strict';
import {createAccountApi} from '../server.mjs';
import {validateCloudItem} from '../cloud_state.mjs';

const issuer='https://identity.example.test';
const owner='Bearer good';
const fetchImpl=async url=>{
  if(url.endsWith('/account/profile'))
    return new Response(JSON.stringify({identity_id:'farcaster:42'}));
  if(url.endsWith('/account/connections'))
    return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
  return new Response('{}',{status:404});
};
test('public display exposes only explicitly visible mood, authenticated PUT enforces revision',async()=>{
 let current=null;
 const calls=[];
 const store={
  async getPublicProfileStatus(fid){calls.push(['public',fid]);return current?.visible&&current.mood!=='none'?current:null},
  async getOwnProfileStatus(who){assert.equal(who.fid,'42');return current},
  async putProfileStatus(who,item){
    assert.equal(who.fid,'42');
    if((current?Number(current.revision):0)!==item.revision)return null;
    current={fid:'42',...item,revision:String(item.revision+1)};
    return current;
  }
 };
 const api=createAccountApi({catalogUrl:issuer+'/catalog',issuer,store,fetchImpl});
 await new Promise(resolve=>api.listen(0,'127.0.0.1',resolve));
 const origin='http://127.0.0.1:'+api.address().port+'/api/v1/cloud/profile_status';
 const request=(path='',method='GET',payload,auth=owner)=>fetch(origin+path,{
   method,headers:{...(auth?{authorization:auth}:{}),'content-type':'application/json'},
   ...(payload?{body:JSON.stringify(payload)}:{})
 });
 try{
  assert.equal((await request('?fid=42','GET',undefined,null)).status,200);
  assert.equal((await request('?fid=other','GET',undefined,null)).status,400);
  assert.equal((await request('?fid=42','PUT',{mood:'busy'},null)).status,401);
  const bad={mood:'busy',note:'',visible:true,revision:0};
  assert.equal((await request('','PUT',{...bad,mood:'sleeping'})).status,400);
  assert.equal((await request('','PUT',{...bad,note:'a'.repeat(73)})).status,400);
  assert.equal((await request('','PUT',{...bad,mood:'custom'})).status,400);
  let saved=await request('','PUT',{...bad,note:'Building GenyCaster'});
  assert.equal(saved.status,200);
  assert.equal((await saved.json()).item.revision,'1');
  assert.equal((await request('','PUT',bad)).status,409);
  assert.equal((await (await request('?fid=42','GET',undefined,null)).json()).item.mood,'busy');
  assert.equal((await request('','PUT',{mood:'dnd',note:'',visible:false,revision:1})).status,200);
  assert.equal((await (await request('?fid=42','GET',undefined,null)).json()).item,null);
  assert.equal((await (await request()).json()).item.mood,'dnd');
  assert.equal((await request('','PUT',{mood:'none',note:'',visible:false,revision:2})).status,200);
  assert.equal((await (await request('?fid=42','GET',undefined,null)).json()).item,null);
  assert.ok(calls.some(c=>c[0]==='public'));
 }finally{api.close()}
});

test('reserved status records cannot be forged through generic notifications',()=>{
 const sample={id:'00000000-0000-4000-8000-000000000003',
   source:'other',kind:'message',payload:{},
   occurred_at:'2026-10-28T10:00:00Z',revision:0};
 assert.equal(validateCloudItem('notifications',sample),null);
 assert.equal(validateCloudItem('notifications',{
   ...sample,id:'bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb',
   source:'genycaster',kind:'profile_status'
 }),null);
});
