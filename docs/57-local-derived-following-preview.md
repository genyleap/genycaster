# Account-scoped local Following preview when indexed feed is unavailable

## Contract

Genycaster can now render a **partial, on-device, saved Following preview**
from its own SQLite canonical Casts and an already materialized Following graph.
This is NOT a network-wide Following feed, does not page across all authors on
an external Snapchain node and does not claim to have seen posts by every
account the user follows. When no graph has been loaded, the client does not
invent a Following relationship.

- Canonical graph: `social_list_entries` with the exact viewer-scoped
  `social:following:<fid>` list and `relation_kind = 'follows'`.
- Canonical content: already-observed Casts; no fabricated authors or metadata.
- Discovery: root/quote Casts only (no replies), chronological descending
  with deterministic canonical ID tie breaking.
- Deletions: `cast_local_tombstones` filtered in SQL *before* LIMIT/OFFSET.
- Storage: existing SQLite source tables and indexes; no new duplicate feed
  projection or third-party index, no QML-triggered network requests.
- Eligibility: the Following graph must have been successfully materialized
  locally, AND the corresponding provider Following timeline must never have
  been materialized (including an authoritative empty snapshot).
- Provider behavior: a real indexed Following snapshot takes precedence.
  Crossing its last row never switches to the local inventory. Provider
  continuation is never mixed with local offset pagination.
- Identity safety: the query uses both the viewer graph-list ID and the
  exact source FID; another viewer's graph cannot leak into this preview.
- UI: state carries `localDerivedFollowing`; the Following view renders a
  localized explicit incomplete-coverage notice in all 12 locales.
- Errors: route failures retain usable cached rows; SQLite errors are surfaced.
- Tests: `application.local_first_read` and
  `tests/wasm/local_derived_following_feed.test.mjs` exercise limits,
  identity scoping, cached restart, tombstones, no provider cursor and
  authoritative network precedence.

## Remaining critical release gate

The feature is **not** a replacement for a true self-hosted Following feed.
A full raw-node implementation must enumerate the account's effective
Follow links, fetch/batch new messages from each author with bounded fanout,
deduplicate and sort across pages, persist per-author checkpoints/tombstones,
incrementally refresh without starving the UI, and prove completeness semantics
and failure recovery with real independent Snapchain operators.

Do not mark the Balaji prize's `read the feed` requirement as complete merely
because this partial offline preview can render.
