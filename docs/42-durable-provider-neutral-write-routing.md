# Durable multi-provider write routing

## Feature contract

- **Responsibility:** Select a compatible writer by canonical capability, permanently pin each signed message to its selected provider instance before the first submission, and reconcile/retry exclusively through that provider.
- **Origin:** Protocol.
- **Mandatory/optional:** Optional for builds with protocol signing; missing secure signer/storage must fail closed. Browsing does not depend on it.
- **Ports:** `capability::write::IWriteSubmitter`, `local::write::IWriteRouteBindings`, `routing::write::RoutedWriteSubmitter`.
- **Canonical identity:** The existing `domain::write::IdempotencyKey` (20-byte canonical message hash). No provider IDs are added to protocol messages or domain write entities. A non-secret immutable adapter-contract + exact submit/read endpoint identity is also bound so reusing a provider ID with a different URL cannot redirect an old message.
- **Offline:** Outbox stays durable; no network work on the UI thread.
- **Retry/failover:** Every operation is bound to one registered writer. Repeated submissions and reconciliation use the original binding. No fallback when a bound writer disappears. Never infer a route for a previously attempted legacy unbound message.
- **Security:** Secrets never go in provider profiles, domain models or the binding table. Route binding must commit to SQLite before HTTP POST; unknown adapters, missing destinations, or DB errors block writes.
- **Tests:** Persistent SQLite binding roundtrip, conflict/no reassignment, missing-key FK rejection, routing with two mock writers across preference changes/restart, policy-level write eligibility, Snapchain submit/reconcile contract tests.

## Internals

The `CapabilityRouter` evaluates `protocol.message.submit` using
`CapabilityAccess::Write` and the normal per-capability health, deployment,
quality and preference gates. The `RoutedWriteSubmitter` then:

1. Loads the canonical message hash from `write_route_bindings`.
2. If found, uses **only** that registered writer **and only if its adapter contract and exact submit/reconciliation endpoint identity still match**. Its current health, deployment, and privacy eligibility are checked again; a previously permitted managed route is blocked under a new Strict Self-Hosted policy without fallback.
3. If not found during an initial `submit()`, checks the eligible selected
   writer and durably performs an atomic `bindOnce()` to the provider ID **and non-secret endpoint identity**.
4. Transmits the signed canonical message only **after** the SQLite transaction
   commits.
5. If not found during `reconcile()`, fails closed: an older in-flight outbox
   item has unknown route provenance and cannot be blindly reassigned.

Outbox attempt counters and retry scheduling remain owned by
`application::write::WriteService`. Provider association remains exclusively
in local infrastructure, not protocol domain data. The binding has a foreign
key to the canonical outbox row and is deleted with it.

### Operating multiple writer instances

Use `--write-provider-profiles-json=<json>`,
`GENYCASTER_WRITE_PROVIDER_PROFILES_JSON`, or the QSettings value
`network/writeProviderProfilesJson`. An example with placeholder origins:

```json
[
  {
    "id": "operator-north",
    "label": "North validator",
    "contract": "snapchain-v1",
    "endpoint": "https://validator.example.invalid",
    "readEndpoint": "https://reader.example.invalid",
    "selfHosted": true
  },
  {
    "id": "operator-south",
    "contract": "snapchain-v1",
    "endpoint": "https://south.example.invalid",
    "selfHosted": false
  }
]
```

Here, `snapchain-v1` denotes the **installed adapter contract**, not a
mandatory product backend. Other protocols can register their own adapter
implementing the same provider-neutral `IWriteSubmitter`; unsupported
contracts are explicitly rejected. A validator-only writer can use a separate
compatible `readEndpoint` for authoritative reconciliation. For profiles with
no `readEndpoint`, the same endpoint is used for both operations.

To strictly select a writer, set `--preferred-write-provider-id=operator-north`,
`GENYCASTER_PREFERRED_WRITE_PROVIDER_ID`, or the QSettings value
`network/preferredWriteProviderId`. This selection applies **only to new,
previously unbound canonical messages**. Existing outbox entries retain their
persisted destination even after configuration changes. A missing pinned writer
never silently falls back to another one.

Profiles are non-secret and currently carry no per-profile API credentials.
Do not point them at endpoints requiring credentials until per-instance secure
credential storage/transport is implemented. The `selfHosted` flag is an
operator declaration for routing policy, not an automated ownership attestation.

### Current boundaries

The default protocol writer is registered as `primary-protocol-writer` for
backwards compatibility. Signer KEY_ADD/KEY_REMOVE and signer-state inspection
now have a separate provider-neutral capability router (`protocol.signer.manage`)
with the same explicit Write affinity, as specified in
`docs/43-provider-neutral-signer-routing.md`. Only providers advertising signer
management may receive such requests. Media uploads remain outside this router,
and custody wallet authorization is a separate trust boundary.

The initial multi-writer release does not remap already-attempted legacy
outbox items that lack provider bindings. They are retained but **cannot be
safely retried without an explicit migration that proves the original endpoint**.
A failed/missing binding must surface visibly; it is never a signal to reroute.

The raw Snapchain reader, full UI for provider profiles, capability discovery
and complete WebAssembly/browser E2E verification remain separate work.
