/*!
 * GenyCaster Mini App Base Swap broker — zero third-party runtime packages.
 * No wallet keys, signing authority, or transaction broadcast. The 0x API
 * key remains server-side; clients receive only restricted quote fields.
 */
const BASE_CHAIN = '8453';
const ALLOWANCE_HOLDER = '0x0000000000001ff3684f28c67538d4d072c22734';
const NATIVE = '0xeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee';
const ADDRESS = /^0x[0-9a-fA-F]{40}$/;
const UINT = /^(?:0|[1-9][0-9]{0,77})$/;
const MAX_UINT = '115792089237316195423570985008687907853269984665640564039457584007913129639935';
const CHAIN = 'eip155:8453';

export const constants = Object.freeze({BASE_CHAIN, ALLOWANCE_HOLDER, NATIVE, CHAIN});

const isUint = value => typeof value === 'string' && UINT.test(value)
  && (value.length < MAX_UINT.length || value.length === MAX_UINT.length && value <= MAX_UINT);
const nonzero = value => isUint(value) && value !== '0';
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
  const sellToken = asset(body.sellToken);
  const buyToken = asset(body.buyToken);
  if (sellToken.toLowerCase() === buyToken.toLowerCase())
    throw new Error('identical_tokens');
  const taker = body.taker;
  if (typeof taker !== 'string' || !ADDRESS.test(taker) || /^0x0{40}$/i.test(taker))
    throw new Error('invalid_taker');
  const sellAmount = body.sellAmount;
  if (!nonzero(sellAmount)) throw new Error('invalid_amount');
  const slippageBps = body.slippageBps ?? 100;
  if (!Number.isInteger(slippageBps) || slippageBps < 10 || slippageBps > 300)
    throw new Error('invalid_slippage');
  return Object.freeze({sellToken, buyToken, sellAmount, taker: taker.toLowerCase(), slippageBps});
}

const json = value => {
  if (!value || typeof value !== 'object' || Array.isArray(value))
    throw new Error('invalid_upstream_payload');
  return value;
};
const eq = (a,b) => typeof a === 'string' && typeof b === 'string'
  && a.toLowerCase() === b.toLowerCase();
const hexBytes = (value, limit = 128000) => typeof value === 'string'
  && value.startsWith('0x') && value.length > 10 && value.length <= limit
  && value.length % 2 === 0 && /^0x[0-9a-fA-F]+$/.test(value);

export function normalizeFirmQuote(body, upstream) {
  const req = validateQuoteRequest(body);
  const quote = json(upstream);
  const tx = json(quote.transaction);
  const issues = json(quote.issues || {});
  const allowance = issues.allowance;
  if (quote.liquidityAvailable !== true
      || (quote.chainId !== undefined && quote.chainId !== Number(BASE_CHAIN))
      || !eq(quote.allowanceTarget, ALLOWANCE_HOLDER)
      || issues.simulationIncomplete === true
      || issues.invalidSourcesPassed?.length || !nonzero(quote.buyAmount)
      || !nonzero(quote.minBuyAmount) || BigInt(quote.minBuyAmount) > BigInt(quote.buyAmount)
      || !nonzero(quote.sellAmount) || quote.sellAmount !== req.sellAmount
      || !eq(quote.sellToken, req.sellToken)
      || !eq(quote.buyToken, req.buyToken)
      // The 0x Settler is dynamically deployed. transaction.to is the
      // separate execution destination supplied by the trusted 0x API,
      // whereas AllowanceHolder is the ONLY approved ERC-20 spender.
      || !ADDRESS.test(tx.to) || /^0x0{40}$/i.test(tx.to) || eq(tx.to, req.taker)
      || !hexBytes(tx.data) || !isUint(tx.value) || !nonzero(tx.gas)
      || !nonzero(tx.gasPrice) || !isUint(quote.totalNetworkFee)
      || BigInt(tx.value) > BigInt(req.sellAmount)
      || (req.sellToken !== NATIVE && tx.value !== '0')
      || (req.sellToken === NATIVE && tx.value !== req.sellAmount)) {
    throw new Error('unsafe_or_unavailable_quote');
  }
  if (allowance != null && (typeof allowance !== 'object' || !eq(allowance.spender, ALLOWANCE_HOLDER)
      || !isUint(allowance.actual))) {
    throw new Error('untrusted_allowance_target');
  }
  if (req.sellToken === NATIVE && allowance != null)
    throw new Error('native_token_must_not_approve');
  if (issues.balance && (issues.balance.actual == null || !isUint(issues.balance.actual)))
    throw new Error('invalid_balance');
  if (issues.balance && BigInt(issues.balance.actual) < BigInt(req.sellAmount))
    throw new Error('insufficient_balance');
  // AllowanceHolder is fixed as the approved spender; the Settler execution
  // destination is dynamic and supplied by the trusted 0x API, never Mini App JS.
  return Object.freeze({
    chainId: Number(BASE_CHAIN),
    taker: req.taker,
    sellToken: body.sellToken,
    buyToken: body.buyToken,
    sellAmount: req.sellAmount,
    buyAmount: quote.buyAmount,
    minBuyAmount: quote.minBuyAmount,
    gas: tx.gas,
    gasPrice: tx.gasPrice,
    totalNetworkFee: quote.totalNetworkFee,
    slippageBps: req.slippageBps,
    spender: ALLOWANCE_HOLDER,
    requiresApproval: allowance != null && BigInt(allowance.actual) < BigInt(req.sellAmount),
    transaction: {to: tx.to.toLowerCase(), data: tx.data, value: tx.value, gas: tx.gas},
    fetchedAt: Date.now(),
    expiresAt: Date.now() + 20000,
  });
}

export async function fetchFirmQuote(body, apiKey, options = {}) {
  const request = validateQuoteRequest(body);
  if (typeof apiKey !== 'string' || apiKey.length < 12 || apiKey.length > 4096)
    throw new Error('quote_provider_not_configured');
  const params = new URLSearchParams({
    chainId: BASE_CHAIN, sellToken: request.sellToken,
    buyToken: request.buyToken, sellAmount: request.sellAmount,
    taker: request.taker, slippageBps: String(request.slippageBps),
  });
  const upstream = await (options.fetch || fetch)(
    'https://api.0x.org/swap/allowance-holder/quote?' + params.toString(), {
      method: 'GET', redirect: 'error', signal: AbortSignal.timeout(8500),
      headers: {'0x-api-key': apiKey, '0x-version': 'v2', 'Accept': 'application/json'},
    });
  if (!upstream.ok) throw new Error('quote_http_' + upstream.status);
  const length = Number(upstream.headers?.get('content-length') || 0);
  if (length > 262144) throw new Error('quote_too_large');
  const response = await upstream.text();
  if (response.length > 262144) throw new Error('quote_too_large');
  return normalizeFirmQuote(body, JSON.parse(response));
}
