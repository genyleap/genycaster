import test from 'node:test';
import assert from 'node:assert/strict';
import {once} from 'node:events';
import {
  constants,validateQuoteRequest,encodeGetAmountsOut,
  encodeSwapTransaction,createRpcClient,fetchFirmQuote
} from './quote.mjs';
import {createQuoteServer} from './server.mjs';

const usdc='0x833589fcd6edb6e08f4c7c32d4f71b54bda02913';
const token='0x4567890123456789012345678901234567890123';
const taker='0x1234567890123456789012345678901234567890';
const request = () => ({
  sellToken:'eip155:8453/erc20:'+usdc,
  buyToken:'eip155:8453/native',
  sellAmount:'1000000',taker,slippageBps:100
});
const word=n=>BigInt(n).toString(16).padStart(64,'0');
const amountReturn=(length,input,output)=>'0x'+word(32)+word(length)
  +Array.from({length},(_,i)=>word(i===0?input:i===length-1?output:12000000n)).join('');

function mockRpc({allowance=0n,balance=100000000000000000000n,chain='0x2105',
  gas='0x30d40',revert=false,estimateError=false, calls=[]}={}) {
  return async(method,params)=>{
    calls.push({method,params});
    if(method==='eth_chainId')return chain;
    if(method==='eth_gasPrice')return '0x0bebc200';
    if(method==='eth_getBalance')return '0x'+balance.toString(16);
    if(method==='eth_estimateGas') {
      if(estimateError)throw new Error('revert');
      return gas;
    }
    if(method==='eth_call') {
      const [tx]=params;
      const prefix=tx.data.slice(0,10);
      if(prefix==='0xd06ca61f') {
        assert.equal(tx.to,constants.ROUTER);
        const length=Number(BigInt('0x'+tx.data.slice(10+64+64,10+64+64+64)));
        if(revert)throw new Error('pool absent');
        return amountReturn(length,1000000n,length===3?1000000000000000n:500000000000000n);
      }
      if(prefix==='0x70a08231')return '0x'+word(balance);
      if(prefix==='0xdd62ed3e')return '0x'+word(allowance);
    }
    throw new Error('unexpected_rpc_method_'+method);
  };
}

