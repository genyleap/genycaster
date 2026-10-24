# Monotonic, crash-durable indexing waterlines

## Feature gate

| Aspect | Contract |
|---|---|
| Responsibility | Prevent a stale ingestion/sync worker from rewinding a committed semantic checkpoint |
| Origin | Genycaster-owned derived ingestion/synchronization foundation |
| Mandatory | Enforced on all callers of the existing checkpoint storage port |
| Public contract | `sync::checkpoint::ICheckpointStore::saveCheckpoint` and `loadCheckpoint` |
| Canonical data | Unchanged; no provider pagination bytes or backend DTOs |
| Dependencies | Portable SQLite connection/transaction boundary and sync model |
| Expected failures | `Conflict` for rewinding a stored sequence; `InvalidArgument` for unsafe key or sequence above signed SQLite range; `StorageFailure` for unreadable data |
| Local state | Existing `sync_checkpoints(sync_key, sequence, updated_at_ms)` table; no migration needed |
| Offline | Checkpoints survive client restart without network access |
| Multi-provider safety | Independent source/stream keys do not collide; callers must namespace per independently ordered node/stream |
| Concurrency | SQLite `BEGIN IMMEDIATE` serializes independent connection writers before comparing the waterline |
| Tests | `storage.local` checks ordering, duplicate replay, concurrent writers, independent keys, bound validation and restart persistence |

## Confirmed prior defect

The old `saveCheckpoint` used an unconditional SQLite `ON CONFLICT DO UPDATE`:
a late completion carrying sequence 76 could overwrite previously persisted
sequence 77. In an incremental ingestion pipeline this allows a checkpoint
rewind and repeated indexing on restart.

The updated implementation opens an immediate SQLite transaction, checks the
current waterline within the write lock, then applies exactly one of:

- New sequence is greater: update the durable row and commit.
- New sequence is equal: succeed without changing the original `updated_at_ms`.
- New sequence is less: return `Conflict` with the persisted checkpoint intact.

There is no check-then-write race between independent SQLite connections.
Sequence values beyond SQLite's signed 64-bit `INTEGER` representation are
rejected instead of wrapping into negative persisted values. A corrupted
negative stored sequence is surfaced as `StorageFailure`, not misreported as
huge positive progress. Keys must be nonempty, at most 512 bytes and contain
no NUL. No credential or raw cursor is persisted as a checkpoint.

## Boundaries

`SyncCheckpoint` remains a **semantic progress marker**, not a provider
cursor. Source-specific ingest traversals need distinct, explicitly scoped
keys: different raw nodes and derived indexing streams must not share a
sequence namespace. This implementation ensures each key progresses
monotonically; it does not infer source scope from an arbitrary string.

This contract **does not yet atomically commit normalized Casts and their
corresponding sync waterline in one transaction**. The caller must first
persist canonical event data, and only then advance the checkpoint. Duplicate
ingestion must remain idempotent. A future independent network-wide ingestion
service will need transactional entity/checkpoint commits, gap detection,
reorg/deletion repair, backfill, limits and verified live-node integration.
It also does not implement a full public search/trending index.
