# Genycaster + independently operated Snapchain: operator quickstart

**Acceptance status:** Ready for deterministic *configuration and source-level
contract tests*, **not** yet a demonstrated complete independent Farcaster client
and not an accepted Balaji bounty entry. There is no personal node installed in
the project's current server environment. An unavailable node cannot be called
live-verified.

## 0. Compute and ownership

Use a Linux VM/host **owned and operated by the user**, with a public IP,
SSD storage, working Docker/Compose, and network ingress. The official Snapchain
operator guide currently specifies at least **16 GB RAM, four CPU cores,
approximately 1.5 TB free disk**, and ports 3381-3383 with appropriate TCP/UDP
access. Budget separately for bandwidth, snapshots, and ongoing storage.
Do not open the uncredentialed HTTP API on a public address without
operator-chosen access restrictions and TLS; restrict incoming clients at
the firewall or terminate TLS in a trusted reverse proxy.

Official authoritative references:
- https://github.com/farcasterxyz/snapchain
- https://github.com/farcasterxyz/protocol/discussions/265

**No compute-provisioning implementation is shipped here**. Until the
operator can provide a working VM, this is not the prize's three-command
cloud-provisioning tutorial.

## 1. Bootstrap a real raw read node

Follow the **current official Snapchain bootstrap instructions** from
the exact upstream repository above. Review the upstream script and the
matching release notes, then run it in the user's own operator environment,
not in Genycaster's deployment container. The usual generated
`snapchain.sh` manages the service via Docker Compose. Wait until all
shards are fully synced (often hours on the first snapshot), not just
until the HTTP service responds.

Local HTTP health:

```sh
curl -fsS http://127.0.0.1:3381/v1/info
```

A response that is HTTP 200 with a height but large `blockDelay` is
**not** sufficient evidence of a fully caught-up node.

## 2. Verify only the selected read node

With a public FID known to have CastAdd messages:

```sh
python3 tools/check_bounty_read_node.py --read-url http://127.0.0.1:3381 --fid 3 --check-graph
```

The probe must return `result:pass` with shard heights, single-digit
block delays, canonical posts and both Follow link endpoints. It uses **no
API key**, disables ambient proxies and redirects, enforces HTTPS except
loopback, and never submits a signed message. If FID 3 lacks casts on
that deployment, use another independently verified public FID.

The check validates the read-node API and basic chain sync. It cannot
establish whether someone controls the node, whether the user owns their
Farcaster keys, or whether the separate validator accepts signed writes.

## 3. Launch the native client with explicit independent routes

Use the native `farcaster_client` built from this repository (WASM
builds have their own browser/transport requirements). You need an
independently verified compatible **writer/validator** endpoint; it
is *not* safe to POST to the read node just because it exposes HTTP:

```sh
python3 tools/launch_self_hosted.py \
  --read-url http://127.0.0.1:3381 \
  --write-url https://your-validated-writer.example \
  --fid 3 --client /absolute/path/to/farcaster_client
```

The `--read-url` is the origin accessible **from the client machine**:
loopback is correct only if the client itself runs beside the read node.
For a desktop/mobile device connecting remotely, use the operator's
protected HTTPS read origin. Replace the placeholder writer with an
actual tested validator that supports `/v1/submitMessage`; no URL
is automatically discovered or invented. Passing a managed writer
does not make it self-hosted.

The launcher builds the exact typed `snapchain-v1` read/write
provider profiles and pins their IDs separately; configures signer
reads and canonical write destination; disables compatible hosted
Read fallback; and strips inherited legacy API keys and write/read
auth headers so an old token cannot leak to newly selected endpoints.
It does **not** persist secrets or capture seed phrases.

`--dry-run` prints route metadata without contacting a node or
launching a client, suitable for initial preflight on a development
machine. `--skip-probe` allows offline startup while explicitly
sacrificing any claim that the node was verified. The optional
`--writer-self-hosted` is an **operator assertion**, not ownership proof.
The configured URLs apply at application startup, not automatically
to an already running Qt instance.

## Limits and acceptance criteria

| Operation | Direct personal raw node | Notes |
|---|---|---|
| FID/profile metadata | Implemented, contract-tested | Full rich profile enrichment not guaranteed |
| Personal Casts/Quotes/Replies | Implemented, contract-tested | Per-FID pagination |
| Following/Followers graph | Implemented, contract-tested | Canonical link FIDs; no fabricated profile labels |
| Following home feed aggregated across followed authors | **BOUNDED LIVE IMPLEMENTATION** | Up to 256 followed FIDs (four graph pages) and up to eight 100-message Cast pages per author with validated keyset cursor; rejects wider/deeper graphs. Partial offline preview is separate; full-scale E2E remains missing |
| Network-wide Recent/Trending and search | **NOT IMPLEMENTED** | Local previously observed items are only partial and labeled as such |
| Signed casts, reactions, follows | Implemented in routed Write source | Requires real signer, separate validator, E2E |
| Existing custody seed import | **NOT IMPLEMENTED** | Must use audited BIP39/32 and platform vault |
| Cross-client signer attribution and include/exclude | **PARTIAL** | Canonical signer-key filters and node-attested, currently active gasless-requester App FID Include/Exclude work on observed raw casts; historical attribution, full-network coverage and live independent E2E remain outstanding |
| Fully independent account setup without managed OpenProof | **NOT VERIFIED** | External FID onboarding and signer flows have separate trust roots |
| MIT submission and human voucher | **NOT COMPLETE** | Repository is GPL-3.0-only; do not relicense by editing LICENSE |

### Offline no-node regression

```sh
python3 -m unittest discover -s tests/ops -p '*self_hosted_test.py' -v
python3 -m unittest discover -s tests/ops -p 'check_bounty_read_node_test.py' -v
node --test tests/wasm/raw_snapchain_follow_graph.test.mjs tests/wasm/local_derived_following_feed.test.mjs
ctest --test-dir build/secure-signer -R provider.snapchain.raw_read.contract --output-on-failure
```

Before declaring production acceptance, connect at least two independently
configured node identities, confirm all supported raw reads, verify a real
signed message at the selected writer and observe canonical confirmation
from the chosen read node after sync; check that the message is *not*
resubmitted to another host on timeout. Test both native and WebAssembly
runtime builds. Do not claim prize qualification without the specified
MIT deliverable, genuine third-party operator run and public testimonial.
