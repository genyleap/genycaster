# Bounded live Following read, no managed index

## Status and acceptance

The `snapchain-v1` asynchronous reader now advertises
`feed.following.read` as **Derived quality**, not a native protocol query.
It builds a real live feed directly from the selected raw-node origin, when
that node can prove complete coverage under the limits below. This is separate
from the explicitly partial SQLite offline Following preview.

Canonical source endpoint names were verified against the upstream
`farcasterxyz/snapchain/src/network/http_server.rs` parser:
- `GET /v1/linksByFid?fid=<viewer>&link_type=follow&pageSize=33`
- `GET /v1/linksByTargetFid?target_fid=<viewer>&link_type=follow&pageSize=...`
- `GET /v1/castsByFid?fid=<author>&pageSize=100&reverse=1`

Both `target_fid` and `link_type` are snake_case. The previous
`targetFid`/`type` requests were not the accepted canonical Rust parameters.
No global Hypersnap/Neynar API key, absolute hosted URL, or WebAssembly-specific
protocol model is introduced.

## Algorithm

1. Reject an invalid viewer or page size outside 1..30.
2. On page one, fetch and validate the effective Follow graph from the raw
   node, with a complete endpoint page. Fail closed if it has a continuation
   token, more than 32 followed authors, malformed link data, or wrong FID.
3. Include the authenticated viewer's own FID, deduplicate and sort FIDs to
   create a deterministic author snapshot (at most 33 accounts).
4. Fetch at most 100 newest raw CastAdd messages per author, off the GUI
   thread, honoring cancellation and existing transport retry policies.
   Validate canonical author/hash/timestamp and reverse ordering; root Casts
   and quotes participate but replies do not.
5. If a source has a continuation but the first 100 messages provide no more
   than the requested page's worth of eligible items, refuse completion.
   Otherwise sufficient newer eligible items are known to dominate deeper
   history for that author's contribution to this top-K page.
6. Merge, dedupe and sort candidate Casts by confirmed timestamp then
   canonical cast ID. Return at most the requested amount with canonical
   FeedEntry semantics.
7. When more than K entries are present, persist an opaque continuation in
   SQLite scoped to provider ID, Following capability, exact endpoint,
   viewer FID and page size. It includes the fixed author snapshot and
   last-item keyset boundary; the next page does not rescan a potentially
   changed Follow graph or reveal backend author cursors to the UI.
8. Refuse mismatched provider, endpoint, viewer, page size, malformed author
   snapshot or invalid saved cursor, before starting an HTTP request.

No results are silently claimed if a known author might have unseen
newer messages. A node request or decode failure is propagated. This is a
bounded implementation, not a complete network-wide indexing solution.

## Known limitations

- The graph must fit a single 33-entry raw endpoint page (32 followees plus
  self). Larger accounts require iterative graph pagination and scalable
  fanout; the current implementation refuses their live Following refresh.
- For each author, only the first 100 raw CastAdd messages are inspected.
  An account with dense reply activity or deep keyset pages may refuse
  refresh until true per-author incremental checkpoints are implemented.
- The serialized graph membership snapshot is stable per pagination
  session but not across refresh sessions. Replies, reactions, profile
  enrichment, moderation, quote hydration and view counts remain separate
  capabilities; this operation does not synthesize metadata it cannot read.
- Sequential author requests may be slow for 32 followees; next stage
  should introduce bounded parallel fanout, per-author checkpoints,
  rate-aware scheduling, deep history cursors and verified live-node tests.
- There is no own independently operated node today; fake async transport
  and SQLite contract tests cannot prove live E2E interoperability.

## Verification

```sh
cmake --build build/secure-signer --target farcaster_snapchain_read_contract_tests -j 2
ctest --test-dir build/secure-signer -R provider.snapchain.raw_read.contract --output-on-failure
node --test tests/wasm/raw_snapchain_live_following.test.mjs tests/wasm/raw_snapchain_read_adapter.test.mjs
python3 -m unittest discover -s tests/ops -p check_bounty_read_node_test.py -v
```

Native fake-node coverage asserts page-one and page-two keyset semantics,
correct chronological interleaving, frozen author membership, exact
provider/endpoint/viewer/page-size cursor rejection, oversized graph and
incomplete source failure and cancellation. The raw no-API-key smoke probe
also checks the exact canonical query-field spellings.
