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
