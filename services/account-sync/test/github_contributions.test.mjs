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

test('current-year calendar accepts explicit accessible counts and whitespace inside cells',()=>{
  const now=Date.parse('2026-11-08T10:00:00Z');
  const today='2026-11-08';
  const days=(Date.parse(today+'T00:00:00Z')-Date.UTC(2026,0,1))/86400000+1;
  let html='<div class="js-yearly-contributions">';
  let expected=0;
  for(let i=0;i<days;i++){
    const date=new Date(Date.UTC(2026,0,i+1)).toISOString().slice(0,10);
    const n=i%9===0?5:0;
    expected+=n;
    const id='github-day-'+i;
    const label=n? n+' contributions on '+date : 'No contributions on '+date;
    html+='<td class="ContributionCalendar-day" data-level="'+(n?2:0)+
      '" data-date="'+date+'" id="'+id+'" aria-label="'+label+'">\n'+
      (i%2 ? '<span class="sr-only">'+label+'</span>' : '')+'\n</td>';
    if(i%3===0)html+='<tool-tip for="'+id+'">'+label+'</tool-tip>';
  }
  html+='</div>';
  const result=parseGithubCalendar(html,{login:'thecompez',year:2026,now});
  assert.equal(result.days.length,days);
  assert.equal(result.count,expected);
  assert.equal(result.observedDays,Math.ceil(days/9));
  assert.throws(()=>parseGithubCalendar(html.replace(/aria-label="[^"]+"/g,'')
    .replace(/<span class="sr-only">[^<]+<\/span>/g,'')
    .replace(/<tool-tip\b[^>]*>[\s\S]*?<\/tool-tip>/g,''),{
      login:'thecompez',year:2026,now}),/incomplete/);
});
test('owner-verified GitHub profile URL survives refresh and resolves absent username',async()=>{
  const issuer='https://openproof.example.test';
  let confirmed=[];
  let profileUrl='https://github.com/thecompez';
  const fetchImpl=async url=>{
    if(url.includes('github.com/users'))
      return new Response(fixture(),{headers:{'content-type':'text/html'}});
    if(url.endsWith('/account/profile'))
      return new Response(JSON.stringify({identity_id:'farcaster:42'}));
    if(url.endsWith('/account/connections'))
      return new Response(JSON.stringify({connections:[
        {provider:'farcaster',fid:'42'},
        {provider:'github',preferred_username:'',profile_url:profileUrl}
      ]}));
    throw new Error('unexpected upstream '+url);
  };
  const store={
    async refreshConnections(_principal,rows){
      confirmed=rows.map(row=>({...row,last_confirmed_at:'2026-11-08T00:00:00Z'}));
    },
    async list(entity){
      assert.equal(entity,'connections');
      return confirmed;
    }
  };
  const api=createAccountApi({catalogUrl:issuer+'/catalog',issuer,store,fetchImpl});
  await new Promise(resolve=>api.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+api.address().port+'/api/v1/cloud/';
  const headers={authorization:'Bearer good'};
  try {
    assert.equal((await fetch(base+'connections/refresh',{method:'POST',headers})).status,200);
    assert.equal(confirmed.find(row=>row.provider==='github').preferred_username,'thecompez');
    assert.equal(confirmed.find(row=>row.provider==='github').provider_profile_url,'https://github.com/thecompez');
    assert.equal((await fetch(base+'github_contributions?username=thecompez&year=2025',
      {headers})).status,200);

    // An arbitrary host/profile path never establishes a provider identity.
    profileUrl='https://github.com.evil.example/thecompez';
    assert.equal((await fetch(base+'connections/refresh',{method:'POST',headers})).status,200);
    assert.equal(confirmed.find(row=>row.provider==='github').preferred_username,'');
    assert.equal(confirmed.find(row=>row.provider==='github').provider_profile_url,null);
    const rejected=await fetch(base+'github_contributions?username=thecompez&year=2025',
      {headers});
    assert.equal(rejected.status,403);
    assert.equal((await rejected.json()).error,'github_identity_unconfirmed');
  }finally{api.close();}
});
