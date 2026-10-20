import assert from 'node:assert/strict';
import test from 'node:test';
import {createBrowserReactionService,normalizeReactionBatch} from '../browser_cast_reactions.mjs';

const a='0x9aea66ddab7a51563cc54e881931a5acb9e2bf16';
const b='0x88039a6986f69044ef7aaad2bb9dd58398b3ff82';
const hash=i=>'0x'+i.toString(16).padStart(40,'0');
const ok=rows=>new Response(JSON.stringify({result:{reactions:rows}}),{
  status:200,headers:{'content-type':'application/json'}
});

test('only matching reactor FIDs and verified positive like/recast events affect a cast',async()=>{
  const calls=[];
  const service=createBrowserReactionService({now:()=>1000,fetchImpl:async(url,options)=>{
    const parsed=new URL(url);
    calls.push(parsed);
    assert.equal(options.method,'GET');
    assert.equal(parsed.host,'api.farcaster.xyz');
    assert.equal(parsed.pathname,'/v2/cast-reactions');
    assert.equal(parsed.searchParams.get('limit'),'100');
    if(parsed.searchParams.get('castHash')===a)
      return ok([
        {type:'like',reactor:{fid:5818}},
        {type:'recast',reactor:{fid:234616}},
        {type:'like',reactor:{fid:234616}},
        {type:'like',reactor:{fid:321969}},
        {type:'like',reactor:{fid:0}},
      ]);
    return ok([{type:'like',reactor:{fid:42}}]);
  }});
  const first=await service.resolve({viewerFid:234616,casts:[a,b]});
  assert.equal(first.status,200);
  assert.deepEqual({...first.data.reactions},{[a]:{liked:true,recasted:true}});
  const second=await service.resolve({viewerFid:5818,casts:[a]});
  assert.deepEqual({...second.data.reactions},{[a]:{liked:true}});
  assert.equal(calls.length,2,'Responses shared across viewers for the same cast');
});

test('a truncated or unavailable public reaction response must never invent negative viewer state',async()=>{
  const service=createBrowserReactionService({fetchImpl:async url=>{
    return new URL(url).searchParams.get('castHash')===a
      ? ok([{type:'like',reactor:{fid:22}}])
      : new Response('down',{status:503,headers:{'content-type':'text/plain'}});
  }});
  const result=await service.resolve({viewerFid:234616,casts:[a,b]});
  assert.equal(result.status,200);
  assert.deepEqual({...result.data.reactions},{});
  assert.ok(!JSON.stringify(result.data).includes('false'));
});

test('requests are bounded, deduplicated, limited to two simultaneous upstream requests',async()=>{
  const casts=Array.from({length:8},(_,i)=>hash(i+100));
  assert.equal(normalizeReactionBatch({viewerFid:1,casts:Array(9).fill(a)}),null);
  assert.equal(normalizeReactionBatch({viewerFid:0,casts:[a]}),null);
  assert.equal(normalizeReactionBatch({viewerFid:1,casts:['0x123']}),null);
  assert.equal(normalizeReactionBatch({viewerFid:1,casts:'abc'}),null);
  assert.equal(normalizeReactionBatch({viewerFid:234616,casts:[a,a]}).casts.length,1);
  let active=0,max=0,calls=0;
  const service=createBrowserReactionService({fetchImpl:async ()=>{
    active++;max=Math.max(max,active);calls++;
    await new Promise(done=>setTimeout(done,3));
    active--;return ok([{type:'recast',reactor:{fid:234616}}]);
  }});
  const response=await service.resolve({viewerFid:234616,casts});
  assert.equal(Object.keys(response.data.reactions).length,8);
  assert.equal(calls,8);
  assert.ok(max<=2,'At most two concurrent upstream requests');
});

test('malformed upstream reactions and malicious properties are ignored',async()=>{
  const service=createBrowserReactionService({fetchImpl:async()=>ok([
    {type:'like',reactor:{fid:'abc'}},
    {type:'recast',reactor:{fid:234616}},
    {type:'like',reactor:{fid:234616.5}},
    {type:'like',reactor:{fid:234616}},
  ])});
  const result=await service.resolve({viewerFid:234616,casts:[a]});
  assert.deepEqual({...result.data.reactions},{[a]:{liked:true,recasted:true}});
});