test('requests use fixed Base CAIP-19 assets, exact uint256 and bounded slippage',()=>{
  const actual=validateQuoteRequest(request());
  assert.equal(actual.sellAmount,'1000000');
  for(const bad of [
    {...request(), chainId:1},{...request(),sellToken:'eip155:1/native'},
    {...request(),buyToken:request().sellToken},
    {...request(),buyToken:'eip155:8453/erc20:0x'+'0'.repeat(40)},
    {...request(),buyToken:'eip155:8453/erc20:'+constants.WETH,sellToken:'eip155:8453/native'},
    {...request(),sellAmount:'0'},{...request(),sellAmount:'-1'},
    {...request(),sellAmount:'0'.repeat(100)},{...request(),sellAmount:'9'.repeat(79)},
    {...request(),taker:'0x'+'0'.repeat(40)},
    {...request(),slippageBps:0},{...request(),slippageBps:301},
    {...request(),slippageBps:100.5},{...request(),to:taker},
    null,{},[]
  ])assert.throws(()=>validateQuoteRequest(bad));
});
test('on-chain quote uses Uniswap V2 Router02 and correct fee-bounded token-to-ETH ABI',async()=>{
  const calls=[];
  const result=await fetchFirmQuote(request(),undefined,{
    rpc:mockRpc({calls})
  });
  assert.equal(result.provider,'uniswap-v2-onchain');
  assert.equal(result.spender,constants.ROUTER);
  assert.equal(result.transaction.to,constants.ROUTER);
  assert.equal(result.transaction.value,'0');
  assert.ok(result.transaction.data.startsWith('0x18cbafe5'));
  assert.equal(result.minBuyAmount,'495000000000000');
  assert.deepEqual(result.route,[usdc,constants.WETH]);
  assert.equal(result.requiresApproval,true);
  assert.ok(result.deadline>=Math.floor(Date.now()/1000)+170);
  assert.ok(result.expiresAt>result.fetchedAt);
  assert.ok(calls.some(c=>c.method==='eth_call' && c.params[0].data.startsWith('0xd06ca61f')));
  assert.ok(calls.some(c=>c.method==='eth_call' && c.params[0].data.startsWith('0xdd62ed3e')));
  assert.ok(!calls.some(c=>c.method==='eth_estimateGas'),'wallet may not have allowance yet');
  assert.ok(!JSON.stringify(result).includes('api.0x.org'));
});
test('native ETH swaps send exact value and have zero ERC20 approval',async()=>{
  const body={...request(),sellToken:'eip155:8453/native',buyToken:'eip155:8453/erc20:'+usdc,
    sellAmount:'1000000000000000'};
  const calls=[];
  const result=await fetchFirmQuote(body,undefined,{
    rpc:mockRpc({calls})
  });
  assert.equal(result.transaction.value,body.sellAmount);
  assert.ok(result.transaction.data.startsWith('0x7ff36ab5'));
  assert.equal(result.requiresApproval,false);
  assert.deepEqual(result.route,[constants.WETH,usdc]);
  assert.ok(calls.some(c=>c.method==='eth_estimateGas'));
  assert.ok(!calls.some(c=>c.method==='eth_call' && c.params[0].data.startsWith('0xdd62ed3e')));
});
test('token-to-token swap compares direct and WETH intermediate routes',async()=>{
  const body={...request(),buyToken:'eip155:8453/erc20:'+token};
  const result=await fetchFirmQuote(body,undefined,{rpc:mockRpc()});
  assert.deepEqual(result.route,[usdc,constants.WETH,token]);
  assert.equal(result.buyAmount,'1000000000000000');
  assert.ok(result.transaction.data.startsWith('0x38ed1739'));
});
test('quote suppresses unsupported or dangerous trades without fabricating outputs',async()=>{
  await assert.rejects(fetchFirmQuote(request(),undefined,{
    rpc:mockRpc({chain:'0x1'})}),/wrong_base_rpc_chain/);
  await assert.rejects(fetchFirmQuote(request(),undefined,{
    rpc:mockRpc({revert:true})}),/no_liquid_uniswap_v2_route/);
  await assert.rejects(fetchFirmQuote(request(),undefined,{
    rpc:mockRpc({balance:1n})}),/insufficient_balance/);
  const alreadyApproved=await fetchFirmQuote(request(),undefined,{
    rpc:mockRpc({allowance:1000000n})
  });
  assert.equal(alreadyApproved.requiresApproval,false);
  const noGas=()=>fetchFirmQuote(request(),undefined,{
    rpc:mockRpc({allowance:1000000n,estimateError:true})
  });
  await assert.rejects(noGas(),/swap_simulation_failed/);
});
test('pure calldata constructors use verified function selectors, ABI path offsets and deadlines',()=>{
  const path=[usdc,constants.WETH];
  const query=encodeGetAmountsOut('1000000',path);
  assert.ok(query.startsWith('0xd06ca61f'));
  assert.equal(query.slice(10,10+64),word(1000000));
  assert.equal(query.slice(74,138),word(64));
  const swap=encodeSwapTransaction(request(),'500000000000000',path,1800000000);
  assert.equal(swap.to,constants.ROUTER);
  assert.ok(swap.data.startsWith('0x18cbafe5'));
  assert.equal(swap.data.slice(10,74),word(1000000));
  assert.equal(swap.data.slice(74,138),word(495000000000000n));
  assert.equal(swap.data.slice(138,202),word(160));
  assert.equal(swap.data.slice(202,266),word(BigInt(taker)));
  assert.equal(swap.data.slice(266,330),word(1800000000));
  assert.equal(swap.data.slice(330,394),word(2));
  const eth=encodeSwapTransaction({...request(),sellToken:'eip155:8453/native',
    buyToken:'eip155:8453/erc20:'+usdc},'1000',[constants.WETH,usdc],1800000000);
  assert.ok(eth.data.startsWith('0x7ff36ab5'));
  assert.equal(eth.data.slice(74,138),word(128));
  assert.equal(eth.value,'1000000');
});
test('RPC client uses HTTPS, method-specific responses and concurrent request IDs',async()=>{
  const seen=[];
  const rpc=createRpcClient('https://mainnet.base.org',async(url,init)=>{
    assert.equal(url,'https://mainnet.base.org/');
    const req=JSON.parse(init.body);
    seen.push(req);
    assert.equal(init.redirect,'error');
    assert.equal(init.method,'POST');
    return {ok:true,headers:{get:()=>null},text:async()=>JSON.stringify({
      id:req.id,jsonrpc:'2.0',result:req.method==='eth_chainId'?'0x2105':'0x1'
    })};
  });
  const result=await Promise.all([rpc('eth_chainId',[]),rpc('eth_gasPrice',[])]);
  assert.deepEqual(result,['0x2105','0x1']);
  assert.deepEqual(seen.map(s=>s.id),[1,2]);
  assert.throws(()=>createRpcClient('http://untrusted.invalid'));
  assert.throws(()=>createRpcClient('https://user:pass@untrusted.invalid'));
  assert.throws(()=>createRpcClient('https://example.com/?redirect=https://bad.invalid'));
});
test('optional standalone broker requires private ingress token, never a commercial API key',async()=>{
  const token='a'.repeat(64);
  let calls=0;
  const server=createQuoteServer({accessToken:token,rpcUrl:'https://mainnet.base.org',
    fetchImpl:async(_url,init)=>{
      calls++;
      const message=JSON.parse(init.body);
      const result=await mockRpc() (message.method,message.params);
      return {ok:true,headers:{get:()=>null},text:async()=>JSON.stringify({
        jsonrpc:'2.0',id:message.id,result
      })};
    }});
  server.listen(0,'127.0.0.1');
  await once(server,'listening');
  const origin='http://127.0.0.1:'+server.address().port;
  const send=(body,auth)=>fetch(origin+'/v1/quote',{
    method:'POST',
    headers:{'Content-Type':'application/json',...(auth?{'x-genycaster-broker-token':token}:{})},
    body:JSON.stringify(body)
  });
  try {
    assert.equal((await send(request(),false)).status,401);
    assert.equal((await send({...request(),sellAmount:'0'},true)).status,422);
    const response=await send(request(),true);
    assert.equal(response.status,200);
    const json=await response.json();
    assert.equal(json.spender,constants.ROUTER);
    assert.equal(json.provider,'uniswap-v2-onchain');
    assert.ok(calls>0);
  }finally{server.close();await once(server,'close');}
});
