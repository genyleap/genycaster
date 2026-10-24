# Provider-neutral channel discovery from observed casts

## Feature boundary

Channel suggestions retained offline must not require a managed indexed
Search endpoint. In addition to the already implemented cache of remotely
returned Search suggestions, the local read-store now learns *explicitly
resolved* channel identities from canonical Casts that were successfully
persisted by cast/detail/conversation and feed reads.

This is an optional local-derived capability. It does not change Farcaster
protocol semantics, register new remote Search capabilities, crawl unknown
channels, or infer channel IDs from arbitrary parent URLs.

## Implementation

- `SqliteReadStore::putCast` learns channel data after the canonical Cast
  has been stored. Nested direct replies are included.
- `SqliteReadStore::writeFeed` learns channel data only after the cached feed
  transaction commits; it processes all page casts, batching at 30 suggestions
  per transaction to preserve the existing bounded SQLite contract.
- An explicitly resolved `channelId` is required. The known name and image
  are recorded if present; missing names use the ID as a local display
  fallback. The protocol `parentUrl` is preserved only for root casts without
  a parent Cast; it is never manufactured from an ID or provider hostname.
- Sparse records with an ID-only display name must not overwrite previously
  discovered rich names. Previously known links, descriptions and follower
  counts also remain intact. Malformed/oversized channel metadata is skipped.
- Secondary index errors do not invalidate a valid canonical Cast or feed.
  The existing local Search pipeline retrieves these channels immediately,
  retains them while offline and survives application restart.
- No network requests, credentials, node-specific models, or additional UI
  dependencies are added.

## Acceptance

`application.local_first_read` verifies that richer names survive sparse
updates, nested replies and feed snapshots contribute channel suggestions,
35 channel observations cross the 30-entry batch boundary, malformed channel
metadata does not reject canonical Cast persistence, and reopened SQLite
continues serving local search. The WASM source contract guard
`tests/wasm/observed_channel_offline_search.test.mjs` verifies the same
provider-neutral wiring.

## Remaining limitations

A raw Snapchain node may provide only `parent_url` without a channel ID;
this increment does not guess what that URL means or promise any network-wide
directory. Correct independent indexing across arbitrary node contracts
still requires explicit protocol ingestion/backfill, channel URL resolution
and metadata mapping, tombstones, health probes, authenticated provider
management, and multi-operator live plus browser/device E2E validation.
