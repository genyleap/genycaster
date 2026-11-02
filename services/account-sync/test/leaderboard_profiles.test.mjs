import test from 'node:test';
import assert from 'node:assert/strict';
import {createLeaderboardProfileReader} from '../leaderboard_profiles.mjs';

test('public leaderboard profiles come from matching verified FIDs and cache reads',async()=>{
  let requests=0;
  const fetchImpl=async(url)=>{
    requests++;
    const fid=new URL(url).searchParams.get('fid');
    return new Response(JSON.stringify({user:{fid:Number(fid),
      display_name:'Builder '+fid,username:'builder'+fid,
      pfp_url:'https://images.example.test/avatar/'+fid+'.png'}}),
      {status:200,headers:{'content-type':'application/json'}});
  };
  const reader=createLeaderboardProfileReader({fetchImpl});
  const leaders=[{fid:'42',rank:1,current:7,longest:9},
    {fid:'7',rank:2,current:4,longest:10}];
  const a=await reader.decorate(leaders);
  assert.deepEqual(a[0],{...leaders[0],display_name:'Builder 42',
    username:'builder42',avatar_url:'https://images.example.test/avatar/42.png'});
  assert.equal(a[1].username,'builder7');
  assert.equal(requests,2);
  await reader.decorate(leaders);
  assert.equal(requests,2);
});
test('mismatched FID or unsafe avatar can never be presented as a verified profile',async()=>{
  const mismatch=createLeaderboardProfileReader({
    fetchImpl:async()=>new Response(JSON.stringify({
      user:{fid:43,display_name:'Wrong account',pfp_url:'https://img.example.test/x'}}))});
  assert.equal((await mismatch.decorate([{fid:'42',rank:1}]))[0].display_name,undefined);
  const malformed=createLeaderboardProfileReader({
    fetchImpl:async()=>new Response(JSON.stringify({
      user:{fid:42,username:'unsafe handle <>',display_name:'Correct',
        pfp_url:'javascript:alert(1)'}}))});
  const user=(await malformed.decorate([{fid:'42',rank:1}]))[0];
  assert.equal(user.display_name,'Correct');
  assert.equal(user.avatar_url,'');
  assert.equal(user.username,'');
});
