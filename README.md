# Genycaster — Native Local-First Farcaster Client

This repository contains the architectural contract and the buildable foundation for a native Farcaster client designed around five non-negotiable properties:

1. **Protocol-first** — Farcaster protocol semantics are distinct from product/backend semantics.
2. **Local-first** — the UI renders authoritative local state; the network synchronizes it.
3. **Capability-driven** — application behavior depends on operation-level capabilities, never vendor identity.
4. **Provider-agnostic** — Hypersnap is the preferred protocol provider policy, not an architectural dependency; managed/legacy backends remain compatibility adapters only.
5. **Feature-compartmentalized** — optional product surfaces do not become core dependencies merely because a flagship client ships them.

## Current implementation state

> **Current source of truth:** [`STATUS.md`](STATUS.md). The phase notes below are retained as historical implementation/acceptance evidence; the tree has advanced beyond the last numbered phase described here.

Phases 0–3 established the architecture, Modern C++26 module foundation, operation-level routing, SQLite state, and the first protocol-provider adapter.

**Phase 4 is implemented:**

- cache-first Application use cases for identity, profile, cast, following/trending/channel feeds, followers, and following;
- serialized cache/session executors plus a bounded RAII live-read worker pool that keep SQLite/provider work off the Qt GUI thread;
- an operation-level routed read gateway with no concrete provider dependency in Application or Presentation;
- SQLite schema migration 5 for durable read continuations and social-list materialization;
- immediate cached snapshots, background refresh, canonical reconciliation, stale/error/source-degradation preservation, OS-reachability-aware presentation, and append/deduplicate pagination;
- a thin Qt presentation adapter and adaptive QML shell with explicit local-first lifecycle states;
- a Qt Network transport adapter under `src/transport/qt_network/` for the graphical runtime, preserving TLS verification while honoring system proxy configuration;
- bounded feed reads with 5-second Following and 6-second derived-feed budgets, stage-aware timeout diagnostics, and an idle Home that does not auto-fetch derived Trending on first launch;
- deterministic cache-order, reconciliation, pagination, offline-restart, and worker-thread tests;
- actual Apple Silicon Clang 22 + Qt 6.11.1 clean-build verification across the required matrix.

**Phase 4.5A is implemented:** Home now means the identity-scoped Following feed, while Trending is an explicit Explore surface. Opening or restoring a usable identity deterministically continues through Profile into Following, with isolated settings and end-to-end runtime coverage so discovery cannot gate the primary read path. The feature gate is recorded in `docs/30-phase4.5a-identity-following-reliability.md`; exact build and test evidence is in `PHASE_4_5A_VERIFICATION.md`.

**Phase 4.5B is implemented:** routed read outcomes now maintain independent health and circuit state for each `(provider, capability)` pair. Repeated transient failures enter a monotonic cooldown, ordinary navigation fails fast or fails over, and explicit Retry performs a bounded recovery probe. Runtime diagnostics identify the capability, provider, endpoint, request budget, latency, result, and Qt transport stage. The Qt Network implementation is owned by Transport rather than Platform. See `docs/31-phase4.5b-capability-health-and-circuit-breaking.md` and `docs/adr/0012-runtime-capability-circuit-breaker.md`.


**Phase 4.5C source is implemented:** rich cast presentation now includes canonical engagement counts, bounded rich embeds, safe external opening, cache-preserving skeleton/refresh behavior, and infinite pagination. Final macOS Clang 22 + Qt 6.11.1 verification is still pending; see `PHASE_4_5C_REVIEW.md` and `PHASE_4_5C_VERIFICATION.md`.

**Phase 5A source is implemented:** Genycaster is guest-first. Public and cached reads remain usable without an account; Sign In With Farcaster establishes a separately modeled verified account session; protocol-changing interactions remain locked until a valid protocol signer is provisioned. Viewing identity and authenticated account state are intentionally independent. See `docs/33-phase5a-guest-auth-session.md`, `PHASE_5A_REVIEW.md`, and `PHASE_5A_VERIFICATION.md`.

