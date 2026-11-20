import assert from 'node:assert/strict';
import test from 'node:test';
import {createHash,webcrypto} from 'node:crypto';
import {deriveFarcasterCustody,validateRecoveryWords} from '../../ui/wasm/farcaster_recovery.mjs';
import {blake3Digest20,hex} from '../../ui/wasm/farcaster_protocol.mjs';
import {createBrowserNativeRegistry,generateNativePhrase,parseNativeLookup,
  parseRegistrationChallenge,sha256,nativePowHash,nativeCommitment,
  mineNativePow,encodeNativeOnboard} from '../../ui/wasm/hypersnap_native_account.mjs';

const phrase=('abandon '.repeat(23)+'art').trim();
const browser={isSecureContext:true,crypto:globalThis.crypto||webcrypto};
const address='0x'+('a'.repeat(40));
const anchor='0x'+('f'.repeat(64));
const challenge={active:true,anchorBlockHash:anchor,anchorBlockHeight:142,
  anchorWindow:1024,firstNativeFid:'4294967296',minDifficultyBits:1,nextBlock:142};
const status=(code,body)=>new Response(JSON.stringify(body),{status:code,headers:{
  'content-type':'application/json'}});
const notFound={error:'Native registration lookup failed',
  error_detail:'status: NotFound, message: "native account not found"'};
const native=(fid,custody)=>({fid,custodyAddress:custody,
  blockNumber:155,message:{data:{}}});

