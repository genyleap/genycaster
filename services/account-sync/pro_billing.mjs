// GenyCaster Pro billing is a product extension, separate from protocol signing.
// The database is the only subscription authority; a wallet balance never grants
// Pro and an unsigned client-supplied amount never creates a payment quote.
import {randomUUID} from 'node:crypto';
import {verifiedFarcasterWallets} from './geny_assets.mjs';

const ADDRESS=/^0x[0-9a-fA-F]{40}$/;
const HASH=/^0x[0-9a-fA-F]{64}$/;
const GENY='0x2a3d6f8c1fc4acdcf3a75d19b445bae02f03676b';
const BASE_CHAIN_ID=8453n;
const TRANSFER_TOPIC='0xddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef';
const PRICE_URL='https://api.geckoterminal.com/api/v2/simple/networks/base/token_price/'+GENY;
const TOKEN_UNITS=10n**18n;
const USD_DECIMALS=10n**18n;
const QUOTE_TTL_MS=5*60_000;
const CONFIRMATIONS=6n;
const NORMAL_CAPABILITIES=Object.freeze({
  pro_badge:false,custom_bookmark_collections:false,advanced_code_editor:false,
  multiple_miniapps:false,extra_market_pulse:false,provider_connections:false,
  extended_casts:false,extra_images:false,long_form_threads:false,
  multi_image_threads:false
});
const NORMAL_LIMITS=Object.freeze({cast_bytes:1024,cast_characters:1024,
  image_embeds:1,concurrent_miniapps:1,market_pulse:4});

