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
