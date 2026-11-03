import test from 'node:test';
import assert from 'node:assert/strict';
import {createCloudHandler} from '../cloud_state.mjs';
import {createPgStore} from '../pg_store.mjs';
import {createServer} from 'node:http';

const owner={issuer:'https://identity.example.test',subject:'farcaster:42',fid:'42'};
test('Preferences are revision-checked, scoped to owner and reject invalid writes',async()=>{
  let revision=0,prefs={activity_source:'casts',leaderboard_visible:false};
  const store={
    streakReady:()=>true,
    getStreakPreferences:async()=>({...prefs,revision}),
    putStreakPreferences:async(account,update)=>{
      assert.equal(account.subject,owner.subject);
      if(update.revision!==revision)return null;
      revision++;
      prefs={activity_source:update.activity_source,
        leaderboard_visible:update.leaderboard_visible};
      return {...prefs,revision};
    },
    listStreakLeaderboard:async()=>[],
    getPublicActivitySource:async()=>({activity_source:prefs.activity_source,github_login:'verifiedGitHub'})
  };
  const upstream=async(url,options)=>{
    if(options?.headers?.authorization!=='Bearer good')return new Response('{}',{status:401});
    if(url.endsWith('/account/profile'))
      return new Response(JSON.stringify({identity_id:'farcaster:42'}));
    if(url.endsWith('/account/connections'))
      return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
    return new Response('{}',{status:404});
  };
  const handler=createCloudHandler({store,issuer:owner.issuer,fetchImpl:upstream});
  const server=createServer((req,res)=>handler(req,res,new URL(req.url,'http://localhost').pathname));
  await new Promise(ok=>server.listen(0,'127.0.0.1',ok));
  const url='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/';
  const call=(name,method='GET',payload,token='Bearer good')=>fetch(url+name,
    {method,headers:{authorization:token,'content-type':'application/json'},
      ...(payload?{body:JSON.stringify(payload)}:{})});
  try{
    assert.equal((await call('streak/preferences','GET',null,'Bearer bad')).status,401);
    assert.deepEqual((await(await call('streak/preferences')).json()).preferences,
      {...prefs,revision:0});
    assert.equal((await call('streak/preferences','PUT',
      {activity_source:'github',leaderboard_visible:'yes',revision:0})).status,400);
    const first=await call('streak/preferences','PUT',
      {activity_source:'github',leaderboard_visible:true,revision:0});
    assert.equal(first.status,200);
    assert.equal((await first.json()).preferences.revision,1);
    assert.equal((await call('streak/preferences','PUT',
      {activity_source:'casts',leaderboard_visible:false,revision:0})).status,409);
    const publicRead=await call('profile_activity?fid=42','GET',null,'');
    assert.equal(publicRead.status,200);
    assert.deepEqual(await publicRead.json(),{fid:'42',activity_source:'github',github_login:'verifiedGitHub'});
    const forbidden=await call('streak/leaderboard?track=other');
    assert.equal(forbidden.status,400);
    assert.equal((await call('streak/leaderboard')).status,200);
    // Opting out must never gate the public READ endpoint.
    assert.equal((await call('streak/leaderboard','GET',null,'')).status,200);
    assert.equal((await call('streak/leaderboard','POST',null,'')).status,405);
  }finally{server.close();}
});

test('Leaderboard SQL filters explicit opt-in before computing all ranks and limits to 100',async()=>{
  const queries=[];
  const pool={
    async query(sql,args) {
      queries.push({sql,args});
      if(sql.startsWith('SELECT version FROM schema_migrations'))
        return {rows:[{version:3},{version:10}]};
      if(sql.includes('SELECT substring(subject from 11)'))
        return {rows:[{fid:'42',current:7,longest:14,total:20}]};
      if(sql.includes('SELECT payload,revision::text FROM notifications'))
        return {rows:[]};
      return {rows:[],rowCount:0};
    },
    async end(){}
  };
  const store=await createPgStore('postgres://unused',{poolFactory:()=>pool});
  try {
    const leaders=await store.listStreakLeaderboard('activity');
    assert.deepEqual(leaders,[{rank:1,fid:'42',current:7,longest:14,total:20}]);
    const sql=queries.at(-1).sql;
    assert.match(sql,/leaderboard_visible'='true'/);
    assert.match(sql,/INNER JOIN eligible/);
    assert.match(sql,/ORDER BY current DESC,longest DESC,total DESC/);
    assert.match(sql,/LIMIT 100/);
    assert.match(sql,/e\\.credited_on >= \\$3::date/);
    assert.deepEqual(queries.at(-1).args,
      ['00000000-0000-4000-8000-000000000004','activity','2026-11-02']);
    assert.doesNotMatch(sql,/LIMIT 100[\\s\\S]+FROM ranked/);
    // Personal history and public rankings must use the same launch cutoff.
    await store.listStreakEvents(owner);
    assert.match(queries.at(-1).sql,/credited_on >= \\$3::date/);
    assert.equal(queries.at(-1).args[2],'2026-11-02');
    await assert.rejects(
      store.recordStreakCast(owner,'0x'+'aa'.repeat(20),'reply','2026-11-01'),
      /streak_before_feature_launch/);
    assert.equal((await store.getStreakPreferences(owner)).leaderboard_visible,false);
  }finally{await store.close();}
});
