# Phase 5F Review — Write Hardening

Date: 2026-08-29

## Scope reviewed

Phase 5F hardens the existing session -> scoped signer -> canonical protobuf ->
exact signed envelope -> SQLite outbox -> reconcile/submit pipeline for:

- network and signer lifecycle recovery;
- concurrent recovery wakeup coalescing;
- timeout-after-merge and conflict/delayed-index ambiguity;
- restart-safe Follow/Unfollow presentation state in addition to the existing
  Cast-removal and Like/Recast projections;
- signer replacement, expiry and revocation while writes are pending;
- authoritative sliding-TTL restore semantics;
- operation-sensitive authoritative absence for Cast removal.

## Architecture result

PASS in source review.

There is still one mutation architecture. Application owns orchestration;
canonical protocol encoding and Snapchain adaptation remain explicit
capability/provider boundaries; SQLite owns durable local state; presentation
owns UI projection; QML remains presentation-only.

Network reachability is now a recovery signal rather than write-domain logic.
The presentation bridge emits a typed reachability transition and the
composition root wakes `WriteService`; the write service itself remains
provider-neutral.

## Recovery result

PASS in source design and focused tests.

- network-online and signer-ready transitions wake the durable outbox;
- a 60-second timer is only a safety fallback and skips known-offline state;
- concurrent `resume()` calls coalesce before executor dispatch;
- transient submit failures reconcile before any later submit;
- timeout-after-merge can confirm on the next reconcile with one total POST;
- Conflict enters a durable reconciliation-only state and never blindly POSTs
  again, including after restart;
- maximum attempts still perform a final authoritative reconcile.

## Durable social-state result

PASS in the storage test surface.

Migration v15 adds `following_local_state`, keyed by `(author_fid, target_fid)`.
Follow/Unfollow state is written atomically with the outbox operation and
re-applied after provider refresh/session restore. The migration backfills the
latest existing Follow/Unfollow intent, old duplicate idempotency keys do not
regress newer intent, state survives close/reopen, and different author FIDs
remain isolated.

This complements:

- v13 durable local Cast-removal tombstones;
- v14 durable Like/Recast desired-state projection.

## Signer lifecycle result

PASS in source design.

Queued records remain bound to the signer key that sealed them. A replacement
key causes the old record to fail terminally before network submission.
Authoritative signer Expired/Revoked transitions explicitly invalidate all
pending records for the current account, including records whose retry deadline
is in the future. Guest/public reads are not disabled by this write failure.

The local sliding-TTL checkpoint is subordinate to authoritative signer registry
state during restore. A locally refreshed `lastUsedAt` cannot resurrect an
expired authoritative signer.

## Snapchain reconciliation result

PASS in source contract review.

Cast-removal reconciliation treats authoritative Cast absence as confirmation
of the desired deletion; publication treats the same absence as unconfirmed.
Conflict and transient failures preserve their distinct retry semantics.

## Remaining acceptance

Phase 5F is **not release-complete yet**. The Linux verification environment
cannot supply the required product toolchain/live gates:

- Qt 6.11.x/qmllint/runtime are unavailable;
- GCC 14.2 has a reproducible C++ Modules serializer ICE before semantic
  compilation of several higher-level module interfaces;
- Clang 17 is present but `clang-scan-deps` is unavailable;
- deployed App-FID/live Snapchain fault-injection acceptance requires the macOS
  product environment and live credentials/account.

Do not begin Phase 6 on the assumption that these release gates passed. The next
checkpoint is final Phase 5F macOS/live acceptance.

## Read/profile acceptance support

After live Phase 5F testing exposed a derived-feed timeout and a QML
`canLoadMore` binding loop, the acceptance surface was expanded without changing
the write architecture.

Profile is now a first-class provider-neutral read surface with Posts, Replies,
and Popular feeds, isolated local cache keys, Followers/Following navigation,
author navigation, parent-cast traversal, pagination, refresh and existing Cast
interaction controls. The authenticated Profile navigation opens the
authenticated account rather than conflating it with the separately browsed
guest identity. Profile/social navigation targets are separate from the Home
viewing FID, preventing author browsing from retargeting Following.

The pagination contract now exposes stable `hasMore` continuation state
independent of `busy`. Main-feed and profile QML defer near-bottom load-more
through `Qt.callLater`, coalesce pending triggers and keep footer geometry
constant. Source review therefore removes the causal binding cycle reported for
`FeedSurface.canLoadMore`; macOS warning-fatal runtime acceptance is still
required before claiming the regression closed in the shipping Qt environment.

Reachable-but-unavailable derived capabilities now degrade to Saved when usable
local content exists and receive a low-frequency automatic recovery probe after
the capability-health cooldown. Details and explicit Retry remain available.
See `docs/40-profile-read-expansion.md`.
