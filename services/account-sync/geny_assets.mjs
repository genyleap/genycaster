import {serverDeployment} from '../deployment_config.mjs';
// Read-only GENY holdings and protocol-verified wallet discovery.
// No holding amount can grant a GenyCaster Pro subscription.
const ADDRESS = /^0x[0-9a-fA-F]{40}$/;
const HEX_UINT = /^0x[0-9a-fA-F]{1,64}$/;
const DEFAULT_TOKEN = '0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b';
const MAX_WALLETS = 100;
const MAX_PAGES = 12;

export function createGenyAssetsPolicy(env=process.env) {
  const contract = String(env.GENYCASTER_GENY_CONTRACT_ADDRESS || DEFAULT_TOKEN).toLowerCase();
  if (!ADDRESS.test(contract)) throw new Error('Invalid GENY token address');
  const rpcUrl = String(env.GENYCASTER_GENY_RPC_URL || 'https://mainnet.base.org');
  const snapchainUrl = String(env.GENYCASTER_SNAPCHAIN_URL || serverDeployment(env).snapchainOrigin);
  const hypersnapUrl = String(env.GENYCASTER_HYPERSNAP_URL || serverDeployment(env).hypersnapOrigin);
  for (const endpoint of [rpcUrl,snapchainUrl,hypersnapUrl]) {
    const url = new URL(endpoint);
    if (url.protocol!=='https:' || url.username || url.password || url.hash
        || url.search || url.pathname!=='/' || url.port)
      throw new Error('Geny assets require credential-free HTTPS provider origins');
  }
  return Object.freeze({contract,rpcUrl,snapchainUrl,hypersnapUrl});
}
async function checkedJson(fetchImpl,url,options={}) {
  const response=await fetchImpl(url,{...options,redirect:'error',
    signal:AbortSignal.timeout(7000)});
  if (!response.ok || Number(response.headers?.get?.('content-length')||0)>262144)
    throw new Error('provider_unavailable');
  // Reject oversized and non-JSON responses before trusting any provider data.
  const text=await response.text();
  if(text.length>262144)throw new Error('provider_unavailable');
  return JSON.parse(text);
}

function verificationWallet(record,fid) {
  const data=record?.data;
  const body=data?.verificationAddAddressBody;
  const address=String(body?.address||'').toLowerCase();
  if(String(data?.fid)!==String(fid)
     || data?.type!=='MESSAGE_TYPE_VERIFICATION_ADD_ETH_ADDRESS'
     || !['FARCASTER_NETWORK_MAINNET',1].includes(data?.network)
     || !['PROTOCOL_ETHEREUM',0,undefined].includes(body?.protocol)
     || !ADDRESS.test(address))return null;
  return address;
}

