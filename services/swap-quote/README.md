# GenyCaster Base Swap — authenticated quote integration

The Mini App SDK `actions.swapToken` **is not advertised as supported** until the
full approval, receipt verification, quote-refresh, swap execution, and user
consent state machine is audited and verified. A read-only quote is **not** an
executable trade; do not work around the manifest capability gate.

## Backend

The existing account-sync API handles `POST /api/v1/cloud/swap/quote`.
Its middleware verifies the GenyCaster/OpenProof account principal via
a bearer token or a Secure HttpOnly session cookie plus CSRF token. No
mini-app-supplied identity, arbitrary endpoint, or private wallet data is
accepted. The account gets a maximum of 12 quote calls per minute.

Set `GENYCASTER_0X_API_KEY` **only on the account-sync server**, via its
secret manager. It must never be included in the Qt application, QML,
WASM bundle, client deployment resources, or CI logs. Without the
credential, the endpoint returns HTTP 503 `swap_quote_not_configured`.
Do not try to provision a credential by scraping an external user's key.

The isolated `server.mjs` is an optional loopback-only, service-to-service
broker for separate deployments. Set its `GENYCASTER_SWAP_BROKER_TOKEN`
to an independently generated secret, protect it at the reverse proxy, and
do not expose the endpoint directly to browsers. The main account-sync
integration does **not** require this separate service.

The server requests an 0x v2 AllowanceHolder quote on Base (8453) using
`https://api.0x.org/swap/allowance-holder/quote` over HTTPS with redirects
disabled. Both server and Qt C++ client validate chain, wallet taker,
exact CAIP-19 token pair, base-unit amount, minimum expected receive,
network fee, time-to-live, and the fixed token-approval spender
`0x0000000000001ff3684f28c67538d4d072c22734`.

The dynamic 0x Settler address is read from **`transaction.to`**. It
is **not** the ERC-20 allowance spender. NEVER approve the Settler.
The only authorized ERC-20 approval destination is AllowanceHolder,
and the supplied `approve(address,uint256)` calldata is for the exact
requested amount, not unlimited allowance.

## Client

The runtime preserves the originating Mini App document generation and
requires a foreground session, wallet account, and Base chain identity
before sending a quote request through the trusted Qt account client.
The first-party modal allows editing token prefills and base-unit amount,
and invalidates stale quotes after changes or expiration.

The network adapter cannot expose the OpenProof bearer, CSRF token,
0x API key or quote response to the embedded Mini App iframe.
No wallet approval or on-chain transaction is initiated in the quote path.

## Verification

```sh
node --test services/swap-quote/quote.test.mjs \
  services/account-sync/test/cloud_state.test.mjs \
  tests/wasm/miniapp_swap_preflight_20261118.test.mjs \
  tests/wasm/miniapp_swap_quote_contract_20261118.test.mjs

cmake -S tests/miniapp -B build/swap-intent-check -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --build build/swap-intent-check --parallel 2
ctest --test-dir build/swap-intent-check --output-on-failure
```

Before enabling `actions.swapToken` for published Mini Apps, add and
test a permission-aware **user-initiated** execution coordinator with
fresh quotes after approvals, exact spender and amount checks, chain and
account revalidation before signing, transaction receipt/status tracking,
and proper Farcaster SDK success/failure results. Test with non-custodial
wallets on Base and a controlled funded test account, never a production
user wallet without explicit approval.
