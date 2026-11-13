// Farcaster IdRegistry proof verification belongs to the Neynar relay only.
// No custody keys, mnemonic or Neynar API credentials are accepted here.
import {Signature} from '../../ui/wasm/vendor/noble-secp256k1.mjs';
import {keccak256} from '../../ui/wasm/browser_protocol_signer.mjs';
import {fromHex,hex} from '../../ui/wasm/farcaster_protocol.mjs';

export const OPTIMISM_ID_REGISTRY='0x00000000fc6c5f01fc30151999387bb99a9f489b';
const utf8=new TextEncoder();
const hashText=text=>keccak256(utf8.encode(text));
const combine=(...arrays)=>Uint8Array.from(arrays.flatMap(x=>[...x]));
const numberWord=n=>{
  const val=typeof n==='bigint'?n:
    typeof n==='number'&&Number.isSafeInteger(n)&&n>=0?BigInt(n):
    typeof n==='string'&&/^(0|[1-9][0-9]*)$/.test(n)?BigInt(n):null;
  if(val===null||val<0n||val>=(1n<<256n))throw new Error('invalid uint256');
  return fromHex(val.toString(16).padStart(64,'0'),32);
};
const addressWord=address=>combine(new Uint8Array(12),fromHex(address,20));
export const exactFid=value=>{
  const s=typeof value==='string'?value:
    typeof value==='number'&&Number.isSafeInteger(value)?String(value):'';
  if(!/^[1-9][0-9]{0,18}$/.test(s)||BigInt(s)>BigInt(Number.MAX_SAFE_INTEGER))
    throw new Error('invalid fid');
  return s;
};
export const exactAddress=value=>{
  if(typeof value!=='string'||!/^0x[0-9a-fA-F]{40}$/.test(value))
    throw new Error('invalid custody address');
  return value.toLowerCase();
};
export const exactUint=value=>{
  const s=typeof value==='string'?value:
    typeof value==='number'&&Number.isSafeInteger(value)?String(value):'';
  if(!/^(0|[1-9][0-9]{0,76})$/.test(s)||BigInt(s)>=(1n<<256n))
    throw new Error('invalid uint');
  return s;
};
export function hashFarcasterTransfer({fid,to,nonce,deadline}) {
  const id=exactFid(fid), custody=exactAddress(to);
  const due=exactUint(deadline), sequence=exactUint(nonce);
  const domain=keccak256(combine(
    hashText('EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)'),
    hashText('Farcaster IdRegistry'),hashText('1'),numberWord(10),
    addressWord(OPTIMISM_ID_REGISTRY)));
  const struct=keccak256(combine(
    hashText('Transfer(uint256 fid,address to,uint256 nonce,uint256 deadline)'),
    numberWord(id),addressWord(custody),numberWord(sequence),numberWord(due)));
  return keccak256(combine(Uint8Array.of(0x19,0x01),domain,struct));
}
export function verifyFarcasterTransfer({fid,to,nonce,deadline,signature}) {
  try {
    const custody=exactAddress(to);
    if(typeof signature!=='string'||!/^0x[0-9a-fA-F]{130}$/.test(signature))
      return false;
    const raw=fromHex(signature,65);
    if(raw[64]!==27&&raw[64]!==28)return false;
    const sig=Signature.fromBytes(raw.slice(0,64)).addRecoveryBit(raw[64]-27);
    if(sig.hasHighS())return false;
    const digest=hashFarcasterTransfer({fid,to,nonce,deadline});
    const pub=sig.recoverPublicKey(digest).toBytes(false);
    return '0x'+hex(keccak256(pub.slice(1)).slice(-20))===custody;
  }catch{return false;}
}
const selector=method=>'0x'+hex(keccak256(utf8.encode(method)).slice(0,4));
const REGISTRY_CALLS=Object.freeze({
  nonce:selector('nonces(address)'),
  idOf:selector('idOf(address)'),
  custodyOf:selector('custodyOf(uint256)')
});
function wordFromRpc(result) {
  if(typeof result!=='string'||!/^0x[0-9a-fA-F]{64}$/.test(result))
    throw new Error('malformed registry call');
  return BigInt(result);
}
export function createOptimismIdRegistryReader({
  rpcUrl,fetchImpl=globalThis.fetch, timeoutMs=8000
}) {
  const url=new URL(rpcUrl);
  if(url.protocol!=='https:'||url.username||url.password||url.hash||url.search
    ||url.port||url.pathname!=='/')
    throw new Error('optimism RPC requires credential-free HTTPS origin');
  async function call(method,arg) {
    const parameter=method==='custodyOf' ? numberWord(exactFid(arg)) :
      addressWord(exactAddress(arg));
    const data=REGISTRY_CALLS[method]+hex(parameter);
    const response=await fetchImpl(url.origin,{
      method:'POST',redirect:'error',signal:AbortSignal.timeout(timeoutMs),
      headers:{'content-type':'application/json','accept':'application/json'},
      body:JSON.stringify({jsonrpc:'2.0',id:1,method:'eth_call',
        params:[{to:OPTIMISM_ID_REGISTRY,data},'safe']})
    });
    if(!response.ok||Number(response.headers.get('content-length')||0)>2048)
      throw new Error('optimism RPC unavailable');
    const payload=await response.text();
    if(payload.length>2048)throw new Error('optimism RPC response too large');
    const dataResult=JSON.parse(payload);
    if(dataResult.jsonrpc!=='2.0'||dataResult.id!==1
       ||dataResult.error||!dataResult.result)
      throw new Error('invalid optimism RPC response');
    return wordFromRpc(dataResult.result);
  }
  return Object.freeze({
    nonce:async address=>(await call('nonce',address)).toString(),
    idOf:async address=>(await call('idOf',address)).toString(),
    custodyOf:async fid=>{
      const value=await call('custodyOf',fid);
      return '0x'+value.toString(16).padStart(40,'0');
    },
    confirmed:async(fid,custody)=>{
      const id=exactFid(fid),owner=exactAddress(custody);
      const found=await call('idOf',owner);
      if(found!==BigInt(id))return false;
      const stored=await call('custodyOf',id);
      return '0x'+stored.toString(16).padStart(40,'0')===owner;
    }
  });
}
