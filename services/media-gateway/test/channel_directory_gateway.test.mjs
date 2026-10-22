import test from 'node:test';
import assert from 'node:assert/strict';
import {createMediaGateway,loadConfig} from '../gateway.mjs';

const cfg=loadConfig({
  GENYLEAP_MEDIA_TICKET_SECRET:'0123456789abcdef0123456789abcdef',
});
const img='https://imagedelivery.net/abc/def/original';

async function serve(fetchImpl,action) {
  const requests=[];
  const server=createMediaGateway(cfg,{},{
    fetchImpl:async (url,options)=>{
      requests.push({url:String(url),options});
      return fetchImpl(url,options);
    },
    farcasterBroker:{},
  });
  await new Promise((resolve,reject)=>{
    server.once('error',reject);
    server.listen(0,'127.0.0.1',resolve);
  });
  const base='http://127.0.0.1:'+server.address().port;
  try {await action({base,requests});}
  finally {await new Promise(resolve=>server.close(resolve));}
}

test('channel directory returns real image metadata over public CORS without affecting cast URLs',async()=>{
  await serve(async(url)=>{
    const dest=new URL(url);
    assert.equal(dest.hostname,'client.warpcast.com');
    assert.equal(dest.pathname,'/v2/search-channels');
    assert.equal(dest.searchParams.get('q'),'genyleap');
    return new Response(JSON.stringify({result:{channels:[{
      type:'channel',key:'genyleap',name:'Genyleap',
      imageUrl:img,followerCount:10485,memberCount:3
    }]}}),{headers:{'content-type':'application/json'}});
  },async({base,requests})=>{
    const pathname='/v1/browser/channels/search-channels?q=genyleap&limit=25';
    const preflight=await fetch(base+pathname,{method:'OPTIONS',headers:{
      origin:'https://caster.genyleap.com',
      'access-control-request-method':'GET',
    }});
    assert.equal(preflight.status,204);
    assert.equal(preflight.headers.get('access-control-allow-origin'),'*');
    const response=await fetch(base+pathname,{headers:{origin:'https://caster.genyleap.com'}});
    assert.equal(response.status,200);
    assert.match(response.headers.get('content-type'),/application\/json/);
    const body=await response.json();
    assert.equal(body.result.channels[0].imageUrl,img);
    const cached=await fetch(base+pathname);
    assert.equal(cached.status,200);
    assert.equal(requests.length,1,'channel data cached by bounded query');
  });
});

test('channel directory enforces bounded inputs, no mutable methods or caller-controlled upstream',async()=>{
  await serve(async()=>{throw Error('unreachable');},async({base,requests})=>{
    const route='/v1/browser/channels/search-channels';
    for(const path of [
      route+'?q=genyleap&url=https://evil.example',
      route+'?q='+encodeURIComponent('x'.repeat(65)),
      route+'?q=genyleap&limit=999',
      '/v1/browser/channels/channel?key=chain://eip155:1/erc20:0xabc',
      '/v1/browser/channels/channel?key=genyleap&endpoint=https://evil.example',
    ]) {
      const reply=await fetch(base+path);
      assert.equal(reply.status,400,path);
    }
    assert.equal((await fetch(base+route+'?q=geny',{method:'POST'})).status,405);
    assert.equal(requests.length,0);
  });
});

test('channel directory metadata retrieval is channel-key scoped and fails closed',async()=>{
  await serve(async(url)=>{
    const dest=new URL(url);
    assert.equal(dest.hostname,'client.warpcast.com');
    assert.equal(dest.pathname,'/v2/channel');
    assert.equal(dest.searchParams.get('key'),'genyleap');
    return new Response(JSON.stringify({result:{channel:{
      key:'genyleap',imageUrl:img,description:'Hello',followerCount:1000
    }}}),{headers:{'content-type':'application/json'}});
  },async({base,requests})=>{
    const response=await fetch(base+'/v1/browser/channels/channel?key=genyleap');
    assert.equal(response.status,200);
    const data=await response.json();
    assert.equal(data.result.channel.imageUrl,img);
    assert.equal(requests.length,1);
  });
  await serve(async()=>new Response('<html>error</html>',{
    status:200,headers:{'content-type':'text/html'}
  }),async({base})=>{
    const r=await fetch(base+'/v1/browser/channels/search-channels?q=test');
    assert.equal(r.status,502);
    assert.deepEqual(await r.json(),{error:'channel_directory_unavailable'});
  });
});
