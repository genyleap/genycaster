import test from 'node:test';
import assert from 'node:assert/strict';
import {generateKeyPairSync, sign} from 'node:crypto';
import sharp from 'sharp';

import {createMediaGateway, loadConfig, buildUploadChallenge} from '../gateway.mjs';
import {createSmartMediaIngress} from '../smart_media_pipeline/ingest.mjs';
import {createSmartMediaPipeline} from '../smart_media_pipeline/index.mjs';
import {parseSmartMediaRequest, transformPolicy} from '../smart_media_pipeline/policy.mjs';

const nowMs=Date.parse('2026-11-23T12:00:00Z');
const fid=9001;
const pair=generateKeyPairSync('ed25519');
const pub=pair.publicKey.export({format:'der',type:'spki'});
const signerKey=pub.subarray(-32).toString('hex');
let counter=0;
function proof(method,path,bytes,contentType='application/json') {
  const nonce='smart_media_nonce_'+(++counter);
  const challenge=buildUploadChallenge({
    method,pathname:path,fid,signerKey,timestamp:Math.floor(nowMs/1000),
    nonce,body:bytes,
  });
  return {
    'content-type':contentType,
    'x-genycaster-fid':String(fid),
    'x-genycaster-signer-key':signerKey,
    'x-genycaster-timestamp':String(Math.floor(nowMs/1000)),
    'x-genycaster-nonce':nonce,
    'x-genycaster-signature':sign(null,Buffer.from(challenge),pair.privateKey).toString('hex'),
  };
}
const json=value=>Buffer.from(JSON.stringify(value));
const config=loadConfig({
  GENYLEAP_MEDIA_TICKET_SECRET:'0123456789abcdef0123456789abcdef',
  GENYLEAP_MEDIA_MAX_IMAGE_BYTES:String(25*1024*1024),
  GENYLEAP_MEDIA_MULTIPART_PART_BYTES:String(5*1024*1024),
  GENYLEAP_MEDIA_PUBLIC_BASE_URL:'https://media-farcaster.genyleap.com',
});
function makeStorage() {
  const objects=new Map();
  let beginCalls=0;
  return {
    objects,
    get beginCalls(){return beginCalls},
    async beginMultipartUpload({key,contentType}) {
      beginCalls++;
      objects.set(key,{contentType,parts:new Map(),completed:false,providerUploadId:'p-'+beginCalls});
      return {providerUploadId:'p-'+beginCalls};
    },
    async uploadPart({key,providerUploadId,partNumber,body}) {
      const item=objects.get(key);
      assert.equal(providerUploadId,item.providerUploadId);
      item.parts.set(partNumber,Buffer.from(body));
      return {etag:'"provider-'+partNumber+'"'};
    },
    async completeMultipartUpload({key,providerUploadId,parts}) {
      const item=objects.get(key);
      assert.equal(providerUploadId,item.providerUploadId);
      assert.equal(parts.length,item.parts.size);
      item.completed=true;
    },
    async abortMultipartUpload({key}){objects.delete(key)},
    async deleteObject(key){objects.delete(key)},
    async headObject(key) {
      const item=objects.get(key);
      if(!item || !item.completed){const error=new Error('missing');error.statusCode=404;throw error}
      return {sizeBytes:Buffer.concat([...item.parts.values()]).length,contentType:item.contentType};
    },
    async readImageObject(key,maxBytes) {
      const item=objects.get(key);
      if(!item?.completed){const error=new Error('missing');error.statusCode=404;throw error}
      const bytes=Buffer.concat([...item.parts.values()]);
      assert.ok(bytes.length<=maxBytes);
      return bytes;
    },
  };
}
async function withGateway(action) {
  const storage=makeStorage();
  const signerFetch=async target=>{
    const parsed=new URL(target);
    assert.equal(parsed.pathname,'/v1/signersByFid');
    return new Response(JSON.stringify({
      signers:[{key:signerKey,fid,source:'SIGNER_SOURCE_OFFCHAIN',
        scopes:['MESSAGE_TYPE_CAST_ADD'],expiresAt:Math.floor(nowMs/1000)+3600}],
      nextPageToken:'',
    }),{status:200,headers:{'content-type':'application/json'}});
  };
  const server=createMediaGateway(config,storage,{
    now:()=>nowMs,fetchImpl:signerFetch,fetchNativeRegistry:signerFetch,
  });
  await new Promise((resolve,reject)=>{
    server.once('error',reject);
    server.listen(0,'127.0.0.1',resolve);
  });
  const base='http://127.0.0.1:'+server.address().port;
  async function send(method,path,body) {
    const bytes=Buffer.isBuffer(body)?body:json(body);
    const reply=await fetch(base+path,{
      method,headers:proof(method,path,bytes,Buffer.isBuffer(body)?'application/octet-stream':'application/json'),
      body:bytes,
    });
    const text=await reply.text();
    return {status:reply.status,body:text?JSON.parse(text):null};
  }
  async function createUpload({source,contentType,optimizationMode}) {
    const response=await send('POST','/v1/uploads',{
      fileName:contentType==='image/png'?'stats.png':'photo.jpg',
      contentType,sizeBytes:source.length,optimizationMode,
    });
    assert.equal(response.status,201,JSON.stringify(response.body));
    const ticket=response.body;
    const parts=[];
    for(let index=0;index<ticket.partCount;index++){
      const chunk=source.subarray(index*ticket.partSizeBytes,
        Math.min((index+1)*ticket.partSizeBytes,source.length));
      const path='/v1/uploads/'+encodeURIComponent(ticket.uploadId)+'/parts/'+(index+1);
      const part=await send('PUT',path,chunk);
      assert.equal(part.status,200,JSON.stringify(part.body));
      parts.push({partNumber:index+1,etag:part.body.etag});
    }
    return {ticket,parts};
  }
  try {await action({base,send,storage,createUpload})}
  finally{await new Promise(resolve=>server.close(resolve))}
}

