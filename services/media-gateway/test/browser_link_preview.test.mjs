import assert from 'node:assert/strict';
import test from 'node:test';
import {EventEmitter} from 'node:events';
import {PassThrough} from 'node:stream';
import {extractOpenGraph,publicLinkPreview,safePreviewPage} from '../browser_link_preview.mjs';

const publicDns=async()=>[{address:'8.8.8.8',family:4}];
const sample='<html><head><title>Old title</title><meta property="og:title" content="Account &amp; security"><meta name="description" content="A real account page for Genyleap"><meta property="og:site_name" content="Genyleap"></head></html>';
function requesterWith(body,headers={'content-type':'text/html; charset=UTF-8'},status=200) {
  return (_url,_options,callback)=>{
    const req=new EventEmitter();
    req.end=()=>setImmediate(()=>{
      const response=new PassThrough();
      response.statusCode=status;response.headers=headers;
      callback(response);response.end(body);
    });
    req.destroy=()=>req.emit('error',new Error('destroyed'));
    return req;
  };
}
test('public URL policy rejects internal hosts, credentials and non-HTTPS',()=>{
  for(const url of ['http://genyleap.com','https://localhost/',
    'https://127.0.0.1/private','https://[::1]/',
    'https://username:password@genyleap.com','https://genyleap.com:8080',
    'https://genyleap.com/.well-known/private','https://genyleap.com/api/private/session'])
    assert.equal(safePreviewPage(url),null,url);
  assert.equal(safePreviewPage('https://genyleap.com/account').hostname,'genyleap.com');
});
test('extracts actual metadata, decodes HTML entities, and rejects unsafe images',()=>{
  const result=extractOpenGraph(sample,new URL('https://genyleap.com/account'));
  assert.equal(result.title,'Account & security');
  assert.equal(result.summary,'A real account page for Genyleap');
  assert.equal(result.siteName,'Genyleap');
  assert.equal(result.previewImage,'');
  assert.equal(result.verifiedImageUrl,'');
  const safe=extractOpenGraph('<meta property="og:image" content="https://assets.geckoterminal.com/public-logo.png"><title>Token</title>',new URL('https://example.org'));
  assert.ok(safe.previewImage.startsWith('image://remote/'));
  const separate=extractOpenGraph('<meta property="og:image" content="https://cdn.example.org/post.jpg"><title>Page</title>',new URL('https://example.org'));
  assert.equal(separate.verifiedImageUrl,'https://cdn.example.org/post.jpg');
  assert.ok(separate.previewImage.startsWith('image://remote/'));
  const unsafe=extractOpenGraph('<meta property="og:image" content="http://127.0.0.1/logo.png"><title>Private</title>',new URL('https://example.org'));
  assert.equal(unsafe.previewImage,'');
});
test('safe HTTPS fetch pins public DNS and never downloads arbitrary assets',async()=>{
  const calls=[];
  const result=await publicLinkPreview('https://genyleap.com/account',{
    lookupHost:publicDns,
    requester:(url,opts,callback)=>{calls.push({url:url.href,lookup:typeof opts.lookup});return requesterWith(sample)(url,opts,callback);},
  });
  assert.equal(result.status,200);
  assert.equal(result.data.title,'Account & security');
  assert.equal(calls.length,1);
  assert.equal(calls[0].lookup,'function');
  assert.equal(calls[0].url,'https://genyleap.com/account');
});
test('rejects DNS rebinding and oversized HTML before parsing',async()=>{
  let connected=false;
  const secret=await publicLinkPreview('https://genyleap.com/account',{
    lookupHost:async()=>[{address:'10.10.1.1',family:4}],
    requester:()=>{connected=true;throw Error('not public');},
  });
  assert.equal(secret.status,404);
  assert.equal(connected,false);
  const oversized=await publicLinkPreview('https://genyleap.com/account',{
    lookupHost:publicDns,requester:requesterWith('a'.repeat(194*1024)),
  });
  assert.equal(oversized.status,404);
});
test('cross-origin redirects are blocked',async()=>{
  const response=await publicLinkPreview('https://genyleap.com/account',{
    lookupHost:publicDns,
    requester:requesterWith('',{location:'https://example.com'},302),
  });
  assert.equal(response.status,404);
});

test('OpenSea canonical NFT asset resolves verified artwork even if its HTML blocks scrapers', async()=>{
  const {parseOpenSeaAsset,openSeaNftPreview}=await import('../browser_link_preview.mjs');
  const link='https://opensea.io/assets/ethereum/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/1';
  const parsed=parseOpenSeaAsset(new URL(link));
  assert.equal(parsed.apiUrl,'https://api.opensea.io/api/v2/chain/ethereum/contract/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/nfts/1');
  for(const url of [
    'https://opensea.io/assets/ethereum/0xabc/1',
    'https://opensea.io/assets/ethereum/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/abc',
    'https://evil.opensea.io/assets/ethereum/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/1',
    'https://opensea.io/assets/ethereum/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/1/extras',
  ])assert.equal(parseOpenSeaAsset(new URL(url)),null,url);
  const image='https://i2c.seadn.io/public-image.avif';
  const publicBody=JSON.stringify({nft:{name:'Ape #1',display_image_url:image,description:'Original onchain artwork'}});
  const calls=[];
  const result=await publicLinkPreview(link,{
    lookupHost:publicDns,
    requester:(url,options,callback)=>{
      calls.push({url:url.href,lookup:typeof options.lookup});
      assert.equal(url.hostname,'api.opensea.io');
      return requesterWith(publicBody,{'content-type':'application/json'})(url,options,callback);
    },
  });
  assert.equal(result.status,200);
  assert.equal(result.data.title,'Ape #1');
  assert.equal(result.data.siteName,'OpenSea');
  assert.equal(result.data.verifiedImageUrl,image);
  assert.equal(result.data.previewImage,'image://remote/'+Buffer.from(image).toString('base64url'));
  assert.equal(calls.length,1);
  assert.equal(calls[0].lookup,'function');
  assert.equal(openSeaNftPreview({nft:{name:'Bad',image_url:'http://127.0.0.1/private'}},new URL(link)),null);
});
test('OpenSea API fallback refuses private DNS, oversized data and arbitrary API response content',async()=>{
  const link='https://opensea.io/assets/base/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/42';
  let apiCalls=0;
  const result=await publicLinkPreview(link,{
    lookupHost:publicDns,
    requester:(url,options,callback)=>{
      apiCalls++;
      if(url.hostname==='api.opensea.io')
        return requesterWith('{not json}',{'content-type':'application/json'})(url,options,callback);
      return requesterWith(sample)(url,options,callback);
    },
  });
  assert.equal(result.status,200);
  assert.equal(result.data.title,'Account & security');
  assert.equal(apiCalls,2);
  const denied=await publicLinkPreview(link,{
    lookupHost:async()=>[{address:'192.168.1.15',family:4}],
    requester:()=>{throw Error('should never reach an internal IP');},
  });
  assert.equal(denied.status,404);
});
