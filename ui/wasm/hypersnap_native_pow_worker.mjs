/**
 * Public-only, cancellable HyperSnap shard-0 proof-of-work computation.
 *
 * Runs away from Qt/WebAssembly's UI/event loop. The caller passes only the
 * PUBLIC custody address, challenge hash, difficulty and work budget.
 * Recovery phrases, secret keys, signatures and wallets NEVER enter here.
 */
import {mineNativePow} from './hypersnap_native_account.mjs';

let active=false;
self.onmessage=async event=>{
  const task=event?.data;
  if(active||task?.type!=='start')return;
  active=true;
  try{
    const nonce=await mineNativePow({
      address:task.address,
      anchor:task.anchor,
      bits:task.bits,
      maxAttempts:task.maxAttempts,
      chunkSize:8192,
      // A dedicated worker may compute continuously. Aborting the UI job
      // terminates the worker rather than requiring a stale event callback.
      yieldControl:()=>Promise.resolve(),
      onProgress:(done,total)=>self.postMessage({type:'progress',done,total})
    });
    self.postMessage({type:'done',nonce});
  }catch{
    // Never send a private value, network response or exception stack to UI.
    self.postMessage({type:'error'});
  }finally{active=false;}
};