// Exact decimal parsing is mandatory: Number/float rounding is not safe for
// ERC-20 base units, especially when a token is priced well below one cent.
export function decimalUnits(text,decimals=18) {
  if(typeof text!=='string' || !/^(?:0|[1-9][0-9]{0,25})(?:\.[0-9]{1,18})?$/.test(text))
    throw new Error('invalid_decimal');
  const [whole,fraction='']=text.split('.');
  if(fraction.length>decimals)throw new Error('invalid_decimal');
  return BigInt(whole)*10n**BigInt(decimals)
    +BigInt((fraction.padEnd(decimals,'0'))||'0');
}
function validOrigin(value) {
  const url=new URL(value);
  if(url.protocol!=='https:' || url.username || url.password || url.hash
     || url.search || url.pathname!=='/' || url.port)
    throw new Error('invalid_billing_rpc');
  return url.origin;
}
export function createProBillingPolicy(env=process.env) {
  const cents=String(env.GENYCASTER_PRO_MONTHLY_USD_CENTS??'299');
  if(!/^[1-9][0-9]{0,5}$/.test(cents))
    throw new Error('invalid_pro_price_cents');
  const treasury=String(env.GENYCASTER_PRO_TREASURY_ADDRESS||'').toLowerCase();
  if(treasury && (!ADDRESS.test(treasury) || /^0x0{40}$/.test(treasury)
      || treasury===GENY))
    throw new Error('invalid_pro_treasury');
  const rpcUrl=validOrigin(String(env.GENYCASTER_GENY_RPC_URL||'https://mainnet.base.org'));
  const snapchainUrl=validOrigin(String(env.GENYCASTER_SNAPCHAIN_URL||'https://snapchain.genyleap.com'));
  return Object.freeze({
    monthlyUsdCents:Number(cents),treasury,token:GENY,
    rpcUrl,snapchainUrl,chainId:Number(BASE_CHAIN_ID),quoteTtlMs:QUOTE_TTL_MS,
    confirmations:Number(CONFIRMATIONS),
    enabled:!!treasury,periodDays:30
  });
}
function isActive(endsAt,now) {
  return !!endsAt && Number.isFinite(Date.parse(endsAt))
    && Date.parse(endsAt)>now;
}
export function subscriptionEntitlements(subscription,policy,now=Date.now()) {
  const until=subscription?.expires_at instanceof Date
    ?subscription.expires_at.toISOString()
    :String(subscription?.expires_at||'');
  const eligible=isActive(until,now);
  return {
    status:'verified',tier:eligible?'pro':'basic',eligible,
    billing_mode:'monthly_payment',period_days:30,
    monthly_usd_cents:policy.monthlyUsdCents,
    payment_available:policy.enabled,
    expires_at:eligible?until:null,
    capabilities:eligible
      ?{...NORMAL_CAPABILITIES,pro_badge:true,custom_bookmark_collections:true,
         advanced_code_editor:true,multiple_miniapps:true,
         extra_market_pulse:true,provider_connections:true,extra_images:true,
         long_form_threads:true,multi_image_threads:true}
      :{...NORMAL_CAPABILITIES},
    limits:eligible
      ?{...NORMAL_LIMITS,cast_characters:4096,image_embeds:4,
         concurrent_miniapps:4,market_pulse:16}
      :{...NORMAL_LIMITS}
  };
}
async function safeJson(fetchImpl,url,{method='GET',body,limit=16_384}={}) {
  const r=await fetchImpl(url,{method,redirect:'error',signal:AbortSignal.timeout(6500),
    headers:{'Accept':'application/json','Content-Type':'application/json'},
    ...(body?{body:JSON.stringify(body)}:{})});
  if(!r.ok || Number(r.headers?.get?.('content-length')||0)>limit)
    throw new Error('billing_upstream_unavailable');
  const text=await r.text();
  if(text.length>limit)throw new Error('billing_upstream_unavailable');
  return JSON.parse(text);
}
export function createProRateReader({fetchImpl=globalThis.fetch,now=Date.now}={}) {
  // One-minute bounded single-flight cache shared by all callers; fail closed.
  let cache=null;let pending=null;
  return async () => {
    if(cache && cache.until>now())return cache;
    if(pending)return pending;
    pending=(async()=>{
      const document=await safeJson(fetchImpl,PRICE_URL);
      const pairs=document?.data?.attributes?.token_prices;
      const raw=pairs?.[GENY]??pairs?.[GENY.toLowerCase()];
      const price=String(raw||'');
      const units=decimalUnits(price,18);
      if(units<=0n || units>USD_DECIMALS)
        throw new Error('invalid_pro_rate');
      cache={priceUsd:price,priceUnits:units,asOf:new Date(now()).toISOString(),
        until:now()+60_000,source:'GeckoTerminal/Base'};
      return cache;
    })().finally(()=>{pending=null;});
    return pending;
  };
}
export function createProQuote({policy,rate,now=Date.now()}) {
  if(!policy.enabled)throw new Error('billing_not_configured');
  const units=(BigInt(policy.monthlyUsdCents)*TOKEN_UNITS*USD_DECIMALS
    +100n*rate.priceUnits-1n)/(100n*rate.priceUnits);
  // Explicit max to fail closed on bad feeds or catastrophic price swings.
  if(units<1n || units>100_000_000n*TOKEN_UNITS)
    throw new Error('invalid_pro_rate');
  return {
    id:randomUUID(),price_usd_cents:policy.monthlyUsdCents,
    amount_base_units:units.toString(),amount_geny:formatGeny(units),
    geny_price_usd:rate.priceUsd,
    token:policy.token,treasury:policy.treasury,chain_id:policy.chainId,
    created_at:new Date(now).toISOString(),
    expires_at:new Date(now+policy.quoteTtlMs).toISOString(),
    period_days:policy.periodDays,source:rate.source
  };
}
export function formatGeny(value) {
  const whole=value/TOKEN_UNITS;
  const fraction=(value%TOKEN_UNITS).toString().padStart(18,'0')
    .replace(/0+$/,'');
  return whole.toString()+(fraction?'.'+fraction:'');
}
function normalHash(value) {
  return typeof value==='string' && HASH.test(value)?value.toLowerCase():null;
}
async function rpc(fetchImpl,policy,method,params=[]) {
  const response=await safeJson(fetchImpl,policy.rpcUrl,{method:'POST',
    body:{jsonrpc:'2.0',id:1,method,params},limit:32_768});
  if(response?.error || response?.jsonrpc!=='2.0'
     || !Object.prototype.hasOwnProperty.call(response,'result'))
    throw new Error('billing_chain_unavailable');
  return response.result;
}
function hexBigInt(value) {
  if(typeof value!=='string' || !/^0x[0-9a-fA-F]{1,64}$/.test(value))
    throw new Error('billing_chain_unavailable');
  return BigInt(value);
}
function addressTopic(value) {
  return '0x'+'0'.repeat(24)+value.slice(2).toLowerCase();
}
// The displayed 18-decimal quote is valid only if the configured Base token
// contract actually reports 18 decimals. Never rely on its symbol alone.
export function createProTokenVerifier({fetchImpl=globalThis.fetch,now=Date.now}={}) {
  let checkedUntil=0;let pending=null;
  return async policy=>{
    if(checkedUntil>now())return true;
    if(pending)return pending;
    pending=(async()=>{
      if(hexBigInt(await rpc(fetchImpl,policy,'eth_chainId'))!==BASE_CHAIN_ID)
        throw new Error('wrong_billing_chain');
      const code=await rpc(fetchImpl,policy,'eth_getCode',[policy.token,'latest']);
      if(typeof code!=='string' || !/^0x[0-9a-fA-F]{2,}$/.test(code)
         || /^0x0+$/.test(code))
        throw new Error('invalid_token_contract');
      const decimals=hexBigInt(await rpc(fetchImpl,policy,'eth_call',[
        {to:policy.token,data:'0x313ce567'},'latest']));
      if(decimals!==18n)throw new Error('unsupported_token_decimals');
      checkedUntil=now()+300_000;
      return true;
    })().finally(()=>{pending=null;});
    return pending;
  };
}

