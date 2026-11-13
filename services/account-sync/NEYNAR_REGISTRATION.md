# Neynar registration relay — security and deployment

## Scope

GenyCaster supports two distinct identities, not two aliases for one FID:

- **HyperSnap Native**: native HyperSnap FID; proof of work, NATIVE_ONBOARD,
  native shard-0 confirmation, and an independent signer.
- **Farcaster/Neynar**: Optimism IdRegistry FID, reserved by the same
  Neynar app wallet used to submit the signed custody transfer.

The Neynar relay is a first-party service, implemented in:
`neynar_registration_relay.mjs`, `neynar_id_registry.mjs`,
`neynar_registration_http.mjs`, `neynar_enrollment_voucher.mjs`, and
`neynar_registration_store.mjs`. It never receives a mnemonic or private key.

## Fail-closed requirements

Registration endpoints are **not served by default**. Setting
`GENYCASTER_NEYNAR_REGISTRATION_ENABLED=true` on the account service
requires **all** of the following before the service starts:

- PostgreSQL migration `migrations/013_neynar_registration.sql`, applied
  to the isolated GenyCaster account database (no OpenProof database access).
- `GENYCASTER_NEYNAR_API_KEY` — Neynar API credential, server only.
- `GENYCASTER_NEYNAR_WALLET_ID` — same funded Neynar app wallet for
  `GET /v2/farcaster/user/fid` and `POST /v2/farcaster/user/`.
- `GENYCASTER_OPTIMISM_RPC_URL` — pinned, credential-free HTTPS origin
  supporting `eth_call` with the `safe` block tag.
- `GENYCASTER_NEYNAR_ENROLLMENT_SECRET` — a random, server-only secret of
  at least 32 characters used to verify **paid/sponsored enrollment vouchers**.

**IMPORTANT:** A payment verification and voucher issuing workflow must be
provided by a trusted first-party operator before a customer can enroll.
The account API does **not** mint vouchers, accept client-asserted payments,
or authorize FID allocations based on an arbitrary custody address or FID.
A developer may inject an already verified enrollment policy for tests; that
is not a payment gateway. Do not enable the feature for unrestricted public
enrollment without a production payment/refund, abuse and quota policy.

## Signed enrollment vouchers

`createPaidEnrollmentVerifier({secret})` creates a voucher *issuer* for
trusted operators and an *authorize* method for the relay. A voucher binds
`(invoice UUID, exact EVM custody address, expiry timestamp)` under an HMAC.
The token must only be issued **after** independent payment verification
or an explicitly approved sponsorship. Its issuer is not exposed as an HTTP
route. A lease is permanently unique per paid invoice **and** per custody address;\nPostgreSQL enforces one FID allocation per paid invoice even if a second\nrequest supplies a different wallet.
The voucher does not and cannot replace the EIP-712 custody authorization.

The account server expects an already-issued voucher in the
`x-genycaster-paid-enrollment` header. All three endpoints are same-origin
for browsers, POST-only, no-store, and strict JSON:

- `POST /api/v1/registration/farcaster/reserve`:
  `{"custody_address":"0x…","idempotency_key":"uuid"}`
- `POST /api/v1/registration/farcaster/submit`:
  `{"custody_address":"0x…","reservation_id":"uuid","deadline":"unix-seconds","signature":"0x…","fname":"name"}`
- `POST /api/v1/registration/farcaster/status`:
  `{"custody_address":"0x…","reservation_id":"uuid"}`

On an active reservation, the response also includes `id_registry_nonce` as
an exact decimal **string** read from the canonical Optimism `safe` block.
If the RPC is temporarily unavailable it is `null` (the reservation stays
valid and can be queried again). The native GenyCaster wallet signs the exact
FID, nonce and deadline locally with its EIP-712 IdRegistry signer; the
mnemonic is never a QML property or an HTTP payload.

`reserve` atomically records the request **before** making the
cost-bearing Neynar GET. It rejects an already registered custody wallet
according to the Optimism registry. A duplicate concurrent or restarted
request may only return the previously stored reservation; it cannot buy
another one. The same app wallet must be used in both Neynar calls.

`submit` checks the unexpired 10-minute reservation, exact custody
address, current IdRegistry nonce, deadline, username and a recovered
low-s EIP-712 `Transfer` signature bound to:
`Farcaster IdRegistry`, version `1`, `chainId=10`,
`0x00000000fc6c5f01fc30151999387bb99a9f489b`.
The caller's parameters are never trusted as an authoritative FID or nonce.
A durable atomic claim protects exactly one irreversible Neynar POST.
Any ambiguous upstream response remains pending/unknown rather than
blindly retried.

`status` confirms the owner **only if both** the IdRegistry
`idOf(custody)` and `custodyOf(fid)` agree on the Optimism `safe` chain.
An HTTP 200 is not proof of FID ownership or signer authorization.

## Work remaining before public launch

- Complete *user-facing* paid voucher issuance, refund/accounting,
  abuse/rate caps and first-party payment confirmation.
- The Qt 6.12 self-custody controller is connected to Setup (twelve
  translations) for voucher acceptance, reserve, exact FID+nonce+deadline
  EIP-712 transfer in the native wallet, submit, and read-only confirmation.
  A verified onchain FID is handed to the C++ application session and
  one-use device Signer authorization; the signer remains unusable until
  the protocol's own registry confirms KEY_ADD. No mnemonic is sent to QML
  or Neynar. This native flow is covered by local mock HTTP and unit tests,
  **not** by a live funded registration.
- Complete actual paid voucher issuance, test payment settlement/refunds,
  signer network KEY_ADD confirmation, app restarts/recovery and independent
  cross-platform builds before enabling public registration.
- Test a funded staging enrollment, crash/restart and ambiguity recovery,
  then macOS, Windows, Linux, Android and WASM packaging.
- Do not equate passing the mocked relay tests with a real successful
  Neynar payment or mainnet enrollment.

## Checks

```sh
npm test --prefix services/account-sync
node --test services/account-sync/test/neynar_registration.test.mjs \
  services/account-sync/test/neynar_enrollment_http.test.mjs
```

Protocol API sources: Neynar wallet-integration registration guide,
`GET /v2/farcaster/user/fid`, `POST /v2/farcaster/user/`,
and Farcaster `IdRegistry.sol`.
