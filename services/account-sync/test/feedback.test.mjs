import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createAccountApi} from '../server.mjs';
import {validateFeedback} from '../feedback.mjs';
import {createPgStore} from '../pg_store.mjs';

const submitted=[];
const store={
  feedbackReady:()=>true,
  async submitFeedback(owner,item) {
    submitted.push({owner,item});
    return submitted.length>10?null:{
      id:'00000000-0000-4000-8000-000000000001',
      category:item.category,status:'submitted',points_awarded:0,
      created_at:'2026-11-04T00:00:00Z'
    };
  },
  async getFeedbackOverview(owner) {
    return {stats:{total:submitted.filter(x=>x.owner.subject===owner.subject).length,
      accepted:0,resolved:0,rejected:0,points:0},reports:[]};
  }
};
const fetchImpl=async(url,options)=>{
  if(options?.headers?.authorization!=='Bearer good')
    return new Response('{}',{status:401});
  if(url.endsWith('/account/profile'))
    return new Response(JSON.stringify({identity_id:'farcaster:42'}),{status:200});
  if(url.endsWith('/account/connections'))
    return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}),{status:200});
  return new Response('{}',{status:404});
};
async function withServer(action) {
  const api=createAccountApi({
    catalogUrl:'https://identity.example.test/auth/providers',
    store,issuer:'https://identity.example.test',fetchImpl
  });
  await new Promise(resolve=>api.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+api.address().port+'/api/v1/cloud/feedback';
  const request=(method,body,authorization='Bearer good')=>
    fetch(base,{method,headers:{authorization,'content-type':'application/json'},
      ...(body===undefined?{}:{body:JSON.stringify(body)})});
  try{await action(request);}finally{await new Promise(resolve=>api.close(resolve));}
}
const good={category:'bug',sentiment:2,message:'The mobile profile form closes unexpectedly',
  platform:'macos',appVersion:'1.0',route:'profile'};
test('feedback body excludes spoofed owner/status/rewards and validates lengths',()=>{
  assert.deepEqual(validateFeedback(good),good);
  for(const key of ['fid','issuer','subject','status','pointsAwarded','reward'])
    assert.equal(validateFeedback({...good,[key]:'attacker'}),null);
  assert.equal(validateFeedback({...good,message:'short'}),null);
  assert.equal(validateFeedback({...good,sentiment:9}),null);
});
test('feedback API authenticates and saves reports for verified owner; stats are server-owned',async()=>{
  submitted.length=0;
  await withServer(async request=>{
    assert.equal((await request('GET',undefined,'Bearer invalid')).status,401);
    assert.equal((await request('POST',{...good,status:'accepted'})).status,400);
    assert.equal((await request('POST',good)).status,201);
    const overview=await (await request('GET')).json();
    assert.equal(overview.stats.total,1);
    assert.equal(overview.stats.accepted,0);
    assert.equal(submitted[0].owner.subject,'farcaster:42');
    assert.equal(submitted[0].item.message,good.message);
    assert.equal(submitted[0].item.status,undefined);
    assert.equal((await request('PUT',good)).status,405);
    for(let i=0;i<9;i++) assert.equal((await request('POST',good)).status,201);
    assert.equal((await request('POST',good)).status,429);
  });
});
test('feedback migration preserves server-only review fields and least-privilege writes',()=>{
  const sql=readFileSync(new URL('../migrations/011_feedback_reports.sql',import.meta.url),'utf8');
  assert.match(sql,/GRANT SELECT, INSERT ON TABLE feedback_reports TO genycaster_api/);
  assert.doesNotMatch(sql,/GRANT [^;]*UPDATE[^;]* TO genycaster_api/);
  assert.match(sql,/points_awarded integer NOT NULL DEFAULT 0/);
  assert.match(sql,/INSERT INTO schema_migrations\(version\) VALUES\(11\)/);
});

test('PostgreSQL feedback quota is serialized per identity and releases transactions',async()=>{
  const calls=[];
  const client={
    async query(sql,values) {
      calls.push({sql,values});
      if(sql.startsWith('INSERT INTO feedback_reports'))
        return {rows:[{id:'00000000-0000-4000-8000-000000000002',
          category:'bug',status:'submitted',points_awarded:0}]};
      return {rows:[]};
    },
    release(){ calls.push({sql:'release'}); }
  };
  const pool={
    query:async sql=>({rows:sql.includes('schema_migrations')
      ?[{version:3},{version:11}]:[]}),
    connect:async()=>client,
    end:async()=>{}
  };
  const store=await createPgStore('not-used',{poolFactory:()=>pool});
  const saved=await store.submitFeedback({issuer:'https://identity.example.test',
    subject:'farcaster:42'},good);
  assert.equal(saved?.status,'submitted');
  assert.equal(saved?.points_awarded,0);
  assert.deepEqual(calls.map(x=>x.sql.startsWith('INSERT INTO feedback_reports')
    ? 'insert' : x.sql.startsWith('SELECT pg_advisory') ? 'lock' : x.sql),
    ['BEGIN','lock','insert','COMMIT','release']);
  assert.deepEqual(calls[1].values,['https://identity.example.test','farcaster:42']);
  assert.match(calls[2].sql,/created_at > now\(\)-interval '24 hours'/);
  assert.ok(calls[2].values.includes('farcaster:42'));
});
