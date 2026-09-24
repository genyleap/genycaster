# Phase 5C Review — Publish Cast Vertical Slice

Date: 2026-08-28

## Decision

**Phase 5C is implementation-final in source.**

The repository now has a provider-neutral, durable, signer-gated root
PublishCast vertical slice from Qt intent to canonical Farcaster message,
optimistic local materialization, Snapchain submission and authoritative
reconciliation.

Per-platform release acceptance remains required because this Linux container
cannot build the Qt UI and GCC 14.2 has known C++ Modules ICEs in the new
Application/provider interface CMIs.

## Delivered architecture

### Canonical write domain

New/extended write contracts model:

- PublishCast intent;
- prepared canonical operation;
- signed canonical operation;
- deterministic idempotency identity;
- durable pending/confirmed/failed outbox state;
- stable provider-neutral failure classes.

The canonical CastId and idempotency key are the exact lower-case hex BLAKE3-20
hash of MessageData.

### Write authority

`IWriteAuthority` is the only signing authority seen by the write use case.
`SignerService` implements it and must prove the current FID + exact `CastAdd`
scope before exposing a public key or signing the canonical hash.

This preserves the Phase 5B invariant:

```text
Sign In != write authority
```

### Canonical protocol codec

`farcaster.protocol.write` builds standard mainnet `CAST_ADD`, derives the
canonical hash/CastId, and seals the final Ed25519 protobuf envelope.

The root-cast slice supports Domain URL and cast-reference embeds. Reply parent,
mentions and long/10k cast product rules remain deliberately outside Phase 5C.

### Durable local outbox

`farcaster.storage.write` is independent from the read storage target and owns a
SQLite outbox/projection implementation behind `farcaster.local.write`.

Enqueue is atomic across:

- optimistic cast entity;
- exact signed protobuf envelope;
- signer public key;
- idempotency/retry metadata;
- projection write state.

Retry reuses the exact signed envelope and never re-signs user intent.

### Submit and reconciliation

`farcaster.provider.snapchain.write` is independent from signer provisioning.
It:

- POSTs binary protobuf to `/v1/submitMessage` with
  `application/octet-stream`;
- verifies the authoritative response hash/FID/type;
- reconciles by `/v1/castById?fid=...&hash=...`;
- treats 404 as absent;
- maps HTTP failures without claiming false success.

The feature flag `FARCASTER_ENABLE_SNAPCHAIN_WRITE_PROVIDER` is independent from
`FARCASTER_ENABLE_SNAPCHAIN_SIGNER_PROVIDER`.

### Application orchestration

`farcaster.application.write::WriteService` is an independently testable
application target.

Publish sequence:

```text
session check
 -> current CAST_ADD signer check
 -> canonical MessageData/hash
 -> Ed25519 signature
 -> final Message envelope
 -> atomic outbox enqueue + optimistic cast
 -> reconcile
 -> submit if absent
 -> reconcile again
 -> confirm / bounded retry / terminal failure
```

Resume sequence is FID-scoped. A queued envelope cannot cross account
boundaries. Current signer authority is checked again before network I/O.

A replaced signer public key makes an old queued envelope terminal; temporary
unavailability leaves it blocked and recoverable.

### Qt composition

The runtime composes write protocol/storage/submitter behind existing feature
flags. Signer/write work uses the signer executor rather than the read/session
executor, so wallet or publish I/O does not intentionally block local-first read
work.

The PresentationBridge/QML root composer exposes only product lifecycle and user
text. It performs the standard 320 UTF-8 byte preflight and delegates canonical
validation to Protocol again.

Optimistic rows carry the real network CastId and deduplicate against later
authoritative feed rows. Sign-out/account-switch removes prior-account
optimistic rows; late write callbacks are ignored when their author FID no
longer matches the current account.

Runtime destruction drains accepted signer/write work before destroying the
services/providers borrowed by those jobs.

## Security and privacy review

The Phase 5C path does not persist or expose:

- Ed25519 seed/private key;
- custody private key;
- wallet credential;
- SIWF proof as protocol authority;
- provider DTOs in Domain/Application/QML.

SQLite stores the signed Farcaster message envelope and public signer key. Those
are public protocol artifacts, not private signing material.

Guest/public/cached reads remain independent of write setup, write endpoint,
secure-store and signer failures.

## Cross-platform review

No new platform-specific storage or signing code was introduced in Phase 5C.
The pipeline consumes the Phase 5B secure signer boundary and ordinary
cross-platform SQLite/HTTP/Qt composition.

Shipping support is therefore gated per platform by the same real secure-store,
wallet, current Snapchain and Qt runner acceptance already required by Phase 5B,
plus compilation/execution of the Phase 5C write targets on that runner.

## Scope intentionally deferred

Phase 5C does **not** claim:

- reply publishing;
- mention authoring;
- media upload;
- long/10k casts;
- reaction/recast/follow writes.

The next phase should extend the composer/reply experience over this existing
pipeline rather than create another write stack.
