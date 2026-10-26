import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
import {createAccountApi} from '../server.mjs';
import {validateCloudItem} from '../cloud_state.mjs';

const id='00000000-0000-4000-8000-000000000002';
const normal='Bearer good';
const other='Bearer other';
const issuer='https://identity.example.test';
const key=owner=>owner.issuer+'|'+owner.subject;
const current=new Map();
const store={
  async listUpdates(){return [];},
  async list(entity,owner){return entity==='drafts' ? [...(current.get(key(owner))||[])]:[];},
  async getDraft(owner,requestedId){
    return (current.get(key(owner))||[]).find(row=>row.id===requestedId)||null;
  },
  async put(entity,owner,item){
    if(entity!=='drafts') throw Error('unexpected');
    const k=key(owner), rows=current.get(k)||[];
    const previous=rows.find(row=>row.id===item.id);
    if((previous?.revision||0)!==item.revision) return null;
    const saved={id:item.id,kind:item.kind,title:item.title,content:item.content,
      revision:item.revision+1,deleted_at:item.is_deleted?'2026-10-26T09:00:00Z':null};
    current.set(k,[saved]);
    return saved;
  }
};
const fetchImpl=async(url,opt)=>{
  const auth=opt.headers.authorization;
  if(auth!==normal && auth!==other) return new Response('{}',{status:401});
  if(url.endsWith('/account/profile')) return new Response(
    JSON.stringify({identity_id:auth===normal?'alpha':'beta'}),{status:200});
  if(url.endsWith('/account/connections')) return new Response(JSON.stringify({
    connections:[{provider:'farcaster',fid:auth===normal?'42':'93'}]
  }),{status:200});
  return new Response('{}',{status:404});
};

test('draft revisions and deletions are validated, without trusting owner claims',()=>{
  const draft={id,kind:'cast',title:'',content:{document:{version:1,blocks:[]}},
    revision:0,issuer:'attacker',subject:'farcaster:93'};
  assert.deepEqual(validateCloudItem('drafts',draft),{id,kind:'cast',title:'',
    content:draft.content,revision:0,is_deleted:false});
  assert.equal(validateCloudItem('drafts',{...draft,revision:undefined}),null);
  assert.equal(validateCloudItem('drafts',{...draft,revision:-1}),null);
  assert.equal(validateCloudItem('drafts',{...draft,is_deleted:'yes'}),null);
  assert.equal(validateCloudItem('drafts',{...draft,is_deleted:true}),null);
  assert.equal(validateCloudItem('drafts',{...draft,title:'',content:{},
    is_deleted:true}).is_deleted,true);
});

test('two verified devices see saved drafts and tombstones; stale writes cannot resurrect',async()=>{
  const server=createAccountApi({catalogUrl:issuer+'/auth/providers',issuer,store,fetchImpl});
  await new Promise(done=>server.listen(0,'127.0.0.1',done));
  const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/drafts';
  const request=(token,method='GET',body)=>fetch(base,{
    method,headers:{authorization:token,'content-type':'application/json'},
    ...(body?{body:JSON.stringify(body)}:{})});
  const original={id,kind:'cast',title:'',content:{document:{version:1,blocks:['a']}},
    revision:0};
  try{
    assert.equal((await request('Bearer invalid')).status,401);
    const created=await request(normal,'PUT',original);
    assert.equal(created.status,200);
    assert.equal((await created.json()).item.revision,1);
    const remote=await request(normal);
    assert.equal((await remote.json()).items[0].content.document.blocks[0],'a');
    const byId=await fetch(base+'?id='+id,{headers:{authorization:normal}});
    assert.equal(byId.status,200);
    assert.equal((await byId.json()).items[0].id,id);
    const foreignById=await fetch(base+'?id='+id,{headers:{authorization:other}});
    assert.equal((await foreignById.json()).items.length,0);
    const invalidId=await fetch(base+'?id=../../other',{headers:{authorization:normal}});
    assert.equal(invalidId.status,400);
    assert.equal((await request(other)).status,200);
    assert.equal((await (await request(other)).json()).items.length,0);
    const deleted=await request(normal,'PUT',{
      ...original,content:{},revision:1,is_deleted:true});
    assert.equal(deleted.status,200);
    assert.equal((await deleted.json()).item.revision,2);
    const tombstone=(await (await request(normal)).json()).items[0];
    assert.ok(tombstone.deleted_at);
    assert.equal((await request(normal,'PUT',original)).status,409);
    assert.equal((await request(normal,'PUT',{...original,revision:1})).status,409);
    assert.equal((await request(normal,'PUT',{...original,revision:2})).status,200);
    const longDraft={...original,revision:3,content:{body:'x'.repeat(100_000)}};
    assert.equal((await request(normal,'PUT',longDraft)).status,200);
    const oversized={...original,revision:4,content:{body:'x'.repeat(220_000)}};
    assert.equal((await request(normal,'PUT',oversized)).status,413);
  }finally{await new Promise(done=>server.close(done));}
});

test('PostgreSQL draft CAS updates retain tombstones and only accept expected revision',()=>{
  const sql=readFileSync(new URL('../pg_store.mjs',import.meta.url),'utf8');
  assert.match(sql,/deleted_at=EXCLUDED.deleted_at/);
  assert.match(sql,/WHERE drafts.revision=\$7/);
  assert.match(sql,/CASE WHEN \$8::boolean THEN now\(\) ELSE NULL END/);
});
