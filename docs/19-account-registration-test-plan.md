# GenyCaster — account registration acceptance tests

Status: native Qt/Node fake-hub and C++26 component tests have passed.
**Funded, live HyperSnap and Farcaster account registration has not been
executed or verified by these tests.** A successful relay HTTP response is
not a registered FID and not a permitted signer.

## Offline regression suite (no funds spent)

From the repository root with Qt 6.12 installed:

```sh
npm test --prefix services/account-sync
node --test tests/wasm/setup_wireframe_flow.test.mjs
python3 -m unittest -q ci.test_pipeline
ctest --test-dir build/custody-vectors-gcc16 --output-on-failure
ctest --test-dir build/native-registration-verify -R session --output-on-failure
```

Use the regular CMake project/Qt Creator configuration for a full release
build; do not disable Mini Apps, QtKeychain, WebEngine or protocol signing just
to make this test pass. The isolated component test paths above are not full
application build substitutes.

## HyperSnap: manually test native self-custody

1. Install a signed build with a functioning OS-secure signer vault. On the
   Welcome/Setup screen choose **Create account > HyperSnap**.
2. Generate the internal wallet, record the 24 recovery words **offline**
   (never paste them into chat or a support ticket), and complete the word
   verification challenge.
3. Query the chosen native HyperSnap node for an existing FID; do not
   authorize a new registration if that address already owns one.
4. Prepare the native Hashcash proof off-thread. The loading state should
   not freeze scrolling or navigation. If the anchor expires, refresh it.
5. Explicitly acknowledge the irreversible registration and publish **once**.
   Expect pending, then a confirmed FID only after a separate shard-0 lookup.
6. Do not mark Setup complete or enable Cast/Like until device signer
   authorization and registry verification succeed.
7. Restart, log out and recover from the 24 offline words. Check recovery uses
   HyperSnap's own registry, not the Farcaster/Optimism IdRegistry.

## Farcaster via Neynar: funded operator test

The account API is intentionally disabled by default. Before any live run,
an authorized operator must:

1. Provision the separate funded Neynar application wallet. Both Neynar
   `GET /v2/farcaster/user/fid` and
   `POST /v2/farcaster/user/` use **that exact wallet ID**.
2. Apply `services/account-sync/migrations/013_neynar_registration.sql`
   to the dedicated account-sync PostgreSQL database after migration 012.
   Uniqueness constraints restrict each paid invoice and each custody address
   to one registration lease. Never run a migration on an unknown database.
3. Set the backend-only configuration documented in
   `services/account-sync/NEYNAR_REGISTRATION.md`, including a credential-
   free HTTPS Optimism RPC endpoint, funding and app-wallet credentials,
   `GENYCASTER_NEYNAR_ENROLLMENT_SECRET`, and
   `GENYCASTER_NEYNAR_REGISTRATION_ENABLED=true`.
4. **Only after independently verifying payment or approving an explicitly
   funded sponsorship**, issue a voucher bound to one invoice UUID and one
   public custody address with
   `createPaidEnrollmentVerifier({secret}).issue({invoiceId,
   custodyAddress,expiresAtMs})` in a trusted operator process.
   Never expose the issuing secret or voucher minting endpoint to users.
   Record the funded invoice in the operator's durable accounting system.

Then in the normal GenyCaster Qt Setup:

1. Choose **Create account > Farcaster / Neynar**. Create the wallet locally
   and verify the **offline** backup. The 24 words must never reach Neynar.
2. Enter the paid enrollment voucher and tap **Verify code**. The code is
   merely loaded in memory; backend authorization happens on FID reservation.
3. Tap **Reserve Farcaster FID** once. The backend must atomically acquire
   the invoice/custody lease before calling Neynar, return the exact FID,
   reservation deadline and authoritative Optimism IdRegistry nonce.
4. Choose an available `fname`, explicitly consent to custody transfer and
   tap **Sign and submit registration**. The local Qt C++ custody wallet signs
   Farcaster IdRegistry EIP-712 on chain ID 10. No external wallet/browser
   redirect is expected.
5. Expect `submitted`/pending until both onchain
   `idOf(custody)` and `custodyOf(fid)` agree on the `safe` chain.
   A 200 from Neynar alone is **not** confirmation.
6. Verify an authoritative Farcaster session and a separate, registered,
   ready signer with exactly the expected FID and the Snapchain adapter;
   editing the read-node choice must not redirect signed messages to
   HyperSnap. Setup cannot complete a **new** account until signerReady.
7. Reopen/restart, retry a stale voucher/duplicate invoice and test an
   interrupted reservation/unknown submission. Do not send duplicate paid
   allocations or transfers; use the read-only status lookup and the
   existing reservation ID.

## Launch gates

- Operator-verified payment/voucher issuance, reconciliation, refund and
  rate/abuse policy for the funded app wallet
- PostgreSQL migration 013 on the intended isolated production database
- Reliable crash/restart restoration of an **unconfirmed reserved custody
  wallet**, without exposing recovery words to QML, logs or the server
- Live network FID registration and Signer KEY_ADD confirmation
- Full Qt 6.12 release builds and runtime QA on macOS, Windows, Linux,
  Android and WebAssembly, with their actual supported secure-vault backends

Until the launch gates above are verified, do not present a simulated
registration or a network HTTP response as an end-to-end successful signup.
