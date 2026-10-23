import assert from 'node:assert/strict';
import {EventEmitter} from 'node:events';
import {PassThrough} from 'node:stream';
import test from 'node:test';
import {publicMiniAppPreview, validMiniAppTarget} from '../browser_miniapp_preview.mjs';

const publicDns = async () => [{address:'8.8.8.8',family:4}];
function responder(payload,{status=200,headers={'content-type':'application/json'}}={}) {
  return (_url, options, callback) => {
    const req=new EventEmitter();
    req.end=()=>setImmediate(()=>{
      const response=new PassThrough();
      response.statusCode=status;
      response.headers=headers;
      callback(response);
      response.end(typeof payload==='string'?payload:JSON.stringify(payload));
    });
    req.destroy=()=>req.emit('error',new Error('destroyed'));
    return req;
  };
}
const goodHost = 'https://farmverse.farverse.games/scene';
const manifest={frame:{
  name:'Farmverse', homeUrl:'https://farmverse.farverse.games',
  imageUrl:'https://assets.foundly.dev/farmverse/embed.png',
  iconUrl:'https://assets.foundly.dev/farmverse/icon.png',
  buttonTitle:'Play',
}};
test('miniapp discovery only permits canonical public HTTPS hosts', async()=>{
  for (const value of ['http://farmverse.farverse.games','https://localhost/',
    'https://127.0.0.1/', 'https://192.168.0.1/', 'https://a.internal/',
    'https://user:pass@farmverse.farverse.games/',
    'https://farmverse.farverse.games:8443/', 'javascript:alert(1)']) {
    assert.equal(validMiniAppTarget(value),null,value);
  }
  assert.equal(validMiniAppTarget(goodHost)?.hostname,'farmverse.farverse.games');
});
test('verified Farcaster frame manifest creates a playable Mini App preview',async()=>{
  const calls=[];
  const result=await publicMiniAppPreview(goodHost,{
    lookupHost:publicDns,
    requester:(url,opts,callback)=>{
      calls.push({url:url.href,lookup:typeof opts.lookup,
        agent:opts.headers?.['user-agent']});
      return responder(manifest)(url,opts,callback);
    },
  });
  assert.equal(result.status,200);
  assert.equal(result.data.providerKind,'miniapp');
  assert.equal(result.data.castMetadataPending,true);
  assert.equal(result.data.title,'Farmverse');
  assert.equal(result.data.miniAppLaunchUrl,goodHost);
  assert.ok(result.data.previewImage.startsWith('image://remote/'));
  assert.deepEqual(result.data.verifiedImageUrls, [
    'https://assets.foundly.dev/farmverse/embed.png',
    'https://assets.foundly.dev/farmverse/icon.png'
  ]);
  assert.equal(calls.length,2);
  assert.equal(calls[0].url,'https://farmverse.farverse.games/.well-known/farcaster.json');
  assert.equal(calls[1].url,goodHost);
  assert.ok(calls[1].agent.includes('GenycasterPreview/1.0'));
  assert.equal(calls[0].lookup,'function');
  assert.ok(calls[0].agent.includes('GenycasterPreview/1.0'));
});
test('legacy frame splash artwork and animated GIF posters are recognized',async()=>{
  const gif=await publicMiniAppPreview('https://atgif.com/', {
    lookupHost:publicDns,
    requester:responder({miniapp:{name:'@gif',homeUrl:'https://atgif.com',
      imageUrl:'https://atgif.com/api/opengraph-image',
      splashImageUrl:'https://atgif.com/logo_full.gif',
      iconUrl:'https://atgif.com/favicon.webp'}}),
  });
  assert.equal(gif.status,200);
  assert.ok(gif.data.previewImage.startsWith('image://remote/'));
  assert.equal(gif.data.animatedPoster,'https://atgif.com/logo_full.gif');
  assert.ok(gif.data.verifiedImageUrls.includes('https://atgif.com/favicon.webp'));
  const legacy=await publicMiniAppPreview('https://universal-caster-income.vercel.app/', {
    lookupHost:publicDns,
    requester:responder({frame:{name:'UCI',homeUrl:'https://universal-caster-income.vercel.app',
      ogImageUrl:'https://pub-2fb244e19fde48759a9cc4dfb18ababf.r2.dev/cover.png'}}),
  });
  assert.equal(legacy.status,200);
  assert.ok(legacy.data.previewImage.startsWith('image://remote/'));
});
test('brand-new Mini App domains work without a source code allowlist entry',async()=>{
  const result=await publicMiniAppPreview('https://fresh-miniproject.example.org/game',{
    lookupHost:publicDns,
    requester:responder({miniapp:{name:'Fresh app',homeUrl:'https://fresh-miniproject.example.org',
      imageUrl:'https://independent-assets.example.net/poster.webp',
      iconUrl:'https://unseen-assets.example.com/icon.png'}}),
  });
  assert.equal(result.status,200);
  assert.equal(result.data.title,'Fresh app');
  assert.equal(result.data.verifiedImageUrls.length,2);
  assert.ok(result.data.previewImage.startsWith('image://remote/'));
});

