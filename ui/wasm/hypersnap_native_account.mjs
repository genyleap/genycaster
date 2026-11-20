/**
 * HyperSnap native registration and recovery, matched against:
 * src/platform/custody_recovery.cpp::signNativeOnboard/solveNativeOnboardPow
 * src/protocol/message.cpp::encodeNativeOnboard
 * ui/app/identity/hypersnap_registration_transport.cpp
 *
 * The 24-word phrase and secp256k1 private key never leave this JS module
 * boundary, Qt/QML, HTTP requests or persistent storage. Explicit user
 * consent is required before sending a one-shot irreversible registration.
 */
import {deployment} from './deployment_config.mjs';
import {deriveFarcasterCustody, validateRecoveryWords} from './farcaster_recovery.mjs';
import {wordlist} from './vendor/bip39-english.mjs';
import {blake3Digest20,fromHex,hex,varint,farcasterTimestamp} from './farcaster_protocol.mjs';
import {keccak256} from './browser_protocol_signer.mjs';

const encoder=new TextEncoder();
const FIRST_NATIVE_FID=4294967296n;
const MAX_SAFE=BigInt(Number.MAX_SAFE_INTEGER);
const POW_PREFIX=encoder.encode('hyper.onboard.pow:');
const COMMITMENT_PREFIX=encoder.encode('hyper.onboard.pow.commitment:');
const K=Object.freeze([
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
]);
const H0=Object.freeze([0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
  0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]);
const right=(x,n)=>x>>>n;
const rotate=(x,n)=>(x>>>n)|(x<<(32-n));
const validAddress=a=>typeof a==='string'&&/^0x[0-9a-f]{40}$/i.test(a);
const fail=message=>{throw new Error('HyperSnap native registration: '+message);};
function requireSecure(browser) {
  if (browser?.isSecureContext!==true
      ||typeof browser.crypto?.subtle?.digest!=='function'
      ||typeof browser.crypto?.getRandomValues!=='function')
    fail('HTTPS and WebCrypto are required to handle recovery words');
}
function endpointUrl(base) {
  const u=new URL(base);
  if(u.protocol!=='https:'||u.username||u.password||u.hash||u.search
      ||(u.pathname!==''&&u.pathname!=='/')||u.port&&u.port!=='443')
    fail('native registry requires a configured HTTPS origin');
  return u.origin;
}
function concat(...pieces) {
  const result=new Uint8Array(pieces.reduce((n,p)=>n+p.length,0));let at=0;
  for(const part of pieces){result.set(part,at);at+=part.length;}
  return result;
}
function encodeField(tag,bytes) {
  return concat(varint(BigInt(tag)*8n+2n),varint(bytes.length),bytes);
}
function numberField(tag,value){
  const num=BigInt(value);
  return num===0n?new Uint8Array():concat(varint(BigInt(tag)*8n),varint(num));
}
function uint32BE(v){
  if(!Number.isInteger(v)||v<0||v>0xffffffff)fail('invalid 32-bit number');
  return Uint8Array.of((v>>>24)&255,(v>>>16)&255,(v>>>8)&255,v&255);
}
function uint64BE(v){
  v=BigInt(v);
  if(v<0n||v>0xffffffffffffffffn)fail('invalid 64-bit number');
  const b=new Uint8Array(8);
  for(let i=7;i>=0;i--){b[i]=Number(v&255n);v>>=8n;}
  return b;
}

/** Deterministic SHA-256 for bounded, 78-byte browser PoW input.
 * Synchronous per-hash work is deliberately small; minePow yields to the
 * event loop and checks cancellation between every bounded chunk.
 */
