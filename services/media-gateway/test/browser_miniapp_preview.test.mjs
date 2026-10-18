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
      calls.push({url:url.href,lookup:typeof opts.lookup});
      return responder(manifest)(url,opts,callback);
    },
  });
  assert.equal(result.status,200);
  assert.equal(result.data.providerKind,'miniapp');
  assert.equal(result.data.title,'Farmverse');
  assert.equal(result.data.miniAppLaunchUrl,goodHost);
  assert.ok(result.data.previewImage.startsWith('image://remote/'));
  assert.deepEqual(result.data.verifiedImageUrls, [
    'https://assets.foundly.dev/farmverse/embed.png',
    'https://assets.foundly.dev/farmverse/icon.png'
  ]);
  assert.equal(calls.length,1);
  assert.equal(calls[0].url,'https://farmverse.farverse.games/.well-known/farcaster.json');
  assert.equal(calls[0].lookup,'function');
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