test('sha256 uses the canonical digest for variable-length inputs and Pow exact prefix',()=>{
  for(let n of [0,1,32,55,56,63,64,65,78,128,150]){
    const bytes=Uint8Array.from({length:n},(_,i)=>i*37&255);
    assert.equal(hex(sha256(bytes)),createHash('sha256').update(bytes).digest('hex'));
  }
  const payload=Buffer.concat([Buffer.from('hyper.onboard.pow:'),
    Buffer.alloc(20,0xaa),Buffer.alloc(32,0xff),Buffer.alloc(8)]);
  assert.equal(hex(nativePowHash(address,anchor,0n)),
    createHash('sha256').update(payload).digest('hex'));
  assert.equal(hex(nativeCommitment('0x0000000000000001',1)),
    createHash('sha256').update(Buffer.concat([Buffer.from('hyper.onboard.pow.commitment:'),
      Buffer.from('0000000000000001','hex'),Buffer.from('00000001','hex')])).digest('hex'));
});
test('generator produces 24 canonical BIP39 words using a cryptographic source',async()=>{
  const words=await generateNativePhrase(browser);
  assert.equal(words.split(' ').length,24);
  await validateRecoveryWords(words);
  const custody=await deriveFarcasterCustody(words);
  assert.match(custody.address,/^0x[0-9a-f]{40}$/);
  custody.dispose();
});
test('exact u64 FIDs and custody binding: never accept unsafe float or unrelated identity',()=>{
  assert.deepEqual(parseNativeLookup(400,notFound,address),{state:'not-found'});
  assert.deepEqual(parseNativeLookup(400,{error:'unknown'},address),{state:'unavailable'});
  assert.equal(parseNativeLookup(200,native('18446744073709551615',address),address).fid,
    '18446744073709551615');
  assert.throws(()=>parseNativeLookup(200,native(9007199254740992,address),address));
  assert.throws(()=>parseNativeLookup(200,native('4294967296','0x'+'b'.repeat(40)),address));
  assert.throws(()=>parseNativeLookup(200,native('4294967295',address),address));
  assert.equal(parseRegistrationChallenge(challenge).bits,1);
  assert.throws(()=>parseRegistrationChallenge({...challenge,minDifficultyBits:0}));
  assert.throws(()=>parseRegistrationChallenge({...challenge,anchorWindow:0}));
});
test('bounded PoW finds canonical BE nonce and honors AbortSignal',async()=>{
  const proof=await mineNativePow({address,anchor,bits:8,maxAttempts:2000,
    chunkSize:17,yieldControl:async()=>{}});
  const digest=nativePowHash(address,anchor,proof);
  assert.equal(digest[0],0);
  const abort=new AbortController();abort.abort();
  await assert.rejects(mineNativePow({address,anchor,bits:12,maxAttempts:2000,
    signal:abort.signal}));
});
test('native onboarding canonical protobuf has native data type 160 and BLAKE3-20 digest',()=>{
  const unsigned=encodeNativeOnboard({address,
    signature:'0x'+'ab'.repeat(64)+'1b',anchorHeight:142,anchorHash:anchor,
    powNonce:'0x0000000000000000',bits:1,timestamp:123456});
  assert.equal(unsigned[0],10); // outer Message.data
  assert.ok(unsigned.length>175&&unsigned.length<700);
  const decode=(bytes)=>{
    const fields=new Map();let pos=0;
    const takeVarint=()=>{
      let value=0n,shift=0n;
      while(true){
        assert.ok(pos<bytes.length,'bounded protobuf varint');
        const byte=BigInt(bytes[pos++]);
        value|=(byte&127n)<<shift;
        if((byte&128n)===0n)return value;
        shift+=7n;assert.ok(shift<70n);
      }
    };
    while(pos<bytes.length){
      const tag=takeVarint(),number=Number(tag>>3n),wire=Number(tag&7n);
      if(wire===0){fields.set(number,takeVarint());continue;}
      assert.equal(wire,2,'only canonical protobuf wire types');
      const size=Number(takeVarint());
      assert.ok(size>=0&&pos+size<=bytes.length);
      fields.set(number,bytes.slice(pos,pos+size));pos+=size;
    }
    return fields;
  };
  const envelope=decode(unsigned);
  const data=envelope.get(1),hash=envelope.get(2);
  assert.equal(hash.length,20);
  assert.equal(hex(hash),hex(blake3Digest20(data)),
    'native envelope hash must cover the exact canonical MessageData');
  assert.deepEqual(envelope.get(7),data,
    'native envelope must duplicate the exact MessageData at field 7');
  const messageData=decode(data);
  assert.equal(messageData.get(1),160n);
  assert.equal(messageData.get(2),undefined,'native initial message has no FID');
  assert.equal(messageData.get(4),1n);
  assert.ok(messageData.has(1160));
  assert.throws(()=>encodeNativeOnboard({address,signature:'0x'+'ab'.repeat(65),
    anchorHeight:142,anchorHash:anchor,powNonce:'0x'+'00'.repeat(8),bits:1}));
});
test('verified HyperSnap import makes no writes and zero authority when registry rejects it',async()=>{
  const calls=[];
  const registry=createBrowserNativeRegistry({browser,
    fetchImpl:async(url,opts)=>{
      calls.push({url,method:opts.method,body:opts.body});
      assert.equal(opts.credentials,'omit');
      assert.equal(opts.redirect,'error');
      return status(400,notFound);
    }});
  await assert.rejects(registry.recover(phrase),/no HyperSnap FID/);
  assert.equal(calls.length,1);
  assert.equal(calls[0].method,'GET');
  assert.equal(calls[0].body,undefined);
  const addr=(await deriveFarcasterCustody(phrase)).address;
  assert.ok(calls[0].url.includes(addr));
});
test('registration requires explicit consent and never posts for existing FID',async()=>{
  const custody=await deriveFarcasterCustody(phrase);
  let posts=0;
  const registry=createBrowserNativeRegistry({browser,fetchImpl:async(url,opts)=>{
    if(opts.method==='POST')posts++;
    if(url.endsWith('/v1/nativeRegistration'))return status(200,challenge);
    return status(200,native('4294967380',custody.address));
  }});
  await assert.rejects(registry.register(phrase),/explicit submission consent/);
  await assert.rejects(registry.register(phrase,{confirmIrreversible:true}),/already has a HyperSnap FID/);
  assert.equal(posts,0);
  custody.dispose();
});
test('registration signs native EIP712, submits exactly once and confirms only on-chain FID',async()=>{
  const custody=await deriveFarcasterCustody(phrase);
  const calls=[];
  let submitted=false;
  const registry=createBrowserNativeRegistry({browser,
    delay:async()=>{},
    maxAttempts:500,
    fetchImpl:async(url,options)=>{
      calls.push({url,method:options.method});
      if(url.endsWith('/v1/nativeRegistration'))return status(200,challenge);
      if(url.includes('/v1/nativeAccount?'))return submitted
        ?status(200,native('4294967380',custody.address)):status(400,notFound);
      if(url.endsWith('/v1/submitMessage')){
        assert.equal(options.method,'POST');
        assert.equal(options.headers['content-type'],'application/octet-stream');
        assert.ok(options.body instanceof Uint8Array);
        assert.ok(options.body.length>175);
        submitted=true;
        return status(200,{ok:true});
      }
      throw new Error('unexpected URL');
    }});
  const result=await registry.register(phrase,{confirmIrreversible:true});
  assert.equal(result.fid,'4294967380');
  assert.equal(calls.filter(x=>x.method==='POST').length,1);
  custody.dispose();
});
test('no unsafe resend when native submission is ambiguous',async()=>{
  const custody=await deriveFarcasterCustody(phrase);
  let submits=0;
  const registry=createBrowserNativeRegistry({browser,maxAttempts:500,
    delay:async()=>{},fetchImpl:async(url,options)=>{
      if(url.endsWith('/v1/nativeRegistration'))return status(200,challenge);
      if(url.includes('/v1/nativeAccount?'))return status(400,notFound);
      if(url.endsWith('/v1/submitMessage')){submits++;throw new Error('network interrupted after POST');}
    }});
  await assert.rejects(async()=>{
    try{await registry.register(phrase,{confirmIrreversible:true});}
    catch(error){assert.equal(error.outcomeUnknown,true);throw error;}
  },/network interrupted/);
  assert.equal(submits,1);
  custody.dispose();
});