**Phase 5B is implementation-final in source:** Genycaster now has the complete guest/session/signer authority split, cross-platform secure signer storage (Apple Keychain on macOS/iOS; QtKeychain secure backends on Windows/Linux/Android), canonical Snapchain gasless EIP-712 authorization, BLAKE3/protobuf KEY_ADD and self-revoking KEY_REMOVE submission, mandatory network re-verification, a one-shot EIP-1193 compatibility authorizer, and Qt signer-readiness UI. Missing secure-store/wallet/Snapchain dependencies fail closed for writes without disabling guest/public/cached reads. Platform-native live acceptance remains required before a release claims interaction support on that OS. See `docs/34-phase5b-protocol-signer-readiness.md`, `PHASE_5B_REVIEW.md`, and `PHASE_5B_VERIFICATION.md`.

**Phase 5C is implementation-final in source:** root Cast publication now runs through a canonical CAST_ADD codec, BLAKE3-20 CastId/idempotency identity, the current Phase 5B `CastAdd` signer authority, an atomic SQLite optimistic projection + durable exact-signed-message outbox, reconcile-before-submit recovery, bounded retries, per-account isolation, and a concrete Snapchain `/v1/submitMessage` + `/v1/castById` adapter. A minimal Qt root composer drives the use case without owning protobuf/signing/storage/transport. See `docs/35-phase5c-publish-cast-outbox.md`, `PHASE_5C_REVIEW.md`, and `PHASE_5C_VERIFICATION.md`.

**Phase 5D is implementation-final in source:** replies reuse the same PublishCast authority/outbox/reconciliation pipeline. Canonical parent context is modeled as parent FID + 20-byte CastId and encoded as `CastAddBody.parent_cast_id`; malformed parents fail before signing. Feed/detail Reply actions open the same Qt composer, while parent FID resolution stays in C++ presentation state so QML never constructs protocol identities or crosses the JavaScript u64 precision boundary. Restart retry uses the exact signed reply envelope without re-signing. See `docs/36-phase5d-reply-composer.md`, `PHASE_5D_REVIEW.md`, and `PHASE_5D_VERIFICATION.md`.

**Phase 5E is implementation-final in source:** the durable protocol-write pipeline now covers Cast deletion, Like/Unlike, Recast/Undo Recast, and Follow/Unfollow with exact signer scopes, canonical protobuf messages, restart-safe target persistence, reconcile-before-submit desired-state checks, and Qt Feed/Detail controls. Production signer provisioning also supports a Genycaster App FID: an HTTPS attestation boundary signs `SignedKeyRequest` with the App-FID custody authority so the user custody wallet signs only the final gasless `KEY_ADD`; the App-FID private key never ships in the desktop binary. See `docs/37-phase5e-social-writes-app-fid.md`, `PHASE_5E_REVIEW.md`, and `PHASE_5E_VERIFICATION.md`.

**Phase 5F source hardening is implemented:** outbox recovery now wakes on network/signer lifecycle transitions, concurrent resume scans are coalesced, timeout retries reconcile before exact-envelope resubmission, Conflict becomes durable reconciliation-only state, and signer replacement/expiry/revocation terminally invalidates stale queued envelopes before network access. SQLite migration v15 also makes Follow/Unfollow desired state account-scoped and restart-safe, completing the local interaction projections alongside v13 Cast-removal tombstones and v14 Like/Recast state. Per-platform Qt/toolchain and live fault-injection acceptance is still required before Phase 5F is release-complete; the existing macOS result is one platform evidence set, not the product portability boundary. See `docs/39-phase5f-write-hardening.md`, `PHASE_5F_REVIEW.md`, and `PHASE_5F_VERIFICATION.md`.


**Phase 5G localization is implemented in source:** Genycaster now uses ID-based translation catalogs with complete English and Persian packs, runtime language switching, Persian RTL application mirroring, content-sensitive direction for casts and composer/input text, independent regional formatting, locale-aware numbers/compact counts, Gregorian/Jalali/Shahanshahi and additional calendar presentation, IANA time zones, Smart/Relative/Calendar timestamp presentation, and user-controlled primary plus optional secondary monetary display units (including IRR/IRT presentation). Localization remains a device-local Qt presentation concern: it does not call providers, alter canonical timestamps, or invent token/FX rates. The same source contract targets macOS, Windows and Linux desktop compositions, with mobile acceptance added when iOS/Android compositions ship. See `docs/42-phase5g-localization.md`, `docs/43-phase5g-monetary-valuation.md`, `docs/adr/0013-id-based-localization-and-locale-formatting.md`, `PHASE_5G_REVIEW.md`, and `PHASE_5G_VERIFICATION.md`.

