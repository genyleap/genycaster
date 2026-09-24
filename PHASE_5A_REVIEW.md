# Phase 5A Review — Guest-First Authentication and Client Session

## Status

Phase 5A source implementation is complete for guest-first authentication/session behavior. Final release acceptance is **NOT VERIFIED** in this execution environment because the repository's required macOS/Homebrew Clang 22 + Qt 6.11.1 matrix is unavailable here.

## Product behavior

- Guest is a first-class state: public/cached reads do not require login.
- The currently viewed Farcaster identity remains browsing context and never becomes account authentication implicitly.
- Account state is modeled separately through a normalized `ClientSession`.
- An authenticated session alone does not enable protocol writes.
- Write readiness requires both a verified account session and a valid protocol signer.
- Sign out clears the account session but preserves public read cache.
- Re-authentication/account replacement does not temporarily invalidate an already verified session; the old session remains authoritative until a replacement proof succeeds.

## Architecture

The implementation keeps the boundaries separated:

```text
QML presentation
  -> C++ PresentationBridge
      -> Application SessionService
          -> ICompatibilityAuthentication
              -> Farcaster SIWF compatibility adapter
          -> IClientSessionStore
              -> SQLite session adapter
```

The provider-neutral layers contain no relay URL, relay channel token, provider DTO, JSON representation, cryptographic implementation, or Optimism contract address.

## SIWF compatibility review

The native compatibility adapter follows the current Farcaster auth-client model:

- create relay channel;
- keep the raw channel token inside the authentication attempt;
- treat the returned approval URI as an opaque transient user-facing protocol artifact;
- poll channel status with the token as bearer authorization;
- validate SIWF domain, URI, nonce, statement, chain, singular FID resource, `expirationTime`, and `notBefore`;
- verify the returned signature locally before committing a session;
- verify the recovered/validated address against either an active Farcaster auth-address or the FID custody address.

The approval URI is not rendered as editable/readable QML text. This is intentional because the protocol URI can encode relay challenge material.

## Signature verification

`@farcaster/auth-client` 0.7.1 declares `viem ^2.29.2`. The C++ adapter implements the deployless universal-signature validation model from that dependency floor, covering:

- EOA signatures;
- deployed ERC-1271 smart-contract signatures;
- ERC-6492 counterfactual smart-account signatures.

This is an explicit compatibility baseline. Future signature schemes added by newer viem versions require a new review rather than being assumed compatible automatically.

## Persistence and secret handling

SQLite migration 7 stores only normalized session metadata and expiry. It does not store:

- relay channel token;
- approval URI;
- signed SIWF message;
- SIWF signature;
- wallet or signer private keys.

Unknown persisted authentication-method values are rejected as corrupt storage rather than silently downgraded to another method.

## UI behavior

The account surface distinguishes:

```text
Viewing identity  !=  Signed-in account  !=  Protocol signer
```

Guest users can continue navigating reads. Sign-in state exposes system status and an `Open Farcaster` action without revealing the raw approval URI. Existing engagement controls remain display-only until the later signer/write slices are real.

## Deliberate non-goals

Phase 5A does not implement:

- Farcaster FID/account creation;
- protocol signer generation/registration;
- secure private-key persistence;
- Cast/Reply/Like/Recast/Follow writes;
- Composer;
- outbox submission/reconciliation.

Those begin with Phase 5B and later slices.
