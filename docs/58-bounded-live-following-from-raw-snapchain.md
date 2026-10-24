# Bounded live Following read from independent Snapchain

## Verified source contract

The `snapchain-v1` reader advertises `feed.following.read` with **Derived**
origin/quality. It does not need a managed index or API key, but only supports
accounts within explicit resource bounds. This is distinct from the partial
offline Following preview.

Snapchain upstream `src/network/http_server.rs` accepts:
- `GET /v1/linksByFid?fid=<viewer>&link_type=follow&pageSize=100&pageToken=...`
- `GET /v1/linksByTargetFid?target_fid=<viewer>&link_type=follow&pageSize=...`
- `GET /v1/castsByFid?fid=<author>&pageSize=100&reverse=true&pageToken=...`

`link_type` and `target_fid` are snake_case. The earlier `type` and
`targetFid` request fields were incompatible. Opaque raw cursor values are
URL-escaped and never inserted directly into canonical domain models.

## Current bounded algorithm

1. Require an authenticated viewer FID and page size 1..30.
2. Read the effective Follow graph in **up to four** 100-message pages.
   Validate source FIDs, link type, shape and per-page bounds, deduplicate
   author IDs, include the viewer's own FID. Reject graph cycles, repeated
   tokens, too many pages or **more than 256 followed accounts**. Never
   assume that the first graph page represents the complete graph.
3. Fetch the newest Casts of every followed FID (and the viewer), one author
   at a time. Use up to **eight 100-message raw pages per author**. Check
   canonical IDs, author ownership, Farcaster timestamps, and descending
   chronological order across HTTP pages, not just within each page.
4. Skip replies. Roots and quotes are eligible. Honor an optional keyset
   boundary from a saved opaque Following cursor.
5. Keep only the **globally strongest K+1 canonical Casts** at each page
   boundary, sorted by timestamp then hash. This bounds in-memory candidate
   storage independently of the number of authors/pages.
6. Stop an author's history scan only if its next raw page cannot possibly
   displace the globally ranked K+1 candidates: the latest timestamp seen
   must be **strictly older** than the K+1 threshold. Equal timestamp
   boundaries are scanned further because hashes break ties. If this proof
   needs more than eight pages, refuse completion instead of truncating.
7. Return K canonical feed entries and Casts, with an opaque SQLite-bound
   continuation if at least K+1 are available. The saved cursor freezes
   sorted author membership, viewer, selected provider ID, exact endpoint,
   Following capability, page size, and last canonical keyset position.
   Subsequent pages reject any mismatched source or malformed snapshot.

No secrets are persisted in cursors. All HTTP reads remain asynchronous
through the existing bounded decoder pool; cancellation prevents publishing
a cancelled result. The selected raw reader has no hidden hosted fallback.

## Limits and open work

- A full-scale feed for arbitrary Follow graphs or extremely reply-dense
  author histories is **not yet provided**. Account graphs above 256 authors,
  4 graph pages, or 8 required Cast pages per author yield a clear Unsupported
  error. Current per-author reads are sequential and can be slow.
- The author snapshot is fixed for one keyset pagination session; a refresh
  starts a new snapshot. Efficient retained per-author checkpoints, rate-aware
  bounded concurrency and continuous ingestion are still required.
- This only provides Following for accounts within the bounds. It is not
  network-wide Recent/Trending, Search, profile enrichment, cast-by-hash
  resolution, signer provenance, or production E2E verification.
- There is **no actual independently operated Snapchain node** in the
  project's current infrastructure. Fake-network contract tests establish
  internal correctness but do not prove production interoperability.

## Regression

```sh
cmake --build build/secure-signer --target farcaster_snapchain_read_contract_tests -j 2
ctest --test-dir build/secure-signer -R provider.snapchain.raw_read.contract --output-on-failure
node --test tests/wasm/raw_snapchain_live_following.test.mjs
python3 -m unittest discover -s tests/ops -p check_bounty_read_node_test.py -v
```

Native tests exercise merged page one/two, endpoint/viewer/limit affinity,
multi-page Follow graphs, an exact 256-followee SQLite cursor, dense Reply pages before root Casts, graph width,
cursor repetition, deep-history budget and cancellation. They are not
substitutes for a live read-node E2E.
