# 12 — Roadmap After Phase 0

This roadmap is architectural sequencing, not a promise that all details are fixed.

## Phase 0 — Architecture Contract

Completed.

Deliverables:

- invariants;
- capability taxonomy;
- dependency rules;
- provider boundaries;
- local-first rules;
- feature compartmentalization;
- security/signing rules;
- derived infrastructure policy;
- build profiles;
- architecture tests/acceptance criteria.

## Phase 1 — Foundation + Canonical Domain

Completed foundation scaffold.

Build:

- repository/CMake module graph;
- foundation primitives;
- canonical IDs/entities;
- error/result/task primitives;
- capability identifiers/descriptors;
- no provider implementation required to validate core compilation.

## Phase 2 — Routing + Local Storage Skeleton

**Implemented and core-verified.**

Build:

- capability registry;
- policy engine;
- health snapshot model;
- routing result model;
- SQLite/WAL storage foundation;
- entity/timeline store abstractions;
- architecture tests.

## Phase 3 — Hypersnap Read Adapter

**Implemented and contract-verified on the available Linux/GCC 14 toolchain.**

First real provider behind capability contracts:

- profiles;
- casts;
- social graph;
- following/trending/channel feed reads as supported;
- provider DTO → canonical mapper;
- provider-affine pagination;
- contract tests.

Hypersnap is first implementation, not core architecture. Apple/Clang 22 final-device verification remains an explicit acceptance item after the SDK include-path fix.

## Phase 4 — Read-Only Local-First Vertical Slice

**Implemented and verified on macOS Apple Silicon with Clang 22 + Qt 6.11.1.**

User-visible flow:

```text
Launch
→ local store
→ following feed
→ cast detail
→ profile
→ followers/following
→ channel feed
→ offline restart
→ cached instant rendering
→ reconnect/incremental sync
```

Phase 4 deliberately began with one serialized I/O executor. ADR 0014 now
uses a bounded live-read worker pool while cache/session work remains serialized,
without changing the blocking capability contracts or presentation-facing state.

## Phase 4.5 — Read Reliability and Real Home

**4.5A and 4.5B are implemented and verified.**

- Home is the identity-scoped Following feed; Trending is optional Explore.
- Opening or restoring identity continues through Profile into Following.
- Runtime health and circuit state are independent per provider/capability.
- Repeated transient failures cool down, ordinary reads fail fast or fail over,
  and explicit Retry performs a bounded recovery probe.
- Runtime diagnostics compose routing, provider-request, and transport-stage
  context without leaking provider details into Application or QML.
- The Qt Network implementation lives under Transport, not Platform.

Phase 4.5C rich-read source is implemented: engagement counts, rich embeds,
skeletons, pull/refresh semantics, and infinite pagination. Its final macOS
Clang 22 + Qt 6.11.1 acceptance matrix remains pending and must be completed
before the read milestone is called release-verified.

## Phase 5 — Authentication, Signing, and Protocol Writes

Phase 5 is intentionally staged so public reading never depends on account
state and an authenticated session is never confused with signing authority.

### Phase 5A — Guest-first authentication and client session

Source implemented. The client remains fully readable as Guest. Sign In With
Farcaster establishes a verified account/FID session, but writes remain locked
until a protocol signer is ready. Viewing another identity is browsing context,
not authentication. Platform-specific release acceptance is tracked separately.

### Phase 5B — Protocol signer provisioning and write readiness

**Implementation-final in source; per-platform live release acceptance remains required.**

The source now has a secure local Ed25519 signer/vault boundary, explicit
least-privilege Snapchain scope/TTL policy, replay-safe nonce handling,
non-secret SQLite signer metadata, readiness/revocation orchestration,
cross-platform secure-store composition (Apple Keychain on macOS/iOS and
QtKeychain on Windows/Linux/Android), a canonical custody EIP-712 builder, a
one-shot EIP-1193 compatibility authorizer, a production unified Snapchain
signer registry, and a reusable canonical protocol codec. The adapter reads
`/v1/signersByFid`, constructs protobuf `KEY_ADD` / self-revoking `KEY_REMOVE`,
hashes MessageData with BLAKE3-20, signs the hash through the opaque Ed25519
signer, submits binary messages to `/v1/submitMessage`, and re-reads network
state before success.

SIWF proof is never reused as signer authorization. Missing secure-store,
wallet, or Snapchain dependencies fail closed for writes while guest/public/
cached reads remain available. Each shipping OS must still prove its real secure
store + wallet + current Snapchain flow before interaction support is promoted
for that release; see `PHASE_5B_VERIFICATION.md`.

### Phase 5C — Publish Cast vertical slice

**Implementation-final in source; per-platform live release acceptance remains required.**

