/**
 * Read-only Base RPC validation: confirms chain, Router02 bytecode and a
 * current USDC/WETH V2 quote without a wallet, signature or paid API.
 *
 * Usage: node services/swap-quote/smoke-readonly.mjs
 */
import {constants,createRpcClient,encodeGetAmountsOut} from './quote.mjs';

const rpc=createRpcClient(
  process.env.GENYCASTER_BASE_RPC_URL ||
  process.env.GENYCASTER_GENY_RPC_URL ||
  'https://mainnet.base.org');
try {
  const chain=await rpc('eth_chainId',[]);
  if(BigInt(chain)!==8453n)throw new Error('Wrong chain');
  const code=await rpc('eth_getCode',[constants.ROUTER,'latest']);
  if(!/^0x[0-9a-fA-F]{64,}$/.test(code))throw new Error('Router02 not deployed');
  const usdc='0x833589fcd6edb6e08f4c7c32d4f71b54bda02913';
  const result=await rpc('eth_call',[{
    to:constants.ROUTER,
    data:encodeGetAmountsOut('1000000',[usdc,constants.WETH])
  },'latest']);
  if(!/^0x[0-9a-fA-F]{256,}$/.test(result))throw new Error('Invalid pair quote');
  console.log('PASS: Base 8453, Uniswap V2 Router02 deployed, direct USDC/WETH quote returned');
} catch(error) {
  console.error('Read-only Base smoke failed:',String(error?.message||error));
  process.exitCode=1;
}
