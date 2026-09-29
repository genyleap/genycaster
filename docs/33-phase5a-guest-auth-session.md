# 33 — Phase 5A Guest-First Authentication and Client Session

## Status

Source implementation gate for the first Phase 5 product slice. The implementation is complete in-tree; release verification remains pending on the declared macOS/Qt matrix.

This phase does **not** publish protocol writes. It establishes the account/session boundary that later write operations must pass through without making public reads depend on authentication.

## Product rule

Genycaster is guest-first:

```text
Guest
  ├─ may open Explore
  ├─ may open public identities, profiles, casts, followers/following and cached data
  └─ may not perform protocol-changing interaction

Authenticated
  ├─ retains every guest read capability
  ├─ owns a verified canonical OpenProof account session
  ├─ has a required linked Farcaster FID
  └─ still requires a valid protocol signer before a write can be submitted
```

Opening or resolving an arbitrary Farcaster identity is browsing context. It MUST NOT establish an authenticated client session.

## Authority and compatibility behavior

Shipping Genycaster is OpenProof-centric and exposes two entry routes:

1. **Continue with Genyleap** starts OpenProof OAuth device authorization. The
   browser opens the Genyleap account surface with a local return target for the
   OpenProof device approval. If a valid Genyleap/OpenProof browser session
   already exists, that session is reused without another login prompt.
2. **Sign in with Farcaster** starts a Farcaster challenge from OpenProof,
   obtains the SIWF signature through the Farcaster relay, and completes the
   proof back at OpenProof.

Both routes converge on the same canonical result: an OpenProof issuer/subject
with a linked Farcaster FID. A Genyleap-authenticated identity that has no
Farcaster connection enters `LinkingFarcaster`; Genycaster starts
`/account/connections/web3/start`, obtains the Farcaster proof, completes
`/account/connections/web3/complete`, and only then persists the client session.

The legacy Farcaster compatibility adapter remains an optional infrastructure
boundary for standalone compatibility verification and authoritative custody
address resolution. During any relay flow:

1. create an authentication/link channel through the Farcaster relay;
2. present the returned approval URL to the user;
3. keep the raw channel token inside the authentication attempt and use it as the bearer credential for status polling;
4. treat the returned approval URI as a transient protocol artifact;
5. complete the exact OpenProof-issued SIWF challenge;
6. let OpenProof verify and bind the returned Farcaster proof to the canonical identity;
7. persist only the normalized local client session, never OAuth tokens, relay channel tokens, approval URIs, SIWF messages, signatures, or ceremony cookies.

The SIWF validation policy matches the current auth-client source contract:

- statement is `Farcaster Auth` (legacy `Farcaster Connect` remains accepted for compatibility);
- chain id is `10`;
- nonce, domain and relying-party URI must match the challenge;
- nonce must remain SIWE-compatible (at least eight alphanumeric characters);
- `expirationTime`, when present, must still be in the future;
- `notBefore`, when present, must already be satisfied;
- exactly one `farcaster://fid/<fid>` resource is required;
- the signing address must be either the custody address for the FID or an active KeyRegistry auth-address (`keyType == 2`, `state == ADDED`).

Signature verification uses the same deployless universal-signature model as the declared `viem >= 2.29.2` dependency floor of `@farcaster/auth-client` 0.7.1. The native adapter therefore verifies EOA, deployed ERC-1271, and ERC-6492 counterfactual signatures before FID authorization. This is a compatibility baseline, not a promise that every future signature scheme added by newer viem versions is automatically supported.

## Ownership

### Domain

Owns the normalized session identity and authentication method. An
OpenProof-backed session stores only the canonical issuer/subject plus linked
FID and presentation metadata. It contains no OAuth token, relay token,
signature, ceremony cookie, wallet private key, or provider DTO.

### Capability

`IAuthenticationProvider` owns the provider-neutral account-authentication
contract. `IFarcasterIdentityResolver` independently resolves protocol custody
metadata. `ICompatibilityAuthentication` combines those roles only for the
legacy Farcaster adapter. Attempt objects keep OAuth/relay credentials opaque
inside concrete adapters.

### Application

`SessionService` owns state transitions:

```text
Guest -> Starting -> AwaitingApproval -> Authenticated
  ^                          |
  |                          +-> LinkingFarcaster -> Authenticated
  |                                  |
  +-------------- failure/sign out --+
```

If a user who already has a verified session starts a replacement/re-authentication attempt, that established session remains authoritative until the new proof succeeds. Attempt phase and authenticated-account state are therefore independent.

Application session state and future protocol signer state remain separate.

### Local state / storage

`IClientSessionStore` is the persistence port. SQLite stores only the normalized session snapshot and expiry.

### Provider adapters

The OpenProof authentication adapter owns:

- OAuth device authorization/token exchange;
- first-party Genyleap SSO approval routing;
- canonical OpenProof issuer/subject resolution;
- required Farcaster-connection discovery and linking;
- direct Farcaster onboarding through an OpenProof-issued challenge;
- transient OAuth, ceremony-cookie and relay-token lifetime.

The legacy Farcaster compatibility adapter owns relay compatibility semantics
and Optimism RPC custody/auth-address resolution. It is not the shipping
canonical account authority when OpenProof authentication is enabled.

