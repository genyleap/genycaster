# Genycaster dual-network Setup: implementation notes (2026-10-16)

## Authorities (never conflate)

1. OpenProof issuer/subject plus linked FID establishes the verified native application session.
2. Farcaster Quick Auth validates a signed wallet's relationship to a FID; it does not establish an OpenProof session or provision an FID.
3. A separately provisioned Snapchain signer authorizes protocol writes. Protocol Quick Auth alone never authorizes publishing.

## Network preference

The initial Setup and the signed-in Language and region settings allow Farcaster or Hypersnap selection. This persists under connection/preferredNetwork, defaults to hypersnap, and is limited to these two validated strings. It is not a global identity toggle: switching does not erase FID, signer secrets, local caches or verified session state.

The live capability router uses atomic preference affinity only after deployment, capability and health eligibility checks. Hypersnap v2 is the only current registered protocol read adapter. On Farcaster preference, the sign-in method goes through existing OpenProof Farcaster sign-in; protocol reads continue on Hypersnap until a Farcaster read adapter is registered. Writes still use Snapchain. **A fully operational network data-source switch is not yet available.**

## Profile handling

Existing profiles follow the current OpenProof PKCE + linked FID ceremony. New profiles open the verified Farcaster signup page (https://farcaster.xyz/~/signup), which routes users to a compatible app, and offer a return-to-connect action. Genycaster currently has no native FID registration implementation. QML never asks for seed/recovery material.

## Quick Auth sidecar

services/quick-auth pins @farcasterorg/quick-auth 0.2.0, with Node 20+ and localhost-only HTTP. An internal caller sends POST /v1/protocol-auth/verify with the Authorization Bearer token and its private X-Genycaster-Internal-Secret; accepted proofs return fid/method/address, rejected proofs return HTTP 401, and hub failures HTTP 503. The domain must be explicit. The verifier handles sdk.protocolAuth.getToken() wallet-signature tokens and classic sdk.quickAuth.getToken() JWTs. No signing keys or persistent user sessions are stored.

**Not yet linked to native authentication:** The OpenProof backend must verify the protocol proof against its logged-in subject and linked FID and enforce anti-replay/session controls before changing account state. This opt-in verifier is intentionally not auto-started or public-facing. Qt native does not automatically have the Mini App JS SDK; native wallet proof acquisition requires a separate authorized integration.

## Validation

- npm ci --ignore-scripts, node --test services/quick-auth/handler.test.mjs
- cmake --preset ui-debug -DCMAKE_PREFIX_PATH=/opt/Qt/current/gcc_64
- cmake --build --preset ui-debug and ctest --preset ui-debug
- Render signed-out Setup on small/wide RTL/LTR windows with both account intents, language changes, cancel and error states, and verify preference persistence after restart.
- Test healthy route affinity, absent Farcaster adapter fallback, ineligible-provider policy, offline/read retry, and unchanged signer/account authority.
- On hosts without QtKeychain >=0.17, signing must remain unavailable (no plaintext fallback).

## Setup UX and consent hardening (2026-10-16 follow-up)

- Welcome uses a single language selector without a redundant checked-language summary. The header and step rail share an aligned progress hierarchy; step labels are allowed to wrap and selection cards use single-choice radio markers instead of completion checks.
- Terms and Privacy are separate legal documents: the official URLs are https://genyleap.com/terms-of-use and https://genyleap.com/privacy. Each document requires an explicit external document open and review acknowledgement. Two independent, reversible switches then capture affirmative Terms agreement and Privacy acknowledgement before moving to Network.
- Qt's canonical consent persistence now uses `setup/legalConsentVersion=2` to invalidate previous implicit/auto-checked consent. The saved record also includes `setup/legalConsentAcceptedAtUtc`, `setup/legalTermsUrl` and `setup/legalPrivacyUrl`; only consent metadata is stored, no document content or user secrets.
- Existing profiles can choose either verified OpenProof device authorization or the OpenProof-bound Farcaster challenge directly; errors are categorized and the alternative route stays accessible. New FID registration opens the verified official https://farcaster.xyz/~/signup page, which directs to compatible mobile apps. Returning users must still link the newly registered FID to the verified account; the native Qt client has no custody seed import and does not claim in-app FID creation.
- **Known integration boundary:** the source cannot independently guarantee successful sign-in against a live OpenProof deployment. Both routes depend on valid remotely configured device authorization / SIWF relay contracts and the currently deployed endpoints. The verifier sidecar is opt-in, not a replacement for canonical native authentication. Testing with a real user account is still required.
- **Known network gap:** Farcaster protocol read transport/codec is not registered. Preference changes apply to existing eligible capability routes but still fall back to Hypersnap for reads. Do not declare a complete independent read-provider switch until Farcaster hub read methods have been wired into normalized feeds, profiles, graphs, health/fallback and continuation storage.

## October 16 runtime error follow-up

- Fixed `FeedSurface.qml`: the new-post sticky banner reads the instantiated `ListView.headerItem` geometry, not an out-of-scope `newPostsSlot` id from its header component. The design guard now rejects the original runtime ReferenceError.
- Fixed disabled SetupActionCard semantics (neutral foreground on neutral disabled backgrounds), removed global dimming of locked sidebar steps, and increased the legibility of the expanded network-status diagnostic pane.
- Account onboarding now separates create-profile copy and former-authentication error visibility. Switching sign-in methods **does not mutate the preferred read network**. Creating an FID is still an external Farcaster signup handoff, not an in-app custody-key creation flow.
- Hypersnap following, discovery and profile-feed request budgets were uniformly raised from 5-6 seconds to 12 seconds across async and compatibility readers to tolerate cold TLS and proxy retries; a genuine offline or blocked node still fails closed and should be diagnosed by endpoint, proxy and node-selection checks. No fake content is substituted on timeout.
- Direct Farcaster 401 **is not bypassed**. OpenProof validates the signed challenge, time window, custody/auth-address authorization and canonical account linkage. Genycaster now preserves an optional sanitized OpenProof `request_id` in debug failure logs, enabling server-side correlation without logging tokens or cookies. This still requires a fresh real-account SIWF/Quick Auth end-to-end verification against the deployed OpenProof issuer and Farcaster relay. The working Genyleap device authorization route remains available independently.

## Dual-source read composition (October 2026)

Genycaster always registers the compatible Hypersnap v2 read source. It now conditionally registers a **second, separately identified** `farcaster-snapchain` provider instance when `GENYCASTER_FARCASTER_READ_BASE_URL` is set, or when `GENYCASTER_FARCASTER_READ_API_KEY` is provided (the latter defaults the URL to `https://api.neynar.com`). The optional hosted API key is sent as an `x-api-key` HTTP header and is **not** baked into app settings or source code. The source has separate capability routing, health observations, provider ID and continuation scope. UI shows when the hosted endpoint is not configured, and Hypersnap remains the safe default.

**Important compatibility gate:** This source reuses the current Hypersnap/Neynar-v2-compatible JSON reader. Different Farcaster hub /v1 endpoints, providers or Neynar API revisions may not satisfy all operation shapes (search, notifications, follow graph, pagination, etc.). Supplying an arbitrary Farcaster hub URL is not sufficient. Before claiming independent production read coverage, exercise a real key and endpoint against all registered capabilities with profile, following, trending, cast and pagination contract fixtures, then narrow registration for unsupported operations. The UI must not imply that specifying a host automatically guarantees API compatibility.

Farcaster is a decentralized **protocol**, but Neynar API access is a **hosted/centralized data service**. The short labels in setup refer to the selected read-service topology, not custody, FID ownership or the Farcaster protocol itself.

The legal onboarding now uses its internal scrollable reader. Users explicitly confirm each document at the end; confirmation submits an independent two-document consent record and advances after the native bridge confirms persistence. The previous requirement to open a browser before acknowledging a document was removed. The small in-app text is a *product summary* and does not replace the canonical full legal agreements; before final legal launch, distribute versioned complete offline policy text and secure legal approval for that snapshot.

**Wallet custody and FID registration are distinct.** Genycaster currently provides a verified external FID registration handoff and account linking, not direct native Ethereum custody-wallet creation or BIP-39 recovery. Never ask for a recovery phrase in a generic Qt text field, log a mnemonic, or treat new-wallet generation as a registered Farcaster identity. Proper native create/import requires audited BIP-39/BIP-32, secp256k1/EVM signing, encrypted Keychain-only custody, recovery-confirmation, onchain IdRegistry registration, payment/gas, identity linking and rollback/retry tests.

## Runtime screenshots follow-up: account action, node labels and server 401

- The account page now has **two independent sign-in action cards**. Their selected color and in-flight spinner follow `authenticationRoute` once a ceremony begins, not `preferredNetwork`; clicking Farcaster cannot leave a busy Genyleap card.
- Setup step labels/icons use explicit `stepButton.hovered/current` state and accessible ink tokens, without rendering unvisited steps as blank Qt-disabled content. The selection indicator uses the frameless check SVG.
- Farcaster's short read-service label is now `Centralized`, with no `Hosted` suffix. The optional Neynar node shows its configured endpoint or `https://api.neynar.com` and the missing `GENYCASTER_FARCASTER_READ_API_KEY` requirement. User-facing API topology language does not characterize Farcaster protocol custody.
- The Neynar API key is **never checked into the binary**. A deployment may supply it via `GENYCASTER_FARCASTER_READ_API_KEY`, or supply a compatible API gateway via `GENYCASTER_FARCASTER_READ_BASE_URL` without a client key. Credentialed async API requests refuse HTTP redirect following to avoid cross-host key forwarding.
- Farcaster sign-in authentication **does not use** the Neynar read API key. The user observed an OpenProof `/auth/web3/complete` HTTP 401 with request id `YBp5roTwa1TNUvNNHj2ihA`. The start and relay approval completed, but OpenProof refused proof completion. OpenProof canonical journal is not allowlisted by Remote Operations, so the definitive server rejection reason is not yet available. Do not bypass replay/nonce, SIWF time-window, custody/auth-address, or signature verification to make the UI appear to work. Correlate the request id with the deployed backend's secure logs and check exact verifier version/configuration and signature proof; a successful local contract test is not a live sign-in verification.

## 2026-10-16 live read investigation and bounded recovery

The primary public Hypersnap node was tested against the [published feeds contract](https://hypersnap.org/docs/reference/reads/feeds). `/v1/info` is healthy, `/v2/farcaster/feed/user/casts?fid=3&limit=2` succeeds, and `/feed/following?fid=3&limit=20` returns 20 live casts and a usable cursor. However `/v2/farcaster/feed/trending?limit=2`, generic `/feed?feed_type=trending`, and `/feed/for_you` stalled beyond ten seconds, and `/feed/channels?channel_ids=base,dev,memes` returned an empty `casts` array. Consequently a healthy node probe does **not** prove that `feed.trending.read` is healthy.

The async **Hypersnap-only** Trending reader now bounds the ranking attempt to 3500 ms and, on timeout/service-unavailable, fetches a public *community discovery* feed from documented `/feed/following?fid=3`. The seed FID is a public, stable community discovery source, **not** the signed-in user and **not** a ranking algorithm; no synthetic posts are made. Continuation tokens use a private `community:` provider-cursor marker so all later pages stay on the same source. This mode is explicitly labelled **"Trending is temporarily unavailable. Showing live community posts instead."** above the feed. Other providers and other capabilities never silently substitute community data. If both reads fail the error remains surfaced. The live probe `python3 tools/probe_hypersnap_feed.py` exercises real service responses and checks nonempty fallback and cursor integrity.

The healthy sidebar `Live / Ready` card now collapses details automatically and does not offer expansion only because debug diagnostics exist. On failures users can still inspect details and retry. Setup descriptions emphasize that both ecosystems support reading and publishing, without surfacing developer API-key configuration in the main selection cards.

### Authentication boundary

The 401 observed after Farcaster relay approval is emitted by OpenProof `/auth/web3/complete`, not by the Hypersnap or Neynar data adapter. A direct native client cannot call Mini App SDK `sdk.protocolAuth.getToken()` as a drop-in replacement for an account session; the existing `services/quick-auth` token verifier is intentionally protocol-proof-only and does not mint Genycaster sessions. Keep signer/nonce/expiration/domain/FID ownership validation enabled. Correlate OpenProof request ID `YBp5roTwa1TNUvNNHj2ihA` to secure backend verification logs; remote access to that unit is currently not allowlisted. Before calling Farcaster sign-in resolved, retry with an actual matching custody/auth-address signer and verify the new canonical OpenProof session end to end. The desktop sign-in flow remains independently selectable from data network.

## Direct native Farcaster sign-in — routing and Keccak fix (2026-10-16)

Root cause in Genycaster, not OPP: Runtime constructed both adapters but registered
only OpenProof as the active authentication provider. Consequently both login
routes entered OpenProof, and clicking Farcaster sent a completed relay proof to
the OpenProof verification endpoint (HTTP 401).

SessionService now has two independent authentication ports. Genyleap invokes
OpenProof OAuth/device authorization; Farcaster invokes the existing native
FIP-11 relay and EIP-191/Optimism on-chain custody or auth-address verifier.
Farcaster sessions are identified by their verified FID, without requiring an
OpenProof issuer or subject. Protocol signer and publishing permissions remain
separate. A missing direct Farcaster provider fails closed as Unsupported;
it never silently uses OpenProof instead.

Running the previously unbuilt provider.farcaster_auth.contract test exposed
the broken OpenSSL EVP_MD_fetch("KECCAK-256") assumption. Ethereum uses
legacy Keccak-256, NOT the standardized SHA3-256 padding; normal OpenSSL
providers do not supply the named digest. The adapter now includes Keccak-f[1600]
with Ethereum padding. Regression tests check independent reference function
selectors: keyDataOf(uint256,bytes)=ac34cc5a,
idOf(address)=d94fe832 and custodyOf(uint256)=65269e47.
Session routing tests cover the two separate routes and failure when direct
Farcaster auth is not compiled.

@farcasterorg/quick-auth remains available for native Mini App protocol tokens
with a separately verified domain and hub. It does not replace the approval
ceremony in a Qt desktop application. Actual macOS wallet approval and network
availability still require real-device end-to-end checks; contract tests are
not proof of a live production sign-in.

## Network selector: exact live-read routing (verified 2026-10-16)

The network choice sets `CapabilityRouter::PreferredNetwork`, not a fixed HTTP
host. Farcaster is preferred by provider ID `farcaster-snapchain` when registered
and healthy; otherwise the compatible `hypersnap-v2` provider remains eligible.
The user can therefore see a Farcaster preference while a read is actually
served from Hypersnap. That fallback must not be described as a Neynar connection.

- **Hypersnap:** configured `GENYCASTER_HYPERSNAP_BASE_URL`, else
  `https://haatz.quilibrium.com`, with the v2-indexed Farcaster-compatible
  paths implemented by the Hypersnap adapter.
- **Farcaster/Neynar:** registers only with `GENYCASTER_FARCASTER_READ_BASE_URL`
  or `GENYCASTER_FARCASTER_READ_API_KEY`. Supplying the latter alone selects
  `https://api.neynar.com`; request credentials use `x-api-key`.
  Supports `/v2/farcaster/feed/trending`,
  `/v2/farcaster/feed/following`,
  `/v2/farcaster/feed/user/casts`,
  `/v2/farcaster/user` and `/v2/farcaster/cast` through the configured
  Neynar-compatible JSON gateway. This key must not ship in a distributed
  desktop binary; a deployment-owned proxy is preferable.
- **Sign-in:** native FIP-11 SIWF uses Farcaster Relay plus Optimism ownership
  verification independently of the read network.
- **Protocol writes:** require signer authorization/attestation and submit
  signed messages using Snapchain. Changing the read network does not replace
  signer authorization or publishing destinations.

UI regression (2026-10-16): the lazily loaded notifications route previously
bound a child `busy` property to a shell snapshot still updated by the shared
`notificationsChanged` signal and produced QML binding loop warnings.
The child loading flag now receives *imperative assignments* on loader completion
and notification updates, never an intra-route QML binding. GenyDrawer now
animates the Qt-owned `position` property for edge-correct, swipe-compatible
open/close transitions, respecting the user's reduced-motion preference.
Mobile account sheets size from their identity/action content instead of always
occupying a fixed 660px panel. The writer action is disabled when no App-FID
attestation authority has been configured; this is an honest UI state, not a
replacement for the missing server-side signer.