test('a private DNS answer is rejected before connecting',async()=>{
  let connected=false;
  const result=await publicMiniAppPreview(goodHost,{
    lookupHost:async()=>[{address:'127.0.0.1',family:4}],
    requester:()=>{connected=true;throw Error('private DNS request');},
  });
  assert.equal(result.status,404);
  assert.equal(connected,false);
});
test('invalid manifests and origin mismatches cannot launch a frame',async()=>{
  for(const payload of [{frame:{...manifest.frame,homeUrl:'https://evil.test'}},{notAnApp:true}]) {
    const result=await publicMiniAppPreview(goodHost,{
      lookupHost:publicDns,requester:responder(payload),
    });
    assert.equal(result.status,404);
  }
});
test('manifest responses are byte bounded',async()=>{
  const result=await publicMiniAppPreview(goodHost,{
    lookupHost:publicDns,
    requester:responder(' '.repeat(129*1024)),
  });
  assert.equal(result.status,404);
});

test('Framedl per-result fc:miniapp metadata overrides generic manifest poster and launch action',async()=>{
  const shared='https://framedl.com/app/v2?app=1&id=9e673b45-f99f-47a0-aa09-68cf411c8440';
  const resultImage='https://framedl.com/app/v2/frames/image?id=9e673b45-f99f-47a0-aa09-68cf411c8440';
  const launch='https://framedl.com/app/v2?id=9e673b45-f99f-47a0-aa09-68cf411c8440';
  const pageEmbed={
    version:'next',imageUrl:resultImage,
    button:{title:'Play',action:{
      type:'launch_miniapp',name:'Framedl',url:launch,
      splashImageUrl:'https://framedl.com/splash-v2.png',
      splashBackgroundColor:'#f3f0f9',
    }},
  };
  const html='<html><head><meta name="fc:miniapp" content="'+
    JSON.stringify(pageEmbed).replaceAll('&','&amp;').replaceAll('"','&quot;')+
    '"/></head><body>Game</body></html>';
  const calls=[];
  const resolved=await publicMiniAppPreview(shared,{
    lookupHost:publicDns,
    requester:(url,options,callback)=>{
      calls.push(url.href);
      return responder(url.pathname==='/.well-known/farcaster.json'?
        {miniapp:{
          name:'Framedl',homeUrl:'https://framedl.com/app/v2',
          imageUrl:'https://framedl.com/init-v2.png',
          iconUrl:'https://framedl.com/icon-v2.png',buttonTitle:'Play',
        }}:html,{
        headers:{'content-type':url.pathname==='/.well-known/farcaster.json'
          ? 'application/json':'text/html; charset=utf-8'}
      })(url,options,callback);
    },
  });
  assert.equal(resolved.status,200);
  assert.equal(resolved.data.providerKind,'miniapp');
  assert.equal(resolved.data.miniAppLaunchUrl,launch);
  assert.equal(resolved.data.miniAppButtonTitle,'Play');
  assert.equal(resolved.data.castMetadataPending,false);
  assert.equal(resolved.data.previewImage,
    'image://remote/'+Buffer.from(resultImage).toString('base64url'));
  assert.ok(resolved.data.verifiedImageUrls.includes(resultImage));
  assert.deepEqual(calls,[
    'https://framedl.com/.well-known/farcaster.json',shared
  ]);
});
test('frame HTML cannot replace manifest app identity or launch an unrelated host',async()=>{
  const shared='https://framedl.com/app/v2?id=game';
  const fake={version:'next',imageUrl:'https://safe.example/frame.webp',
    button:{title:'Play',action:{type:'launch_miniapp',url:'https://other.example/app'}}};
  const result=await publicMiniAppPreview(shared,{
    lookupHost:publicDns,
    requester:(url,options,callback)=>{
      const payload=url.pathname==='/.well-known/farcaster.json'
        ? {miniapp:{name:'Framedl',homeUrl:'https://framedl.com/app/v2',
            imageUrl:'https://framedl.com/init-v2.png'}}
        : '<head><meta name="fc:miniapp" content="'+
          JSON.stringify(fake).replaceAll('"','&quot;')+'"/></head>';
      return responder(payload,{headers:{'content-type':typeof payload==='string'
        ? 'text/html':'application/json'}})(url,options,callback);
    }
  });
  assert.equal(result.status,200);
  assert.equal(result.data.miniAppLaunchUrl,shared);
  assert.equal(result.data.previewImage,
    'image://remote/'+Buffer.from('https://framedl.com/init-v2.png').toString('base64url'));
  assert.equal(result.data.verifiedImageUrls.includes('https://safe.example/frame.webp'),false);
});