**Media upload is implementation-final in source:** the native Composer can select local images/videos, preview image selections, upload with progress/cancel/retry, block Cast publication while bytes are in flight, and insert only a verified stable media URL after completion. The desktop client talks only to `storage-farcaster.genyleap.com`; qStorage credentials and provider semantics stay in `services/media-gateway/`. Uploads use bounded gateway multipart relay chunks signed server-side with normal SigV4 Authorization headers because live qStorage tests accept header-authenticated PUT and presigned GET but reject presigned PUT query authentication. Casts persist only `https://media-farcaster.genyleap.com/m/...` URLs that resolve through short-lived signed read redirects. The gateway storage boundary remains provider-neutral and delegates signing to the pinned Quilibrium C++ SDK through a small Node-API addon. qStorage can later be replaced by R2, S3, B2, or another backend without changing QML, application contracts, existing Cast URLs, or ordinary desktop configuration. Source/unit/architecture verification is recorded in `MEDIA_UPLOAD_VERIFICATION.md`; one live qStorage multipart smoke test plus the normal Qt/macOS acceptance build remain deployment gates before calling the feature production-accepted. See `docs/44-media-upload-gateway.md` and `services/media-gateway/README.md`.

**Profile/read expansion is implemented in source:** Profile now has isolated local-first Posts, Quotes, Replies, Likes, and Popular timelines with continuation pagination, refresh, Followers/Following navigation, author-profile navigation, cast-detail parent traversal, and the existing reply/reaction/delete controls on profile Cast rows. Posts exclude replies and quotes; Replies exclude recast entries from the provider compatibility endpoint. Profile/social targets are isolated from the Home viewing identity, so browsing another author cannot silently retarget Following. Feed pagination no longer derives continuation availability from busy state; deferred constant-geometry load-more handling removes the reported `canLoadMore` binding cycle. Reachable-but-stalled provider capabilities also get a bounded automatic recovery probe while cached content degrades to a non-blocking Saved state. Profile fidelity now preserves PFP, banner, URL, location and social counts, and Cast image embeds are rendered through the C++ remote-image boundary rather than direct QML network I/O. Shipping source now compiles the dedicated Genycaster/Genyleap App FID `866408`; publishing still stays fail-closed until its custody-backed signer-attestation service is configured. macOS Qt runtime/qmllint verification remains required. See `docs/40-profile-read-expansion.md` and `docs/41-profile-fidelity-media.md`.

Hypersnap remains a composition-root provider choice rather than a core dependency. Provider-free core, strict self-hosted, and minimal presets still compile it out explicitly; a fresh runnable UI configuration now enables the sole implemented live-read adapter by default so IDE/default builds cannot silently produce a dead providerless application.


## Production identity and Snapchain access

Shipping builds have first-party deployment defaults and therefore do not require command-line identity/network flags for normal use:

- SIWF domain: `genycaster.genyleap.com`
- SIWF URI: `https://genycaster.genyleap.com/login`
- Snapchain protocol origin: `https://snapchain.genyleap.com`

These are build-configurable through `FARCASTER_DEFAULT_SIWF_DOMAIN`, `FARCASTER_DEFAULT_SIWF_URI`, and `FARCASTER_DEFAULT_SNAPCHAIN_BASE_URL`. At runtime, explicit CLI values win over `GENYCASTER_SIWF_DOMAIN`, `GENYCASTER_SIWF_URI`, and `GENYCASTER_SNAPCHAIN_BASE_URL`, which in turn win over the shipping defaults. This keeps localhost/custom-node workflows available without making development configuration a release requirement.

The shipping desktop composition uses `https://snapchain.genyleap.com` as the stable signer/write protocol origin. Upstream provider identity and credentials are deliberately absent from the client: the Genyleap gateway injects any required server-side authorization and can be repointed to another managed or self-hosted writable Snapchain origin without a desktop release. The gateway implementation and deployment contract live in `services/snapchain-gateway/` and `docs/38-genyleap-snapchain-gateway.md`.