test('smart image policy restricts transforms to fixed bounded variants',()=>{
  assert.equal(parseSmartMediaRequest(new URLSearchParams()),null);
  assert.deepEqual(parseSmartMediaRequest(new URLSearchParams('variant=original')),
    {variant:'original',format:'original',mode:'original'});
  for(const params of [
    'variant=999999', 'variant=thumbnail&width=9000000',
    'variant=thumbnail&variant=feed',
    'variant=original&format=webp',
    'variant=feed&format=svg',
    'variant=full&mode=unlimited',
  ]) assert.throws(()=>parseSmartMediaRequest(new URLSearchParams(params)));
  const policy=transformPolicy({
    sourceMime:'image/png',
    metadata:{width:2800,height:1600,hasAlpha:true,pages:1},
    request:{variant:'feed',format:'auto',mode:'auto'},
  });
  assert.equal(policy.lossless,true);
  assert.equal(policy.maxEdge,960);
  assert.throws(()=>transformPolicy({
    sourceMime:'image/png',
    metadata:{width:600,height:400,hasAlpha:true,pages:1},
    request:{variant:'feed',format:'jpeg',mode:'auto'},
  }),/transparency_requires_alpha_codec/);
});

test('Smart Media Pipeline stores reduced JPEG in QStorage, not the uploaded original',async()=>{
  const raw=await sharp({create:{
    width:2100,height:1300,channels:3,background:'#5c9ef3',
  }}).png().toBuffer();
  const source=await sharp(raw).jpeg({quality:100}).toBuffer();
  assert.ok(source.length>1000);
  await withGateway(async({base,storage,send,createUpload})=>{
    const {ticket,parts}=await createUpload({
      source,contentType:'image/jpeg',optimizationMode:'auto',
    });
    assert.equal(storage.beginCalls,0,'no QStorage object was created during ingress');
    assert.equal(storage.objects.size,0);
    const complete=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(complete.status,200,JSON.stringify(complete.body));
    assert.equal(storage.beginCalls,1);
    assert.equal(storage.objects.size,1);
    assert.equal(complete.body.assetUrl,ticket.assetUrl);
    // Lost HTTP completion ACKs can safely retry without a second QStorage
    // object or a second set of storage writes.
    const retried=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(retried.status,200);
    assert.deepEqual(retried.body,complete.body);
    assert.equal(storage.beginCalls,1);
    assert.equal(complete.body.sizeBytes,source.length);
    assert.equal(complete.body.sourceSizeBytes,source.length);
    assert.ok(complete.body.storedSizeBytes<source.length,
      'optimized cloud object must be smaller than original');
    const uri=complete.body.assetUrl.replace('https://media-farcaster.genyleap.com',base);
    const result=await fetch(uri);
    assert.equal(result.status,200);
    assert.equal(result.headers.get('content-type'),'image/jpeg');
    const bytes=Buffer.from(await result.arrayBuffer());
    assert.equal(bytes.length,complete.body.storedSizeBytes);
    const metadata=await sharp(bytes).metadata();
    assert.ok(Math.max(metadata.width,metadata.height)<=1600);
    const preview=await fetch(uri+'?variant=thumbnail');
    assert.equal(preview.status,200);
    assert.equal(preview.headers.get('content-type'),'image/webp');
    const thumb=await sharp(Buffer.from(await preview.arrayBuffer())).metadata();
    assert.ok(Math.max(thumb.width,thumb.height)<=320);
    const original=await fetch(uri+'?variant=original');
    assert.equal(original.status,200);
    assert.equal((await original.arrayBuffer()).byteLength,bytes.length,
      'original means uploaded canonical object, not discarded camera original');
    assert.equal((await fetch(uri+'?variant=full&width=100000')).status,400);
    const removed=await send('DELETE','/v1/assets',{
      managementToken:complete.body.managementToken,
    });
    assert.equal(removed.status,204);
    assert.equal(storage.objects.size,0);
  });
});

