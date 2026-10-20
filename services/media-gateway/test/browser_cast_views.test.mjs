import assert from 'node:assert/strict';
import test from 'node:test';
import {createCastViewService, normalizeViewRequest} from '../browser_cast_views.mjs';

const a='0x9aea66ddab7a51563cc54e881931a5acb9e2bf16';
const b='0x88039a6986f69044ef7aaad2bb9dd58398b3ff82';
const c='0x1122334455667788990011223344556677889900';
const row=(castId,authorFid=321969,text='Remember when the early Farcaster client supported animated GIF avatars')=>
  ({castId,authorFid,text});
const json=(data,status=200)=>new Response(JSON.stringify(data),{
  status,headers:{'content-type':'application/json; charset=utf-8'},
});
test('requests up to 50 recent author casts once for multiple hashes and caches values',async()=>{
  const seen=[];
  const service=createCastViewService({now:()=>10000,fetchImpl:async url=>{
    seen.push(url);
    assert.equal(new URL(url).pathname,'/v2/casts');
    assert.equal(new URL(url).searchParams.get('limit'),'50');
    return json({result:{casts:[
      {hash:a,author:{fid:321969},viewCount:1300},
      {hash:b,author:{fid:321969},viewCount:0},
      {hash:c,author:{fid:1234},viewCount:9999},
    ]}});
  }});
  const first=await service.resolve([row(a),row(b)]);
  assert.equal(first.status,200);
  assert.equal(first.data.counts[a],1300);
  assert.equal(first.data.counts[b],0); // A genuine upstream zero is valid.
  assert.equal(first.data.counts[c],undefined);
  assert.equal(seen.length,1);
  const second=await service.resolve([row(a)]);
  assert.equal(second.data.counts[a],1300);
  assert.equal(seen.length,1);
});
test('older casts fall back to text search but only matching hash AND author are accepted',async()=>{
  const calls=[];
  const service=createCastViewService({fetchImpl:async url=>{
    calls.push(url);
    const parsed=new URL(url);
    if(parsed.pathname==='/v2/casts')return json({result:{casts:[]}});
    assert.equal(parsed.pathname,'/v2/search-casts');
    assert.ok(parsed.searchParams.get('q').startsWith('Remember when'));
    return json({result:{casts:[
      {hash:c,author:{fid:321969},viewCount:9999},
      {hash:a,author:{fid:321969},viewCount:1300},
    ]}});
  }});
  const result=await service.resolve([row(a)]);
  assert.deepEqual({...result.data.counts},{[a]:1300});
  assert.equal(calls.length,2);
});
test('the public search endpoint never manufactures zero impressions for missing or malformed data',async()=>{
  const service=createCastViewService({fetchImpl:async url=>{
    const parsed=new URL(url);
    if(parsed.pathname==='/v2/casts')return json({result:{casts:[
      {hash:a,author:{fid:321969},viewCount:'1300'},
      {hash:b,author:{fid:321969},viewCount:-1},
    ]}});
    return json({result:{casts:[{hash:c,author:{fid:321969},viewCount:22}]}});
  }});
  const result=await service.resolve([row(a),row(b)]);
  assert.equal(result.status,200);
  assert.equal(Object.keys(result.data.counts).length,0);
});
test('strictly validates requests, caps batches and rejects another author matching a cast hash',async()=>{
  const valid=normalizeViewRequest(row(a));
  assert.equal(valid.castId,a);
  assert.equal(normalizeViewRequest({castId:'0x1234',authorFid:123}),null);
  assert.equal(normalizeViewRequest({castId:a,authorFid:0}),null);
  assert.equal(normalizeViewRequest({castId:a,authorFid:'NaN'}),null);
  const service=createCastViewService({fetchImpl:async url=>{
    const parsed=new URL(url);
    return json({result:{casts:parsed.pathname==='/v2/casts'
      ? [{hash:a,author:{fid:2},viewCount:50000}] : []}});
  }});
  assert.equal((await service.resolve([])).status,400);
  assert.equal((await service.resolve(Array(17).fill(row(a)))).status,400);
  const result=await service.resolve([row(a)]);
  assert.deepEqual({...result.data.counts},{});
});
test('all old replies in a full view batch are checked, without a four-result truncation',async()=>{
  const requests=Array.from({length:10},(_,i)=>({
    castId:'0x'+(500+i).toString(16).padStart(40,'0'),
    authorFid:321969,
    text:'Unique archived reply text number '+i+' from the original thread',
  }));
  let active=0,maxConcurrent=0,searchCount=0,authorCount=0;
  const service=createCastViewService({fetchImpl:async url=>{
    const parsed=new URL(url);
    if(parsed.pathname==='/v2/casts'){
      authorCount++;
      return json({result:{casts:[]}});
    }
    searchCount++;
    active++;
    maxConcurrent=Math.max(maxConcurrent,active);
    await new Promise(done=>setTimeout(done,3));
    active--;
    const requested=requests.find(row=>
      row.text.slice(0,85)===parsed.searchParams.get('q'));
    assert.ok(requested,'Unexpected search text');
    return json({result:{casts:[{
      hash:requested.castId,
      author:{fid:requested.authorFid},
      viewCount:200+Number(requested.text.match(/number (\d+)/)[1]),
    }]}});
  }});
  const resolved=await service.resolve(requests);
  assert.equal(resolved.status,200);
  assert.equal(Object.keys(resolved.data.counts).length,requests.length);
  assert.equal(resolved.data.counts[requests[9].castId],209);
  assert.equal(authorCount,1,'Recent author data is fetched once');
  assert.equal(searchCount,10,'No archived reply is silently skipped');
  assert.ok(maxConcurrent<=3,'Parallel searches are bounded');
});


