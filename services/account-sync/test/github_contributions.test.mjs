import assert from 'node:assert/strict';
import test from 'node:test';
import {parseGithubCalendar,createGithubCalendarReader} from '../github_contributions.mjs';
import {createAccountApi} from '../server.mjs';

function fixture(year=2025) {
  let html='<div class="js-yearly-contributions">';
  const days=(Date.UTC(year+1,0,1)-Date.UTC(year,0,1))/86400000;
  for(let i=0;i<days;i++) {
    const date=new Date(Date.UTC(year,0,i+1)).toISOString().slice(0,10);
    const n=i%7===0?3:0;
    const id='contribution-day-'+i;
    html+='<td data-date="'+date+'" id="'+id+'" data-level="'+(n?2:0)+'" class="ContributionCalendar-day"></td>';
    html+='<tool-tip for="'+id+'">'+(n?'3 contributions':'No contributions')+' on January 1.</tool-tip>';
  }
  return html+'</div>';
}
test('calendar counts tooltip values, not color intensities',()=>{
  const c=parseGithubCalendar(fixture(),{login:'thecompez',year:2025});
  assert.equal(c.days.length,365);assert.equal(c.count,159);
  assert.equal(c.days[0].count,3);assert.equal(c.days[0].level,2);
});
test('partial HTML and malformed login fail closed',()=>{
  assert.throws(()=>parseGithubCalendar('<p>bad</p>',{login:'thecompez',year:2025}),/incomplete/);
  assert.throws(()=>parseGithubCalendar(fixture(),{login:'bad/name',year:2025}));
});
test('reader fixed origin, in-flight dedupe and cache',async()=>{
  let calls=0;
  const read=createGithubCalendarReader({
    now:()=>Date.parse('2026-10-28T08:00:00Z'),
    fetchImpl:async url=>{
      assert.match(url,/^https:\/\/github\.com\/users\/thecompez\/contributions\?from=2025-01-01&to=2025-12-31$/);
      calls++;
      return new Response(fixture(),{headers:{'content-type':'text/html'}});
    }
  });
  const [a,b]=await Promise.all([read('thecompez',2025),read('thecompez',2025)]);
  assert.deepEqual(a,b);assert.equal(calls,1);
  await read('thecompez',2025);assert.equal(calls,1);
  await assert.rejects(read('bad/name',2025));
});
test('public calendar endpoint requires authenticated owner and confirmed GitHub link',async()=>{
  const issuer='https://openproof.example.test';
  let hits=0;
  const fetchImpl=async url=>{
    if(url.includes('github.com/users')) {
      hits++;return new Response(fixture(),{headers:{'content-type':'text/html'}});
    }
    if(url.endsWith('/account/profile'))
      return new Response(JSON.stringify({identity_id:'farcaster:42'}));
    if(url.endsWith('/account/connections'))
      return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
    throw new Error('unexpected upstream');
  };
  let linked=true;
  const store={async list(entity){
    assert.equal(entity,'connections');
    return linked?[{provider:'github',preferred_username:'thecompez',
      last_confirmed_at:'2026-10-27T12:00:00Z'}]:[];
  }};
  const api=createAccountApi({catalogUrl:issuer+'/catalog',issuer,store,fetchImpl});
  await new Promise(resolve=>api.listen(0,'127.0.0.1',resolve));
  const path='http://127.0.0.1:'+api.address().port+
    '/api/v1/cloud/github_contributions?username=thecompez&year=2025';
  try{
    assert.equal((await fetch(path)).status,401);
    const r=await fetch(path,{headers:{authorization:'Bearer good'}});
    assert.equal(r.status,200);assert.equal((await r.json()).count,159);
    assert.equal(hits,1);
    linked=false;
    assert.equal((await fetch(path,{headers:{authorization:'Bearer good'}})).status,403);
    assert.equal(hits,1);
  }finally{api.close();}
});
