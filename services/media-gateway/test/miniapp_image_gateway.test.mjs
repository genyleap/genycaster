import test from 'node:test';
import assert from 'node:assert/strict';
import { createMediaGateway, loadConfig, miniAppIconUrls, miniAppArtworkUrls } from '../gateway.mjs';

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

test('manifest artwork is whitelisted dynamically by exact URL, not by Mini App domain',async()=>{
  const poster='https://media.unknownnewminiapp.example.net/banner.webp';
  const iconUrl='https://cdn.unknownnewminiapp.example.org/icon.png';
  const seen=[];
  const gateway=createMediaGateway(config,{},{
    fetchImpl:async()=>new Response('not available',{status:503}),
    farcasterBroker:{},
    resolveMiniAppPreview:async()=>({
      status:200,data:{
        resolved:true,providerKind:'miniapp',title:'Fresh App',
        domain:'newminiapp.example.com',previewImage:'image://remote/'+Buffer.from(poster).toString('base64url'),
        favicon:'image://remote/'+Buffer.from(iconUrl).toString('base64url'),
        verifiedImageUrls:[poster,iconUrl],
      }
    }),
    retrieveImage:async(url,opts)=>{
      seen.push({url,opts});
      return {status:200,body:Buffer.from([137,80,78,71]),contentType:'image/png'};
    },
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+gateway.address().port;
  try {
    const premature=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(poster));
    assert.equal(premature.status,400);
    const preview=await fetch(base+'/v1/browser/miniapp-preview?url='+
      encodeURIComponent('https://newminiapp.example.com/game'));
    assert.equal(preview.status,200);
    const body=await preview.json();
    assert.equal(body.title,'Fresh App');
    assert.equal(Object.hasOwn(body,'verifiedImageUrls'),false);
    for(const image of [poster,iconUrl]){
      const response=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(image));
      assert.equal(response.status,200,image);
    }
    assert.equal(seen.length,2);
    for(const item of seen)assert.ok(item.opts.allowedHosts.includes(new URL(item.url).hostname));
    const unrelated=await fetch(base+'/v1/browser/images?url='+
      encodeURIComponent('https://media.unknownnewminiapp.example.net/private.png'));
    assert.equal(unrelated.status,400);
  } finally {await new Promise(resolve=>gateway.close(resolve));}
});


test('trusted catalogue grants exact Mini App hero covers, not unrelated images', async()=>{
  const hero='https://new-cover-cdn.example.net/art/featured.webp';
  const alternate='https://app-covers.example.org/gallery/one.png';
  const appIcon='https://myapp.example/icon.png';
  const doc={frames:[{frames_url:'https://myapp.example/miniapp',manifest:{
    miniapp:{home_url:'https://myapp.example/miniapp',icon_url:appIcon,
      hero_image_url:hero,screenshot_urls:[alternate]},
    frame:{hero_image_url:hero}
  }}]};
  assert.deepEqual([...miniAppIconUrls(doc)],[appIcon]);
  assert.deepEqual([...miniAppArtworkUrls(doc)].sort(),[alternate,appIcon,hero].sort());
  const seen=[];
  const gateway=createMediaGateway(config,{},{
    fetchImpl:async()=>new Response(JSON.stringify(doc),{status:200,
      headers:{'content-type':'application/json'}}),
    retrieveImage:async(url,options)=>{
      seen.push({url,options});
      return {status:200,body:Buffer.from([137,80,78,71]),contentType:'image/png'};
    },farcasterBroker:{}
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  try{
    const base='http://127.0.0.1:'+gateway.address().port;
    assert.equal((await fetch(base+'/v1/browser/miniapps?kind=catalog')).status,200);
    for(const image of [hero,alternate,appIcon]){
      const result=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(image));
      assert.equal(result.status,200,image);
    }
    assert.equal((await fetch(base+'/v1/browser/images?url='+
      encodeURIComponent('https://new-cover-cdn.example.net/admin/private.png'))).status,400);
    assert.equal(seen.length,3);
  }finally{await new Promise(resolve=>gateway.close(resolve));}
});
