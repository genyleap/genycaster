// GenyCaster Pro admission: authenticated FID -> protocol-verified Ethereum
// addresses -> Base ERC-20 balances. The caller never supplies an address or
// an entitlement; client-side balance widgets are informational only.
const ADDRESS = /^0x[0-9a-fA-F]{40}$/;
const HEX_UINT = /^0x[0-9a-fA-F]{1,64}$/;
const DEFAULT_TOKEN = '0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b';
const MAX_WALLETS = 100;
const MAX_PAGES = 12;

export function createGenyProPolicy(env=process.env) {
  const min = String(env.GENYCASTER_PRO_REQUIRED_GENY ?? '16000');
  if (!/^[1-9][0-9]{0,11}$/.test(min))
    throw new Error('GENYCASTER_PRO_REQUIRED_GENY must be a positive integer');
  const contract = String(env.GENYCASTER_GENY_CONTRACT_ADDRESS || DEFAULT_TOKEN).toLowerCase();
  if (!ADDRESS.test(contract)) throw new Error('Invalid GENY token address');
  const rpcUrl = String(env.GENYCASTER_GENY_RPC_URL || 'https://mainnet.base.org');
  const snapchainUrl = String(env.GENYCASTER_SNAPCHAIN_URL || 'https://snapchain.genyleap.com');
  for (const endpoint of [rpcUrl,snapchainUrl]) {
    const url = new URL(endpoint);
    if (url.protocol!=='https:' || url.username || url.password || url.hash
        || url.search || url.pathname!=='/' || url.port)
      throw new Error('Geny Pro requires credential-free HTTPS provider origins');
  }
  return Object.freeze({minimumGeny:min, contract, rpcUrl, snapchainUrl});
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

export async function verifiedFarcasterWallets(fid,{policy,fetchImpl=globalThis.fetch}={}) {
  if(!/^[1-9][0-9]{0,18}$/.test(String(fid)))throw new Error('invalid_fid');
  const wallets=new Set();
  let pageToken='';
  for(let page=0;page<MAX_PAGES;page++) {
    const endpoint=new URL('/v1/verificationsByFid',policy.snapchainUrl);
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
async function rpc(fetchImpl,policy,method,params){
  const response=await checkedJson(fetchImpl,policy.rpcUrl,{
    method:'POST',headers:{'content-type':'application/json'},
    body:JSON.stringify({jsonrpc:'2.0',id:1,method,params})
  });
  if(response?.error || !HEX_UINT.test(String(response?.result||'')))
    throw new Error('provider_unavailable');
  return BigInt(response.result);
}

export async function evaluateGenyPro(fid,{
  policy=createGenyProPolicy(),fetchImpl=globalThis.fetch
}={}) {
  const standard={tier:'basic',eligible:false,minimum_geny:policy.minimumGeny,
    verified_wallet_count:0,qualified_wallet:null,
    capabilities:{pro_badge:false,custom_bookmark_collections:false,
      advanced_code_editor:false,multiple_miniapps:false,extra_market_pulse:false,
      provider_connections:false,extended_casts:false,extra_images:false,
      long_form_threads:false,multi_image_threads:false},
    // cast_bytes describes each signed Farcaster message, not the total draft.
    limits:{cast_bytes:1024,cast_characters:1024,image_embeds:1,
      concurrent_miniapps:1,market_pulse:4}};
  try {
    const wallets=await verifiedFarcasterWallets(fid,{policy,fetchImpl});
    standard.verified_wallet_count=wallets.length;
    if (!wallets.length)return {...standard,status:'verified'};
    if(await rpc(fetchImpl,policy,'eth_chainId',[])!==8453n)
      throw new Error('provider_unavailable');
    const decimals=Number(await rpc(fetchImpl,policy,'eth_call',[
      {to:policy.contract,data:'0x313ce567'},'latest']));
    if(!Number.isInteger(decimals)||decimals<0||decimals>36)
      throw new Error('provider_unavailable');
    const threshold=BigInt(policy.minimumGeny)*(10n**BigInt(decimals));
    let failures=0;
    for(const address of wallets){
      const calldata='0x70a08231'+address.slice(2).padStart(64,'0');
      try {
        const balance=await rpc(fetchImpl,policy,'eth_call',[
          {to:policy.contract,data:calldata},'latest']);
        if(balance>=threshold){
          return {status:'verified',tier:'pro',eligible:true,
            minimum_geny:policy.minimumGeny,verified_wallet_count:wallets.length,
            qualified_wallet:address,
            capabilities:{pro_badge:true,custom_bookmark_collections:true,
              advanced_code_editor:true,multiple_miniapps:true,
              extra_market_pulse:true,provider_connections:true,
              // Farcaster Pro is separate. Longer Geny Pro content is
              // published as a thread ONLY after explicit user approval.
              extended_casts:false,extra_images:true,
              long_form_threads:true,multi_image_threads:true},
            limits:{cast_bytes:1024,cast_characters:4096,image_embeds:4,
              concurrent_miniapps:4,market_pulse:16}};
        }
      }catch{failures++;}
    }
    return {...standard,status:failures?'unavailable':'verified'};
  }catch {
    // Network faults never turn into a fabricated "not a holder" result,
    // and never grant Pro from stale or partially verified balances.
    return {...standard,status:'unavailable'};
  }
}

// Read-only Base ERC-20 holdings for a verified Farcaster account. This is
// deliberately independent of Pro authorization: balances may be summed for
// display, but eligibility still requires one qualifying wallet.
export async function evaluateGenyAssets(fid,{
  policy=createGenyProPolicy(),fetchImpl=globalThis.fetch
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
