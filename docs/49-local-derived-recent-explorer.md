# Local-derived Recent inventory for Explorer

## Feature gate

| Gate | Decision |
|---|---|
| Responsibility | Present already-observed canonical Casts when a network-wide Recent timeline has never been materialized locally |
| Capability origin | Local, derived; **does not** claim a network-wide feed capability |
| Mandatory or optional | Optional local cache fallback inside existing Recent view |
| Public contract | `local::read::FeedWindow::isDerivedRecent` and provider-neutral `FeedState::localDerivedRecent` |
| Canonical data | Existing Cast, author Profile and FeedEntry, ordered by canonical created time and cast ID |
| Dependencies | Existing portable SQLite schema, entity hydration, cache executor, QML presentation |
| Expected failures | Invalid local offset/limit; SQLite read; missing provider route; offline provider error |
| Local-state representation | SQLite casts and existing tombstones, no separate materialized derived feed or network cursor |
| Offline | Local Recent window is available after restart, even if indexed provider is unreachable |
| Provider policy | No bypass of router eligibility; fallback does not issue an independent HTTP request |
| Failover | Only when the persisted provider Recent timeline has no visible entries; the live indexed feed wins whenever present |
| Tests | `application.local_first_read` and `tests/wasm/local_derived_recent_explorer.test.mjs` |

## Why this is safe

The previous cached Recent feed was a timeline specific to an indexed
provider. If the client had read Casts via profile, detail, replies or another
node but had never populated that indexed Recent timeline, Explorer Recent
displayed an empty offline result.

The new SQLite query reads the canonical `casts` table with an ordered
`(created_at_ms DESC, id DESC)` B-tree index and removes entries present in
`cast_local_tombstones` **before** applying `LIMIT/OFFSET`. It requests
one extra row to determine `hasMore`, hydrates each Cast and its author
through the existing canonical entity store, and always removes any provider
continuation token from this local-only response.

If a provider Recent snapshot has been materialized—even a successfully
retrieved **empty** page—it wins over the locally derived inventory. The
snapshot is detected using the existing durable `read_continuations` row,
which `writeFeed` commits even for empty network pages. Provider entries
and their opaque cursor remain unchanged. In particular, scrolling past the
last provider entry never starts paginating the local-derived dataset.
When a fresh indexed provider response arrives, it replaces the local
preview using existing materialized feed logic.

The Read pipeline propagates a distinct `localDerivedRecent` indicator to
Qt/QML. Explorer Recent displays a localized notice stating that the items
were saved **on this device** and are **not** the latest posts across all
Farcaster. All 12 installed translation catalogs carry the notice.

## What is not implemented

This feature is **not** network-wide incremental ingestion, a globally
complete Recent feed, trending, global search or channel indexing. Those
require an independently deployable ingestion and derived-index service,
cursor/checkpoint durability, backfill, deleted-message reconciliation,
deduplication and explicit privacy and retention contracts. Those remain
separate architecture and live-release acceptance tasks.
