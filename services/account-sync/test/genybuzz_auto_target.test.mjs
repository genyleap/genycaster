import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createCloudHandler} from '../cloud_state.mjs';

const issuer='https://identity.example.test';
const owner={issuer,subject:'farcaster:42',fid:'42'};
const verify=async(url,{headers})=>{
  if(headers?.authorization!=='Bearer verified')
    return new Response('{}',{status:401});
  if(url.endsWith('/account/profile'))
    return new Response(JSON.stringify({identity_id:'account:42'}));
  if(url.endsWith('/account/connections'))
    return new Response(JSON.stringify({connections:[{provider:'farcaster',fid:'42'}]}));
  return new Response('{}',{status:404});
};

test('server resolves namespace only for a single consenting identity',async()=>{
  let sent=0;
  const resolved=[];
  const store={
    buzzReady:()=>true,
    resolveBuzzTarget:async(fid,accountIssuer,context)=>{
      resolved.push({fid,accountIssuer,context});
      if(fid==='53')
        return {issuer,subject:'farcaster:53',context};
      if(fid==='42')
        return {issuer:owner.issuer,subject:owner.subject,context};
      return null; // no match and two possible matches look identical to sender
    },
    sendBuzz:async(sender,target)=>{
      assert.equal(sender.subject,owner.subject);
      assert.equal(sender.issuer,owner.issuer);
      assert.deepEqual(target,{issuer,subject:'farcaster:53',context:'profile'});
      sent++;
      return {status:'sent',id:'3ba02b4f-fb7f-4f62-abec-0f4b9ade3f19'};
    }
  };
  const handler=createCloudHandler({store,issuer,fetchImpl:verify});
  const server=createServer((req,res)=>handler(req,res,new URL(req.url,'http://localhost').pathname));
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const url='http://127.0.0.1:'+server.address().port+'/api/v1/cloud/buzz';
  const call=async(payload,authorization='Bearer verified')=>fetch(url,{
    method:'POST',headers:{authorization,'content-type':'application/json'},
    body:JSON.stringify(payload)
  });
  try{
    const make=fid=>({target_fid:fid,target_kind:'auto',context:'profile'});
    assert.equal((await call(make('53'),'Bearer invalid')).status,401);
    assert.equal(resolved.length,0);
    assert.equal((await call(make('53'))).status,201);
    assert.equal(sent,1);
    assert.deepEqual(resolved.at(-1),{fid:'53',accountIssuer:issuer,context:'profile'});
    assert.equal((await call(make('42'))).status,400,'auto cannot send to own identity');
    assert.equal((await call(make('4294967296'))).status,403);
    assert.equal((await call(make('0'))).status,400);
    assert.equal((await call({...make('53'),issuer:'evil'})).status,400);
    assert.equal((await call({...make('53'),context:'dm'})).status,400);
    assert.equal(sent,1,'invalid, unconsenting and ambiguous targets are never sent');
  }finally{server.close();}
});
