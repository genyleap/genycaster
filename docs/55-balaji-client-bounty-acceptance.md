# Balaji's October 2026 Farcaster client bounty — acceptance audit

**Status as of 2026-10-24: NOT QUALIFIED (independent node and human sign-off still absent).**
This is an evidence ledger, not an announcement of a prize submission.
Do not remove the NOT QUALIFIED status until every hard requirement is
demonstrated and a third-party human vouches publicly.

Primary public cast supplied by the project:
https://farcaster.xyz/balajis.eth/0xcd34bb74
Transcribed prize announcement indexed publicly:
https://farcaster.xyz/balajis.eth/0x50184b62
Balaji's announcement as quoted by GM Farcaster:
https://www.gmfarcaster.com/episodes/ep397

The announcement states a **seven-day exact deadline**, calculated from the
original October 17, 2026 cast's timestamp: October 24, 2026 at precisely
the same time of day (verify on the original cast before submitting).

## Hard acceptance matrix

| Prize gate | Source evidence now | Result / release gate |
| --- | --- | --- |
| Three-command (0) scripted compute provisioning, (1) independent Snapchain node, (2) runnable custom client | No integrated three-command user quickstart/provisioner in this repository | **MISSING**; real fresh-machine execution required |
| Node syncs without third-party API keys | `docs/44-raw-protocol-read-adapter.md`; optional raw node profiles; `tools/check_bounty_read_node.py` | **PARTIAL**; script test uses a fake local server, not a separately provisioned synced real node |
| Client exclusively reads from owned node for required tutorial flow | `src/providers/snapchain/`, `docs/44-raw-protocol-read-adapter.md` | **PARTIAL**; raw supports profiles, FID-scoped casts and direct Follow/Followers graph; aggregate Following feed, Recent/Trending/search still missing; disabling managed fallback exposes missing aggregate feeds |
| Signed canonical Cast submits to compatible independent validator, not read-only node | `docs/42-durable-provider-neutral-write-routing.md`, `docs/43-provider-neutral-signer-routing.md` | **IMPLEMENTED IN SOURCE, LIVE UNVERIFIED**; endpoint-pinned outbox and reconciliation contract tests; run independent read/write operator test |
| Import *existing seed phrase* for an account with storage | `docs/50-setup-network-quick-auth.md` | **MISSING**; external onboarding and FID linking are not audited in-app BIP39 custody-wallet import |
| Use custody wallet to issue own app key with an Optimism transaction | `docs/34-phase5b-protocol-signer-readiness.md`, `docs/37-phase5e-social-writes-app-fid.md` | **ALTERNATIVE ONLY**; Ed25519 scoped gasless KEY_ADD and app-FID attestation exist; specific Optimism onchain signer flow not equivalent or E2E proven |
| Read feed, publish casts, Follow, Reaction | `docs/44-raw-protocol-read-adapter.md` and `docs/37-phase5e-social-writes-app-fid.md` | **PARTIAL**; writes implemented; raw independent aggregate feed not complete, live end-to-end acceptance outstanding |
| Include/exclude casts from *other clients* | `src/domain/cast.cppm` and `docs/49-share-deeplink-moderation.md` | **MISSING**; optional upstream app-name badges are NOT a trustworthy signer-key-derived filter, nor is there a filter control |
| Mobile/desktop-friendly microsite, top-of-page quickstart and detailed human-readable protocol tutorial | No bounty-specific microsite exists | **MISSING**; explain FIDs, custody, app keys, message signatures, read node vs validator, storage rent, sync, cost and troubleshooting |
| Associated GitHub repository with ALL submission source MIT-licensed | Root `LICENSE` and README explicitly `GPL-3.0-only` | **BLOCKED**; root cannot be presented as MIT; a truly standalone independently licensed tutorial/example repo requires rights/dependency audit |
| One *other human* successfully follows tutorial and launches own node+client | No independent run and sign-off recorded | **MISSING**; mock tests and the creator's own success do not count |
| Other human's Farcaster post publicly vouching for quality | No verified public voucher | **MISSING**; must be genuine, no synthetic testimonial |
| Deadline and reply with three URLs: microsite, MIT GitHub repo, public voucher | No verified submission links | **MISSING**; must be posted as reply to prize cast before exact deadline |

## Proof procedure: read-node slice

Run on the machine hosting a *personally provisioned* Snapchain read node
with a public FID known to have casts:

```sh
python3 tools/check_bounty_read_node.py --read-url http://127.0.0.1:3381 --fid 3 --check-graph
```

The script makes two ordinary read requests by default (sync and canonical casts), or four with `--check-graph` (also effective Follow/Followers). No API key is provided. It requests `/v1/info` and `/v1/castsByFid?fid=...&pageSize=10`. It verifies
every reported shard has nonzero height and block delay at most 9,
and requires a canonical CastAdd object. It rejects redirects, userinfo,
non-loopback HTTP, empty cast responses and malformed sync state.
A **pass** demonstrates only this read-node slice; it is not a prize pass.

For local regression tests, no node or network is required:

```sh
python3 -m unittest discover -s tests/ops -p check_bounty_read_node_test.py -v
```

## Safe implementation sequence

1. Publish a truthful stand-alone three-command reference flow using a
   reproducible, pinned Snapchain bootstrap and real compute provisioning.
   Provide cleanup/cost warnings, no long-lived secrets in shell history.
   Link the independent client to the locally owned node and verify sync.
2. Implement a *fully local* following/home feed projection from protocol
   events and social links. Ensure offline/pagination/tombstones and no
   implicit hosted index dependency in the prize flow.
3. Implement custody wallet import only via audited BIP-39/BIP-32 +
   secp256k1 EVM derivation, encrypted platform keystores, never in logs,
   never to OpenProof or other hosts; explicitly require a previously
   registered FID/storage and demonstrate signer authorization.
4. Attribute other-client Casts by verified signer-to-app metadata with
   an explicit **unknown** provenance state. Include/exclude filters must
   never fabricate an app identity from author, selected node or UI text.
5. Create a separate properly MIT-licensed code deliverable (if authorized
   by copyright owners), tutorial microsite, supported desktop/mobile
   instructions and real-user written verification. Never relabel GPL
   root source as MIT without appropriate authorization.
6. Obtain genuine external human endorsement and reply with all three
   required URLs before the original deadline.

**Safety:** Do not store a user's seed phrase in app settings, shell
history, analytics, a web form or a server; do not invent a voucher
or claim prize eligibility based on static source contract tests.
