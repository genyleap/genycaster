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

1. In first-run **Setup**, "Continue with recovery words" is the primary
   browser account path; OpenProof and Farcaster SIWF remain optional under
   "Other sign-in methods". Direct recovery does not start an SIWF challenge.
2. The recovery dialog accepts a **Farcaster-specific** 12/24-word phrase.
   Check BIP39 checksum and derive its custody address in browser JavaScript;
   neither the words nor custody private key cross the Qt/WASM bridge.
3. Resolve the custody address to its FID via Optimism IdRegistry
   `idOf(address)`, then verify the inverse `custodyOf(fid)` mapping on chain
   10. Both lookups must match the derived custody address. No session is
   established from a supplied or guessed FID.
4. Authorize the origin-bound, non-extractable Ed25519 signer using the same
   ephemeral custody key and wait for authoritative Snapchain KEY_ADD/nonce
   confirmation. Only afterward deliver the verified FID and a one-shot
   correlation ID to Qt to create an ephemeral local account session.
5. At later sign-ins via SIWF/OpenProof, restore an existing signer from
   origin-scoped IndexedDB and check registration. If missing, use the
   explicit recovery action. Session restoration never prompts for a phrase.
6. BIP39 uses WebCrypto PBKDF2/HMAC and vendored noble-secp256k1.
   Sign the canonical SignedKeyRequest and KEY_ADD EIP-712 structures locally;
   never infer that writing is ready until network registration is confirmed.
7. Clear the dialog and dispose of mutable custody key bytes after success
   or failure. Only the authorized Ed25519 key remains for regular interaction.
8. New devices, revoked signers, or expired registrations require explicit
   recovery again. The custody phrase is never persisted. Sign-out and account
   changes invalidate outstanding recovery requests and stale responses.

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