async function verifiedWalletsByFid(fid,origin,fetchImpl) {
  if(!/^[1-9][0-9]{0,18}$/.test(String(fid))
      || !Number.isSafeInteger(Number(fid)))throw new Error('invalid_fid');
  const wallets=new Set();
  let pageToken='';
  for(let page=0;page<MAX_PAGES;page++) {
    const endpoint=new URL('/v1/verificationsByFid',origin);
    endpoint.searchParams.set('fid',String(fid));
    endpoint.searchParams.set('pageSize','100');
    if(pageToken)endpoint.searchParams.set('pageToken',pageToken);
    const payload=await checkedJson(fetchImpl,endpoint.href);
    if(!Array.isArray(payload?.messages) || payload.messages.length>100)
      throw new Error('provider_unavailable');
    for(const entry of payload.messages) {
      const wallet=verificationWallet(entry,fid);
      if(wallet)wallets.add(wallet);
    }
    if(wallets.size>MAX_WALLETS)throw new Error('provider_unavailable');
    const next=String(payload.nextPageToken||'');
    if(!next)return [...wallets];
    if(next===pageToken || next.length>1024)throw new Error('provider_unavailable');
    pageToken=next;
  }
  throw new Error('provider_unavailable');
}
// The account issuer, not the UI-selected read network or a bare caller FID,
// determines which signed protocol verification records may authorize payment.
export async function verifiedAccountWallets(owner,{policy,fetchImpl=globalThis.fetch}={}) {
  if(!owner || typeof owner!=='object')throw new Error('verified_account_required');
  const fid=String(owner.fid||'');
  const namespace=String(owner.issuer||'');
  if(namespace==='hypersnap:native-registry' &&
      owner.subject==='hypersnap:'+fid && Number(fid)>=4294967296)
    return verifiedWalletsByFid(fid,policy.hypersnapUrl,fetchImpl);
  // The account API has already authenticated this issuer; the provider
  // function rejects all caller-supplied FID/subject mismatches.
  if((namespace.startsWith('https://') ||
      namespace==='farcaster:optimism-id-registry') &&
      owner.subject==='farcaster:'+fid)
    return verifiedWalletsByFid(fid,policy.snapchainUrl,fetchImpl);
  throw new Error('verified_account_required');
}
export async function verifiedFarcasterWallets(fid,{policy,fetchImpl=globalThis.fetch}={}) {
  return verifiedWalletsByFid(fid,policy.snapchainUrl,fetchImpl);
}
async function rpc(fetchImpl,policy,method,params){
  const response=await checkedJson(fetchImpl,policy.rpcUrl,{
    method:'POST',headers:{'content-type':'application/json'},
    body:JSON.stringify({jsonrpc:'2.0',id:1,method,params})
  });
  if(response?.error || !HEX_UINT.test(String(response?.result||'')))
    throw new Error('provider_unavailable');
  return BigInt(response.result);
}

// Read-only Base ERC-20 holdings for a verified Farcaster account. This is
// independent of Pro authorization: balances can be summed for display but
// never establish an account entitlement.
export async function evaluateGenyAssets(fid,{
  policy=createGenyAssetsPolicy(),fetchImpl=globalThis.fetch
}={}) {
  const unavailable={status:'unavailable',available:false,partial:false,
    wallets:[],addresses:[],balance:0,holder:false,source:'Base RPC',
    error:'provider_unavailable'};
  try {
    const verified=await verifiedFarcasterWallets(fid,{policy,fetchImpl});
    if (!verified.length)
      return {...unavailable,status:'verified',error:'',wallets:[],available:false};
    if (await rpc(fetchImpl,policy,'eth_chainId',[])!==8453n)
      throw new Error('provider_unavailable');
    const decimals=Number(await rpc(fetchImpl,policy,'eth_call',
      [{to:policy.contract,data:'0x313ce567'},'latest']));
    if(!Number.isInteger(decimals)||decimals<0||decimals>36)
      throw new Error('provider_unavailable');
    // Provider resources are bounded independently of the verification set.
    // A partial view is labeled as such rather than misrepresenting a total.
    const scanned=verified.slice(0,24);
    const addresses=[];
    let total=0,readable=0;
    for(let index=0;index<scanned.length;index+=4) {
      const group=await Promise.all(scanned.slice(index,index+4).map(async address=>{
        try {
          const raw=await rpc(fetchImpl,policy,'eth_call',[
            {to:policy.contract,data:'0x70a08231'+address.slice(2).padStart(64,'0')},'latest']);
          const unit=10n**BigInt(decimals);
          const integer=raw/unit;
          const fraction=(raw%unit).toString().padStart(decimals,'0').slice(0,8);
          const readableAmount=Number(integer)+Number('0.'+(fraction||'0'));
          if(!Number.isFinite(readableAmount))throw new Error('provider_unavailable');
          return {wallet:address,available:true,balance:readableAmount,
            holder:raw>0n,decimals};
        }catch {
          return {wallet:address,available:false,balance:0,holder:false,
            error:'provider_unavailable'};
        }
      }));
      addresses.push(...group);
    }
    for(const row of addresses) {
      if(row.available){++readable;total+=row.balance;}
    }
    const partial=readable!==verified.length;
    return {status:readable?'verified':'unavailable',
      available:readable>0,partial,wallets:verified,addresses,
      balance:total,holder:total>0,source:'Base RPC',
      error:readable?'':'provider_unavailable'};
  }catch {
    return unavailable;
  }
}
