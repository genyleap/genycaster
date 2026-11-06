# 34 — Phase 5B Protocol Signer Provisioning and Write Readiness

## Status

Phase 5B is **implementation-final in source** as of 2026-08-28. The guest/auth
boundary, secure local Ed25519 signer, canonical Snapchain gasless authorization
payloads, KEY_ADD / self-revoking KEY_REMOVE submission, post-submit network
verification, signer readiness state machine, cross-platform secure-vault
composition, and wallet-approval compatibility transport are all present.

Promotion to a platform-specific production release still requires acceptance on
that target's real OS secure store, wallet, and Snapchain endpoint. A missing
secure store, wallet transport, or Snapchain endpoint fails closed for writes and
must never disable guest/public/cached reads.

## Product invariant

```text
Guest
  -> public/cached reads

Authenticated session
  -> public/cached reads as the verified account
  -> still no protocol-changing authority

Authenticated session + network-verified signer + exact required scope
  -> protocol-changing interaction may proceed
```

Viewed identity, authenticated account, local secure key, custody authorization,
and network signer registration are independent states. Sign In With Farcaster
must never be reused as arbitrary signer authorization.

## Canonical Snapchain signer contract

The implementation targets the current Snapchain gasless signer model, not the
legacy assumption that every signer must be registered on Optimism KeyRegistry.
The source was aligned to Snapchain revision
`8f82bcc68dd4e5c75b4a4814bf6fc6b54215a73c`.

Gasless KEY_ADD binds:

- target FID;
- 32-byte Ed25519 public key and key type 1;
- explicit admissible scopes;
- positive TTL, maximum 90 days;
- strictly increasing user nonce;
- Farcaster-epoch deadline;
- ABI-encoded SignedKeyRequest metadata;
- custody EIP-712 authorization;
- Ed25519 envelope signature proving possession of the new key.

Genycaster's least-privilege default social signer requests:

```text
CAST_ADD
CAST_REMOVE
REACTION_ADD
REACTION_REMOVE
LINK_ADD
LINK_REMOVE
```

KEY_ADD and KEY_REMOVE are not delegated signer scopes.

## Canonical custody EIP-712

`farcaster.protocol.key_authorization` owns the exact provider-neutral typed-data
construction. Platform and wallet transports are not allowed to recreate the
protocol schema independently.

Current domain:

```text
name: Farcaster KeyAdd
version: 1
chainId: 1
verifyingContract: absent
```

Two signatures are independently required for the self-attributed flow:

1. `SignedKeyRequest(requestFid, key, deadline)`;
2. `KeyAdd(fid, key, keyType, scopes, ttl, nonce, deadline)`.

The wallet response is converted to the exact ABI
`SignedKeyRequestMetadata(uint256,address,bytes,uint256)` representation before
it enters the canonical KEY_ADD protobuf encoder.

The shipping compatibility authorizer currently supports
`requestFid == authenticated fid`. This deliberately avoids an app-owned custody
secret or a proprietary backend authority. More advanced app-FID delegation can
be added as another `ICustodyAuthorizer` implementation without changing Domain
or Application.

## Canonical message submission

`farcaster.protocol.message` constructs the protobuf wire representation.
MessageData is BLAKE3-hashed to the protocol 20-byte hash and that hash is signed
by the opaque Ed25519 signer. The Snapchain provider submits binary protobuf to
`POST /v1/submitMessage` with `application/octet-stream`.

A successful HTTP response is not sufficient because submission acknowledges
mempool admission rather than an authoritative state merge. `ISignerRegistry`
polls `/v1/signersByFid` for a bounded confirmation window and validates the
resulting registration/nonces before reporting success.

Self-revocation uses KEY_REMOVE signature type 2. The message envelope is signed
by the key being removed and the requester/app nonce namespace is checked after
submission.

## Ownership map

```text
Domain
  farcaster.domain.signer
    scopes / TTL / registration / permission rules

Capabilities
  farcaster.capability.signer
    INativeSigner
    ISignerVault
    ICustodyAuthorizer
    ISignerRegistry

Protocol
  farcaster.protocol.key_authorization
  farcaster.protocol.message
  farcaster.protocol.hash

Application
  farcaster.application.signer
    restore / provision / revoke / write gate

Local state
  farcaster.local.signer

Storage
  farcaster.storage.signer
    non-secret SQLite signer metadata only

Platform
  farcaster.platform.signer_vault
    OS-protected Ed25519 seed ownership

Compatibility provider
  farcaster.provider.compatibility.browser_wallet.authorizer
    one-shot EIP-1193 wallet approval transport

Canonical provider
  farcaster.provider.snapchain.signer
    network-authoritative signer inspection/submission
```

Provider DTOs, wallet credentials, seeds, private keys, Keychain handles and
EIP-1193 details terminate at their boundaries.

## Cross-platform secure vault policy

`farcaster.platform.signer_vault` exposes one provider-neutral vault contract.
The selected secure backend is a build/platform detail:

