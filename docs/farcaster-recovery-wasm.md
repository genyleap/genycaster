# Browser-native Farcaster recovery and signer authorization

## Reference: Quorum / Hypersnap

These were reviewed from **QuilibriumNetwork/quorum-mobile** (default
`master`) and **QuilibriumNetwork/quorum-desktop** (default `main`) on
2026-10-17. The Farcaster implementation to learn from is *mobile*;
desktop's current `src/components/farcaster/FarcasterPage.tsx` is a
“Coming soon” UI and does not implement a wallet-less Farcaster recovery flow.

Mobile reference files:

- [Farcaster onboarding](https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/app/(onboarding)/farcaster-setup.tsx): optional 12/24-word Farcaster recovery phrase; BIP39 validation before importing.
- [Farcaster key derivation](https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/services/onboarding/farcasterService.ts): BIP39 seed, secp256k1 BIP32/BIP44 custody path `m/44'/60'/0'/0/0`, Ed25519 key derivation and Hypersnap lookup by custody address.
- [SecureStorage](https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/services/onboarding/secureStorage.ts): native OS SecureStore for recovery-derived custody and signer keys.
- [Signer lifecycle](https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/hooks/useHypersnapSignerLifecycle.ts) and [provisioning](https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/services/farcaster/hypersnapProvision.ts): when an FID appears, ensure/renew the Hypersnap signer automatically; the user is not prompted for each Cast.
- [Farcaster writes](https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/services/farcaster/hypersnapWrites.ts): sign each mutation with the approved device Ed25519 signer and submit to Hypersnap; no fallback to fake success.

**Farcaster identity versus authorization:** SIWF/OpenProof verifies the FID,
but the source apps distinguish custody control from protocol Ed25519 signer
permissions. Importing a mnemonic does not allow writing as a different FID.

## Genycaster WASM flow

1. Keep the existing Farcaster/OpenProof login (identity only).
2. Restore the previously stored **non-extractable Ed25519** CryptoKey for the
   FID from the browser origin's IndexedDB, and verify its registration against
   Snapchain. When valid, publishing is enabled with no wallet extension and
   no recovery phrase. User signs Casts using that key.
3. If no registered local signer exists, `Enable interactions` opens an
   explicit, localized, in-browser **Farcaster recovery** dialog. The phrase
   stays within DOM / JavaScript and never crosses Qt/WASM, analytics or any
   network request.
4. Validate 12/24-word English BIP39 checksum and derive secp256k1 custody
   using BIP44 `m/44'/60'/0'/0/0`. The code uses WebCrypto PBKDF2/HMAC and
   vendored MIT-licensed `noble-secp256k1` v2.3.0. The English wordlist is
   vendored from `scure-bip39` at revision
   `a337acacb232b612985758b265a3e9aefc0dc58a`.
   Source and LICENSE files are shipped with WASM, never fetched from a CDN.
5. Verify the derived custody address against Farcaster IdRegistry's
   `custodyOf(fid)` on **Optimism chain 10** (pinned HTTPS JSON-RPC
   `mainnet.optimism.io`, fixed registry contract and read-only
   `eth_call`). A different owner fails closed and never creates KEY_ADD.
6. Sign precisely the existing `SignedKeyRequest` and `KeyAdd` EIP-712
   structures **locally**, verify the recovered signer address, submit a
   canonical Ed25519 KEY_ADD signed by the local browser CryptoKey, and wait
   for authoritative signer registration and nonce confirmation.
7. Discard the ephemeral custody private-key bytes after authorization (or
   error) and clear the form. Subsequent Cast/Reply/Reaction/Follow/Delete
   traffic uses only the authorized Ed25519 key. No external wallet extension
   is needed.
8. On a new browser origin/device, revoked signer or expired registration,
   require explicit recovery/authorization again. We do **not** retain the
   custody private key or mnemonic in IndexedDB for unattended renewals.

## Threat model / limitations

- A **recovery phrase grants custody authority**. A web page cannot claim
  the same private-key isolation as an OS SecureStore. A compromised origin,
  malicious extension, browser debugger or compromised device can steal phrase
  data while the form is open. Use HTTPS/localhost and a trusted device.
  JavaScript strings cannot be deterministically zeroized; the input is
  cleared and the derived secret is held in mutable bytes and overwritten on
  disposal. The form is never prefilled and has no clipboard auto-read.
- The browser origin is part of the IndexedDB key boundary. Never migrate
  secrets via localStorage or URL/query parameters.
- EOA/secp256k1 custody only: smart-contract (EIP-1271) custody and non-BIP39
  recovery schemes need a separate verified flow, not a fake derived key.
- RPC unavailability or nonmatching chain/network ownership fails closed.
- A refused / ambiguous Snapchain submission must never be automatically
  retried as a new Cast.
- The native (desktop/mobile) signer remains untouched. This browser
  integration is not proof of production readiness until a **real Qt 6.12
  WASM browser build, recovery import, KEY_ADD confirmation and Cast**
  have been tested with user-approved disposable credentials. Automated
  tests use standard public vectors and mocked network responses.
