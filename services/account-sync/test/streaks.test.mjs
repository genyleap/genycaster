import test from 'node:test';
import assert from 'node:assert/strict';
import {calculateStreaks} from '../streaks.mjs';
import {createCloudHandler,verifyStreakCastOwnership,discoverVerifiedStreakCasts} from '../cloud_state.mjs';
import {createServer} from 'node:http';
const hash='0x'+'a1'.repeat(20);
const owner={issuer:'https://identity.example.test',subject:'farcaster:42',fid:'42'};
const now=new Date('2026-11-02T18:00:00Z');
test('Streak days are unique, UTC, badge thresholds respect longest and grace yesterday',()=>{
  const rows=['2026-10-30','2026-10-31','2026-11-01'].map(day=>({day,kind:'cast'}));
  rows.push({day:'2026-11-01',kind:'cast'},{day:'2026-11-02',kind:'reply'});
  const summary=calculateStreaks(rows,now);
  assert.equal(summary.activity.current,4);
  assert.equal(summary.creator.current,3);
  assert.deepEqual(summary.creator.badges,[1,3]);
  assert.deepEqual(summary.activity.milestone_definitions
    .filter(level=>level.unlocked).map(level=>level.threshold_days),[1,3]);
  assert.equal(summary.activity.milestone_definitions[0].id,'streak-1');
  assert.equal(summary.activity.milestone_definitions[0].icon,'flame');
  assert.equal(summary.activity.milestone_definitions[0].name_id,
    'genycaster.streak.badge_first_spark');
  assert.equal(summary.activity.milestone_definitions[2].name_id,
    'genycaster.streak.badge_momentum');
  assert.equal(summary.activity.milestone_definitions.at(-1).name_id,
    'genycaster.streak.badge_eternal_flame');
  assert.equal(summary.activity.milestone_definitions[0].reward_kind,'none');
  assert.equal(summary.activity.milestone_definitions[2].progress_days,4);
  assert.equal(summary.activity.milestone_definitions[2].unlocked,false);
  assert.ok(summary.activity.milestone_definitions.some(level=>level.threshold_days===180));
  assert.equal(summary.activity.total_days,4);
  assert.equal(summary.creator.week.length,7);
  assert.equal(summary.activity.week[0].date,'2026-11-02');
  assert.equal(summary.activity.week_saturday.length,7);
  assert.equal(summary.activity.week_saturday[0].date,'2026-10-31');
  assert.equal(summary.activity.week_saturday[2].date,'2026-11-02');
  assert.equal(summary.activity.week_saturday[2].completed,true);
  assert.equal(summary.creator.week_saturday[2].completed,false);
  assert.deepEqual(summary.activity.week_saturday.map(day=>day.date),
    ['2026-10-31','2026-11-01','2026-11-02','2026-11-03',
     '2026-11-04','2026-11-05','2026-11-06']);
  assert.deepEqual(calculateStreaks([],now).creator.badges,[]);
  assert.equal(calculateStreaks([{day:'2026-11-01',kind:'cast'}],now).creator.current,1);
});
test('Streak verifier accepts only exact fid, message hash and CastAdd payload',async()=>{
  const reader=async(url)=>new Response(JSON.stringify({message:{hash,
    data:{fid:42,timestamp:Math.floor((Date.now()-Date.UTC(2021,0,1))/1000),
      castAddBody:{parentCastId:{fid:9,hash:'0x'+'11'.repeat(20)}}}}}),{status:200});
  assert.deepEqual(await verifyStreakCastOwnership(hash,'42',{fetchImpl:reader}),
    {kind:'reply',day:new Date().toISOString().slice(0,10)});
  assert.equal(await verifyStreakCastOwnership(hash,'43',{fetchImpl:reader}),null);
  assert.equal(await verifyStreakCastOwnership('invalid','42',{fetchImpl:reader}),null);
  const falseCast=async()=>new Response(JSON.stringify({message:{hash,
    data:{fid:42,castAddBody:null}}}),{status:200});
  assert.equal(await verifyStreakCastOwnership(hash,'42',{fetchImpl:falseCast}),null);
  const feed=async(url)=>new Response(JSON.stringify({casts:[{
    hash:hash.slice(2),author:{fid:42},
    timestamp:new Date().toISOString(),parent_hash:null
  }]}),{status:200});
  assert.deepEqual(await verifyStreakCastOwnership(hash,'42',{fetchImpl:feed}),
    {kind:'cast',day:new Date().toISOString().slice(0,10)});
  const old=async()=>new Response(JSON.stringify({casts:[{
    hash:hash.slice(2),author:{fid:42},
    timestamp:'2023-01-01T12:00:00Z',parent_hash:null
  }]}),{status:200});
  assert.equal(await verifyStreakCastOwnership(hash,'42',{fetchImpl:old}),null);
});
const fetchUpstream=async(url,options)=>{
  if(options?.headers?.authorization!=='Bearer good')
    return new Response('{}',{status:401});
  if(url.endsWith('/account/profile'))return new Response(JSON.stringify({identity_id:'farcaster:42'}));
  if(url.endsWith('/account/connections'))return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
  return new Response('{}',{status:404});
};
async function withApi(action,verified={kind:'cast',day:new Date().toISOString().slice(0,10)}) {
 const events=new Map();
 let verifierCalls=0;
 const store={
  streakReady:()=>true,
  async listStreakEvents(){return [...events.values()];},
  async recordStreakCast(account,id,kind,day) {
   assert.equal(account.subject,'farcaster:42');
   if(events.has(id))return {recorded:false};
   assert.match(day,/^\d{4}-\d{2}-\d{2}$/);
   events.set(id,{day,kind});return {recorded:true};
  }
 };
 const handler=createCloudHandler({store,issuer:owner.issuer,fetchImpl:fetchUpstream,
    verifyStreakCast:async()=>{verifierCalls++;return verified;}});
 const server=createServer((req,res)=>handler(req,res,new URL(req.url,'http://localhost').pathname));
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const base='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
 const req=(path,method='GET',data,token='Bearer good')=>fetch(base+path,{
   method,headers:{authorization:token,'content-type':'application/json'},
   ...(data?{body:JSON.stringify(data)}:{})});
 try{await action(req,()=>verifierCalls);}finally{server.close();}
}
test('Streak API cannot award anonymous, invalid, unconfirmed or duplicate casts',async()=>{
 await withApi(async(req,calls)=>{
  assert.equal((await req('streak','GET',undefined,'Bearer invalid')).status,401);
  assert.equal((await req('streak/cast','PUT',{cast_hash:'bad'})).status,400);
  assert.equal((await req('streak/cast','PUT',{cast_hash:hash,kind:'cast'})).status,400);
  const saved=await req('streak/cast','PUT',{cast_hash:hash});
  assert.equal(saved.status,200);
  assert.equal((await saved.json()).recorded,true);
  const again=await req('streak/cast','PUT',{cast_hash:hash});
  assert.equal((await again.json()).recorded,false);
  assert.equal(calls(),2);
  const result=await (await req('streak')).json();
  assert.equal(result.streak.activity.total_days,1);
 });
 await withApi(async(req)=>{
  assert.equal((await req('streak/cast','PUT',{cast_hash:hash})).status,422);
 },null);
});

