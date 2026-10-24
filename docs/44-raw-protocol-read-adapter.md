# Raw Farcaster Read Node adapter — implementation contract

## Responsibility and feature gate

| Dimension | Design |
|---|---|
| Responsibility | Adapt only the raw node's documented HTTP read endpoints to canonical read contracts |
| Capability origin | Protocol for FID-scoped reads and links; Derived for bounded Following aggregation |
| Mandatory or optional | Optional adapter; existing indexed providers remain independent |
| Public contract | `farcaster.provider.snapchain.read` creates `IAsyncReadGateway` with explicit `Config` |
| Canonical data | `domain::profile::Profile`, `domain::cast::Cast`, canonical `FeedPage` and `Fid` |
| Dependencies | Async HTTP reactor, bounded CPU decoder pool, SQLite infrastructure continuation store |
| Failures | Invalid protocol JSON, mismatched FID, unsupported aggregate read, HTTP error, invalid cursor, cancelled request |
| Local state | Provider+capability+FID-scoped cursors mapped to opaque app-safe tokens; canonical entities persisted by ReadService |
| Offline behavior | Existing cache remains available; no remote retry on the GUI thread |
| Provider routing | One instance per explicit `snapchain-v1` profile. Does not replace indexed `hypersnap-v2` instances |
| Failover | Capability router uses only advertised capabilities; unsupported aggregate reads may use another eligible provider |
| Tests | `provider.snapchain.raw_read.contract` covers profiles, root/quote/reply normalization, ID integrity, scoped cursors, fake async transport |

## Supported capabilities

- `identity.fid.resolve`: username proof query, only if node exposes `/v1/userNameProofByName`
- `profile.read`: `GET /v1/userDataByFid`; fields normalized to canonical Profile
- `feed.user.posts.read`: filtered root `GET /v1/castsByFid`
- `feed.user.quotes.read`: filtered cast-reference root posts from that same endpoint
- `feed.user.replies.read`: filtered parent-cast replies
- `graph.following.read`: effective Follow links via `/v1/linksByFid`, normalized to canonical source/target FIDs
- `graph.followers.read`: effective incoming Follow links via `/v1/linksByTargetFid`, normalized and checked against the requested target FID
- `feed.following.read`: **bounded live derived feed** from a complete <=32-account Follow graph and one reverse-ordered 100-message Cast window per author. Includes own root/quote posts; canonical newest-first merge, ID tie-breaks, deduplication and opaque viewer/provider/endpoint-bound keyset cursors. Refuses deeper author history or oversized Follow graphs rather than claiming completeness.

Raw graph results only contain canonical FIDs; the UI can show basic identities and any profiles already cached locally, but names/avatars from unknown FIDs require separate profile reads. Both graph directions use opaque, direction-specific, FID/provider/endpoint-affine continuation tokens. A raw node must actually expose the documented link endpoints for these capabilities to work.

**Not advertised:** network-wide Recent/Trending, channel feed, search, notifications, reactions-by-user, view counts, automatic author-profile hydration, or lookup of an arbitrary cast by hash when its author FID isn't provided. These require a separate indexing/projection system, a raw-node adapter that can prove the requested contract, or a compatible indexed provider. Never silently claim a feed is complete when it isn't.

## Build profiles

The raw adapter is enabled by default in UI builds and remains disabled by
default in provider-free core builds. The explicit CMake switch
`FARCASTER_ENABLE_SNAPCHAIN_READ_PROVIDER` controls independent inclusion.
Browser builds use portable Boost headers through the WASM adapter path, not
native Boost binaries.

## Connection

A deployment can register any number of compatible raw node instances through the existing profile mechanism; Example placeholder (not a live node):

```json
[
  {
    "id": "my-raw-node",
    "label": "My read node",
    "contract": "snapchain-v1",
    "endpoint": "https://read.example.invalid",
    "selfHosted": true
  }
]
```

Set using `GENYCASTER_READ_PROVIDER_PROFILES_JSON`, the equivalent CLI
`--read-provider-profiles-json=...`, or saved Qt setting
`network/readProviderProfilesJson`.

For capability-specific preference/fallback, the generic CapabilityRouter is
used. `GENYCASTER_PREFERRED_READ_PROVIDER_ID=my-raw-node` **strictly pins**
all read operations, so unsupported aggregate operations correctly fail
rather than pretending to exist; do not pin a raw-only node for an
indexed-feature-complete product without adding derived index capabilities.
This is a conscious fail-closed limitation awaiting UI configuration of
per-capability preferences.

## Correctness and isolation

- Raw HTTP request execution is async and accepts cancellation.
- JSON normalization runs on the bounded CPU pool, never the QML thread.
- Cast timestamps use the Farcaster epoch and canonical hashes.
- Parent and cast-reference embeds preserve author/FID identities where present.
- Next-page tokens are opaque SQLite mappings, never raw provider cursors at
  the presentation boundary.
- Continuation mappings enforce provider ID, canonical capability ID, exact
  HTTP endpoint origin, and exact FID. A token from another provider,
  operation, node origin, or FID fails closed. This endpoint-bound `raw-v2`
  cursor format intentionally rejects tokens saved by the previous raw adapter
  implementation; users must restart affected feeds from page one after upgrade.
- `/v1/linksByFid` and `/v1/linksByTargetFid` use the upstream Rust query fields `link_type=follow` and `target_fid=<fid>` (NOT `type`/`targetFid`), in line with Snapchain `src/network/http_server.rs`.
- `/v1/castsByFid` explicitly uses the documented `reverse=1` parameter
  for newest-first ordering, with provider page tokens URL encoded.
- Credentials remain in the adapter config and are never added to domain
  models; profile JSON is non-secret.

## Remaining acceptance requirements

- Full raw-network-derived indexing for aggregated feed/query semantics.
- Interactive provider directory, capability discovery, per-provider secrets,
  runtime health/selection and user-friendly route fallback controls.
- Native and WebAssembly end-to-end integration against independently operated
  raw nodes; production Qt/WASM build and browser smoke tests.
- Extend bounded Following aggregation to arbitrarily large Follow graphs and deep author histories with persisted multi-FID checkpoints, rate-aware bounded parallelism and independent E2E verification. The current implementation stops rather than truncates when its limits are reached.
- Integration of exact full-reply/quote metadata and engagement counts with
  local projections and provider-enrichment routes.

This adapter is the *first* direct raw-node component, not evidence that the
entire multi-provider architecture is complete.
