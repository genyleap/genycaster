# Genycaster-owned local profile search (first vertical slice)

## Architecture and feature gate

| Category | Contract |
|---|---|
| Responsibility | Search previously materialized profile metadata immediately without depending on a remote indexed backend |
| Capability origin | Local derived index; **not** network-wide search coverage |
| Mandatory/optional | Optional feature of installed search UI; canonical Read and authentication stay independent |
| Public contract | `local::read::IReadStore::searchCachedProfiles(query, limit)`, existing provider-neutral SearchState/SearchPage |
| Canonical data/invariants | Same persisted Profile/FID used by feeds; no provider DTOs or network credentials |
| Dependencies | Existing portable SQLite core, profile table, local serial cache executor |
| Expected failures | Empty, oversized or unsafe user query; SQLite failure; remote provider not eligible/unavailable |
| Local state | Indexed username and display_name columns; no additional duplicate profile table |
| Offline behavior | Cached matches emitted immediately and retained on remote failure; no network requests for local results |
| Provider disablement | Without indexed remote adapter, known local profiles still display; unknown identities are not invented |
| Failover | Local snapshot survives errors/rejections; successful remote search replaces the local suggestion snapshot |
| Tests | Local ordering/escaping/restart, async completion/rejection/offline/cache, existing search behavior |

## Why this is independently owned

The core already stores canonical `profiles` rows from profile, social,
conversation and feed reads. Previously Search went straight to `m_gateway`
and cleared its results on every failed indexed query, ignoring usable local
information. This first local index uses portable B-tree indexes on
`username COLLATE NOCASE` and `display_name COLLATE NOCASE`. The profile
lookup accepts a bounded **literal prefix** with ASCII case-folding; reserved
SQL LIKE characters `%`, `_` and `!` are escaped as literal text.

Candidates rank exact username first, username prefix second, display-name
prefix third; follower count then canonical FID determine stable ties. The
limit is 1..30. The result is hydrated through the canonical entity store
rather than copied into any index-owned DTO. Database schema evolution is
non-destructive and compatible with the SQLite amalgamation used by WASM;
unlike SQLite FTS5 it does not require a separately compiled optional
extension on every platform.

## UI lifecycle

Interactive asynchronous Search on the cache worker:

1. Query the local SQLite index on the existing serialized cache executor.
2. Publish `Refreshing` with known cached profiles (marked stale if nonempty).
3. Start one normal capability-routed remote search request.
4. On remote success, publish authoritative remote profiles/channels.
5. On synchronous routing refusal or asynchronous failure, preserve local
   profiles in the failed snapshot, with accurate unavailable/stale metadata.
6. On success, enqueue provider-returned profile summaries on the serial cache
   executor; merge them non-destructively into canonical SQLite profiles. A
   later search can therefore find online-discovered identities offline.

Existing authoritative bios, usernames (when summary omits them), display
names (when omitted), verified addresses, quality scores, and follower counts
are retained. A missing/failed local write never prevents the already fetched
search result from appearing.

This is not a global Farcaster crawler. Names not yet seen locally remain
unavailable while remote search cannot execute, and local profile search
currently has **no channel search**. It does not turn an unsupported raw node
into a provider that falsely declares `search.read`, nor does it bypass
router privacy/strict self-hosted enforcement.

## Still required

A network-wide independent Cast/Profile/channel event-ingestion pipeline,
persistent distributed indexing, freshness/retention policies, richer Unicode
full-text tokenization, self-hosted ranking/trending, multi-node live tests,
and full client/WASM release validation remain separate acceptance items.
