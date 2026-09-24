# Phase 5B Review — Protocol Signer Provisioning and Write Readiness

Date: 2026-08-28

## Decision

**Implementation-final in source.** Phase 5B now provides the complete
session → secure signer → custody authorization → Snapchain registration →
network-verified readiness path without making authentication itself a write
authority.

The implementation is cross-platform by contract. No Domain/Application/QML
code depends on macOS Keychain, Windows Credential Store, Linux Secret Service,
Android Keystore, browser extension APIs, or Snapchain JSON shapes.

Production release acceptance remains platform-specific and is tracked in
`PHASE_5B_VERIFICATION.md`.

## Delivered

- guest-first read policy from Phase 5A remains unchanged;
- `farcaster.domain.signer` models least-privilege scopes, TTL and registration;
- `farcaster.capability.signer` isolates native signer, vault, custody
  authorization and network registry ports;
- `farcaster.application.signer` owns restore/provision/revoke/readiness policy;
- SQLite stores only non-secret signer metadata;
- Apple Security.framework vault for macOS/iOS;
- QtKeychain 0.17+ vault path for Windows/Linux/Android;
- no insecure secure-store fallback;
- canonical BLAKE3-20 and protobuf KEY_ADD / self-revoking KEY_REMOVE codec;
- exact Snapchain gasless EIP-712 documents and SignedKeyRequest ABI metadata;
- Snapchain `/v1/submitMessage` binary submission;
- mandatory post-submit `/v1/signersByFid` re-observation;
- one-shot EIP-1193 browser/wallet compatibility authorizer;
- explicit wallet rejection mapping;
- separate signer executor so wallet approval cannot block the read/session
  executor;
- Qt presentation/QML signer state and Account actions;
- independent Snapchain endpoint and wallet-launch configuration.

## Security properties

1. SIWF proves account authentication only. It is never reused as custody or
   signer authorization.
2. Private Ed25519 seed bytes remain inside the platform vault boundary.
3. Custody private keys never enter Genycaster.
4. Custody signatures are transient and have no SQLite/QML persistence model.
5. `writeReady` requires fresh network evidence for the exact key and scopes.
6. HTTP 2xx from message submission is not treated as authoritative success.
7. Nonces are observed from Snapchain and incremented with overflow checking.
8. Wallet approval is bound to key/scopes/TTL/nonce/deadline.
9. Missing wallet/secure-store/Snapchain dependencies fail closed for writes
   while read behavior remains available.
10. The current compatibility authorizer accepts only self-attributed
    `requestFid == fid` registrations; it does not invent an app custody
    authority.

## Cross-platform composition

| Platform family | Secure signer | Custody transport composition |
| --- | --- | --- |
| macOS | Apple Keychain | EIP-1193 compatibility launcher or future native adapter |
| iOS | Apple Keychain | injected `ICustodyAuthorizer` transport; loopback compatibility is not an architectural requirement |
| Windows | QtKeychain / Credential Store | EIP-1193 compatibility launcher or future native adapter |
| Linux | QtKeychain / Secret Service/KWallet | EIP-1193 compatibility launcher or future native adapter |
| Android | QtKeychain / Android Keystore | injected `ICustodyAuthorizer` transport; wallet deep-link/native adapter may replace compatibility transport |

The wallet transport is intentionally replaceable. A future WalletConnect or
native SDK implementation consumes the same `farcaster.protocol.key_authorization`
documents and does not modify Domain/Application.

## Upstream alignment

The protocol implementation was aligned to Snapchain revision
`8f82bcc68dd4e5c75b4a4814bf6fc6b54215a73c`.

The official Farcaster client snapshot
`b6922e24036cac6f5e6d51904a59ff7cfcdd8483` was reviewed to confirm the product
separation between ordinary authenticated session state and explicit signer
approval/network confirmation. Genycaster does not adopt Warpcast-private
backend signer authority from that client.

QtKeychain 0.17.0 is the non-Apple secure-store portability dependency. It is
used only at the Platform boundary and its plaintext fallback is disabled.

## Architecture review

The architecture guard passes after this continuation. New project-owned C++
remains in named modules except explicit Qt/Apple textual ABI boundaries.
Provider wire details remain outside Domain/Application.

## Remaining release acceptance

Phase 5B source implementation is complete, but a binary release may claim
interaction support only after its own platform runner proves the full live
flow with a real secure store, wallet and current Snapchain endpoint. Current
container limitations are documented in `PHASE_5B_VERIFICATION.md`.

This acceptance requirement does not change the implementation boundary and
should not be worked around by weakening the write gate.
