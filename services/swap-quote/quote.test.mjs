import test from 'node:test';
import assert from 'node:assert/strict';
import {once} from 'node:events';
import {constants,validateQuoteRequest,normalizeFirmQuote,fetchFirmQuote} from './quote.mjs';
import {createQuoteServer} from './server.mjs';

const request = () => ({
  sellToken:'eip155:8453/erc20:0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913',
  buyToken:'eip155:8453/native',
  sellAmount:'1000000',
  taker:'0x1234567890123456789012345678901234567890',
  slippageBps:100,
});
const response = () => ({
  liquidityAvailable:true,
  chainId:8453,
  allowanceTarget:constants.ALLOWANCE_HOLDER,
  buyAmount:'500000000000000',
  minBuyAmount:'495000000000000',
  sellToken:'0x833589fcd6edb6e08f4c7c32d4f71b54bda02913',
  buyToken:constants.NATIVE,
  sellAmount:'1000000',
  gas:'141000',
  gasPrice:'12500000',
  totalNetworkFee:'1762500000000',
  issues:{balance:{actual:'2000000'},allowance:{actual:'0',spender:constants.ALLOWANCE_HOLDER},simulationIncomplete:false},
  transaction:{to:'0x7F6cEE965959295cC64d0E6c00d99d6532d8e86b',data:'0x1234567800',gas:'141000',gasPrice:'12500000',value:'0'},
});
test('quote request restricts chain, addresses, positive uint256 amount and slippage',()=>{
  const actual=validateQuoteRequest(request());
  assert.equal(actual.sellAmount,'1000000');
  assert.equal(actual.slippageBps,100);
  for(const input of [
    {...request(),chainId:1},{...request(),sellToken:'eip155:1/native'},
    {...request(),buyToken:request().sellToken},{...request(),buyToken:'eip155:8453/erc20:0x'+'0'.repeat(40)},
    {...request(),sellAmount:'0'},{...request(),sellAmount:'-1'},
    {...request(),sellAmount:1e6},{...request(),sellAmount:'1.4'},
    {...request(),sellAmount:'0'.repeat(89)},{...request(),sellAmount:'9'.repeat(79)},
    {...request(),taker:'0x'+'0'.repeat(40)},{...request(),slippageBps:301},
    {...request(),slippageBps:0},{...request(),slippageBps:100.5},
    {...request(),to:'0xdeadbeef'},null,[],{},
  ]) assert.throws(()=>validateQuoteRequest(input),undefined,JSON.stringify(input));
});
test('accept a normalized firm quote only for audited Base AllowanceHolder',()=>{
  const quote=normalizeFirmQuote(request(),response());
  assert.equal(quote.requiresApproval,true);
  assert.equal(quote.spender,constants.ALLOWANCE_HOLDER);
  assert.equal(quote.taker,request().taker);
  assert.equal(quote.transaction.to,'0x7f6cee965959295cc64d0e6c00d99d6532d8e86b');
  assert.equal(quote.sellAmount,'1000000');
  assert.ok(quote.expiresAt>quote.fetchedAt);
  const officialShape=response();
  delete officialShape.chainId; // The documented 0x v2 response may omit chainId.
  assert.equal(normalizeFirmQuote(request(),officialShape).chainId,8453);
  const noApproval=response();
  noApproval.issues.allowance.actual='1000000';
  assert.equal(normalizeFirmQuote(request(),noApproval).requiresApproval,false);
});
test('reject quote router, arbitrary approvals, wrong tokens, fake liquidity, value, malformed calldata',()=>{
 for(const [key,modified] of Object.entries({
  router:{transaction:{...response().transaction,to:request().taker}},
  badAllowanceTarget:{allowanceTarget:request().taker},
  wrongChain:{chainId:1},
  spender:{issues:{...response().issues,allowance:{actual:'0',spender:'0x1234567890123456789012345678901234567890'}}},
  balance:{issues:{...response().issues,balance:{actual:-1}}},
  insufficientBalance:{issues:{...response().issues,balance:{actual:'1'}}},
  mintoken:{minBuyAmount:'600000000000000'},
  liquidity:{liquidityAvailable:false},simulation:{issues:{...response().issues,simulationIncomplete:true}},
  sellamount:{sellAmount:'2000000'},selltoken:{sellToken:constants.NATIVE},
  buytoken:{buyToken:request().sellToken},txvalue:{transaction:{...response().transaction,value:'1000'}},
  data:{transaction:{...response().transaction,data:'0x'}},
  gas:{transaction:{...response().transaction,gas:'0'}},
  invalidSource:{issues:{...response().issues,invalidSourcesPassed:['unknown']}},
 })) assert.throws(()=>normalizeFirmQuote(request(),{...response(),...modified}),undefined,key);
});
test('broker never transmits key to caller and requests 0x v2 with exact Base params',async()=>{
  const result=await fetchFirmQuote(request(),'secret-key-long-enough', {fetch:async(url,options)=>{
    const endpoint=new URL(url);
    assert.equal(endpoint.origin,'https://api.0x.org');
    assert.equal(endpoint.pathname,'/swap/allowance-holder/quote');
    assert.equal(endpoint.searchParams.get('chainId'),'8453');
    assert.equal(endpoint.searchParams.get('sellAmount'),'1000000');
    assert.equal(endpoint.searchParams.get('taker'),request().taker);
    assert.equal(options.headers['0x-version'],'v2');
    assert.equal(options.headers['0x-api-key'],'secret-key-long-enough');
    assert.equal(options.redirect,'error');
    return {ok:true,headers:{get:()=>null},text:async()=>JSON.stringify(response())};
  }});
  assert.ok(!JSON.stringify(result).includes('secret-key'));
});
test('server binds quote endpoint to private ingress token and validates inputs',async()=>{
  let hits=0;
  const server=createQuoteServer({
    apiKey:'secret-key-long-enough',
    accessToken:'a'.repeat(64),
    fetchImpl:async()=>{hits++;return {ok:true,headers:{get:()=>null},text:async()=>JSON.stringify(response())}},
  });
  server.listen(0,'127.0.0.1');
  await once(server,'listening');
  const {port}=server.address();
  const origin='http://127.0.0.1:'+port;
  try {
    const untrusted=await fetch(origin+'/v1/quote',{method:'POST',body:JSON.stringify(request())});
    assert.equal(untrusted.status,401);
    const invalid=await fetch(origin+'/v1/quote',{method:'POST',headers:{'x-genycaster-broker-token':'a'.repeat(64)},body:JSON.stringify({...request(),sellAmount:'0'})});
    assert.equal(invalid.status,422);
    const trusted=await fetch(origin+'/v1/quote',{method:'POST',headers:{'x-genycaster-broker-token':'a'.repeat(64)},body:JSON.stringify(request())});
    assert.equal(trusted.status,200);
    assert.equal(hits,1);
    const parsed=await trusted.json();
    assert.equal(parsed.transaction.to,'0x7f6cee965959295cc64d0e6c00d99d6532d8e86b');
    assert.equal(parsed.requiresApproval,true);
    assert.equal(trusted.headers.get('cache-control'),'no-store, private');
  }finally{server.close();await once(server,'close');}
});
