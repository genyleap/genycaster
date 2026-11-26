# 08 — Security, Authentication, Identity, and Signing

## Principle

Identity, application login, provider credentials, wallet signing, and Farcaster protocol signing are distinct concerns.

Do not collapse them into one `AuthProvider` or `WalletProvider` abstraction.

## Separate concepts

Potential boundaries include:

```text
IIdentityResolver
ISignerService
IProviderCredentialStore
IClientSessionService
IAuthenticationProvider
IFarcasterIdentityResolver
ICompatibilityAuthentication
```

For shipping Genycaster, [OpenProof](https://github.com/genyleap/openproof)
(its separately deployed `opp` service at the configured
`https://openproof.genyleap.com` origin) is the canonical account authority.
See [Identity and Server Services](OPENPROOF_AND_SERVICES.md) for how its
account proofs relate to GenyCaster's independent PostgreSQL
storage, gateway verification and Farcaster provider routes. A local
client session may therefore carry an OpenProof issuer/subject pair together
with the required linked Farcaster FID. Farcaster remains the protocol identity
used for reads/writes and signer authorization; it is not the canonical account
authority when OpenProof composition is enabled.

For wallet functionality, separate concerns may include:

```text
IWalletAccount
ITransactionSigner
IChainRpc
IAssetSource
ISwapProvider
IOnRampProvider
```

## Guest-first access control

Public read access and authenticated protocol interaction are separate product capabilities.

```text
Guest
  -> public/cached reads

Authenticated client session
  -> proves the account/FID used by the application

Authenticated client session + valid protocol signer
  -> protocol-changing writes
```

Browsing or resolving another Farcaster identity must never establish a client session. A client session must never be treated as a protocol signer.

On WebAssembly, an already authenticated browser may keep an origin-scoped,
non-extractable WebCrypto Ed25519 device-continuity key and the previously
verified Farcaster or HyperSnap FID and its protocol in IndexedDB.
A signed local challenge proves possession of that key on reload;
it is **not** a refreshed OpenProof token or server-authenticated session.
Remembered continuity expires after 30 days on that browser (subject to the
limitations of the device clock) and is removed at the next restore. Explicit
sign-out removes the local continuity record. Clearing browser site data also
removes it, whereas publishing a new WASM release does not. Reusing the FID
never authorizes protocol writes. The viewer session opens before any signer
lookup. A stored protocol signer must independently pass registration, scope,
expiry and revocation checks against its own live registry. Other
Mini App wallet grants and authenticated product-service sessions are distinct.

The normal native sign-in path uses OpenProof OAuth device authorization. Genycaster
opens the first-party Genyleap account surface with a local `return_to` target;
an existing identity-provider session is reused by that browser flow, so
the user is not asked to authenticate again merely because they opened the native
client. OAuth access/refresh tokens remain transient inside the authentication
attempt and are not persisted in the domain/session store.

A completed OpenProof account is not sufficient for Genycaster until it has a
linked Farcaster connection. If the connection is missing, the same OpenProof
attempt starts a bearer-bound `/account/connections/web3/*` ceremony, obtains a
Farcaster proof, and commits the client session only after OpenProof reports the
linked FID.

The direct “Sign in with Farcaster” route is retained as onboarding convenience,
but it starts from an OpenProof Farcaster challenge and completes through
OpenProof. It therefore also produces an OpenProof issuer/subject plus linked
FID rather than creating a separate Farcaster-only account authority.

During any Farcaster relay step, the raw channel token remains private to the
authentication attempt. The user-facing approval URI is an opaque transient
artifact and may encode relay challenge material by protocol design; do not
persist it, log it, or expose it as readable/editable QML text.

## Protocol signing

For protocol-native writes, prefer this architecture:

```text
Application command
      ↓
Protocol message builder
      ↓
Local signer boundary
      ↓
Signed canonical message
      ↓
Submission transport capability
      ↓
Provider/node
```

The provider should not own the semantic creation of a Cast/Follow/Reaction when the protocol allows local canonical message construction/signing.

### Current signer-registration semantics

Phase 5 signer provisioning must target the current Farcaster network rather than assuming that signer registration is exclusively an on-chain KeyRegistry transaction. Snapchain gasless signers add `KEY_ADD` / `KEY_REMOVE`, explicit non-empty message scopes, custody-authorized registration, nonce/deadline replay protection, and signer TTL. Existing on-chain signers remain a compatibility lane.

The application-facing signer capability therefore stays network-neutral: it expresses required write scopes and readiness, while a concrete signer-registration adapter selects the currently supported Snapchain or legacy on-chain mechanism. A UI must never silently request broader scopes than the write features it enables.

Current gasless authorization is not a reuse of SIWF. `KEY_ADD` requires an app/requester `SignedKeyRequest` authorization and a custody `KeyAdd` EIP-712 authorization bound to the exact Ed25519 public key, scopes, TTL, nonce, and deadline. The message envelope is also signed by the new Ed25519 key as proof of possession. Login proof and protocol-signer authorization must therefore remain separate ports.

The current off-chain gasless-key EIP-712 domain is `Farcaster KeyAdd`, version `1`, chainId `1`, with no verifying contract. Do not reuse the legacy on-chain SignedKeyRequestValidator domain.

### HyperSnap native FID device authorization

Native HyperSnap FIDs (`fid >= 2^32`) are owned by HyperSnap's own custody registry, **not** Farcaster IdRegistry or OpenProof. The login session carries issuer `hypersnap:native-registry`; every signer-registry operation and signed Cast/Reaction/Link submission is pinned to the HyperSnap writer, irrespective of the selected read network.

For a newly verified native wallet, explicit approval in Setup or Account may use the short-lived in-memory custody wallet to produce a one-shot `KEY_ADD` grant. If that secret is no longer in memory, the user can explicitly enter the **original** 24 recovery words inside GenyCaster. Derive the public custody address locally, match the current session's exact FID and custody, re-check the authoritative HyperSnap native account on shard 0, then construct an FID-bound, zeroizing one-shot custody authorizer. Only then may the existing protocol signer service register and confirm the Ed25519 device signer. Never route a native FID through a Farcaster approval URL or the generic external custody authorizer; the signer service fails closed if native `KEY_ADD` lacks a local one-shot grant. A previously registered, valid signer may be restored by read-only registry inspection without re-entering recovery words. Neither checking account ownership nor restoring a signer submits another FID registration.

Native social writes check the 64-bit FID **inside the signed protobuf MessageData**
against the pinned session author before any POST. A mismatch or malformed
envelope fails locally. A HyperSnap `unknown fid` HTTP 400 remains a rejected
write, not evidence of acceptance. Only when the **same selected node** confirms
both the shard-0 native registration and the exact current, appropriately
scoped signer via the routed data-shard lookup may the existing durable outbox
treat it as transient: it reconciles before retrying the **same signed bytes**
with bounded attempts (default five) and persisted backoff. Missing or
mismatched evidence fails closed. No silent Farcaster cross-posting, new
registration, key rotation, or custody prompt is permitted. Local retry
cannot repair missing registration events in the validator's data shard;
operator-side block-receiver/cross-shard replay remains authoritative.

The 24 words are transient input, cleared on cancel/submit or error; they must not enter settings, analytics, logs, provider requests, or long-lived presentation properties. Device approval is user-initiated, serialized, and cancelled on sign-out; asynchronous results are discarded if session FID, issuer or custody changes. Display account registration, signer authorization, and write readiness as distinct states. Farcaster/Neynar signup retains its own onchain registration and approval requirements.

## Key isolation

Raw private keys must never be exposed to QML.

Use signer handles/capabilities and platform-secure storage adapters.

The Phase 5B secure-vault boundary is cross-platform. Apple targets use the platform Keychain path; Windows/Linux/Android use the QtKeychain secure backend selected for that OS. Plaintext fallback is disabled. SQLite may store only the opaque signer id and public/network metadata, never seed/private-key bytes. If a secure backend is unavailable, interaction capability fails closed while guest/public/cached reads remain available.

Current platform boundaries are:

- macOS/iOS: Apple Keychain / Security.framework;
- Android: QtKeychain backed by Android Keystore;
- Windows: QtKeychain backed by Windows Credential Store;
- Linux: QtKeychain backed by Secret Service/KWallet.

Thin platform-native code is acceptable where required for secure platform integration.

## QR authentication

A flagship backend QR flow is a compatibility capability, not a universal identity primitive.

The architecture must allow alternatives such as:

- standards-based sign-in;
- wallet-based flows;
- device pairing;
- passkey-assisted flows;
- compatibility QR login.

## Provider credentials

Provider API keys/tokens are infrastructure configuration and must remain outside domain entities and UI-facing models.

## Crypto rule for future messaging

Do not invent cryptographic protocols.

Any E2EE messaging layer must use an audited standard/mature protocol design and receive dedicated security review before production deployment.

## GenyCaster Pro billing boundary

Pro is an optional, **product-owned** 30-day subscription, priced in USD
(default `299` cents per period) and settled by an exact GENY or native
Circle USDC ERC-20 transfer on Base. The operator sets `GENYCASTER_PRO_MONTHLY_USD_CENTS`
**on the account server**, and explicitly configures a controlled treasury
wallet before payments are enabled. No QML amount or wallet balance grants
Pro. The API issues owner-scoped, expiring quotes, verifies the onchain
Transfer and confirmed block, then atomically records a uniquely consumed
transaction hash and membership expiry in the independent GenyCaster database.
The selected HyperSnap/Snapchain protocol route remains independent.

Account identity and wallet transfer signing remain separate: authentication
proves the Farcaster FID; the transfer must originate from one of that FID's
currently protocol-verified wallets. No Farcaster Cast signer, locally stored
recovery phrase or browser session token is used to sign payments. Payment
verification and privilege enforcement fail closed during missing migrations
or outages. The restored native Farcaster/OpenProof application login is not
itself an authenticated account-service cookie or a currently verified payment
session. On Pro entry the client tries restoring that session without prompting;
when it is absent, an explicit Farcaster proof for the **same active FID** is
required. This must never route an already signed-in user back into ordinary
account login, imply that protocol signing is a payment authorization, or
silently provision a session from a remembered FID. The approval URL is a
short-lived challenge and never appears in logs, persistent settings or a
payment quote. The WebAssembly wallet transfer uses only a first-party
EIP-1193 provider from an explicit user action; it never reuses Mini App
permission grants, browser authentication secrets or localhost transports.
Wallet responses are not proof of membership until server receipt verification. See `services/account-sync/README.md` for the operational
checkout contract and migration `012_pro_billing.sql`.


### Device-local authentication continuity (native and browser)

Shipping native builds authenticate the SQLite identity snapshot with a
separate OS-vault Ed25519 key in `org.genycaster.device-session`. The seal
binds issuer, subject, FID, custody address, authentication method and
timestamps. The SQLite row is not an authentication credential. Existing
unsealed records require one verified login; the social signer and public
cache are not removed by this migration. The browser uses a non-extractable
WebCrypto device key and version-2 identity seal; old v1 rows require one
verified login. Device-local continuity is capped at 30 days and never
extends an OpenProof bearer or a network signer registration. A missing,
malformed or mismatched seal cannot restore an authenticated account. A
temporarily unavailable native vault is a retryable storage failure.

All protocol signing requests bind the FID, public key and scope. Native
signer state is serialized across social and media workers, and an atomic
generation lock invalidates queued signing authority when the account changes
or the user signs out. Foreground and bounded timer checks are read-only;
they do not prompt for custody words or silently grant KEY_ADD.

Windows keys use local-machine persistence; pre-existing enterprise
credentials are restricted in place on access. Android cloud backup and
device transfer explicitly exclude app data. Apple and Linux retain their
existing secure backend policy. Browser origin compromise, OS compromise
and compromised keyboard/clipboard are outside the local seal's guarantees.

### Independent private-cloud authorization

The application-layer `AuthenticationCoordinator` decides when to restore a
locked device or revalidate a signer, and applies bounded exponential backoff.
Qt only translates lifecycle events and dispatches the selected operations.

Cloud enrollment uses an independent Ed25519 key per exact protocol/account,
separate from the local device identity seal and the social signer. Native
vault namespaces are `org.genycaster.cloud-device.farcaster` and
`org.genycaster.cloud-device.hypersnap`; the browser uses isolated key scopes.
The account service requires a fresh verified OpenProof grant for Farcaster
or EIP-191 custody proof plus authoritative HyperSnap registry ownership.
It never accepts a social signer or old cookie as enrollment authority.
Device proof restores the server cookie without a recovery phrase or KEY_ADD.
Logout revokes the device record and associated server cookies when reachable,
and erases the local credential. The server can enforce the new trust boundary
with `GENYCASTER_ALLOW_LEGACY_SIGNER_SESSIONS=0` after migration 017 and client
rollout. Legacy mode intentionally remains available until the migration is
verified; it is not the final strict security policy.

### Reference comparison

The official Farcaster mobile client's AuthTokenProvider, WalletProvider,
SecureStorageUtils, and revalidationSignOutDecision separate wallet storage,
token restoration and transient network failures. Genycaster follows those
lifecycle principles without sharing Farcaster's proprietary authentication
tokens or storing the root phrase. OpenProof remains the independent central
identity authority. Genycaster additionally separates protocol social signing
from private app-cloud device authorization across all supported platforms.