| Target | Secure backend | Policy |
| --- | --- | --- |
| macOS | Apple Security.framework / Keychain | direct adapter |
| iOS | Apple Security.framework / Keychain | direct adapter |
| Windows | QtKeychain 0.17+ / Windows Credential Store | secure backend required for writes |
| Linux | QtKeychain 0.17+ / Secret Service or KWallet | secure backend required for writes |
| Android | QtKeychain 0.17+ / Android Keystore | secure backend required for writes |
| unsupported/no secure backend | unavailable adapter | writes fail closed |

QtKeychain's insecure fallback is explicitly disabled by Genycaster. No signer
seed may fall back to QSettings, a plaintext file, SQLite, QML, logs, or ordinary
application preferences.

Apple keeps its existing opaque `macos-keychain:fid:*` signer identifier for
metadata compatibility; that identifier is local/opaque and is not protocol
state.

The platform adapter reloads seed material only inside the secure boundary,
derives/signs with OpenSSL Ed25519, and cleanses temporary seed buffers.

## Cross-platform wallet approval transport

The current compatibility authorizer is transport-only and contains no protocol
schema ownership. It:

- binds exclusively to `127.0.0.1` on an ephemeral port;
- uses a random 256-bit one-shot capability token;
- serves only the canonical typed-data documents;
- uses EIP-1193 `eth_requestAccounts` and `eth_signTypedData_v4`;
- applies request size/time limits and a five-minute approval lifetime by
  default;
- sets `no-store`, CSP, frame denial, no-referrer and MIME hardening headers;
- never persists custody signatures;
- maps explicit wallet rejection to `Unauthorized` instead of waiting for a
  timeout;
- delegates URL/deep-link launch policy to the Qt composition root.

`QDesktopServices` provides the default platform URL launcher. A deployment may
supply `--wallet-launch-url-template=` or
`GENYCASTER_WALLET_LAUNCH_URL_TEMPLATE` for a wallet-specific browser/deep-link
wrapper. This is a compatibility lane, not a requirement that all future mobile
wallet transports use loopback HTTP. WalletConnect or a native wallet SDK can be
added behind `ICustodyAuthorizer` without changing protocol/application code.

## Readiness state machine

```text
SignedOut
  -> authenticated session arrives
Checking
  -> no secure key                -> Missing
  -> secure storage unavailable   -> Unavailable
  -> key exists, no registry      -> AuthorizationRequired
  -> registration revoked         -> Revoked
  -> registration expired         -> Expired
  -> registration lacks scope     -> AuthorizationRequired
  -> active + required scopes     -> Ready

Provisioning
  -> ensure secure Ed25519 key
  -> inspect signer registry and user nonce
  -> build canonical SignedKeyRequest + KeyAdd EIP-712
  -> request custody-wallet approval
  -> build canonical KEY_ADD protobuf
  -> sign BLAKE3-20 hash with Ed25519 key
  -> submit to Snapchain
  -> re-read exact signer registration and nonce
  -> Ready

Revocation
  -> inspect requester nonce
  -> build self-revoking KEY_REMOVE
  -> submit
  -> re-read network state
  -> erase local secure key only after verified revocation
  -> Revoked
```

`writeReady` is true only in `Ready`. Sign-out immediately removes write
readiness. A signer/wallet/Snapchain failure affects the signer surface only;
read/session runtime stays available.

## Replay protection

The application never guesses a nonce. It reads the current user/requester
nonce, computes checked `current + 1`, and rejects uint32 overflow. Deadlines are
converted to Farcaster epoch seconds and must not precede the message timestamp.

## Build compartmentalization

Relevant switches:

```text
FARCASTER_ENABLE_PROTOCOL_SIGNING
FARCASTER_ENABLE_QTKEYCHAIN_SIGNER_VAULT
FARCASTER_REQUIRE_SECURE_SIGNER_VAULT
FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER
FARCASTER_ENABLE_BROWSER_WALLET_AUTHORIZER
```

Provider-free core presets compile all write infrastructure out. UI builds may
remain readable even when no secure vault is installed by leaving
`FARCASTER_REQUIRE_SECURE_SIGNER_VAULT=OFF`. Distribution/release jobs that
promise interaction support should set it `ON` so missing platform secure-store
dependencies are a configure-time failure.

The `secure-signer` preset enables the complete signer stack independently of
Hypersnap/auth read providers.

Normal UI composition targets Farcaster's public mainnet Snapchain node by
default, so a freshly configured interaction-capable build owns a real signer
registry and write submitter without requiring an IDE-specific environment
variable. Development, private and self-hosted deployments may override the
origin with `GENYCASTER_SNAPCHAIN_BASE_URL` or
`--snapchain-base-url=<origin>`; the command-line value takes precedence.

## Verification contract

A release that claims full interaction support on a platform must prove:

```text
fresh install
 -> guest read works
 -> sign in
 -> secure signer created in OS secure store
 -> exact wallet EIP-712 approval
 -> KEY_ADD accepted by current Snapchain
 -> post-submit read observes Active + exact scopes/TTL/requestFid
 -> restart
 -> signer returns Ready from secure store + network evidence
 -> self-revoke
 -> post-submit read observes non-active registration
 -> local seed erased
 -> guest/read remains available throughout failures
```

Current signer and security acceptance gates are maintained in
[Development Stages](12-roadmap.md). Platform-native verification must be run on the actual
macOS/iOS, Windows, Linux and Android release runners that Genycaster ships.
