/**
 * Browser-only, read-only Farcaster custody recovery.
 * No signer vault, Snapchain request, or publishing authorization is created.
 * Words and derived private material never leave this browser module.
 */
import {deriveFarcasterCustody} from './farcaster_recovery.mjs';
import {keccak256} from './browser_protocol_signer.mjs';
const registry='0x00000000fc6c5f01fc30151999387bb99a9f489b';
const rpcOrigin='https://mainnet.optimism.io';
const utf8=new TextEncoder();
const selector=name=>'0x'+Array.from(keccak256(utf8.encode(name)).slice(0,4),
    b=>b.toString(16).padStart(2,'0')).join('');
const idOfSelector=selector('idOf(address)');
const custodyOfSelector=selector('custodyOf(uint256)');
const validWord=/^0x[0-9a-f]{64}$/i;
function failure(code,message) {
    const error=new Error(message);
    error.code=code;
    return error;
}
function requireCurrent(signal) {
    if(signal?.aborted)throw failure('user-cancelled','Recovery cancelled');
}
export async function verifyFarcasterRecovery(
    phrase,{signal=null,fetchImpl=globalThis.fetch,origin=rpcOrigin,
            timeoutMs=16000,onVerifiedCustody=null}={}
) {
    if(typeof fetchImpl!=='function')throw failure('recovery-network','RPC fetch is unavailable');
    if(origin!==rpcOrigin)throw failure('recovery-verification','Unexpected Farcaster registry RPC');
    if(!Number.isInteger(timeoutMs)||timeoutMs<1||timeoutMs>120000)
        throw failure('recovery-verification','Invalid registry request deadline');
    requireCurrent(signal);
    let custody=null,step='derivation';
    const call=async(method,params)=>{
        requireCurrent(signal);
        const controller=new AbortController();
        const cancel=()=>controller.abort(signal?.reason);
        if(signal?.aborted)cancel();
        else signal?.addEventListener('abort',cancel,{once:true});
        const timer=setTimeout(()=>controller.abort(),timeoutMs);
        try {
            const response=await fetchImpl(origin,{
                method:'POST',credentials:'omit',mode:'cors',redirect:'error',
                headers:{'content-type':'application/json'},signal:controller.signal,
                body:JSON.stringify({jsonrpc:'2.0',id:1,method,params}),
            });
            if(!response.ok)throw failure('recovery-network',
                'Optimism RPC returned HTTP '+response.status);
            const body=await response.text();
            if(body.length>32768)throw failure('recovery-verification',
                'Oversized Optimism RPC result');
            let json;
            try{json=JSON.parse(body);}catch{
                throw failure('recovery-verification','Optimism RPC sent invalid JSON');
            }
            if(json?.error||typeof json?.result!=='string')
                throw failure('recovery-network','Optimism RPC did not return a result');
            requireCurrent(signal);
            return json.result;
        }catch(error) {
            requireCurrent(signal);
            if(error?.code)throw error;
            throw failure('recovery-network','Optimism RPC is unavailable or timed out');
        }finally{
            clearTimeout(timer);
            signal?.removeEventListener('abort',cancel);
        }
    };
    try {
        custody=await deriveFarcasterCustody(phrase);
        requireCurrent(signal);
        const address=custody.address.toLowerCase();
        if(!/^0x[0-9a-f]{40}$/.test(address))
            throw failure('recovery-derivation','Invalid derived custody address');
        step='ownership';
        if((await call('eth_chainId',[])).toLowerCase()!=='0xa')
            throw failure('recovery-verification','Recovery requires Optimism chain 10');
        const idValue=await call('eth_call',[{
            to:registry,data:idOfSelector+address.slice(2).padStart(64,'0'),
        },'latest']);
        if(!validWord.test(idValue))
            throw failure('recovery-verification','Invalid registry FID response');
        const fidValue=BigInt(idValue);
        if(fidValue===0n)throw failure('recovery-unregistered',
            'This derived custody address has no Farcaster FID on Optimism');
        if(fidValue>BigInt(Number.MAX_SAFE_INTEGER))
            throw failure('recovery-verification','Registry FID is outside supported range');
        const fid=fidValue.toString();
        const custodyValue=await call('eth_call',[{
            to:registry,data:custodyOfSelector+fidValue.toString(16).padStart(64,'0'),
        },'latest']);
        if(!validWord.test(custodyValue)
            ||'0x'+custodyValue.slice(-40).toLowerCase()!==address)
            throw failure('recovery-ownership',
                'Farcaster registry does not confirm the derived custody owner');
        requireCurrent(signal);
        // OpenProof is an independent optional identity binding. Its failure
        // must not erase a locally verified custody login.
        if(typeof onVerifiedCustody==='function'){
            try{await onVerifiedCustody(fid,custody);}catch(error){
                requireCurrent(signal);
            }
        }
        requireCurrent(signal);
        return fid;
    }catch(error){
        if(error?.code)throw error;
        throw failure(step==='derivation'?'recovery-derivation':'recovery-verification',
            step==='derivation'?'Could not derive Farcaster custody wallet':
                'Farcaster custody verification failed');
    }finally{custody?.dispose();}
}