export function sha256(bytes) {
  if(!(bytes instanceof Uint8Array)||bytes.length>8192)fail('invalid SHA-256 input');
  const length=bytes.length,padded=(length+9+63)&~63;
  const input=new Uint8Array(padded);
  input.set(bytes);input[length]=128;
  const tail=new DataView(input.buffer);
  const bits=BigInt(length)*8n;
  tail.setUint32(padded-8,Number(bits>>32n));
  tail.setUint32(padded-4,Number(bits&0xffffffffn));
  const state=H0.slice(),w=new Uint32Array(64);
  for(let base=0;base<padded;base+=64) {
    for(let i=0;i<16;i++)w[i]=tail.getUint32(base+i*4);
    for(let i=16;i<64;i++){
      const a=w[i-15],b=w[i-2];
      const s0=rotate(a,7)^rotate(a,18)^right(a,3);
      const s1=rotate(b,17)^rotate(b,19)^right(b,10);
      w[i]=(w[i-16]+s0+w[i-7]+s1)>>>0;
    }
    let [a,b,c,d,e,f,g,h]=state;
    for(let i=0;i<64;i++){
      const S1=rotate(e,6)^rotate(e,11)^rotate(e,25);
      const ch=(e&f)^(~e&g);
      const t1=(h+S1+ch+K[i]+w[i])>>>0;
      const S0=rotate(a,2)^rotate(a,13)^rotate(a,22);
      const maj=(a&b)^(a&c)^(b&c);
      const t2=(S0+maj)>>>0;
      h=g;g=f;f=e;e=(d+t1)>>>0;
      d=c;c=b;b=a;a=(t1+t2)>>>0;
    }
    for(const [i,x]of [a,b,c,d,e,f,g,h].entries())state[i]=(state[i]+x)>>>0;
  }
  const output=new Uint8Array(32),out=new DataView(output.buffer);
  state.forEach((word,index)=>out.setUint32(index*4,word));
  return output;
}
function zeroBits(digest,count){
  for(let i=0;i<Math.floor(count/8);i++)if(digest[i]!==0)return false;
  const bits=count%8;
  return !bits || (digest[Math.floor(count/8)]&(255<<(8-bits)))===0;
}
export function nativePowHash(address,anchor,nonce) {
  return sha256(concat(POW_PREFIX,fromHex(address,20),fromHex(anchor,32),
    typeof nonce==='bigint'?uint64BE(nonce):fromHex(nonce,8)));
}
export function nativeCommitment(nonce,bits) {
  const raw=typeof nonce==='bigint'?uint64BE(nonce):fromHex(nonce,8);
  return sha256(concat(COMMITMENT_PREFIX,raw,uint32BE(bits)));
}
export async function mineNativePow({
  address,anchor,bits,maxAttempts=24<<20,signal,
  starting=0n,chunkSize=4096,onProgress=()=>{},
  yieldControl=()=>new Promise(resolve=>setTimeout(resolve,0))
}) {
  if(!validAddress(address)||typeof anchor!=='string'
    ||!/^0x[0-9a-f]{64}$/i.test(anchor)
    ||!Number.isInteger(bits)||bits<1||bits>256
    ||!Number.isSafeInteger(maxAttempts)||maxAttempts<1||maxAttempts>100_000_000
    ||!Number.isInteger(chunkSize)||chunkSize<1||chunkSize>65536)fail('invalid PoW challenge');
  const prefix=concat(POW_PREFIX,fromHex(address,20),fromHex(anchor,32));
  const input=new Uint8Array(prefix.length+8);
  input.set(prefix);prefix.fill(0);
  for(let attempted=0;attempted<maxAttempts;attempted+=chunkSize){
    if(signal?.aborted)throw new Error('Native registration cancelled');
    const end=Math.min(maxAttempts,attempted+chunkSize);
    for(let i=attempted;i<end;i++){
      const nonce=starting+BigInt(i);
      if(nonce>0xffffffffffffffffn)fail('PoW nonce overflow');
      input.set(uint64BE(nonce),input.length-8);
      if(zeroBits(sha256(input),bits))return '0x'+hex(input.subarray(input.length-8));
    }
    onProgress(end,maxAttempts);
    await yieldControl(); // never monopolize Qt's WebAssembly GUI thread
  }
  fail('PoW budget exhausted; refresh the node challenge, do not submit');
}
export async function generateNativePhrase(browser=globalThis) {
  requireSecure(browser);
  const entropy=new Uint8Array(32);
  browser.crypto.getRandomValues(entropy);
  const check=sha256(entropy)[0];
  const bit=i=>i<256?(entropy[i>>3]>>(7-(i%8)))&1
    :(check>>(7-((i-256)%8)))&1;
  const words=[];
  for(let i=0;i<24;i++) {
    let value=0;
    for(let j=0;j<11;j++)value=(value<<1)|bit(i*11+j);
    words.push(wordlist[value]);
  }
  entropy.fill(0);
  const phrase=words.join(' ');
  words.fill('');
  await validateRecoveryWords(phrase);
  return phrase;
}
export function encodeNativeOnboard({address,signature,anchorHeight,anchorHash,
  powNonce,bits,timestamp=farcasterTimestamp()}) {
  if(!validAddress(address)||!/^0x[0-9a-f]{130}$/i.test(signature)
      ||![27,28].includes(fromHex(signature,65)[64])
      ||!/^0x[0-9a-f]{64}$/i.test(anchorHash)
      ||!/^0x[0-9a-f]{16}$/i.test(powNonce)
      ||!Number.isSafeInteger(anchorHeight)||anchorHeight<1
      ||!Number.isInteger(bits)||bits<1||bits>256
      ||!Number.isSafeInteger(timestamp)||timestamp<1||timestamp>0xffffffff)
    fail('invalid native onboarding envelope');
  const pow=concat(encodeField(1,fromHex(powNonce,8)),numberField(2,bits));
  const body=concat(encodeField(1,fromHex(address,20)),
    encodeField(2,fromHex(signature,65)),
    numberField(3,anchorHeight),encodeField(4,fromHex(anchorHash,32)),
    encodeField(5,pow));
  const data=concat(numberField(1,160),numberField(3,timestamp),
    numberField(4,1),encodeField(1160,body));
  const digest=blake3Digest20(data);
  return concat(encodeField(1,data),encodeField(2,digest),
    numberField(3,1),encodeField(7,data));
}
const canonicalNumber=(n,min=1n)=>{
  if(typeof n==='number'&&Number.isSafeInteger(n)&&n>=0)n=String(n);
  if(typeof n!=='string'||!/^(0|[1-9][0-9]{0,19})$/.test(n))fail('untrusted native number');
  const value=BigInt(n);
  if(value<min||value>0xffffffffffffffffn)fail('invalid native number range');
  return value;
};
export function parseNativeLookup(status,body,expectedAddress) {
  if(!validAddress(expectedAddress))fail('invalid lookup custody');
  if(status===400 && body?.error==='Native registration lookup failed'
      && typeof body.error_detail==='string'
      && body.error_detail.startsWith('status: NotFound, message: "native account not found"'))
    return {state:'not-found'};
  if(status!==200) return {state:'unavailable'};
  if(!body||typeof body!=='object'||Array.isArray(body))
    fail('malformed native account response');
  if(typeof body.fid!=='string'||!/^0x[0-9a-f]{40}$/i.test(body.custodyAddress||'')
      ||body.custodyAddress.toLowerCase()!==expectedAddress.toLowerCase()
      ||!body.message||typeof body.message!=='object'||Array.isArray(body.message))
    fail('native registry custody identity mismatch');
  const fid=canonicalNumber(body.fid,FIRST_NATIVE_FID);
  const height=canonicalNumber(body.blockNumber,1n);
  return {state:'confirmed',fid:String(fid),address:expectedAddress.toLowerCase(),
    blockNumber:String(height)};
}
export function parseRegistrationChallenge(body) {
  if(body?.active!==true||!/^0x[0-9a-f]{64}$/i.test(body.anchorBlockHash||'')
      ||typeof body.firstNativeFid!=='string'
      ||!Number.isSafeInteger(body.anchorBlockHeight)||body.anchorBlockHeight<1
      ||!Number.isSafeInteger(body.nextBlock)||body.nextBlock<body.anchorBlockHeight
      ||!Number.isInteger(body.anchorWindow)||body.anchorWindow<1||body.anchorWindow>65536
      ||body.nextBlock-body.anchorBlockHeight>body.anchorWindow
      ||!Number.isInteger(body.minDifficultyBits)||body.minDifficultyBits<1
      ||body.minDifficultyBits>256)
    fail('native registry returned an invalid registration challenge');
  canonicalNumber(body.firstNativeFid,FIRST_NATIVE_FID);
  return {height:body.anchorBlockHeight,hash:body.anchorBlockHash.toLowerCase(),
    window:body.anchorWindow,nextBlock:body.nextBlock,bits:body.minDifficultyBits};
}
function nativeTypedData({address,challenge,nonce}) {
  return {
    domain:{name:'HypersnapNativeOnboard',version:'1',chainId:10},
    types:{
      EIP712Domain:[{name:'name',type:'string'},{name:'version',type:'string'},
        {name:'chainId',type:'uint256'}],
      HyperNativeOnboard:[{name:'custody',type:'address'},
        {name:'anchor_block_height',type:'uint64'},
        {name:'anchor_block_hash',type:'bytes32'},
        {name:'gate_commitment',type:'bytes32'}],
    },
    primaryType:'HyperNativeOnboard',
    message:{custody:address.toLowerCase(),anchor_block_height:String(challenge.height),
      anchor_block_hash:challenge.hash,
      gate_commitment:'0x'+hex(nativeCommitment(nonce,challenge.bits))}
  };
}
export function createBrowserNativeRegistry({
  browser=globalThis,fetchImpl=globalThis.fetch,
  base=deployment.hypersnapOrigin,
  delay=ms=>new Promise(resolve=>setTimeout(resolve,ms)),
  maxAttempts=24<<20
}={}) {
  requireSecure(browser);
  if(typeof fetchImpl!=='function')fail('network fetch is unavailable');
  const origin=endpointUrl(base);
  const getJson=async(url,signal)=>{
    const response=await fetchImpl(origin+url,{method:'GET',mode:'cors',
      credentials:'omit',redirect:'error',headers:{accept:'application/json'},
      signal:signal??AbortSignal.timeout(15000)});
    const text=await response.text();
    if(text.length>32768)fail('oversized native registry response');
    let body;
    try{body=JSON.parse(text);}catch{fail('native registry returned non-JSON data');}
    return {status:response.status,body};
  };
  const lookup=async(address,signal)=>{
    if(!validAddress(address))fail('invalid custody address');
    const {status,body}=await getJson('/v1/nativeAccount?custody='
      +encodeURIComponent(address.toLowerCase()),signal);
    return parseNativeLookup(status,body,address);
  };
  const challenge=async(signal)=>{
    const {status,body}=await getJson('/v1/nativeRegistration',signal);
    if(status!==200)fail('native registration node unavailable');
    return parseRegistrationChallenge(body);
  };
  const recover=async(phrase,{signal}={})=>{
    if(signal?.aborted)throw new Error('Recovery cancelled');
    const custody=await deriveFarcasterCustody(phrase);
    try{
      if(signal?.aborted)throw new Error('Recovery cancelled');
      const result=await lookup(custody.address,signal);
      if(result.state!=='confirmed')fail(result.state==='not-found'
        ?'no HyperSnap FID is registered to this recovery phrase'
        :'HyperSnap registry is unavailable; no session was created');
      if(signal?.aborted)throw new Error('Recovery cancelled');
      return result;
    }finally{custody.dispose();}
  };
  const register=async(phrase,{signal,confirmIrreversible=false,onProgress=()=>{}}={})=>{
    if(confirmIrreversible!==true)fail('explicit submission consent is required');
    const custody=await deriveFarcasterCustody(phrase);
    let submissionStarted=false;
    try{
      const address=custody.address;
      let gate=await challenge(signal);
      const before=await lookup(address,signal);
      if(before.state!=='not-found')fail(before.state==='confirmed'
        ?'this wallet already has a HyperSnap FID; use Existing account'
        :'cannot prove this wallet is unregistered');
      const nonce=await mineNativePow({address,anchor:gate.hash,bits:gate.bits,
        maxAttempts,signal,onProgress});
      if(signal?.aborted)throw new Error('Registration cancelled');
      const fresh=await challenge(signal);
      if(gate.hash!==fresh.hash||gate.height!==fresh.height||
          gate.bits!==fresh.bits||fresh.nextBlock-fresh.height>fresh.window)
        fail('node challenge changed while preparing PoW; retry from a fresh anchor');
      gate=fresh;
      // A fresh registry check is mandatory immediately before the write.
      const prior=await lookup(address,signal);
      if(prior.state!=='not-found')fail('custody state changed before submission');
      const signature=await custody.signTypedData(
        nativeTypedData({address,challenge:gate,nonce}));
      if(!/^0x[0-9a-f]{130}$/i.test(signature))fail('invalid custody proof');
      const envelope=encodeNativeOnboard({address,signature,
        anchorHeight:gate.height,anchorHash:gate.hash,powNonce:nonce,bits:gate.bits});
      if(envelope.byteLength>8192)fail('oversized native registration message');
      if(signal?.aborted)throw new Error('Registration cancelled');
      // An ambiguous POST must never be replayed. Check registry and wait.
      submissionStarted=true;
      const response=await fetchImpl(origin+'/v1/submitMessage',{
        method:'POST',mode:'cors',credentials:'omit',redirect:'error',
        headers:{'content-type':'application/octet-stream',accept:'application/json'},
        body:envelope,signal:AbortSignal.timeout(35000)});
      if([400,403,422].includes(response.status))
        fail('native registration was rejected (HTTP '+response.status+')');
      if(response.status!==200)onProgress(0,1,'outcome-unknown');
      for(let i=0;i<60;i++){
        if(signal?.aborted)throw new Error(
          'Registration submitted; check the native registry before retrying');
        const current=await lookup(address,signal);
        if(current.state==='confirmed')return current;
        if(current.state==='unavailable')onProgress(i,60,'checking');
        if(i!==59)await delay(2500);
      }
      fail('registration may be pending; check the native registry, never resend automatically');
    }catch(error){
      if(submissionStarted && !/registration was rejected/.test(String(error?.message)))
        error.outcomeUnknown=true;
      throw error;
    }finally{custody.dispose();}
  };
  return Object.freeze({lookup,challenge,recover,register});
}
