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
    req.write=()=>{};
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
  // Official API + bounded ERC-721/ERC-1155 attempts + public HTML fallback.
  assert.equal(apiCalls,4);
  const denied=await publicLinkPreview(link,{
    lookupHost:async()=>[{address:'192.168.1.15',family:4}],
    requester:()=>{throw Error('should never reach an internal IP');},
  });
  assert.equal(denied.status,404);
});

test('canonical ERC-721 OpenSea card reads public tokenURI and verified IPFS artwork when OpenSea requires a key',async()=>{
  const {normalizeNftAssetUrl}=await import('../browser_link_preview.mjs');
  const link='https://opensea.io/assets/ethereum/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/1';
  const metadataCid='QmeSjSinHpPnmXmspMjwiXyN6zS4E9zccariGR3jxcaWtq';
  const imageCid='QmPbxeGcXhYQQNgsC6a36dDyYUcHgMLnGKnF8pVFmGsvqi';
  const uri='ipfs://'+metadataCid+'/1';
  const uriBytes=Buffer.from(uri);
  const abi='0x'+'20'.padStart(64,'0')+uriBytes.length.toString(16).padStart(64,'0')
    +uriBytes.toString('hex').padEnd(Math.ceil(uriBytes.length/32)*64,'0');
  const calls=[];
  const fakeRequester=(url,options,callback)=>{
    calls.push({host:url.hostname,method:options.method,lookup:typeof options.lookup});
    let text='',headers={'content-type':'application/json'},status=200;
    if(url.hostname==='api.opensea.io'){status=401;text='{"errors":["Missing an API Key"]}';}
    else if(url.hostname==='ethereum-rpc.publicnode.com'){
      assert.equal(options.method,'POST');
      text=JSON.stringify({jsonrpc:'2.0',id:1,result:abi});
    }else if(url.hostname==='gateway.pinata.cloud'){
      assert.match(url.pathname,/\/ipfs\//);
      text=JSON.stringify({image:'ipfs://'+imageCid,description:'Onchain art'});
    }else assert.fail('An unexpected external host was requested: '+url.hostname);
    const request=requesterWith(text,headers,status)(url,options,callback);
    request.write=()=>{};
    return request;
  };
  const result=await publicLinkPreview(link,{lookupHost:publicDns,requester:fakeRequester});
  assert.equal(result.status,200);
  assert.deepEqual(calls.map(x=>x.host),[
    'api.opensea.io','ethereum-rpc.publicnode.com','gateway.pinata.cloud',
  ]);
  assert.ok(calls.every(x=>x.lookup==='function'));
  const artwork='https://gateway.pinata.cloud/ipfs/'+imageCid;
  assert.equal(result.data.verifiedImageUrl,artwork);
  assert.equal(result.data.previewImage,'image://remote/'+Buffer.from(artwork).toString('base64url'));
  assert.equal(result.data.title,'#1');
  assert.equal(normalizeNftAssetUrl('ipfs://'+imageCid),artwork);
  for(const unsafe of ['ipfs://../private','ipfs://'+imageCid+'/../private',
    'http://127.0.0.1/private','https://localhost/private',
    'https://bad:pass@example.org/logo.svg','ftp://example.org/public.jpg']){
    assert.equal(normalizeNftAssetUrl(unsafe),'',unsafe);
  }
});
test('ERC-1155 token metadata fallback is constrained to read-only RPC methods and rejects private DNS',async()=>{
  const {publicOnchainNftPreview,parseOpenSeaAsset}=await import('../browser_link_preview.mjs');
  const link='https://opensea.io/assets/base/0xbc4ca0eda7647a8ab7c2061c2e118a18a936f13d/42';
  const asset=parseOpenSeaAsset(new URL(link));
  let connected=false;
  const denied=await publicOnchainNftPreview(asset,new URL(link),{
    lookupHost:async()=>[{address:'10.1.2.3',family:4}],
    requester:()=>{connected=true;throw Error('invalid destination');},
  });
  assert.equal(denied,null);
  assert.equal(connected,false);
});
