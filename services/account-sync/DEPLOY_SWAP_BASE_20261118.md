# Mini App Base Swap deployment — 18 Nov 2026

The shipped client SDK `actions.swapToken` is guarded by the public,
read-only endpoint `GET https://genycaster.xyz/api/v1/cloud/swap/status`.
It must respond with HTTP 200 and `{"available":true,"chainId":8453}` after
checking the actual Base RPC chain and deployed Uniswap V2 Router02 code.
A signed Mini App manifest requiring Swap must never bypass this guard.

## Operational rollout

The production PM2 process `genycaster-account-api` runs
`/opt/genycaster/caster-deployment/account-api-runner.mjs`.
On 18 Nov it was still importing the 17 Nov pinned release, so the new Swap
status route returned HTTP 404 even when the Qt source contained the feature.
This is **deployment drift**, not a manifest-validation bug.

A new immutable snapshot is staged at
`/opt/genycaster/caster-deployment/account-api-releases/uniswap-v2-20261118/`
using the previously deployed modules plus the tested source modules
`server.mjs`, `cloud_state.mjs`, `pg_store.mjs`,
`genybuzz.mjs` and `services/swap-quote/`.
Runtime vendor files and the shared Node dependency tree remain available
inside the snapshot.

The pinned runner imports `createAccountApi` and
`createBaseSwapBackend` from this snapshot. No private API key and no
commercial quote provider is required. The previous runner was saved to
`account-api-runner.before-uniswap-v2-20261118.mjs` under
`/opt/genycaster/caster-deployment/`.

The read-only canary checked chain/router readiness, `swap/status = 200`,
existing Pro plan `= 200`, unauthenticated session `= 401` and health
`= 200` before production restart. After restart the public
`genycaster.xyz/api/v1/cloud/swap/status` and Pro plan both returned 200.

For an authorized rollback, restore the saved runner, restart the single
PM2 account API process, then verify the Pro plan and authentication
endpoints. Do not copy stale services over current OpenProof, Geny Pro or
GenyBuzz production files.

## Client readiness and sign-in

`MiniAppHostController` now tracks whether the first Swap readiness probe
has resolved. Manifest validation can wait briefly for a pending probe before
deciding required capabilities. The signed account association, published
required chain and capability checks remain strict.

Framedl uses SIWF and requires a wallet matching the active **Farcaster**
FID's custody address or a registered authorization address. A generic
EVM wallet or a HyperSnap identity is not an equivalent cryptographic
authorization. The host now explains wallet selection when verification
fails, but never returns fabricated signed-in credentials.

Live, funded swap execution and Framedl sign-in with the user's real
custody/auth wallet still require consent-based macOS device testing.
