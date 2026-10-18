import test from 'node:test';
import assert from 'node:assert/strict';
import { createMediaGateway, loadConfig, miniAppIconUrls } from '../gateway.mjs';

const icon='https://hypria.app/icon.png';
const catalog={frames:[{manifest:{miniapp:{name:'Hypria Mainframe',icon_url:icon}}}]};
const config=loadConfig({
  GENYLEAP_MEDIA_TICKET_SECRET:'0123456789abcdef0123456789abcdef',
});
function fixture() {
  const seen=[];
  let catalogueFetches=0;
  const gateway=createMediaGateway(config, {}, {
    fetchImpl:async url=>{
      assert.equal(new URL(url).hostname,'api.neynar.com');
      catalogueFetches++;
      return new Response(JSON.stringify(catalog),{
        status:200,headers:{'content-type':'application/json'}
      });
    },
    retrieveImage:async (url,options)=>{
      seen.push({url,options});
      return {status:200,body:Buffer.from([137,80,78,71]),contentType:'image/png'};
    },
    farcasterBroker:{},
  });
  return {gateway,seen,get catalogueFetches(){return catalogueFetches}};
}
async function withServer(run) {
  const subject=fixture();
  await new Promise((resolve,reject)=>{
    subject.gateway.once('error',reject);
    subject.gateway.listen(0,'127.0.0.1',resolve);
  });
  const base='http://127.0.0.1:'+subject.gateway.address().port;
  try { await run(subject,base); }
  finally { await new Promise(resolve=>subject.gateway.close(resolve)); }
}
test('catalogue extraction allows only declared manifest icon URLs',()=>{
  assert.deepEqual([...miniAppIconUrls(catalog)],[icon]);
  assert.deepEqual([...miniAppIconUrls({frames:[{manifest:{miniapp:{
    icon_url:'http://localhost/admin'}}}]})],[]);
});
test('Mini App icons load through bounded proxy; unrelated URLs remain blocked',async()=>{
  await withServer(async (state,base)=>{
    const catalogReply=await fetch(base+'/v1/browser/miniapps?kind=catalog');
    assert.equal(catalogReply.status,200);
    const allowed=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(icon));
    assert.equal(allowed.status,200);
    assert.equal(state.seen.length,1);
    assert.ok(state.seen[0].options.allowedHosts.includes('hypria.app'));
    for(const bad of ['https://hypria.app/secret.png','https://evil.example/logo.png',
                     'https://localhost/icon.png']) {
      const denied=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(bad));
      assert.equal(denied.status,400,bad);
    }
    assert.equal(state.seen.length,1);
  });
});
test('Mini App icon direct request works after Gateway restart or CDN-cached catalogue',async()=>{
  await withServer(async (state,base)=>{
    const reply=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(icon));
    assert.equal(reply.status,200);
    assert.equal(state.catalogueFetches,1);
    assert.equal(state.seen.length,1);
  });
});

test('browser image upload preflight is restricted to first-party GenyCaster origin',async()=>{
  await withServer(async (_state,base)=>{
    for(const pathname of ['/v1/uploads','/v1/uploads/complete','/v1/uploads/ticket_123/parts/1']){
      const ok=await fetch(base+pathname,{method:'OPTIONS',headers:{
        origin:'https://caster.genyleap.com',
        'access-control-request-method':'POST',
        'access-control-request-headers':'content-type,x-genycaster-signature'
      }});
      assert.equal(ok.status,204);
      assert.equal(ok.headers.get('access-control-allow-origin'),'https://caster.genyleap.com');
      assert.match(ok.headers.get('access-control-allow-headers'),/x-genycaster-signature/);
      const blocked=await fetch(base+pathname,{method:'OPTIONS',headers:{
        origin:'https://attacker.example'
      }});
      assert.equal(blocked.status,403);
      assert.equal(blocked.headers.get('access-control-allow-origin'),null);
    }
  });
});