test('large redirecting cast pages use only bounded head metadata, not their oversized hydrated body', async()=>{
  const share='https://celebration-hub.xyz/g/56796108-3921-4c53-bbb4-5599c5e7b7e7?ref=gift';
  const destination='https://celebration-hub.xyz/share-gift-nft/56796108-3921-4c53-bbb4-5599c5e7b7e7';
  const artwork='https://celebration-hub.xyz/api/og-fc?src=%2Fapi%2Fog-gift%2F56796108-3921-4c53-bbb4-5599c5e7b7e7%3Fv%3Dg3';
  const castMeta={version:'1',imageUrl:artwork,
    button:{title:'Open NFT Gift',action:{type:'launch_miniapp',
      name:'Celebration Hub',url:destination}}};
  const head='<html><head><meta name="fc:miniapp" content="'+
    JSON.stringify(castMeta).replaceAll('&','&amp;').replaceAll('"','&quot;')+
    '"/></head>';
  const html=head+'<body>'+ 'x'.repeat(350*1024) +'</body></html>';
  let bytesServed=0;
  const calls=[];
  const response=await publicMiniAppPreview(share,{
    lookupHost:publicDns,
    requester:(url,options,callback)=>{
      calls.push(url.href);
      if(url.pathname==='/.well-known/farcaster.json'){
        return responder({miniapp:{name:'Celebration Hub',
          homeUrl:'https://celebration-hub.xyz/farcaster',
          imageUrl:'https://celebration-hub.xyz/cover.png'}})(url,options,callback);
      }
      if(url.pathname==='/g/56796108-3921-4c53-bbb4-5599c5e7b7e7'){
        return responder('',{status:302,headers:{location:destination}})
          (url,options,callback);
      }
      const req=new EventEmitter();
      req.end=()=>setImmediate(()=>{
        const res=new PassThrough();
        res.statusCode=200;
        res.headers={'content-type':'text/html; charset=utf-8',
          'content-length':String(Buffer.byteLength(html))};
        callback(res);
        // The HTML is fed incrementally. The client must stop after </head>
        // rather than downloading or parsing the 350 KiB rendered body.
        for(let i=0;i<html.length;i+=4096){
          if(res.destroyed)break;
          const chunk=html.slice(i,i+4096);
          bytesServed+=Buffer.byteLength(chunk);
          res.write(chunk);
        }
        if(!res.destroyed)res.end();
      });
      req.destroy=()=>req.emit('error',new Error('destroyed'));
      return req;
    }
  });
  assert.equal(response.status,200);
  assert.equal(response.data.providerKind,'miniapp');
  assert.equal(response.data.miniAppButtonTitle,'Open NFT Gift');
  assert.equal(response.data.miniAppLaunchUrl,destination);
  assert.equal(response.data.previewImage,'image://remote/'+
    Buffer.from(artwork).toString('base64url'));
  assert.ok(response.data.verifiedImageUrls.includes(artwork));
  assert.ok(bytesServed<16*1024,'read only the bounded page head');
  assert.deepEqual(calls,[ 'https://celebration-hub.xyz/.well-known/farcaster.json',
    share,destination ]);
});