Advanced development/self-hosting overrides remain available through `--snapchain-base-url=` / `GENYCASTER_SNAPCHAIN_BASE_URL`. A direct origin that requires HTTP authorization may additionally use the provider-neutral `--snapchain-auth-header=`, `--snapchain-auth-value=`, `GENYCASTER_SNAPCHAIN_AUTH_HEADER`, and `GENYCASTER_SNAPCHAIN_AUTH_VALUE`. Shipping builds do not need these credentials.

Qt networking follows the operating-system proxy configuration by default. If a macOS proxy/PAC entry is stale or incompatible with Qt Network, the user can explicitly request direct transport with `--network-proxy=direct` or `GENYCASTER_NETWORK_PROXY=direct`; `--network-proxy=system` restores the default. Genycaster never silently falls back around a configured system proxy.

Genycaster also does not depend on a Persian font being installed on the host. Shipping UI builds embed the pinned Vazirmatn `v33.003` family into Qt resources. CMake uses `GENYCASTER_UI_FONT_DIR` for offline/local font sources or acquires the pinned upstream files when `GENYCASTER_FETCH_UI_FONT=ON`. End users do not install the font separately. Primary navigation/control icons are vector QML Shapes embedded in the application and therefore have no icon-font or system-font runtime dependency.

Legacy persisted sessions without `custodyAddress` are now upgraded automatically on restore through the configured Optimism RPC; users are not required to sign out merely because their session predates custody verification.

## Production interaction authorization

The shipping build already compiles the dedicated Genycaster/Genyleap App FID `866408`. For the lowest-friction production signer flow, configure its trusted custody-backed attestation endpoint before launching the Qt client:

```text
GENYCASTER_SIGNER_ATTESTATION_URL=https://<trusted-origin>/v1/farcaster/signer-attestation
```

`GENYCASTER_APP_FID=866408` remains available as an explicit runtime/self-hosting override, but ordinary users do not configure it. With the attestation endpoint available, the service supplies the App-FID `SignedKeyRequest` proof and the user wallet signs only the scoped gasless `KEY_ADD` once. After Snapchain confirms the signer, normal Cast/Like/Recast/Follow writes use the isolated local Ed25519 signer without reopening the custody wallet. Shipping UI composition now fails closed when the App FID or attestation service is absent, and it also rejects an App FID equal to the signed-in user's FID. Genycaster therefore never falls back to self-attribution. Farcaster derives the visible `Sent from …` identity from the configured App FID's Farcaster profile; the local `Genycaster` product-name string cannot override that protocol identity. The exact HTTPS request/response and EIP-712 contract is documented in `docs/37-phase5e-social-writes-app-fid.md`.

Signer provisioning verifies the current FID custody address from IdRegistry before opening the wallet. Older persisted sessions that predate this field are upgraded automatically through the configured Optimism RPC during restore; a valid session no longer requires a forced sign-out merely to obtain `custodyAddress`. The approval page shows the exact custody address that must be selected in the EIP-1193 wallet.

## Engineering authority

This repository follows the rules in `thecompez/ai-for-modern-cpp` at the pinned revision recorded in `AGENTS.md` and `docs/19-engineering-authority.md`.

The core reference path is **C++26**, project-owned named modules, minimal standard headers in global module fragments, `FILE_SET CXX_MODULES`, and `.hpp` only at explicit textual compatibility boundaries.

## Start here

For humans:

1. `docs/00-architecture-overview.md`
2. `PHASE_4_REVIEW.md`
3. `PHASE_4_VERIFICATION.md`
4. `PHASE_4_5A_VERIFICATION.md`
5. `PHASE_4_5B_REVIEW.md`
6. `PHASE_4_5B_VERIFICATION.md`
7. `PHASE_4_5C_REVIEW.md`
8. `PHASE_4_5C_VERIFICATION.md`
9. `PHASE_5A_REVIEW.md`
10. `PHASE_5A_VERIFICATION.md`
11. `PHASE_5B_REVIEW.md`
12. `PHASE_5B_VERIFICATION.md`
13. `PHASE_5C_REVIEW.md`
14. `PHASE_5C_VERIFICATION.md`
15. `PHASE_5D_REVIEW.md`
16. `PHASE_5D_VERIFICATION.md`
17. `PHASE_5E_REVIEW.md`
18. `PHASE_5E_VERIFICATION.md`
19. `docs/22-phase4-local-first-read-pipeline.md`
20. `docs/44-media-upload-gateway.md`
21. `MEDIA_UPLOAD_VERIFICATION.md`
20. `docs/33-phase5a-guest-auth-session.md`
21. `docs/34-phase5b-protocol-signer-readiness.md`
22. `docs/35-phase5c-publish-cast-outbox.md`
23. `docs/36-phase5d-reply-composer.md`
24. `docs/37-phase5e-social-writes-app-fid.md`