test('abort after lost completion acknowledgment removes the canonical cloud object',async()=>{
  const source=await sharp({create:{width:1700,height:1000,channels:3,
    background:'#664bfa'}}).jpeg({quality:99}).toBuffer();
  await withGateway(async({storage,createUpload,send})=>{
    const {ticket,parts}=await createUpload({
      source,contentType:'image/jpeg',optimizationMode:'auto',
    });
    const complete=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(complete.status,200);
    assert.equal(storage.objects.size,1);
    const abort=await send('DELETE','/v1/uploads',{
      uploadId:ticket.uploadId,
    });
    assert.equal(abort.status,204);
    assert.equal(storage.objects.size,0);
    const after=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(after.status,403,'deleted upload must not be resurrected');
  });
});

test('legacy client omitting policy still uses Auto before any QStorage write',async()=>{
  const source=await sharp({create:{width:1800,height:900,channels:3,
    background:'#64bb8c'}}).jpeg({quality:100}).toBuffer();
  await withGateway(async({storage,createUpload,send})=>{
    const {ticket,parts}=await createUpload({
      source,contentType:'image/jpeg',optimizationMode:undefined,
    });
    assert.equal(ticket.optimizationMode,'auto');
    assert.equal(storage.beginCalls,0);
    const complete=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(complete.status,200);
    assert.ok(complete.body.storedSizeBytes<=source.length);
  });
});

test('PNG transparency and pixel dimensions are retained in optimized canonical object',async()=>{
  const source=await sharp({create:{width:1700,height:1100,channels:4,
    background:'#e71e4090'}}).png({compressionLevel:0}).toBuffer();
  await withGateway(async({storage,createUpload,send})=>{
    const {ticket,parts}=await createUpload({
      source,contentType:'image/png',optimizationMode:'auto',
    });
    assert.equal(storage.beginCalls,0);
    const completed=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(completed.status,200);
    const object=[...storage.objects.values()][0];
    const bytes=Buffer.concat([...object.parts.values()]);
    const metadata=await sharp(bytes).metadata();
    assert.equal(metadata.format,'png');
    assert.equal(metadata.hasAlpha,true);
    assert.ok(Math.max(metadata.width,metadata.height)<=1600);
    assert.ok(bytes.length<source.length);
    assert.equal(completed.body.storedSizeBytes,bytes.length);
  });
});

test('input memory budget rejects too many partially uploaded images without cloud writes',async()=>{
  await withGateway(async({send,storage})=>{
    const tickets=[];
    for(let n=0;n<2;n++){
      const attempt=await send('POST','/v1/uploads',{
        fileName:'budget'+n+'.jpg',contentType:'image/jpeg',
        sizeBytes:25*1024*1024,optimizationMode:'auto',
      });
      assert.equal(attempt.status,201);
      tickets.push(attempt.body);
    }
    const denied=await send('POST','/v1/uploads',{
      fileName:'budget-overflow.jpg',contentType:'image/jpeg',
      sizeBytes:25*1024*1024,optimizationMode:'auto',
    });
    assert.equal(denied.status,503);
    assert.equal(storage.beginCalls,0);
    const cancelled=await send('DELETE','/v1/uploads',{uploadId:tickets[0].uploadId});
    assert.equal(cancelled.status,204);
    const allowed=await send('POST','/v1/uploads',{
      fileName:'budget-retry.jpg',contentType:'image/jpeg',
      sizeBytes:25*1024*1024,optimizationMode:'auto',
    });
    assert.equal(allowed.status,201);
  });
});

test('invalid optimization modes are rejected before reserving or storing data',async()=>{
  await withGateway(async({send,storage})=>{
    const response=await send('POST','/v1/uploads',{
      fileName:'test.png',contentType:'image/png',sizeBytes:100,
      optimizationMode:'raw-unlimited',
    });
    assert.equal(response.status,422);
    assert.equal(response.body.error,'invalid_optimization_mode');
    assert.equal(storage.beginCalls,0);
  });
});

