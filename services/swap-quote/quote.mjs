/*!
 * GenyCaster Base on-chain Uniswap V2 swap quoting.
 * No commercial swap/aggregator API, SDK token, custody or managed RPC.
 * The only upstream dependency is an Ethereum JSON-RPC node for Base 8453.
 *
 * Canonical contract addresses:
 * https://developers.uniswap.org/docs/protocols/v2/deployments
 * https://developers.uniswap.org/docs/protocols/v3/deployments/v3-base-deployments
 */
const BASE_CHAIN = '8453';
const ROUTER = '0x4752ba5dbc23f44d87826276bf6fd6b1c372ad24';
const WETH = '0x4200000000000000000000000000000000000006';
const NATIVE = '0xeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee';
const ADDRESS = /^0x[0-9a-fA-F]{40}$/;
const UINT = /^(?:0|[1-9][0-9]{0,77})$/;
const MAX_UINT = (1n << 256n) - 1n;
const CHAIN = 'eip155:8453';
const MAX_RPC_BYTES = 262144;
const selectors = Object.freeze({
  getAmountsOut:'d06ca61f',
  swapExactTokensForTokens:'38ed1739',
  swapExactETHForTokens:'7ff36ab5',
  swapExactTokensForETH:'18cbafe5',
  allowance:'dd62ed3e',
  balanceOf:'70a08231'
});

export const constants = Object.freeze({BASE_CHAIN, ROUTER, WETH, NATIVE, CHAIN, selectors});

const isUint = value => typeof value === 'string' && UINT.test(value)
  && value.length <= 78 && BigInt(value) <= MAX_UINT;
const nonzero = value => isUint(value) && BigInt(value) !== 0n;
const word = value => {
  const n=BigInt(value);
  if (n<0n || n>MAX_UINT) throw new Error('invalid_uint256');
  return n.toString(16).padStart(64,'0');
};
const addressWord = value => {
  if (!ADDRESS.test(value) || /^0x0{40}$/i.test(value)) throw new Error('invalid_address');
  return value.slice(2).toLowerCase().padStart(64,'0');
};
const asset = value => {
  if (typeof value !== 'string') throw new Error('token_must_be_caip19');
  if (value === CHAIN + '/native') return NATIVE;
  const prefix = CHAIN + '/erc20:';
  const contract = value.startsWith(prefix) && value.slice(prefix.length);
  if (!contract || !ADDRESS.test(contract) || /^0x0{40}$/i.test(contract))
    throw new Error('unsupported_token');
  return contract.toLowerCase();
};

export function validateQuoteRequest(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body))
    throw new Error('invalid_request');
  const allowed = new Set(['sellToken','buyToken','sellAmount','taker','slippageBps']);
  if (Object.keys(body).some(key => !allowed.has(key)))
    throw new Error('unexpected_parameter');
  const sellToken=asset(body.sellToken);
  const buyToken=asset(body.buyToken);
  const effective = token => token === NATIVE ? WETH : token;
  if (effective(sellToken) === effective(buyToken))
    throw new Error('identical_tokens');
  const taker=body.taker;
  if (typeof taker !== 'string' || !ADDRESS.test(taker) || /^0x0{40}$/i.test(taker))
    throw new Error('invalid_taker');
  if (!nonzero(body.sellAmount)) throw new Error('invalid_amount');
  const slippageBps=body.slippageBps ?? 100;
  if (!Number.isInteger(slippageBps) || slippageBps < 10 || slippageBps > 300)
    throw new Error('invalid_slippage');
  return Object.freeze({
    sellToken,buyToken,sellAmount:body.sellAmount,
    taker:taker.toLowerCase(),slippageBps
  });
}

const pathsFor = (sell,buy) => {
  const first=sell===NATIVE?WETH:sell, last=buy===NATIVE?WETH:buy;
  const result=[[first,last]];
  if (first!==WETH && last!==WETH) result.push([first,WETH,last]);
  return result;
};
const encodePath = path => word(path.length)+path.map(addressWord).join('');
export const encodeGetAmountsOut = (amount,path) =>
  '0x'+selectors.getAmountsOut+word(amount)+word(64)+encodePath(path);

export function encodeSwapTransaction(req,buyAmount,path,deadline) {
  const request=validateQuoteRequest(req);
  const minBuyAmount=BigInt(buyAmount)*(10000n-BigInt(request.slippageBps))/10000n;
  if (!isUint(buyAmount) || minBuyAmount===0n
    || !Number.isSafeInteger(deadline) || deadline<=0)
    throw new Error('unsafe_quote');
  const recipient=addressWord(request.taker);
  const pathWords=encodePath(path);
  let selector,head;
  if (request.sellToken===NATIVE) {
    selector=selectors.swapExactETHForTokens;
    head=word(minBuyAmount)+word(128)+recipient+word(deadline);
  } else {
    selector=request.buyToken===NATIVE
      ?selectors.swapExactTokensForETH:selectors.swapExactTokensForTokens;
    head=word(request.sellAmount)+word(minBuyAmount)+word(160)+recipient+word(deadline);
  }
  return {to:ROUTER,data:'0x'+selector+head+pathWords,
    value:request.sellToken===NATIVE?request.sellAmount:'0',minBuyAmount:String(minBuyAmount)};
}

