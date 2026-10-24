# Endpoint-bound read continuations

## Ownership and invariants

| Item | Contract |
|---|---|
| Responsibility | Ensure a saved pagination token cannot be submitted to a different endpoint after user reconfiguration or failover |
| Capability origin | Infrastructure/local pagination, independent of protocol data |
| Mandatory/optional | Mandatory correctness for any continuation issued by the raw or indexed adapters |
| Public contract | Existing `ContinuationStore` and `PageRequest`, with adapter-private cursor scopes |
| Canonical data | Unchanged; no network cursor or endpoint appears in Cast/Profile/FeedEntry |
| Dependencies | Adapter config, SQLite continuation mapping, capability IDs and provider instance IDs |
| Failures | Wrong capability, provider ID, or endpoint origin returns `Conflict` before network admission |
| Local state | Raw provider cursor wrapped in a versioned adapter-private endpoint scope |
| Offline | Existing cached feed pages remain readable; invalid continuation requires restart at page one |
| Provider routing | Compatible fallback still possible for **new** first-page reads, not for a cursor tied to a previous node |
| Failover | Fail closed when the selected provider changes during a pagination sequence |
| Tests | Raw contract pagination, indexed `testIndexedEndpointBoundContinuation`, cross-adapter JavaScript contracts |

## Versioned cursor scopes

Raw Snapchain adapter: `raw-v2:<HTTP origin>\\n<FID>\\n<opaque cursor>`.

Indexed v2-compatible adapter: `indexed-v2:<HTTP origin>\\n<opaque cursor>`.

In both cases the existing SQLite continuation layer additionally records
the immutable provider instance ID and canonical capability ID. HTTP endpoint
origins are stored as non-secret configuration metadata; API keys and other
headers **must never** enter cursor bindings.

The latest-first raw-node query uses the documented `reverse=1`, while
the indexed adapter preserves each provider's native opaque cursor unmodified
after unwrapping. The former adapter-specific unscoped tokens fail closed
after upgrade to prevent replay against newly configured endpoints. This is
intentional, not a loss of canonical cache records.

The indexed adapter's asynchronous and blocking read implementations now
use the same endpoint-aware continuation wrapper for feeds, notifications,
social graph, likes and recent-feed state. Tests explicitly verify a saved
provider ID with a different URL is rejected, as are legacy unscoped tokens.

## Out of scope / remaining

- Full per-provider credential vault and authenticated node UI;
- User-facing fine-grained capability routing and live health diagnostics;
- Source-owned aggregate index for Recent/Trending/Following/Search;
- Complete native Qt/WASM release smoke tests across independent deployed nodes.

This work does not certify the overall multi-provider architecture as complete.