test('concurrent viewport batches share one upstream concurrency budget',async()=>{
  let active=0,maxActive=0;
  const service=createCastViewService({fetchImpl:async ()=>{
    ++active;
    maxActive=Math.max(maxActive,active);
    await new Promise(resolve=>setTimeout(resolve,7));
    --active;
    return json({result:{casts:[]}});
  }});
  const groups=Array.from({length:3},(_,batch)=>
    Array.from({length:10},(_,index)=>row(
      '0x'+(10000+batch*10+index).toString(16).padStart(40,'0'),
      1000+batch*10+index,'')));
  const outputs=await Promise.all(groups.map(group=>service.resolve(group)));
  assert.equal(outputs.length,3);
  for(const result of outputs){
    assert.equal(result.status,200);
    assert.equal(Object.keys(result.data.counts).length,0);
  }
  assert.ok(maxActive<=3,'global admission must bound independent client batches');
});

test('overall deadline returns verified partial views and preserves unknowns',async()=>{
  const fast=row(a,101,'');
  const slow=row(b,102,'');
  const service=createCastViewService({
    workBudgetMs:35,
    fetchImpl:(url,{signal})=>{
      if(new URL(url).searchParams.get('fid')==='101')
        return Promise.resolve(json({result:{casts:[
          {hash:a,author:{fid:101},viewCount:14},
        ]}}));
      return new Promise((resolve,reject)=>{
        if(signal.aborted){reject(new Error('aborted'));return;}
        signal.addEventListener('abort',()=>reject(new Error('aborted')),{once:true});
      });
    },
  });
  const begin=Date.now();
  // AbortSignal.timeout is intentionally unref'ed by Node; keep the fake
  // event loop active while testing a transport that never resolves.
  const keepAlive=setTimeout(()=>{},90);
  let result;
  try{result=await service.resolve([fast,slow]);}
  finally{clearTimeout(keepAlive);}
  assert.equal(result.status,200);
  assert.equal(result.data.counts[a],14);
  assert.equal(result.data.counts[b],undefined);
  assert.ok(Date.now()-begin<500,'one stalled author must not stall the whole batch');
});


test('a quoted source retains its own unknown count for old short and empty-text casts',async()=>{
  const quotedSource=row(a,777,'');
  const shortSource=row(b,777,'hi');
  const quotingCast=row(c,888,'Long quoting text identifying an unrelated source post');
  let searches=0;
  const service=createCastViewService({fetchImpl:async url=>{
    const u=new URL(url);
    if(u.pathname==='/v2/casts'){
      return json({result:{casts:u.searchParams.get('fid')==='888'
        ? [{hash:c,author:{fid:888},viewCount:81}] : []}});
    }
    ++searches;
    return json({result:{casts:[{hash:c,author:{fid:888},viewCount:81}]}});
  }});
  const resolved=await service.resolve([quotedSource,shortSource,quotingCast]);
  assert.equal(resolved.data.counts[c],81);
  assert.equal(resolved.data.counts[a],undefined);
  assert.equal(resolved.data.counts[b],undefined);
  assert.equal(searches,0,'No speculative text fallback for empty/short sources');
});

