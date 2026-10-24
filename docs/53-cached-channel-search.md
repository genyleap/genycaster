# Locally cached channel discovery for offline Search

## Feature gate

| Category | Contract |
|---|---|
| Responsibility | Show channels previously discovered through a derived Search source while offline |
| Capability origin | Local-derived cache of provider-neutral ChannelSuggestion |
| Mandatory/optional | Optional Search/Discovery enhancement; no required remote index or provider |
| Public contract | `local::read::IReadStore::searchCachedChannels` and `putChannelSuggestions` |
| Canonical data/invariants | Canonical `ChannelId`; no provider cursor or vendor object types in local state |
| Dependencies | Existing portable SQLite, bounded cache/refresh executors, SearchPage |
| Expected failures | Query empty/oversized, unbounded limit, corrupt/oversized suggestion, SQLite error; remote unavailable |
| Local state | `cached_channel_suggestions` with exact channel ID primary key and NOCASE name prefix index |
| Offline behavior | Both blocking and asynchronous Search show saved channels immediately and preserve them on source errors |
| Provider policy | No extra networking or implicit capability routing; only results from the already eligible indexed Search source enter the cache |
| Failover | Fresh remote Search replaces partial cached suggestions; failed Search retains the cached channel list |
| Verification | `application.local_first_read` native C++ and `tests/wasm/owned_cached_channel_search.test.mjs` |

## Local behavior

When a network-capable Search source returns channel suggestions, Genycaster
stores their provider-neutral ID, name, optional parent URL, thumbnail URL,
description and follower count in its own SQLite database. The asynchronous
pipeline batches the write on the existing serialized cache executor. The
blocking pipeline uses the existing refresh executor.

Local channel Search matches **literal prefixes** of channel IDs and names,
case-insensitively for ASCII via SQLite NOCASE. The literal characters
`%`, `_`, and `!` are escaped to avoid unintended wildcard matching.
The results prioritize exact channel ID, matching ID prefix, then name prefix,
and use follower count and stable channel ID to break ties. Per-query limits
are 1..30; invalid requests fail with `InvalidArgument`.

A batch of at most 30 suggestions is validated before the write transaction
starts. Batch upserts are atomic; omitted optional metadata and unknown (zero)
counts cannot erase richer previously observed channel data. Results are
saved and restored across process restarts.

Both the native/blocking and Qt/async Search pipelines now:

1. Read known local profile and channel suggestions first.
2. Publish them with refreshing/stale metadata where applicable.
3. Request the independently routed remote Search capability normally.
4. Publish the authoritative remote search response on success and persist
   useful summaries for future offline queries.
5. Keep cached profiles **and channels** on unavailable/routing failures.

The UI continues consuming its existing `SearchPage` projection; no new
provider-specific types or UI network operations have been introduced.

## Known scope and limitations

This is **not** a global channel crawler or a complete directory. Only
previously discovered channels can be found offline. The cache does not
automatically learn every channel mentioned by every Cast, and it does not
manage channel removals or network-wide freshness. A full independent
search/index service still needs protocol ingestion/backfill, change
propagation, tombstone reconciliation, operator-defined retention, privacy
controls and multi-node live validation.

This module does not provide offline full-text Cast search or Trending.
The Qt/WASM end-to-end release build and deployment are separate gates.