For Codex, Claude, ChatGPT, or another coding agent: **read `AGENTS.md` first**.

## Architectural invariant

> Replacing or removing any managed backend must not redesign the client.
>
> Replacing Hypersnap must not redesign the client.
>
> Provider DTOs, cursors, errors, SDK types, credentials, URLs, and naming conventions terminate at adapter boundaries.
>
> A product moat must never silently become an architectural dependency.

## Verification status

Phase 4 verification is recorded in `PHASE_4_VERIFICATION.md`. The required provider-free/provider/UI profiles were configured, built, and tested from clean directories on macOS Apple Silicon with Homebrew Clang 22 and Qt 6.11.1. Strict QML lint, warning-fatal headless smoke, architecture guards, and compact/standard/wide rendered inspection are included in the evidence.

Snapchain signer provisioning and protocol writes default at composition time to the Genyleap gateway `https://snapchain.genyleap.com`. The desktop client sends no upstream-provider credential to that origin. The gateway owns the current writable Snapchain upstream and injects any required server-side credential. Advanced/self-hosted deployments may override the origin with `--snapchain-base-url=<url>` / `GENYCASTER_SNAPCHAIN_BASE_URL`; optional direct-origin authorization is provider-neutral through `--snapchain-auth-header`, `--snapchain-auth-value`, `GENYCASTER_SNAPCHAIN_AUTH_HEADER`, and `GENYCASTER_SNAPCHAIN_AUTH_VALUE`. Changing the gateway upstream does not require a client release.

For the Phase 5F source-hardening delivery, a fresh Linux/GCC 14.2 configure passes; `architecture_guard`, canonical protocol-write contracts, and `storage.durable_write_outbox` pass from the clean build, including migration-v15 Follow/Unfollow durability. GCC 14.2 still ICEs while serializing several higher-level C++ module interfaces (`cp/module.cc:16297`), including unchanged baseline interfaces; a Clang 17 fallback cannot start module compilation because `clang-scan-deps` is absent. Qt6 is unavailable in the Linux container, so the macOS Clang 22 + Qt 6.11.x full GUI/lint/live fault-injection acceptance remains the release gate. Exact evidence is in `PHASE_5F_VERIFICATION.md`.

For the profile/read expansion, a clean provider-enabled UI-disabled configure and the architecture guard pass; the modified Hypersnap feed interface/implementation/registration and provider contract-test translation unit compile successfully. The full read-pipeline test executable remains blocked by the same GCC 14.2 module-serializer ICE, and Qt6 is unavailable, so strict qmllint plus warning-fatal runtime verification of the reported pagination binding-loop fix remain macOS acceptance gates.

### Clang 22 routing hotfix

The Phase 3 snapshot includes a portability correction for Homebrew Clang 22 + libc++ named-module builds: routing registries avoid `std::ranges` iterator instantiations that failed in the reported macOS toolchain, and implementation units directly include the standard facilities they use. See `docs/21-macos-clang22-routing-hotfix.md`.

### Internal Mini Apps (macOS, source implementation)

The optional `FARCASTER_ENABLE_MINIAPPS=ON` composition opens validated Mini App
cards in an application-owned WKWebView window instead of the external browser.
Existing CMake caches with this option OFF must explicitly enable it; fresh macOS
UI configurations default to ON. This increment implements basic host context,
ready/close, back, profile and cast navigation. Wallet, SIWF/Quick Auth, app
installation and notifications are not implemented or advertised. Apps requiring
those capabilities are not launched. This change has not been configured, built,
or tested at the user's request. See `MINIAPP_HOST_REVIEW.md` for boundaries and
the remaining compatibility/security acceptance gates.