### Presentation

QML receives only normalized account state and user intents. QML performs no network, database, JSON parsing, signature verification, or polling transport.

## Security invariants

- Guest reads do not require an auth session or an auth provider.
- Authentication success does not imply write readiness.
- Write readiness will require an authenticated session **and** a valid protocol signer in later Phase 5 slices.
- OpenProof OAuth access/refresh tokens and ceremony cookies remain adapter-private and are never persisted in the client-session database, logged, or exposed to QML.
- The raw relay `channelToken` remains adapter-private and is never persisted, logged, or exposed as a standalone Domain/Application/QML value.
- A canonical OpenProof identity without a linked Farcaster FID cannot become an authenticated Genycaster session.
- Custody-address enrichment is signer metadata, not account authentication. If custody lookup is temporarily unavailable, the verified OpenProof + Farcaster-linked session remains authenticated while protocol-write readiness stays fail-closed.
- The Farcaster approval URI is a transient user-facing protocol artifact and may encode the channel token by protocol design; it is never persisted or rendered as readable/editable QML text.
- Returned SIWF `message` / `signature` values are verified before an authenticated session is committed.
- Authentication polling has a bounded local approval window; an abandoned challenge cannot remain pending indefinitely.
- Private keys are never requested or stored by this phase.
- Sign-out clears the persisted Genycaster client session and destroys any in-process authentication attempt, but does not sign the browser out of Genyleap/OpenProof and does not delete cached public content.
- OAuth access/refresh tokens are attempt-scoped only; they are not written to SQLite or exposed to presentation code. A later native sign-in reuses the first-party Genyleap browser session instead of persisting a provider token in Genycaster.
- Compatibility auth can be compiled out; the client remains usable as a guest.

## UI contract

Guest state exposes a clear `Sign in` account action and explains that browsing remains available without signing in.

The existing `Open identity` flow remains a separate browse action.

During authentication the UI shows:

- **Continue with Genyleap** as the primary OpenProof/SSO route;
- **Sign in with Farcaster** as the direct onboarding route;
- an `Open Genyleap` or `Open Farcaster` action handled through the C++/platform URL opener without rendering the opaque approval URI as text;
- a dedicated `LinkingFarcaster` state when the canonical Genyleap identity is verified but lacks the required Farcaster connection;
- pending / failed / completed state;
- cancel/close behavior that does not affect guest reads.

An authenticated account surface identifies OpenProof as the account authority,
shows the linked Farcaster username/FID, and exposes a `Sign out` action.

No Like/Recast/Reply/Follow/Composer control becomes active in this phase merely because authentication exists. Those controls are enabled only when the Phase 5 signer/write pipeline is real.

## Build policy

`GENYCASTER_ENABLE_OPENPROOF_AUTH` controls the canonical OpenProof account
adapter and is enabled by default for the normal Qt UI composition.

`FARCASTER_ENABLE_COMPATIBILITY_PROVIDERS` controls the legacy SIWF/custody
compatibility adapter. Provider-free and strict-self-hosted core presets may
keep it disabled; normal Qt builds currently retain it for custody-address
resolution and compatibility testing.

The app-owned SIWF relying-party identity is first-party deployment configuration, not a provider constant. Shipping builds default to:

- domain `genycaster.genyleap.com`
- URI `https://genycaster.genyleap.com/login`

Build-time defaults are configurable with `FARCASTER_DEFAULT_SIWF_DOMAIN` and `FARCASTER_DEFAULT_SIWF_URI`. Runtime development/test overrides remain available through `--siwf-domain=`, `--siwf-uri=`, `GENYCASTER_SIWF_DOMAIN`, and `GENYCASTER_SIWF_URI`. Explicit CLI values take precedence over environment values, which take precedence over the shipping defaults.

Optional infrastructure overrides:

- `GENYCASTER_FARCASTER_AUTH_RELAY`
- `GENYCASTER_OPTIMISM_RPC_URL`

## Acceptance criteria

- [ ] Public read paths work with no session and with compatibility auth compiled out.
- [ ] Viewed identity state is independent from authenticated account state.
- [ ] A verified session can be restored from SQLite without contacting the read provider.
- [ ] Expired sessions restore as Guest and are removed locally.
- [ ] Sign-out clears only session state, not read cache.
- [ ] Raw relay channel token remains adapter-private; the transient approval URI is never persisted, logged, or exposed as readable QML state.
- [ ] Completed SIWF response is rejected for wrong domain, URI, nonce, statement, chain, FID resource, expired/not-yet-valid SIWE lifetime, signature, custody mapping, or auth-address registry state.
- [ ] Native signature verification covers the auth-client 0.7.1 / viem 2.29.2 baseline: EOA, deployed ERC-1271, and ERC-6492 counterfactual signatures.
- [ ] Compatibility authentication runs off the Qt GUI thread.
- [ ] QML contains no network/database/crypto implementation.
- [ ] UI smoke still proves guest navigation is interactive without signing in.
- [ ] Architecture guard passes.
- [ ] Full Qt target, strict qmllint and interaction smoke are verified on the declared macOS/Qt matrix before the phase is called release-verified.