const hexResult = text => {
  if (typeof text!=='string' || !/^0x[0-9a-fA-F]*$/.test(text)
    || text.length>MAX_RPC_BYTES) throw new Error('invalid_rpc_result');
  return text.toLowerCase();
};
const decodeWord = (hex,position) => {
  const offset=2+position*64;
  const value=hex.slice(offset,offset+64);
  if (!/^[a-f0-9]{64}$/.test(value)) throw new Error('invalid_rpc_result');
  return BigInt('0x'+value);
};
const decodeAmountsOut = (hex,expectedLength) => {
  const data=hexResult(hex);
  const offset=decodeWord(data,0);
  if (offset!==32n || decodeWord(data,1)!==BigInt(expectedLength))
    throw new Error('invalid_pool_quote');
  const amounts=Array.from({length:expectedLength},(_,i)=>decodeWord(data,i+2));
  if (amounts.some(a=>a===0n)) throw new Error('no_liquidity');
  return amounts;
};
const decodeUint = hex => {
  const data=hexResult(hex);
  if (data.length!==66) throw new Error('invalid_rpc_result');
  return decodeWord(data,0);
};

export function createRpcClient(rpcUrl,fetchImpl=fetch) {
  const endpoint=new URL(rpcUrl);
  if (endpoint.protocol!=='https:' || endpoint.username || endpoint.password
      || endpoint.search || endpoint.hash) throw new Error('invalid_base_rpc_url');
  let nextId=0;
  return async function rpc(method,params) {
    const id=++nextId;
    const response=await fetchImpl(endpoint.href,{
      method:'POST', redirect:'error',
      headers:{'Content-Type':'application/json','Accept':'application/json'},
      body:JSON.stringify({jsonrpc:'2.0',id,method,params}),
      signal:AbortSignal.timeout(8500)
    });
    if (!response.ok) throw new Error('rpc_http_'+response.status);
    const size=Number(response.headers?.get('content-length')||0);
    if (size>MAX_RPC_BYTES) throw new Error('rpc_oversized');
    const raw=await response.text();
    if (raw.length>MAX_RPC_BYTES) throw new Error('rpc_oversized');
    const result=JSON.parse(raw);
    if (!result || result.jsonrpc!=='2.0' || result.id!==id
        || result.error || typeof result.result!=='string')
      throw new Error('base_rpc_failed');
    return result.result;
  };
}

export async function fetchFirmQuote(body,rpcUrl='https://mainnet.base.org',options={}) {
  const req=validateQuoteRequest(body);
  const rpc=options.rpc || createRpcClient(rpcUrl,options.fetch || fetch);
  const chain=await rpc('eth_chainId',[]);
  if (BigInt(chain)!==8453n) throw new Error('wrong_base_rpc_chain');

  const routes=pathsFor(req.sellToken,req.buyToken);
  const quoted=await Promise.all(routes.map(async path=>{
    try {
      const result=await rpc('eth_call',[{to:ROUTER,
        data:encodeGetAmountsOut(req.sellAmount,path)},'latest']);
      const amounts=decodeAmountsOut(result,path.length);
      return {path,buyAmount:amounts.at(-1)};
    } catch {return null;}
  }));
  const best=quoted.filter(Boolean).sort((a,b)=>a.buyAmount>b.buyAmount?-1:1)[0];
  if (!best) throw new Error('no_liquid_uniswap_v2_route');
  const deadline=Math.floor(Date.now()/1000)+180;
  const unsigned=encodeSwapTransaction(body,best.buyAmount.toString(),best.path,deadline);
  const sellContract=req.sellToken===NATIVE?null:req.sellToken;
  const balancePromise = sellContract
    ?rpc('eth_call',[{to:sellContract,
      data:'0x'+selectors.balanceOf+addressWord(req.taker)},'latest']).then(decodeUint)
    :rpc('eth_getBalance',[req.taker,'latest']).then(value=>BigInt(hexResult(value)));
  const allowancePromise = sellContract
    ?rpc('eth_call',[{to:sellContract,
      data:'0x'+selectors.allowance+addressWord(req.taker)+addressWord(ROUTER)},'latest']).then(decodeUint)
    :Promise.resolve(MAX_UINT);
  const [balance,allowance,gasPriceHex]=await Promise.all([
    balancePromise,allowancePromise,rpc('eth_gasPrice',[])
  ]);
  if (balance<BigInt(req.sellAmount)) throw new Error('insufficient_balance');
  const gasPrice=BigInt(hexResult(gasPriceHex));
  if (gasPrice===0n) throw new Error('invalid_gas_price');
  const requiresApproval=!!sellContract && allowance<BigInt(req.sellAmount);
  let gas=best.path.length===3?450000n:320000n;
  if (!requiresApproval) {
    try {
      const estimate=await rpc('eth_estimateGas',[{
        from:req.taker,to:ROUTER,
        data:unsigned.data,
        value:'0x'+BigInt(unsigned.value).toString(16)
      }]);
      const estimated=BigInt(hexResult(estimate));
      if (estimated>0n && estimated<10_000_000n)
        gas=(estimated*120n+99n)/100n;
      else throw new Error('gas_estimate_unavailable');
    } catch {throw new Error('swap_simulation_failed');}
  }
  const totalNetworkFee=gas*gasPrice;
  if (req.sellToken===NATIVE
      && balance<BigInt(req.sellAmount)+totalNetworkFee)
    throw new Error('insufficient_gas_balance');
  const fetchedAt=Date.now();
  return Object.freeze({
    provider:'uniswap-v2-onchain',chainId:8453,
    taker:req.taker,
    sellToken:body.sellToken,buyToken:body.buyToken,
    sellAmount:req.sellAmount,
    buyAmount:best.buyAmount.toString(),
    minBuyAmount:unsigned.minBuyAmount,
    slippageBps:req.slippageBps,
    spender:ROUTER,
    route:best.path,
    deadline,
    requiresApproval,
    gas:gas.toString(),gasPrice:gasPrice.toString(),
    totalNetworkFee:totalNetworkFee.toString(),
    transaction:{
      to:ROUTER,data:unsigned.data,value:unsigned.value,gas:gas.toString()
    },
    fetchedAt,
    expiresAt:fetchedAt+20000
  });
}