test('oversized head is rejected and falls back to manifest without harvesting body tags',async()=>{
  const share='https://example.org/game/123';
  const maliciousMeta={version:'next',imageUrl:'https://example.org/different.png',
    button:{title:'Open Wrong',action:{type:'launch_miniapp',
      url:'https://example.org/wrong'}}};
  const page='<head>'+' '.repeat(110*1024)+'</head><body><meta name="fc:miniapp" content="'+
    JSON.stringify(maliciousMeta).replaceAll('"','&quot;')+'"/></body>';
  const preview=await publicMiniAppPreview(share,{
    lookupHost:publicDns,
    requester:(url,opts,callback)=>responder(url.pathname==='/.well-known/farcaster.json'
      ? {miniapp:{name:'Example App',homeUrl:'https://example.org/',
         imageUrl:'https://example.org/manifest.png',buttonTitle:'Default'}}
      : page,{headers:{'content-type':url.pathname==='/.well-known/farcaster.json'
      ? 'application/json':'text/html'}})(url,opts,callback),
  });
  assert.equal(preview.status,200);
  assert.equal(preview.data.miniAppButtonTitle,'Default');
  assert.equal(preview.data.miniAppLaunchUrl,share);
  assert.equal(preview.data.previewImage,'image://remote/'+
    Buffer.from('https://example.org/manifest.png').toString('base64url'));
});

test('invalid preferred embed does not suppress valid fc:frame compatibility metadata',async()=>{
  const shared='https://gift.example.com/gift/123';
  const original={version:'next',imageUrl:'https://gift.example.com/hero.png',
    button:{title:'Open Gift',action:{type:'launch_frame',
      url:'https://gift.example.com/open/123'}}};
  const html='<head><meta name="fc:miniapp" content="'+
    JSON.stringify({...original,button:{title:'Unsafe',action:{type:'launch_miniapp',
      url:'https://unrelated.example.net/phishing'}}}).replaceAll('"','&quot;')+
    '"/><meta name="fc:frame" content="'+
    JSON.stringify(original).replaceAll('"','&quot;')+
    '"/></head>';
  const result=await publicMiniAppPreview(shared,{
    lookupHost:publicDns,
    requester:(url,opt,cb)=>responder(url.pathname==='/.well-known/farcaster.json'?
      {miniapp:{name:'Gift',homeUrl:'https://gift.example.com/',
        imageUrl:'https://gift.example.com/default.png'}}:html,{
        headers:{'content-type':url.pathname==='/.well-known/farcaster.json'
          ?'application/json':'text/html'}})(url,opt,cb)
  });
  assert.equal(result.status,200);
  assert.equal(result.data.miniAppButtonTitle,'Open Gift');
  assert.equal(result.data.miniAppLaunchUrl,'https://gift.example.com/open/123');
});