export async function verifyProTransfer({
  owner,quote,txHash,policy,fetchImpl=globalThis.fetch,verifyWallets=verifiedFarcasterWallets
}) {
  const tx=normalHash(txHash);
  if(!tx)throw new Error('invalid_payment_hash');
  if(!quote || quote.status!=='quoted')
    throw new Error('quote_not_payable');
  const wallets=await verifyWallets(owner.fid,{
    policy:{snapchainUrl:policy.snapchainUrl,
      rpcUrl:policy.rpcUrl,contract:policy.token},fetchImpl});
  const addresses=new Set(wallets.map(x=>x.toLowerCase()));
  if(!addresses.size)throw new Error('verified_wallet_required');
  if(hexBigInt(await rpc(fetchImpl,policy,'eth_chainId'))!==BASE_CHAIN_ID)
    throw new Error('wrong_billing_chain');
  const receipt=await rpc(fetchImpl,policy,'eth_getTransactionReceipt',[tx]);
  if(!receipt)throw new Error('payment_pending');
  if(normalHash(receipt.transactionHash)!==tx
     || hexBigInt(receipt.status)!==1n)
    throw new Error('payment_failed');
  const height=hexBigInt(receipt.blockNumber);
  const head=hexBigInt(await rpc(fetchImpl,policy,'eth_blockNumber'));
  if(head<height+CONFIRMATIONS-1n)
    throw new Error('payment_pending');
  const block=await rpc(fetchImpl,policy,'eth_getBlockByNumber',[receipt.blockNumber,false]);
  if(!block || normalHash(block.hash)!==normalHash(receipt.blockHash))
    throw new Error('billing_chain_unavailable');
  const minedAt=Number(hexBigInt(block.timestamp))*1000;
  const issuedAt=Date.parse(quote.created_at);
  const expiresAt=Date.parse(quote.expires_at);
  if(!Number.isSafeInteger(minedAt) || !Number.isFinite(issuedAt)
     || !Number.isFinite(expiresAt) || minedAt<issuedAt-30_000
     || minedAt>expiresAt+60_000)
    throw new Error('payment_outside_quote');
  const recipient=addressTopic(quote.treasury);
  const amount=BigInt(quote.amount_base_units);
  // Only actual Transfer logs from the exact GENY contract count. Transaction
  // input, wallet UI screenshots and token balances are never payment evidence.
  const matching=(receipt.logs||[]).filter(log=>
    log?.address?.toLowerCase()===policy.token
    && Array.isArray(log.topics) && log.topics.length===3
    && log.topics[0]?.toLowerCase()===TRANSFER_TOPIC
    && log.topics[2]?.toLowerCase()===recipient
    && addresses.has('0x'+String(log.topics[1]).slice(-40).toLowerCase())
    && /^0x[0-9a-fA-F]{64}$/.test(String(log.data||''))
    && BigInt(log.data)===amount);
  if(matching.length!==1)throw new Error('payment_not_matched');
  return {tx_hash:tx,block_number:height.toString(),
    block_hash:receipt.blockHash.toLowerCase(),
    payer:'0x'+matching[0].topics[1].slice(-40).toLowerCase(),
    paid_at:new Date(minedAt).toISOString()};
}
