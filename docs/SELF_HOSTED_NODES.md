# Self-hosted protocol nodes

GenyCaster can use a user-configured compatible Farcaster node for capabilities that node exposes. A read node, an indexed feed and a signer/validator endpoint are **different services**; discovering one does not imply the others exist.

## Example environments

All hostnames on this page are **documentation placeholders**. Replace them with endpoints you own or are authorized to use; none is a GenyCaster production URL.

| Role | Example |
|---|---|
| Locally running Snapchain read API | `http://127.0.0.1:3381` |
| TLS-protected remote read node | `https://node.example.com` |
| Separately verified compatible writer | `https://writer.example.com` |

## Start with the node

Use the current official Snapchain operator documentation for installing, provisioning and syncing the exact supported node release. Requirements for storage, memory and ports evolve; consult the upstream release instead of treating old example VM sizes as permanent requirements.

Check basic node status from the node host:

```sh
curl --fail http://127.0.0.1:3381/v1/info
```

Do not expose an unauthenticated local HTTP read API directly to the internet. Use firewall controls and an operator-managed HTTPS endpoint when accessing a remote node.

## Verify the read capability

The reusable source probe validates node sync information, canonical CastAdd messages and optionally the Follow graph:

```sh
python3 ci/check_snapchain_read_node.py \
  --read-url http://127.0.0.1:3381 \
  --fid 123 \
  --check-graph
```

Replace `123` with a public FID that actually has casts. This check does not ask for a provider API key, disables automatic redirects/proxies and fails closed on missing protocol content. It does **not** prove ownership of the node, readiness of a separate writer, full network search or supported signer scope.

## Configure the client

Provider profiles for the chosen node are composed at the application boundary. If the native client is already built, the independent-node launcher can verify route parameters and start the client:

```sh
python3 ci/launch_self_hosted.py \
  --read-url https://node.example.com \
  --write-url https://writer.example.com \
  --fid 123 \
  --client /path/to/Genycaster
```

A writer endpoint is mandatory only for write operations. It must be independently verified as a compatible submission transport; do not assume an arbitrary read node supports signing or publishing.

For a local test, use loopback `http://127.0.0.1:3381` only when the client shares the node's network namespace. For a different device use a protected, reachable HTTPS origin.

## Supported and incomplete areas

- Per-FID reads, canonical profile/links/Casts and a bounded Following reconstruction are implemented in source.
- Full network-wide search, trending, global recent indexes and large-graph coverage cannot be inferred from one raw read node.
- Signed writes use separate scoped signer/transport validation and durable reconciliation. Successful reads do not prove that write authorization works.
- Never copy credentials, signer secrets or unrestricted gateway addresses into a public example.

See [Architecture](ARCHITECTURE.md), [Development Roadmap](12-roadmap.md), [Provider Routing](04-provider-boundary-and-routing.md) and [Raw Read Adapter](44-raw-protocol-read-adapter.md).