test('global reaction admission remains bounded across independent concurrent clients',async()=>{
  let active=0, peak=0, calls=0;
  const service=createBrowserReactionService({
    fetchImpl:async()=>{
      ++active; peak=Math.max(peak,active); ++calls;
      await new Promise(done=>setTimeout(done,6));
      --active;
      return ok([{type:'like',reactor:{fid:12}}]);
    },
  });
  const groups=Array.from({length:4},(_,group)=>Array.from({length:8},
    (_,index)=>hash(1000+group*8+index)));
  const results=await Promise.all(groups.map(casts=>
    service.resolve({viewerFid:12,casts})));
  assert.equal(calls,32);
  assert.ok(peak<=2,'all simultaneous callers share two upstream slots');
  for(const result of results) {
    assert.equal(result.status,200);
    assert.equal(Object.keys(result.data.reactions).length,8);
  }
});

test('concurrent viewers share one in-flight cast query without losing verified positives',async()=>{
  let calls=0;
  const service=createBrowserReactionService({fetchImpl:async()=>{
    ++calls;
    await new Promise(done=>setTimeout(done,15));
    return ok([
      {type:'like',reactor:{fid:12}},
      {type:'recast',reactor:{fid:13}},
    ]);
  }});
  const [first,second]=await Promise.all([
    service.resolve({viewerFid:12,casts:[a]}),
    service.resolve({viewerFid:13,casts:[a]}),
  ]);
  assert.equal(calls,1);
  assert.deepEqual({...first.data.reactions},{[a]:{liked:true}});
  assert.deepEqual({...second.data.reactions},{[a]:{recasted:true}});
});

test('deadline returns immutable positive partial results while slow upstream finishes',async()=>{
  const slow=hash(9001), fast=hash(9002);
  const service=createBrowserReactionService({
    workBudgetMs:35,
    fetchImpl:async url=>{
      const id=new URL(url).searchParams.get('castHash');
      await new Promise(done=>setTimeout(done,id===slow?65:4));
      return ok([{type:'like',reactor:{fid:12}}]);
    },
  });
  const started=Date.now();
  const response=await service.resolve({viewerFid:12,casts:[fast,slow]});
  assert.equal(response.status,200);
  assert.deepEqual({...response.data.reactions},{[fast]:{liked:true}});
  assert.ok(Date.now()-started<65,'one slow lookup cannot block the whole batch');
  await new Promise(done=>setTimeout(done,70));
  assert.deepEqual({...response.data.reactions},{[fast]:{liked:true}},
    'late shared upstream completion never mutates a returned result');
});

test('over-capacity lookups reject promptly without manufacturing zero or negative reactions',async()=>{
  let release;
  const gate=new Promise(resolve=>{release=resolve;});
  const service=createBrowserReactionService({
    workBudgetMs:60,
    fetchImpl:async()=>{
      await gate;
      return ok([{type:'like',reactor:{fid:12}}]);
    },
  });
  const responses=await Promise.all(Array.from({length:42},(_,i)=>
    service.resolve({viewerFid:12,casts:[hash(10000+i)]})));
  assert.equal(responses.length,42);
  for(const result of responses){
    assert.equal(result.status,200);
    assert.deepEqual({...result.data.reactions},{});
    assert.ok(!JSON.stringify(result.data).includes('false'));
  }
  release();
});


test('response body reading stops at a bounded byte budget without content-length',async()=>{
  let produced=0, cancelled=false;
  const service=createBrowserReactionService({fetchImpl:async()=>
    new Response(new ReadableStream({
      pull(controller) {
        ++produced;
        controller.enqueue(new Uint8Array(1024*1024));
      },
      cancel() { cancelled=true; },
    },{highWaterMark:0}),{headers:{'content-type':'application/json'}}),
  });
  const response=await service.resolve({viewerFid:12,casts:[a]});
  assert.equal(response.status,200);
  assert.deepEqual({...response.data.reactions},{});
  assert.ok(cancelled,'the unbounded upstream body must be canceled');
  assert.ok(produced<=4,'stop reading shortly after the 3-MiB limit');
});

test('temporarily unavailable cast reactions use expiring retry admission, never false',async()=>{
  let instant=1000,calls=0;
  const service=createBrowserReactionService({
    now:()=>instant,
    fetchImpl:async()=>{
      ++calls;
      if(calls===1)return new Response('unavailable',{status:503});
      return ok([{type:'like',reactor:{fid:12}}]);
    },
  });
  assert.deepEqual({...((await service.resolve({viewerFid:12,casts:[a]})).data.reactions)},{});
  assert.deepEqual({...((await service.resolve({viewerFid:12,casts:[a]})).data.reactions)},{});
  assert.equal(calls,1,'backoff suppresses immediate repeat failures');
  instant+=5001;
  assert.deepEqual({...((await service.resolve({viewerFid:12,casts:[a]})).data.reactions)},
    {[a]:{liked:true}});
  assert.equal(calls,2,'a visible cast can recover after retry eligibility');
});
