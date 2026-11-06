# 35 — Phase 5C Publish Cast, Durable Outbox, and Reconciliation

Phase 5C is **implementation-final in source** as of 2026-08-28.

This slice proves one real protocol write end to end without weakening the
Phase 5A/5B authority chain:

```text
Guest
  -> public/cached reads only

Authenticated session
  -> verified account/FID
  -> still read-only

Authenticated session + current network-verified signer with CAST_ADD scope
  -> canonical CAST_ADD
  -> durable local outbox
  -> optimistic canonical cast
  -> Snapchain submit/reconcile
```

Platform-native release verification remains separate from source acceptance.
The implementation is platform-neutral except at already-existing Qt/transport/
secure-store composition boundaries.

## Scope

Phase 5C implements **root cast publication only**.

Implemented:

- canonical `MESSAGE_TYPE_CAST_ADD` MessageData/protobuf construction;
- standard Cast type with current 320 UTF-8 byte validation;
- URL and cast-reference embed encoding through canonical Domain embeds;
- BLAKE3-20 protocol hash as both CastId and idempotency key;
- Ed25519 envelope signing through `IWriteAuthority`;
- atomic SQLite optimistic-cast + outbox persistence before network I/O;
- exact signed protobuf retention for retry (no re-signing on retry);
- reconcile-before-retry crash/timeout recovery;
- bounded exponential retry;
- validated successful-submit confirmation plus authoritative
  `castById(fid, hash)` reconciliation before any ambiguous re-submission;
- per-account/FID outbox isolation;
- current signer/scope/public-key re-check before every network attempt;
- terminal failure when a queued envelope belongs to a replaced signer;
- temporary blocking when the current signer is unavailable/revoked/expired;
- Qt root-cast composer trigger and durable write lifecycle presentation.

Not implemented in this phase:

- replies / parent cast publication;
- mentions and mention-position authoring UI;
- long/10k cast product eligibility;
- reactions, recasts, follows or other protocol writes;
- media upload/provider-specific composer services.

Those remain later slices over the same durable write pipeline.

## Module boundaries

### Domain

`farcaster.domain.write` owns provider-neutral write semantics:

- `PublishCastIntent`;
- `PreparedOperation`;
- `SignedOperation`;
- deterministic `IdempotencyKey`;
- durable `OutboxRecord` status/failure metadata.

No provider URL, HTTP object, protobuf implementation type, private key, wallet
credential, SQLite handle or QML type enters Domain.

### Capabilities

`farcaster.capability.write` exposes canonical protocol preparation/sealing.

`farcaster.capability.write_authority` exposes only:

- require the current signer for an exact FID/scope;
- sign already-canonical bytes.

`farcaster.capability.write_submit` exposes only:

- submit an already signed canonical operation;
- reconcile its exact authoritative identity.

Application therefore never receives raw signer secret material and never knows
Snapchain HTTP details.

### Protocol

`farcaster.protocol.write` owns the concrete Farcaster write codec.

For PublishCast:

1. Application supplies authenticated FID, content and wall-clock timestamp.
2. Protocol converts timestamp to Farcaster epoch seconds.
3. `CastAddBody` is encoded under MessageData body field `5` with message type
   `MESSAGE_TYPE_CAST_ADD` (`1`).
4. MessageData is hashed with BLAKE3 and truncated to 20 bytes.
5. The lower-case `0x` hex hash becomes both canonical CastId and outbox
   idempotency key.
6. `IWriteAuthority` signs that exact 20-byte hash.
7. Protocol seals the final protobuf Message with Ed25519 signature and signer
   public key.

Retry never rebuilds or re-signs a message. The exact signed envelope is the
outbox payload.

## Durable outbox

`farcaster.local.write::IOutboxStore` requires `enqueue()` to atomically persist:

- optimistic canonical Cast;
- signed operation bytes;
- signer public key;
- idempotency key;
- enqueue/retry lifecycle.