Root Cast publication now uses canonical CAST_ADD construction, BLAKE3-20
protocol identity, the Phase 5B verified signer gate, an atomic SQLite optimistic
projection + durable signed-message outbox, durable local remove tombstones,
reconcile-before-retry crash recovery, bounded retry, account isolation,
Snapchain binary submission, validated submit-response confirmation and
`castById` retry/restart reconciliation. A minimal Qt root-cast composer is wired
only as a presentation trigger; QML does not own protocol/signing/persistence.
See `docs/35-phase5c-publish-cast-outbox.md`, `PHASE_5C_REVIEW.md`, and
`PHASE_5C_VERIFICATION.md`.

### Phase 5D — Reply and composer expansion

**Implementation-final in source; per-platform Qt/live release acceptance remains required.**

Reply publication now extends the existing PublishCast path with canonical
`CastAddBody.parent_cast_id` semantics, a provider-neutral parent
`CastReference { fid, hash }`, restart-safe optimistic reply persistence and the
same exact-envelope retry/reconciliation behavior as root casts. Feed/detail
Reply actions open the same Qt composer; parent FID is resolved in C++
presentation state and never reconstructed in QML. See
`docs/36-phase5d-reply-composer.md`, `PHASE_5D_REVIEW.md`, and
`PHASE_5D_VERIFICATION.md`.

### Phase 5E — Reaction, recast, and follow writes

**Implementation-final in source; per-platform Qt/live release acceptance remains required.**

Like/Unlike, Recast/Undo Recast and Follow/Unfollow now reuse the same exact
session + scoped signer + canonical message + durable outbox + desired-state
reconciliation path as Cast publication. Cast removal is included in the same
operation model. Production signer provisioning also supports App-FID
`SignedKeyRequest` attestation so the user custody wallet signs only `KEY_ADD`
once while the App-FID custody key remains server-side. See
`docs/37-phase5e-social-writes-app-fid.md`, `PHASE_5E_REVIEW.md`, and
`PHASE_5E_VERIFICATION.md`.

### Phase 5F — Write hardening

**Source hardening implemented; per-platform/live release acceptance remains required.**

Restart-safe write recovery is now event-driven from network/signer lifecycle
transitions with a low-frequency fallback timer, concurrent resume wakeups are
coalesced, and ambiguous writes reconcile before any resubmit. A submit that
returns Conflict enters durable reconciliation-only mode and is never blindly
POSTed again. Signer replacement/expiry/revocation fail closed for queued
envelopes, authoritative signer restore overrides optimistic sliding-TTL
checkpoints, and migration v15 persists account-scoped Follow/Unfollow desired
state alongside the existing v13 Cast-removal and v14 Like/Recast projections.
The production Snapchain deployment boundary remains the provider-neutral
Genyleap gateway at `snapchain.genyleap.com`; upstream credentials and provider
selection are server-side deployment data. See
`docs/39-phase5f-write-hardening.md`, `PHASE_5F_REVIEW.md`, and
`PHASE_5F_VERIFICATION.md`.

### Phase 5G — Internationalization, Localization, and Locale UX

**Implemented in source; cross-platform Qt/runtime acceptance remains required.**

Before derived product surfaces multiply UI copy, Genycaster now establishes a
portable localization contract: ID-based English/Persian catalogs, runtime
language switching, Persian RTL mirroring, mixed-content direction handling,
independent regional formatting, locale-aware numbers, Gregorian/Jalali/
Shahanshahi and additional Qt calendars, IANA time zones, configurable
Smart/Relative/Calendar timestamp presentation, and user-controlled primary plus
optional secondary monetary display units. A provider-neutral valuation contract
(`valuation.asset_price.read` / `valuation.fx.read`) now preserves decimal quote
text, timestamp and provenance without placing market fetching inside QML or
localization. Live quote adapters remain independently deployable derived
infrastructure. Localization itself introduces no OS/provider dependency into core.
See `docs/42-phase5g-localization.md`, `PHASE_5G_REVIEW.md`, and
`PHASE_5G_VERIFICATION.md`.

## Phase 6 — Derived Infrastructure

Begin independently owned:

- search index;
- Mini App index;
- channel discovery;
- ranking/trending/recommendation services.

## Phase 7 — Messaging

Build on the already battle-tested local store/sync/outbox foundation:

- legacy messaging compatibility lane;
- native secure messaging extension lane;
- conversation/message stores;
- realtime/gap repair;
- E2EE protocol chosen only after security review;
- Telegram-class interaction model.

## Phase 8+ — Optional Product Modules

Incrementally add:

- Mini App host/discovery;
- wallet;
- notifications;
- spaces/live;
- advanced desktop surfaces;
- open capability extensions.
