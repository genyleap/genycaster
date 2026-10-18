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
  const safe=extractOpenGraph('<meta property="og:image" content="https://assets.geckoterminal.com/public-logo.png"><title>Token</title>',new URL('https://example.org'));
  assert.ok(safe.previewImage.startsWith('image://remote/'));
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