test('read-side reconciliation credits the exact confirmed November 3 cast without client PUT',async()=>{
 const castHash='0x55c129f6fa06ecf4a974fd5bcd432a8b3e4640c5';
 const yesterday='0x2993e01d2d83560c3209322e3f49d75b891bb8ab';
 const feed = async url => {
   if(url.includes('/feed/user/casts'))return new Response(JSON.stringify({casts:[
     {object:'cast',hash:castHash.slice(2),author:{fid:321969},
      timestamp:'2026-11-03T15:48:21.000Z',parent_url:'https://warpcast.com/~/channel/replyguys'},
     {object:'cast',hash:yesterday.slice(2),author:{fid:321969},
      timestamp:'2026-11-02T13:40:33.000Z',parent_url:'https://warpcast.com/~/channel/geny'},
     {object:'cast',hash:'66'.repeat(20),author:{fid:999},
      timestamp:'2026-11-03T15:48:21.000Z'}
   ]}),{status:200});
   return new Response('{}',{status:404});
 };
 const candidates = await discoverVerifiedStreakCasts('321969',{
   fetchImpl:feed,now:new Date('2026-11-03T20:00:00.000Z')});
 assert.deepEqual(candidates,[{
   hash:castHash,day:'2026-11-03',kind:'cast'
 },{hash:yesterday,day:'2026-11-02',kind:'cast'}]);
 const records = new Map();
 let feedReads=0;
 const store={
   streakReady:()=>true,
   async listStreakEvents(){return [...records.values()];},
   async recordStreakCast(account,hash,kind,day){
     assert.equal(account.fid,'321969');
     if(records.has(hash)) return {recorded:false};
     records.set(hash,{day,kind});return {recorded:true};
   }
 };
 const owner={issuer:'https://identity.example.test',subject:'farcaster:321969',fid:'321969'};
 const upstream=async (url,options)=>{
   if(options?.headers?.authorization !== 'Bearer good')return new Response('{}',{status:401});
   if(url.endsWith('/account/profile'))
     return new Response(JSON.stringify({identity_id:'farcaster:321969'}));
   if(url.endsWith('/account/connections'))
     return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'321969'}]}));
   return new Response('{}',{status:404});
 };
 const handler=createCloudHandler({store,issuer:owner.issuer,
   fetchImpl:upstream,
   discoverStreakCasts:async fid=>{
     ++feedReads;assert.equal(fid,'321969');return candidates;
   }});
 const server=createServer((req,res)=>handler(req,res,new URL(req.url,'http://localhost').pathname));
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 try {
   const url='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/streak';
   const options={headers:{authorization:'Bearer good'}};
   const before=await fetch(url,{headers:{authorization:'Bearer wrong'}});
   assert.equal(before.status,401);
   const result=await (await fetch(url,options)).json();
   assert.equal(result.streak.creator.current,2);
   assert.equal(result.streak.activity.current,2);
   assert.equal(feedReads,1);
   const second=await (await fetch(url,options)).json();
   assert.equal(second.streak.activity.total_days,2);
   assert.equal(feedReads,1);
   assert.equal(records.size,2);
 }finally{server.close();}
});
