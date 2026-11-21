import test from 'node:test';
import assert from 'node:assert/strict';
import {
 mineNativePow,mineNativePowResponsive
} from '../../ui/wasm/hypersnap_native_account.mjs';

const address='0x1111111111111111111111111111111111111111';
const anchor='0x'+'ab'.repeat(32);

test('PoW Worker sees only public address and challenge; its nonce is verified',async()=>{
 const nonce=await mineNativePow({address,anchor,bits:7,maxAttempts:2000,
   yieldControl:()=>Promise.resolve()});
 const workerState={created:0,payloads:[],terminated:0};
 class Worker {
   constructor(url,opts){
     workerState.created++;
     assert.match(String(url),/hypersnap_native_pow_worker\.mjs$/);
     assert.equal(opts.type,'module');
   }
   postMessage(payload){
     workerState.payloads.push(payload);
     queueMicrotask(()=>this.onmessage({data:{type:'done',nonce}}));
   }
   terminate(){workerState.terminated++;}
 }
 const returned=await mineNativePowResponsive({
   browser:{Worker},address,anchor,bits:7,maxAttempts:2000
 });
 assert.equal(returned,nonce);
 assert.equal(workerState.created,1);
 assert.equal(workerState.terminated,1);
 assert.deepEqual(Object.keys(workerState.payloads[0]).sort(),
   ['address','anchor','bits','maxAttempts','type']);
 assert.equal(JSON.stringify(workerState.payloads).includes('phrase'),false);
});

test('Invalid proof from worker is never used as registration authorization',async()=>{
 let terminated=0;
 class Worker {
   postMessage(){queueMicrotask(()=>this.onmessage({data:{type:'done',nonce:'0xdeadbeef12345678'}}));}
   terminate(){terminated++;}
 }
 await assert.rejects(
   mineNativePowResponsive({browser:{Worker},address,anchor,bits:20,maxAttempts:300}),
   /Invalid HyperSnap proof result/);
 assert.equal(terminated,1);
});

test('Aborting public PoW terminates the worker without network submission',async()=>{
 const controller=new AbortController();
 let terminated=0;
 class Worker {
   postMessage(payload) {assert.equal(payload.type,'start');}
   terminate(){terminated++;}
 }
 const proof=mineNativePowResponsive({
   browser:{Worker},address,anchor,bits:22,maxAttempts:1000,
   signal:controller.signal
 });
 controller.abort();
 await assert.rejects(proof,/cancelled/);
 assert.equal(terminated,1);
});

test('Missing Worker falls back to short cooperative chunks',async()=>{
 let yields=0;
 await assert.rejects(
   mineNativePowResponsive({
     browser:{},address,anchor,bits:48,maxAttempts:600,
     onProgress:()=>{yields++;}
   }),
   /budget exhausted/);
 assert.ok(yields>=2, 'Proof must yield while the Qt UI is active');
});
