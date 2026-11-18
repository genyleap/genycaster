# GenyCaster Mini App Swap — direct Uniswap V2 on Base

## Provider model

The swap and quote path is direct Ethereum JSON-RPC plus the on-chain
Uniswap V2 Router02 contract. **No paid or commercial swap API key,
aggregator, subscription, or proprietary trading provider is required.**

Official addresses (Base chain ID 8453):
- Router02: 0x4752ba5dbc23f44d87826276bf6fd6b1c372ad24
- WETH9: 0x4200000000000000000000000000000000000006
- Reference: https://developers.uniswap.org/docs/protocols/v2/deployments
- Router ABI: https://github.com/Uniswap/v2-periphery/blob/master/contracts/interfaces/IUniswapV2Router02.sol

The server queries getAmountsOut(uint256,address[]) through eth_call.
Quotes select the best liquid direct pair or WETH-bridged path (at most
two hops). Transaction calldata for swapExactTokensForTokens,
swapExactTokensForETH and swapExactETHForTokens is built deterministically.
The Qt client independently reproduces that calldata, checks every
argument and refuses untrusted recipients or approval spenders.

## Deployment configuration

GENYCASTER_BASE_RPC_URL: optional HTTPS Base JSON-RPC endpoint. Uses
the existing GENYCASTER_GENY_RPC_URL if available, otherwise the public
https://mainnet.base.org endpoint. No API credential is needed. The
network is verified with eth_chainId and the router bytecode check
before the SDK capability is advertised.

The public Base RPC is free but rate-limited and explicitly **not
suitable for production load** (Base docs, as of November 2026).
Genyleap may host its own RPC node or use an existing properly
provisioned endpoint for reliable high-volume traffic. This is an RPC
infrastructure concern, not a paid swap API dependency.

The account-sync endpoint POST /api/v1/cloud/swap/quote remains
OpenProof-authenticated, user-scoped, rate-limited and protected by
CSRF for browser cookie sessions. The embedded Mini App receives no
secret tokens, quote authority, calldata parameters or direct wallet
signing ability. An optional loopback-only standalone broker is
available for private server-to-server deployments.

## Trading costs and coverage

The swap service has no usage fee or trading API charge. **Swaps are
not economically free**: Base network gas, liquidity-pool trading
fees and price impact still apply. The current implementation routes
Uniswap **V2 pools only**; it cannot promise liquidity in V3/V4-only
markets or support fee-on-transfer/rebasing tokens.

Users see exact input, expected output, minimum output based on their
10–300 basis point slippage, and estimated network fee. Missing pools,
insufficient balance and failed simulation are rejected, not
reported as successful quotes.

## Transaction lifecycle

1. Check current wallet account, chain ID and authenticated account.
2. Get on-chain quote, verify Router02, exact amount, route, 20-second
   freshness, contract deadline and user-selected slippage.
3. If required, user explicitly approves **only the exact ERC-20
   sell amount to Router02**, with an individual wallet confirmation.
4. Wait for mined successful approval receipt, refresh the quote and
   require a **second independent click** for the actual swap.
5. Revalidate wallet account and Base network before any wallet send.
   Let the EIP-1193 wallet estimate the actual transaction gas.
6. Check the mined swap receipt and respond with Farcaster SDK
   success only after a successful, matching on-chain transaction.
   Reverts, declined transactions, timeouts and navigation invalidations
   cannot fabricate success.

## Local validation

Run:

  node --test services/swap-quote/quote.test.mjs services/account-sync/test/cloud_state.test.mjs tests/wasm/miniapp_swap_preflight_20261118.test.mjs tests/wasm/miniapp_swap_quote_contract_20261118.test.mjs tests/wasm/miniapp_swap_execution_20261118.test.mjs

  cmake -S tests/miniapp -B build/swap-intent-check -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
  cmake --build build/swap-intent-check --parallel 1
  ctest --test-dir build/swap-intent-check --output-on-failure

Before public distribution of the financial feature, execute read-only
live Base RPC validation and controlled low-value trades with a funded
test wallet. No keys, seed phrases or private signing material should
be stored by this service.
