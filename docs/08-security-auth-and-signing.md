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
ICompatibilityAuthentication
```

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

During Sign In With Farcaster compatibility login, the relay's raw channel token remains private to the authentication attempt. The user-facing approval URI is an opaque transient artifact and may encode relay challenge material by protocol design; do not persist it, log it, or expose it as readable/editable QML text.

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
