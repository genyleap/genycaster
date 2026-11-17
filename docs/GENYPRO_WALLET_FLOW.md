# GenyCaster Pro — verified external wallet checkout

## Product boundary

- Connecting an EIP-1193 wallet (`eth_requestAccounts`) only confirms browser
  access. It is **not** evidence of ownership of a GenyCaster account, and is
  not sufficient to receive a server-side payment quote.
- The authenticated GenyCaster account starts a short-lived, single-use EIP-191
  ownership challenge. The message binds the exact issuer, account subject,
  wallet address, origin, nonce and expiration time. It explicitly grants
  **no transaction permission and no Pro entitlement**.
- The selected browser wallet signs the message via `personal_sign`, behind
  a separate user interaction on the wallet approval page. No seed phrase,
  private key or arbitrary wallet data is sent to the account API.
- The API independently recovers the signer address, atomically consumes the
  challenge and stores a verified payer **for this account only**.
- The Pro quote and payment verification use the same app-verified payer list,
  merged with existing protocol-verified wallets. They do not trust a
  client-reported payer.
- Payment still requires another intentional user action and the EIP-1193
  `eth_sendTransaction` approval; a wallet connection or message signature
  never initiates a transfer.
- GenyCaster Pro is granted solely by server-confirmed payment receipt,
  not by wallet connect, challenge verification, a transaction hash or
  token holding.

## Flow

`Continue` → `eth_requestAccounts` → authenticated
`POST /api/v1/cloud/pro/wallet/challenge` → `personal_sign` (explicit approval)
→ authenticated `POST /api/v1/cloud/pro/wallet/verify`
→ signed payer registry → `POST /api/v1/cloud/pro/quote`
→ display quote → explicit `Pay` approval
→ confirmed on-chain receipt → entitlements refresh.

The wallet bridge uses the **canonical G-arrow SVG geometry** from
`ui/wasm/genycaster-icon.svg` and theme-aware neutral/blue surfaces. Provider
discovery uses EIP-6963 first; the generic `window.ethereum` fallback is
only shown when no branded provider is announced.

## Deployment requirements

Database schema **15** must be applied under a database administrator account
before the app-owned payer proof route can work. Schema **14** is the separate
GenyBuzz rollout. The restricted runtime role does not have CREATE privileges;
never grant it arbitrary DDL rights.

1. Back up the current PostgreSQL database. Check migration state.
2. Apply `014_genybuzz.sql` and `015_pro_wallet_proof.sql` using a
   database administrator account. Both are transactional and additive.
3. Deploy the updated Account API alongside the updated native/WASM client.
   Check authenticated route responses and the separate health endpoint.
4. Run the `services/account-sync/test/pro_wallet_proof.test.mjs`,
   `services/account-sync/test/pro_billing.test.mjs`, and UI/C++ checkout tests.
5. With **two different accounts**, verify isolated wallet proof ownership,
   replay rejection, signature refusal, quote creation, a declined transfer,
   correct provider identity selection and a real paid receipt.
6. Restart and sign in again to verify that verified payer state is durable.

Until migration 15 and the API release are active, a new wallet cannot be
linked through this first-party proof route. The client must report an error
rather than pretend Pro was granted. Restarting the old API cannot create
new routes.

The browser-local relay (`127.0.0.1`) is a transport shim for installed
browser wallet extensions. Never give it account-session bearer tokens, and
never mistake its local origin for the application's payment domain.