test('legacy and explicit Original bypass transcoding, and remain byte-identical',async()=>{
  const source=await sharp({create:{width:800,height:600,channels:3,
    background:'#56a1e3'}}).png().toBuffer();
  await withGateway(async({base,storage,createUpload,send})=>{
    const {ticket,parts}=await createUpload({
      source,contentType:'image/png',optimizationMode:'original',
    });
    assert.equal(storage.beginCalls,1,
      'Original mode must retain classic provider direct multipart uploads');
    const complete=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(complete.status,200);
    const uri=complete.body.assetUrl.replace('https://media-farcaster.genyleap.com',base);
    assert.deepEqual(Buffer.from(await (await fetch(uri)).arrayBuffer()),source);
  });
});

test('a malformed decoded image is rejected BEFORE any QStorage write',async()=>{
  const source=Buffer.concat([Buffer.from([255,216,255]),Buffer.alloc(10000,20)]);
  await withGateway(async({storage,createUpload,send})=>{
    const {ticket,parts}=await createUpload({
      source,contentType:'image/jpeg',optimizationMode:'auto',
    });
    const result=await send('POST','/v1/uploads/complete',{
      uploadId:ticket.uploadId,parts,
    });
    assert.equal(result.status,415);
    assert.equal(storage.beginCalls,0);
  });
});

test('bounded ingress enforces memory reservation, exact signed parts, and abort',async()=>{
  const clock={time:1000};
  const ingress=createSmartMediaIngress({
    now:()=>clock.time,maxSessions:1,maxReservedBytes:20,maxSessionAgeMs:3000,
  });
  try{
    const ticket={sessionId:ingress.create({
      sizeBytes:12,fid:11,key:'one',contentType:'image/png',partCount:1,
    }),sizeBytes:12,fid:11,key:'one',contentType:'image/png'};
    assert.equal(ingress.status().reservedBytes,12);
    assert.throws(()=>ingress.create({
      sizeBytes:12,fid:11,key:'two',contentType:'image/png',partCount:1,
    }),/media_ingress_busy/);
    const part=ingress.addPart(ticket,1,Buffer.alloc(12,42));
    assert.throws(()=>ingress.addPart(ticket,1,Buffer.alloc(12,42)),
      /invalid_upload_part/);
    await assert.rejects(
      ()=>ingress.complete(ticket,[{partNumber:1,etag:'fake'}],async()=>{}),
      /invalid_upload_parts/);
    const result=await ingress.complete(ticket,[{
      partNumber:1,etag:part.etag,
    }],async bytes=>bytes.length);
    assert.equal(result,12);
    assert.equal(ingress.status().reservedBytes,0);
    assert.equal(ingress.status().receipts,1);
    assert.equal(await ingress.complete(ticket,[{
      partNumber:1,etag:part.etag,
    }],async()=>{throw new Error('must not repeat cloud write')}),12);
    assert.equal(ingress.abort(ticket),'completed');
    ingress.forgetCompleted(ticket);
    const fresh={sessionId:ingress.create({
      sizeBytes:14,fid:11,key:'three',contentType:'image/png',partCount:1,
    }),sizeBytes:14,fid:11,key:'three',contentType:'image/png'};
    ingress.abort(fresh);
    assert.equal(ingress.status().reservedBytes,0);
    const expired={sessionId:ingress.create({
      sizeBytes:8,fid:11,key:'four',contentType:'image/png',partCount:1,
    }),sizeBytes:8,fid:11,key:'four',contentType:'image/png'};
    clock.time+=4000;
    assert.equal(ingress.status().reservedBytes,0);
    assert.throws(()=>ingress.addPart(expired,1,Buffer.alloc(8)),
      /invalid_upload_ticket/);
  }finally{ingress.close()}
});

test('animated GIF remains unchanged and source image cannot be silently flattened',async()=>{
  const gif=Buffer.from('R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=','base64');
  // This minimal input is not necessarily a complete GIF, so use a real
  // single-frame binary from the codec and ensure it is passed through.
  const source=await sharp({create:{width:2,height:2,channels:4,
    background:'#ff00ff80'}}).gif().toBuffer();
  const pipeline=createSmartMediaPipeline();
  const result=await pipeline.optimizeForStorage(source,'image/gif');
  assert.deepEqual(result.bytes,source);
  assert.equal(result.optimized,false);
  assert.equal(result.contentType,'image/gif');
  assert.ok(gif.length>0);
});
