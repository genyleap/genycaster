# Phase 5F — Write Hardening

Date: 2026-08-29

## Purpose

Phase 5F hardens the existing protocol-native mutation path instead of adding a
second write architecture. The invariant remains:

```text
authenticated session
    -> current scoped protocol signer
    -> canonical Farcaster protobuf
    -> exact signed envelope
    -> transactional SQLite outbox + local projection
    -> authoritative reconcile / submit
    -> durable confirmation or bounded terminal failure
```

Public and guest reads remain independent of signer readiness. QML remains a
presentation boundary and does not construct protocol messages, sign bytes,
mutate SQLite, or call providers.

## Recovery lifecycle

Write recovery is event driven. The primary wakeups are:

- the network transitioning from unavailable to reachable;
- the protocol signer transitioning to Ready;
- application/session restoration when write authority is available.

A 60-second timer remains only as a low-frequency safety net. It does not wake
the outbox while the presentation layer knows the network is offline.

`WriteService::resume()` coalesces concurrent wakeups with an atomic scheduling
flag. A network transition, signer transition and fallback timer therefore do
not queue redundant scans of the same durable records.

## Ambiguous submit outcomes

A retry never means "blindly POST again".

### Timeout or transient transport failure

A fresh mutation performs one submit. If the response is ambiguous because of a
timeout or transient endpoint failure, the exact signed envelope stays durable.
Before any later submit, the retry reconciles authoritative protocol state:

```text
Confirmed -> mark durable record Confirmed; do not submit again
Absent    -> resubmit the exact same signed envelope
Error     -> bounded backoff; preserve the exact envelope
```

This covers the important case where Snapchain accepted/merged the message but
the client lost the response.

### Conflict / duplicate response

HTTP conflict is treated more conservatively because it can describe a message
that the protocol already knows while indexes are still converging. Once a
submit returns Conflict, the durable record enters reconciliation-only mode:

```text
Conflict -> reconcile
             |-> Confirmed -> Confirmed
             |-> Absent    -> bounded reconciliation retry
             `-> Error     -> bounded reconciliation retry
```

The record is never blindly POSTed a second time after a Conflict. The Conflict
marker is persisted in the outbox so restart does not forget this rule.

### Maximum attempts

Before terminal failure at the attempt bound, the service performs one final
authoritative reconciliation. A confirmed operation is recovered as Confirmed;
otherwise the record becomes durably Failed with the causal failure preserved.

## Restart-safe desired-state projections

Provider snapshots may be stale, viewer-neutral, or temporarily unavailable.
Genycaster therefore persists the account's own desired social state next to the
outbox and overlays it after provider refresh.

The schema history is:

- v13 — local Cast-removal tombstones;
- v14 — account-scoped Like/Recast projection;
- v15 — account-scoped Follow/Unfollow projection.

The v15 `following_local_state` projection is written transactionally with the
outbox record, uses the latest operation ordering for migration/backfill, and is
isolated by author FID. Refresh, restart, delayed read-provider indexing and an
old duplicate idempotency key cannot regress a newer Follow/Unfollow intent.

Authoritative protocol reconciliation still decides whether the durable write
itself is confirmed. The local projection exists to keep the interaction state
coherent while read providers catch up; it is not a second protocol authority.

## Signer lifecycle safety

Queued envelopes are bound to the signer public key that produced them. Every
attempt re-checks current authority and required scope before touching the
network.

- signer replacement -> old-key queued envelopes fail terminally;
- signer revocation -> all pending envelopes for the current account are
  invalidated without network submission;
- signer expiry -> all pending envelopes for the current account are invalidated
  without network submission;
- missing/transient write readiness -> public reads continue and new writes stay
  blocked/fail closed according to the authoritative signer state.

`invalidatePendingAuthority()` scans pending records through `Timestamp::max()`
so records sleeping in a future backoff window are invalidated too. An old
signed envelope therefore cannot wake later and cross a signer-authority
lifecycle.

## Sliding signer TTL

A confirmed write may refresh the local `lastUsedAt` checkpoint for responsive
signer state. That checkpoint is not authoritative across restore. On signer
restore, current Snapchain registry state wins even if the local checkpoint is
newer. An authoritative expired/revoked signer therefore cannot be resurrected
by optimistic local activity.

## Remove semantics

Authoritative absence is operation-sensitive. For `RemoveCast`, a canonical
Cast lookup returning NotFound confirms the desired deletion. For publication,
that same absence means the Cast is not confirmed. This distinction is covered
by the Snapchain write contract tests.

## Verification contract

Phase 5F source hardening is not the same as release acceptance. Final closure
requires the product toolchain and live protocol environment:

1. clean Qt-enabled configure/build on every desktop OS claimed as a shipping target; current macOS Clang 22 + Qt 6.11.x evidence is one platform gate, not global portability proof;
2. all CTest targets, strict qmllint and deterministic Qt smoke flow on those shipping compositions;
3. deployed App-FID attestation with the App-FID custody key server-side;
4. live Cast, Reply, Delete, Like, Unlike, Recast, Undo Recast, Follow and
   Unfollow;
5. restart and offline -> online recovery;
6. timeout-after-merge and delayed-index/conflict fault injection;
7. signer replacement, expiry and revocation while records are pending;
8. refresh/restart verification for v13/v14/v15 local projections.

Until those gates are collected, Phase 5F is **source-hardening complete but
release acceptance pending**. Phase 6 should not begin by treating the write
foundation as fully accepted before those gates pass.
