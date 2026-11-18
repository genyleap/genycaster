# GenyCaster Base Swap — Farcaster `actions.swapToken`

## Deployment gate

`actions.swapToken` is advertised to Mini Apps only when **both** conditions
are met: (1) an EIP-1193 wallet supporting Base (CAIP-2 `eip155:8453`)
is available and (2) the account-sync API's read-only
`GET /api/v1/cloud/swap/status` confirms the backend quote provider and
cloud identity store are configured. The client refreshes this health
capability on startup and every two minutes. Without either prerequisite
the capability remains **disabled**; no phantom SDK support.

Provide `GENYCASTER_0X_API_KEY` only in the account-sync backend's secrets
environment. **Never** ship it inside Qt binaries, wasm artifacts, QML,
Mini App JS, CI logs, or the remote Mini App. Configure a real 0x v2 key
from the 0x Dashboard. Restart the account-sync service after deployment
to load the new server code and environment. The backend requires a
verified GenyCaster/OpenProof principal via bearer token or secure cookie
plus CSRF for `POST /api/v1/cloud/swap/quote`. Native Qt traffic has no
browser `Origin` header; cross-origin browser POSTs are rejected.

The optional `services/swap-quote/server.mjs` standalone broker is
loopback-only and requires its separate server-to-server ingress token. It
is not required by the account-sync integration.

## Execution

1. The Mini App proposes a sell token, buy token, and optional raw base-unit
   amount. The host opens a **first-party** modal and lets the user edit
   all prefilled values. Only Base CAIP-19 token IDs are accepted.
2. User connects an EIP-1193 wallet. The host verifies live account and
   `eth_chainId = 0x2105`, then requests a firm 0x v2 AllowanceHolder quote
   through the authenticated account-sync API. 0x keys stay server-side.
3. The server validates token pair, 256-bit amount, taker, slippage,
   liquidity, balance, gas, network fee and allowance spender. The Qt
   runtime **independently** validates chain, address, quote expiry, amounts,
   router and allowance spender before using any quote.
4. The host displays minimum output, fees, required approval and an exact
   confirmation button. ERC-20 approvals are **not automatic**: each
   approval requires a separate user click and wallet confirmation. The
   `approve(address,uint256)` calldata grants only the exact amount to
   Base AllowanceHolder
   `0x0000000000001ff3684f28c67538d4d072c22734`.
   The 0x execution `transaction.to` can be a *different Settler*.
   **Never grant token allowances to the Settler.**
5. The host waits for a mined Base receipt with `status = 0x1` and
   matching transaction hash, sender, receiver and block. On confirmed
   approval, the previous quote is discarded and a **new firm quote** is
   fetched. The swap cannot proceed automatically; the user sees it and
   must explicitly tap **Confirm token swap**.
6. Immediately before swap, the host rechecks account and network and
   rejects stale quotes. The wallet signs/broadcasts via `eth_sendTransaction`.
   The host waits for a confirmed mined receipt and only then resolves
   Farcaster `{success:true,swap:{transactions:[approval?,swapHash]}}`.
   Reverts, invalid receipts, declined signatures and timeouts never
   fabricate successes.

Once `eth_sendTransaction` has been submitted, dismissing the first-party
modal or switching Mini Apps is blocked until the receipt is checked.
A broadcast transaction itself **cannot be cancelled**; users should
verify its hash on a trusted Base block explorer if the network times
out. No wallet private keys, recovery words or on-chain signature
capabilities are passed to the Mini App SDK.

## Validation

```sh
node --test services/swap-quote/quote.test.mjs \
  services/account-sync/test/cloud_state.test.mjs \
  tests/wasm/miniapp_swap_preflight_20261118.test.mjs \
  tests/wasm/miniapp_swap_quote_contract_20261118.test.mjs \
  tests/wasm/miniapp_swap_execution_20261118.test.mjs

cmake -S tests/miniapp -B build/swap-intent-check -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --build build/swap-intent-check --target swap_runtime_compile --parallel 1
cmake --build build/swap-intent-check --target swap_host_compile --parallel 1
cmake --build build/swap-intent-check --target swap_intent_contract swap_quote_contract --parallel 1
ctest --test-dir build/swap-intent-check --output-on-failure
```

**Launch condition:** Successful offline tests and compilation are not a
substitute for an end-to-end transaction with a real funded test wallet on
Base and a live authenticated 0x key. Do not call this deployment live
ready until the backend status reports available and the EIP-1193
approval / quote refresh / swap / receipt flow is exercised safely.
