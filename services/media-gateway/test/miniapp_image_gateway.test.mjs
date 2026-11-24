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
test('image compatibility retries negotiate JPEG without sharing artwork cache entries',async()=>{
  await withServer(async (state,base)=>{
    const url=base+'/v1/browser/images?url='+encodeURIComponent(icon);
    const ordinary=await fetch(url,{headers:{accept:'image/webp,image/png,image/jpeg'}});
    assert.equal(ordinary.status,200);
    assert.equal(ordinary.headers.get('vary'),'Accept');
    const jpeg=await fetch(url,{headers:{accept:'image/jpeg,image/png;q=0.95'}});
    assert.equal(jpeg.status,200);
    assert.equal(jpeg.headers.get('vary'),'Accept');
    assert.equal(state.seen.length,2);
    assert.equal(state.seen[0].options.preferJpeg,false);
    assert.equal(state.seen[1].options.preferJpeg,true);
    assert.equal((await fetch(url,{headers:{accept:'image/webp'}})).status,200);
    assert.equal(state.seen.length,2,'default art remains cached separately');
    assert.equal((await fetch(url,{headers:{accept:'image/jpeg'}})).status,200);
    assert.equal(state.seen.length,2,'JPEG art remains cached separately');
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
        origin:'https://genycaster.xyz',
        'access-control-request-method':'POST',
        'access-control-request-headers':'content-type,x-genycaster-signature'
      }});
      assert.equal(ok.status,204);
      assert.equal(ok.headers.get('access-control-allow-origin'),'https://genycaster.xyz');
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

test('catalogue cursor is relayed only as a bounded provider cursor',async()=>{
  const visited=[];
  const secondCursor='eyJ0cmVuZGluZ19yYW5rIjoyMH0%3D';
  const gateway=createMediaGateway(config,{},{
    fetchImpl:async url=>{
      visited.push(new URL(url));
      return new Response(JSON.stringify({
        frames:[{frames_url:'https://demo.example/miniapp',manifest:{miniapp:{
          name:'Sample Mini App',home_url:'https://demo.example/miniapp',
          icon_url:'https://demo.example/icon.png'}}}],
        next:{cursor:visited.length===1?secondCursor:null}
      }),{status:200,headers:{'content-type':'application/json'}});
    },farcasterBroker:{},
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  try{
    const base='http://127.0.0.1:'+gateway.address().port+'/v1/browser/miniapps';
    const first=await (await fetch(base+'?kind=catalog')).json();
    assert.equal(first.next.cursor,secondCursor);
    const page=await fetch(base+'?kind=catalog&cursor='+encodeURIComponent(secondCursor));
    assert.equal(page.status,200);
    assert.equal(visited[1].searchParams.get('cursor'),secondCursor);
    assert.equal(visited[1].searchParams.get('limit'),'20');
    for(const invalid of ['cursor=https%3A%2F%2Fevil.example','cursor=%0A','cursor='+'a'.repeat(481),
                            'kind=search&q=test&cursor=abc','kind=catalog&url=https%3A%2F%2Fevil.example']){
      const denied=await fetch(base+'?'+invalid);
      assert.equal(denied.status,400,invalid);
    }
  }finally{await new Promise(resolve=>gateway.close(resolve));}
});

test('exact verified Mini App images have a separate bounded gallery budget',async()=>{
  const strict={...config,rateLimit:1};
  const icons=Array.from({length:5},(_,index)=>
    'https://gallery-images.example.org/icon'+index+'.png');
  const gateway=createMediaGateway(strict,{},{
    fetchImpl:async()=>new Response(JSON.stringify({frames:icons.map(icon=>({
      manifest:{miniapp:{name:'App',home_url:'https://apps.example.org/',icon_url:icon}}
    }))}),{status:200,headers:{'content-type':'application/json'}}),
    retrieveImage:async()=>({
      status:200,body:Buffer.from([137,80,78,71]),contentType:'image/png'
    }),farcasterBroker:{}
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  try{
    const base='http://127.0.0.1:'+gateway.address().port;
    assert.equal((await fetch(base+'/v1/browser/miniapps?kind=catalog')).status,200);
    for(const icon of icons){
      const result=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(icon));
      assert.equal(result.status,200,icon);
    }
    const unrelated=await fetch(base+'/v1/browser/images?url='+
      encodeURIComponent('https://gallery-images.example.org/private.png'));
    assert.notEqual(unrelated.status,200);
  }finally{await new Promise(resolve=>gateway.close(resolve));}
});

test('Qt search limit=20 is accepted while malformed or oversized limits are rejected',async()=>{
 await withServer(async(_state,base)=>{
  const accepted=await fetch(base+'/v1/browser/miniapps?kind=search&q=genytask&limit=20');
  assert.equal(accepted.status,200);
  for(const suffix of ['limit=21','limit=-1','limit=2000','limit=two','limit=0']){
   const result=await fetch(base+'/v1/browser/miniapps?kind=search&q=genytask&'+suffix);
   assert.equal(result.status,400,suffix);
  }
 });
});

test('saved Mini App artwork is validated against its pinned manifest, not the first catalogue page',async()=>{
 const origin='https://tasks.miniapps.genyleap.com';
 const icon=origin+'/assets/logo.png';
 const cover=origin+'/assets/splash.jpg?v=1.2';
 const unrelated=origin+'/private.png';
 let manifestReads=0;
 let imageReads=0;
 const gateway=createMediaGateway(config,{},{
  fetchImpl:async()=>new Response(JSON.stringify({frames:[]}),{
   status:200,headers:{'content-type':'application/json'}}),
  resolveMiniAppPreview:async(url)=>{
   assert.equal(url,origin);
   manifestReads++;
   return {status:200,data:{
    resolved:true,verifiedImageUrls:[icon,cover],
    title:'GenyTask',domain:'tasks.miniapps.genyleap.com'
   }};
  },
  retrieveImage:async()=>{
   imageReads++;
   return {status:200,body:Buffer.from([137,80,78,71]),contentType:'image/png'};
  },farcasterBroker:{}
 });
 await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
 try{
  const base='http://127.0.0.1:'+gateway.address().port;
  for(const artwork of [icon,cover]){
   const query='url='+encodeURIComponent(artwork)+'&miniapp='+encodeURIComponent(origin);
   assert.equal((await fetch(base+'/v1/browser/images?'+query)).status,200);
  }
  assert.equal(manifestReads,1,'verified Mini App manifest should be cached');
  assert.equal(imageReads,2);
  assert.equal((await fetch(base+'/v1/browser/images?url='+encodeURIComponent(unrelated)+
    '&miniapp='+encodeURIComponent(origin))).status,400);
  assert.equal(imageReads,2,'arbitrary same-domain images must not be proxied');
  const invalid=await fetch(base+'/v1/browser/images?url='+encodeURIComponent(icon)+
    '&miniapp='+encodeURIComponent('https://localhost'));
  assert.equal(invalid.status,400);
  const preview=await fetch(base+'/v1/browser/miniapp-preview?url='+encodeURIComponent(origin));
  assert.equal(preview.status,200,'artwork verification must not corrupt the preview response cache');
 }finally{await new Promise(resolve=>gateway.close(resolve));}
});

test('verified per-cast Mini App image retries one transient 502, deduplicates concurrent readers and caches success',async()=>{
  const appUrl='https://gift.example.org/share/123';
  const poster='https://image-cdn.example.org/gift/123.png';
  let upstreamCalls=0;
  const png=Buffer.from([137,80,78,71,13,10,26,10,0,0,0,13]);
  const gateway=createMediaGateway(config,{},{
    farcasterBroker:{},
    resolveMiniAppPreview:async()=>({status:200,data:{
      resolved:true,providerKind:'miniapp',domain:'gift.example.org',
      title:'Example Gift',previewImage:'image://remote/'+poster,
      verifiedImageUrls:[poster],
    }}),
    retrieveImage:async()=>{
      ++upstreamCalls;
      await new Promise(resolve=>setTimeout(resolve,18));
      return upstreamCalls===1 ? {status:502,error:'image_network_failed'}
        : {status:200,body:png,contentType:'image/png'};
    },
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+gateway.address().port;
  try {
    const endpoint=base+'/v1/browser/images?url='+encodeURIComponent(poster);
    const denied=await fetch(endpoint);
    assert.equal(denied.status,400,'a successful cache must not bypass initial URL verification');
    const granted=await fetch(base+'/v1/browser/miniapp-preview?url='+encodeURIComponent(appUrl));
    assert.equal(granted.status,200);
    const responses=await Promise.all(Array.from({length:3},()=>fetch(endpoint)));
    for (const response of responses){
      assert.equal(response.status,200);
      assert.equal(response.headers.get('content-type'),'image/png');
      assert.deepEqual(Buffer.from(await response.arrayBuffer()),png);
    }
    assert.equal(upstreamCalls,2,'one failed upstream request and exactly one bounded retry');
    const warm=await fetch(endpoint);
    assert.equal(warm.status,200);
    assert.equal(upstreamCalls,2,'warm Mini App card must not redownload its poster');
    const unrelated=await fetch(base+'/v1/browser/images?url='+
      encodeURIComponent('https://image-cdn.example.org/private.png'));
    assert.equal(unrelated.status,400,'the lease is URL-exact, not host-wide');
  } finally { await new Promise(resolve=>gateway.close(resolve)); }
});

test('unsafe and invalid Mini App poster results are never retried or cached',async()=>{
  const poster='https://assets.example.org/poster.png';
  let calls=0;
  const gateway=createMediaGateway(config,{},{
    farcasterBroker:{},
    resolveMiniAppPreview:async()=>({status:200,data:{
      resolved:true,verifiedImageUrls:[poster],providerKind:'miniapp',
    }}),
    retrieveImage:async()=>{++calls;return {status:415,error:'image_invalid_payload'};},
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  const base='http://127.0.0.1:'+gateway.address().port;
  try {
    assert.equal((await fetch(base+'/v1/browser/miniapp-preview?url='+
      encodeURIComponent('https://gift.example.org/'))).status,200);
    const endpoint=base+'/v1/browser/images?url='+encodeURIComponent(poster);
    assert.equal((await fetch(endpoint)).status,415);
    assert.equal((await fetch(endpoint)).status,415);
    assert.equal(calls,2,'invalid image bytes are not stored in cache or retried');
  }finally{await new Promise(resolve=>gateway.close(resolve));}
});

test('transient missing Mini App manifest is cached for seconds, never a full two minutes',async()=>{
  let clock=100_000;
  let calls=0;
  const gateway=createMediaGateway(config,{},{
    now:()=>clock,farCastBroker:{},farcasterBroker:{},
    resolveMiniAppPreview:async()=>{
      ++calls;
      return calls===1?{status:404,error:'miniapp_manifest_unavailable'}
        :{status:200,data:{resolved:true,providerKind:'miniapp',
          domain:'gift.example.org',title:'Gift',verifiedImageUrls:[]}};
    },
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  const endpoint='http://127.0.0.1:'+gateway.address().port+
    '/v1/browser/miniapp-preview?url='+encodeURIComponent('https://gift.example.org/one');
  try {
    assert.equal((await fetch(endpoint)).status,404);
    assert.equal((await fetch(endpoint)).status,404);
    assert.equal(calls,1);
    clock+=5_001;
    assert.equal((await fetch(endpoint)).status,200);
    assert.equal(calls,2,'a fresh valid app is not hidden behind a stale 404');
  }finally{await new Promise(resolve=>gateway.close(resolve));}
});

test('temporary cast-head failure does not cache a generic Mini App preview for two minutes',async()=>{
 let clock=10_000;
 let calls=0;
 const gateway=createMediaGateway(config,{},{
  now:()=>clock,farcasterBroker:{},
  resolveMiniAppPreview:async()=>{
   ++calls;
   return {status:200,data:{resolved:true,providerKind:'miniapp',
     domain:'gift.example.org',title:'Gift',
     castMetadataPending:calls===1,verifiedImageUrls:[]}};
  },
 });
 await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
 const endpoint='http://127.0.0.1:'+gateway.address().port+
   '/v1/browser/miniapp-preview?url='+encodeURIComponent('https://gift.example.org/g/123');
 try {
  assert.equal((await (await fetch(endpoint)).json()).castMetadataPending,true);
  assert.equal((await (await fetch(endpoint)).json()).castMetadataPending,true);
  assert.equal(calls,1);
  clock+=5_001;
  assert.equal((await (await fetch(endpoint)).json()).castMetadataPending,false);
  assert.equal(calls,2);
 }finally{await new Promise(resolve=>gateway.close(resolve));}
});

test('public feed previews and avatars use a separate bounded quota',async()=>{
  const gateway=createMediaGateway({...config,rateLimit:1,imageRateLimit:3},{},{
    limiter:{check:()=>({allowed:false,remaining:0})},
    retrieveImage:async()=>({status:200,body:Buffer.from([137,80,78,71]),contentType:'image/png'}),
    farcasterBroker:{}
  });
  await new Promise(resolve=>gateway.listen(0,'127.0.0.1',resolve));
  try {
    const base='http://127.0.0.1:'+gateway.address().port;
    assert.equal((await fetch(base+'/v1/browser/miniapps?kind=catalog')).status,429);
    for(let i=0;i<3;i++){
      const r=await fetch(base+'/v1/browser/images?url='+encodeURIComponent('https://imagedelivery.net/test/'+i+'/public'));
      assert.equal(r.status,200);
      assert.equal(r.headers.get('access-control-allow-origin'),'*');
    }
    const limited=await fetch(base+'/v1/browser/images?url='+encodeURIComponent('https://imagedelivery.net/test/4/public'));
    assert.equal(limited.status,429);
    assert.equal(limited.headers.get('retry-after'),'60');
  } finally {await new Promise(resolve=>gateway.close(resolve));}
});