The SQLite implementation uses one transaction for optimistic materialization
and outbox insertion. A process crash cannot leave a network-intended write that
was not first made durable locally.

Outbox lifecycle is intentionally compact:

```text
Pending -> Confirmed
Pending -> retry Pending
Pending -> Failed
```

Failure does not silently delete the optimistic cast. Presentation can explain
that the user-authored content remains saved locally.

## Idempotency and crash recovery

The canonical 20-byte cast hash is the natural protocol identity and the local
idempotency key.

Before every submission attempt Application calls authoritative reconciliation:

```text
castById(fid, hash)
```

If the cast already exists, the record is confirmed without another submit.
This resolves the ambiguous case where the previous process/network call may
have succeeded before a crash or timeout.

After a failed submit, reconciliation is attempted again before scheduling a
retry. A successful network merge therefore wins even if the immediate submit
response was lost.

## Retry policy

The default Application policy is bounded and exponential. Retryable classes
are transport/unavailable/timeout/rate-limit failures. Protocol rejection,
authorization failure, conflict, storage corruption and unsupported behavior do
not loop indefinitely.

Attempt count is durably incremented **before** reconciliation/submission I/O.
At the maximum attempt count, one final authoritative reconciliation is allowed;
if the operation is still absent/unobservable, the record becomes terminal
`Failed` instead of continuing forever.

## Signer safety after enqueue

A durable signed envelope does not bypass current authority.

Before any reconcile/submit call, `WriteService` asks `IWriteAuthority` for the
current `CAST_ADD` signer for the authenticated FID.

- no current signer / temporary signer unavailability -> `Blocked`, no network I/O;
- same signer restored -> the pending envelope can resume;
- different current public key -> terminal Authorization failure, because the
  old envelope must never be submitted under a replacement authority;
- different authenticated FID -> the record is not selected at all.

The outbox query is FID-scoped and indexed accordingly, so account switching
cannot project or submit another account's queued operation.

## Presentation contract

QML does not build protocol messages, hashes, signatures, HTTP requests or
outbox records.

The Qt presentation bridge accepts only user text and exposes product-neutral
write lifecycle:

- preparing;
- queued;
- submitting;
- retry-scheduled;
- confirmed;
- failed;
- blocked.

The standard root-cast composer performs a 320 UTF-8 byte preflight before
emitting a publish intent. Protocol validates again independently.

Optimistic rows use the canonical network CastId. When authoritative feed data
later contains the same ID, presentation removes the confirmed optimistic copy
instead of showing a duplicate.

Sign-out or account switch clears optimistic rows belonging to the previous
account from the active presentation surface. Late callbacks are rejected when
their record author FID does not equal the current authenticated account.

## Cross-platform requirement

No Phase 5C Domain/Application/Protocol/SQLite write logic contains macOS-only,
Windows-only, Linux-only, Android-only or iOS-only assumptions.

The implementation depends on existing cross-platform boundaries:

- Qt presentation/runtime shell;
- provider-neutral HTTP transport;
- Phase 5B secure signer vault implementations;
- injected Snapchain endpoint configuration.

The write pipeline is therefore reusable on every shipping target for which
Phase 5B signer authority and the Qt/native runtime are available.

## Verification summary

Current Linux/GCC 14.2 source verification proves:

- canonical protocol message/hash identity;
- durable SQLite outbox/reopen/idempotency behavior;
- FID/account isolation;
- Application lifecycle through a verification-only non-exporting TU using the
  actual committed declaration/implementation/test source;
- Snapchain submit/reconcile behavior through the same verification strategy;
- architecture guard.

GCC 14.2 crashes while emitting the normal `farcaster.application.write` and
`farcaster.provider.snapchain.write` module CMIs (`cp/module.cc: write_location`).
No compiler workaround is committed. Supported newer Clang/platform runners
must compile the normal module targets and Qt UI as release acceptance.

See [Development Stages](12-roadmap.md) for current write-pipeline acceptance gates. Historical phase reports were removed from the active tree and must not be treated as current release evidence.