test('a timed-out consumer cannot cancel an unrelated shared upstream reader',async()=>{
  let calls=0;
  const service=createCastViewService({
    workBudgetMs:50,
    fetchImpl:async (url,{signal})=>{
      ++calls;
      await new Promise(resolve=>setTimeout(resolve,30));
      assert.equal(signal.aborted,false);
      return json({result:{casts:[{hash:a,author:{fid:333},viewCount:14}]}});
    },
  });
  const result=await Promise.all([
    service.resolve([row(a,333,'')]),
    service.resolve([row(a,333,'')]),
  ]);
  assert.equal(calls,1,'One shared recent author lookup');
  assert.equal(result[0].data.counts[a],14);
  assert.equal(result[1].data.counts[a],14);
});

test('a partial HTTP result stays immutable after late upstream completion',async()=>{
  const service=createCastViewService({
    workBudgetMs:18,
    fetchImpl:async url=>{
      const fid=new URL(url).searchParams.get('fid');
      if(fid==='400')return json({result:{casts:[{
        hash:a,author:{fid:400},viewCount:9,
      }]}});
      await new Promise(resolve=>setTimeout(resolve,40));
      return json({result:{casts:[{
        hash:b,author:{fid:401},viewCount:99,
      }]}});
    },
  });
  const first=await service.resolve([row(a,400,''),row(b,401,'')]);
  assert.equal(first.data.counts[a],9);
  assert.equal(first.data.counts[b],undefined);
  await new Promise(resolve=>setTimeout(resolve,70));
  assert.equal(first.data.counts[b],undefined,'Late completion cannot rewrite a returned response');
});

test('expired shared waiting work releases admission before stalled active requests finish',async()=>{
  const requested=Array.from({length:35},(_,index)=>row(
    '0x'+(30000+index).toString(16).padStart(40,'0'),
    600+index,''));
  const newcomerHash='0x'+(88888).toString(16).padStart(40,'0');
  let active=0,maxActive=0;
  const releases=[];
  let launchedExpired=0;
  const service=createCastViewService({
    workBudgetMs:140,
    fetchImpl:async url=>{
      const fid=Number(new URL(url).searchParams.get('fid'));
      active++;
      maxActive=Math.max(maxActive,active);
      if(fid<603) {
        // Deliberately ignore the signal. A blocked transport must not
        // prevent an expired *waiting* request from freeing its queue slot.
        await new Promise(resolve=>releases.push(resolve));
      } else if(fid!==9999) {
        launchedExpired++;
      }
      active--;
      return json({result:{casts:fid===9999 ? [{
        hash:newcomerHash,author:{fid:9999},viewCount:42,
      }] : []}});
    },
  });
  // Fill the global 3 active + 32 waiting admissions with independently
  // timed-out shared reads. Their consumers all get valid unknown responses.
  const keepAlive=setTimeout(()=>{},420);
  try {
    const timedOut=await Promise.all(requested.map(item=>service.resolve([item])));
    assert.equal(releases.length,3,'three active transports are still held');
    assert.equal(launchedExpired,0,'no expired waiting read ran');
    assert.ok(timedOut.every(result=>result.status===200
      && Object.keys(result.data.counts).length===0));
    // A consumer deadline and each separately created shared upstream timeout
    // can differ by a few event-loop turns. Let every shared timeout fire.
    await new Promise(resolve=>setTimeout(resolve,35));
    // The pending queue should already be empty BEFORE releasing the three
    // held transports; otherwise this new visible cast is falsely rejected.
    const admitted=service.resolve([row(newcomerHash,9999,'')]);
    for(const release of releases)release();
    const resolved=await admitted;
    assert.equal(resolved.status,200);
    assert.equal(resolved.data.counts[newcomerHash],42);
    assert.equal(launchedExpired,0,'no expired request launched later');
    assert.ok(maxActive<=3,'global upstream concurrency remains bounded');
  } finally {
    for(const release of releases)release();
    clearTimeout(keepAlive);
  }
});
