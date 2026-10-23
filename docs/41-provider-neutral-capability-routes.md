# Provider-neutral capability routing and multi-instance endpoints

## Feature implementation gate

| Aspect | Contract |
| --- | --- |
| Responsibility | Resolve requested canonical Farcaster operations to registered provider instances |
| Capability origin | Protocol, derived, compatibility and extension operations independently |
| Mandatory or optional | Mandatory routing foundation; concrete adapters optional |
| Public contract | `CapabilityRegistry`, `CapabilityRouter`, `ProviderPreference`, `ReadProviderProfile` (composition-only) |
| Canonical invariants | Provider id is an instance identity, not an API hostname; access and capability are independently checked |
| Dependencies | Domain-free provider/capability metadata, health snapshots, policy; no Qt in routing |
| Expected failures | Unknown provider, unsupported protocol contract, unavailable/blocked node, missing declared access, credential leakage |
| Local state | Non-secret profile metadata/settings only; continuation tokens remain provider-affine |
| Offline | Existing local read cache; no assumption that unavailable upstream becomes reachable |
| Provider policy | Capability/access/health/privacy eligibility precedes affinity |
| Failover | Read preference may fall back when explicitly soft; strict/pinned selection is fail-closed; writes MUST be reconciled before reassignment |
| Tests | Core routing unit coverage, provider registration/health gates, existing Snapchain signer/write contract tests |

## Core design

`ProviderId` is a **runtime adapter instance id**, not a hardcoded vendor
enumeration. The same protocol adapter can serve multiple independent instances.
Every provider registers only the exact operation contracts it actually supports.

- `setPreferredProviderForAccess(access, preference)` applies to all operations
  of that access mode (`Read`, `Write`, `Realtime`, `Historical`).
- `setPreferredProviderForCapability(capabilityId, access, preference)` takes
  precedence for that specific operation.
- `ProviderPreference.require = false` expresses **soft affinity**: another
  compatible, healthy, policy-eligible provider may be used when necessary.
- `require = true` expresses **strict pinning**: absent, incompatible, unhealthy
  or deployment-disallowed selected providers fail closed. This is essential
  for explicitly chosen write destinations.
- Removing an override restores the access default and then the previous
  policy preference. These updates are synchronized and affect future resolves,
  not in-flight operations.
- The registry, health and deployment policy always win over affinity.
  A provider cannot gain an unadvertised write capability by being selected.

The legacy two-option UI remains usable, but maps to registered instance IDs
at the Qt presentation/composition boundary. The core router has no references
to Snapchain, Hypersnap, Genyleap or Neynar.

## Multiple compatible read-provider instances

For deployments, define the optional JSON **array** using
`GENYCASTER_READ_PROVIDER_PROFILES_JSON`,
`--read-provider-profiles-json=...`, or the non-secret Qt settings key
`network/readProviderProfilesJson`. Example:

```json
[
  {
    "id": "operator-east",
    "label": "Operator East",
    "contract": "hypersnap-v2",
    "endpoint": "https://example-node.invalid",
    "selfHosted": true
  },
  {
    "id": "operator-west",
    "label": "Operator West",
    "contract": "hypersnap-v2",
    "endpoint": "https://other-node.invalid",
    "selfHosted": false
  }
]
```

The example domains are placeholders, not discovered or verified nodes.
Only the `hypersnap-v2` adapter contract is currently implemented in
this multi-instance path. Other contracts **must** be implemented by their
own provider adapters and are rejected until supported. Each instance
gets its own provider ID, gateway binding, per-capability health and
provider-scoped continuation state.

Pin a registered read instance using
`GENYCASTER_PREFERRED_READ_PROVIDER_ID`,
`--preferred-read-provider-id=<id>`, or
`network/preferredReadProviderId`. Unknown/unregistered IDs fail closed.
The existing UI network toggle can supersede the startup pin and restores
the old preference on subsequent launches.

The JSON must not contain API keys; authentication must be configured
separately and associated with a specific endpoint to prevent cross-host
credential leakage. No protocol implementation is inferred from URL
or branding. The `selfHosted` property is an explicit operator assertion
for deployment policy, not a network proof of node ownership.

## Writes and limitations

The previous increment already split Snapchain read and validator/write
URLs, signer KEY_ADD/KEY_REMOVE submissions, reconciliation and route-scoped
credentials. The generic router now supports a **Write** preference but the
single-instance `IWriteSubmitter` is not yet replaced with a fully routed,
multi-writer composition. Doing so safely requires durable binding of each
queued canonical message to a route or authoritative global reconciliation,
and explicit user consent to reroute uncertain writes. Never implement
naive retry to another writer after a timeout.

The following are **not** completed by this increment:
- native indexed feed generation directly from a raw Snapchain read node;
- protocol adapters for arbitrary incompatible API contracts;
- UI editing/discovery/health probing for all provider profiles;
- live switching of signed write destinations or durable route binding;
- external chain/on-chain wallet and signer registration variants;
- per-provider credential isolation across all future adapters.

## Acceptance targets for the next increments

1. Build a raw Snapchain read gateway implementing the canonical async
   contracts where feasible, backed by derived self-owned feed indexing.
2. Generalize profile discovery/UI and adapter registration so a node
   can advertise verified contract/capabilities, rather than being assumed
   to serve all social data.
3. Extend write routing with durable idempotency/reconciliation and provider
   health, without leaking provider details into `domain::write`.
4. Verify offline, strict-self-hosted, desktop, mobile and WebAssembly builds,
   with E2E read/write tests against at least two independent operators.
