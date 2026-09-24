# 22 — Phase 4 Local-First Read Pipeline

## Feature implementation gate

```text
Responsibility: Orchestrate canonical local reads, background provider refresh, persistence, and incremental presentation state for the first read-only product slice.
Capability origin: Protocol (identity/profile/cast/social), Derived/Protocol (following/channel feeds), Derived (trending), Local (cache/read status).
Mandatory or optional: Identity/profile/cast/following are mandatory for the Phase 4 slice; trending/channel/social surfaces remain independently removable product composition even though this phase demonstrates them.
Public contract: Provider-neutral application read queries/states, an executor port, a routed read gateway, canonical local entity/timeline/social ports, and a thin Qt presentation adapter.
Canonical data/invariants: Domain Profile/Cast/Fid/FeedEntry/SocialRelation remain authoritative; provider responses and cursors never become presentation models; cache is published before refresh; usable cache survives failure.
Dependencies: Application → capability/domain/local ports; sync execution → application ports; routing gateway → capability router and capability sources; storage → local/application-facing persistence ports; presentation → application only; composition → concrete adapters.
Expected failures: Invalid selectors/limits, missing route/source, offline/timeout/rate limit/transport/protocol failure, storage/migration failure, cancellation, and stale continuation/provider affinity conflict.
Local-state representation: Canonical profiles/casts, normalized timeline entries, social relations, application-safe continuation tokens, identity aliases, and refresh timestamps in SQLite WAL.
Offline behavior: Previously persisted feed/profile/cast/social data publishes as stale when refresh fails; actual device Offline state is owned by OS reachability in the Qt composition layer and never inferred from provider failure.
Provider policy and disablement: Routing remains operation-level; Hypersnap is opt-in and provider-free/strict/minimal profiles remain buildable. With no eligible binding, refresh reports unavailable while cache stays readable.
Failover semantics: Each request resolves one eligible provider per capability. Application tokens remain opaque; a token rejected by a changed provider route produces a recoverable conflict and must not expose or forward a foreign raw cursor.
Tests proving the boundary: Cached-before-refresh ordering, canonical persistence/update, failure preservation, pagination affinity/opacity, offline restart, provider-free imports/builds, strict self-hosted routing, architecture leakage guard, macOS include guard, QML lint/smoke.
```

## Execution decision

The existing blocking capability interfaces remain unchanged. Phase 4 adds one serialized I/O executor behind a small application port. The read service posts cache, SQLite, routing, provider, normalization, and reconciliation work to that executor and invokes observers from the worker. The Qt presentation adapter marshals only small state commits back to the GUI thread.

This is intentionally the smallest runtime topology that proves the boundary. It can later split database/network/model work into dedicated executors without changing capability or presentation contracts.

## Read lifecycle

```text
QML intent
  → C++ presentation adapter
  → application read service
  → serial I/O executor
  → canonical SQLite cache
  → cached/stale observer state
  → capability router + bound provider-neutral source
  → canonical provider result
  → transactional batch/list persistence
  → fresh observer state
  → queued Qt-thread presentation commit
```

On refresh failure, the final state retains any usable cached snapshot, sets stale/error plus source-unavailable classification, and exposes Retry. `isStale` is false when no usable cache exists. Device Offline state is separate OS reachability evidence. Pagination uses the same lifecycle but appends and deduplicates rather than replacing current rows. Canonical entity upserts are idempotent; after they succeed, each list's ordering plus application-safe continuation token commits in one SQLite transaction so a visible page never advances without its matching continuation state.
