# 04 — Provider Boundary and Capability Routing

## Core rule

Providers are infrastructure implementations. They are not the application architecture.

The application asks for a capability. The router determines which eligible provider may satisfy the requested operation under the current deployment policy.

## Conceptual flow

```text
Capability Request
        │
        ▼
Capability Registry
        │
        ▼
Eligibility Filter
        │
        ├── exact operation supported?
        ├── compatible semantic version?
        ├── provider enabled?
        ├── credentials available?
        ├── privacy policy allows it?
        └── rate/cost budget allows it?
        │
        ▼
Policy Engine
        │
        ├── preferred priority
        ├── runtime health
        ├── latency
        ├── quality
        ├── privacy
        ├── cost
        └── deployment policy
        │
        ▼
Selected Route
```

## Default provider policy

Hypersnap may be preferred for protocol-native operations where its capability quality is acceptable.

This must be configuration/policy, not source-level coupling.

## Compatibility-provider policy

A flagship-managed backend or another managed service may satisfy compatibility capabilities.

The core should see a capability such as:

```text
ILegacyMessagingTransport
```

not:

```text
ManagedDirectCastService
```

The implementation may be named `ManagedLegacyMessagingAdapter` inside the provider package.

## Provider health is per capability

Do not model:

```text
Hypersnap = healthy
```

Model:

```text
Hypersnap / cast.read          = healthy
Hypersnap / feed.following     = healthy
Hypersnap / feed.for_you       = emulated
Hypersnap / signer.write       = unavailable
```

Runtime health should include concepts such as:

- state;
- latency EWMA;
- recent error rate;
- last successful request;
- cooldown/circuit-breaker state;
- rate-limit status.

## Health checking must not block the UI path

Live reads and any active network probe execute on the bounded read worker pool,
never the GUI thread. Cache/session work remains serialized. Request routing itself is side-effect free and consumes a
current per-capability snapshot; the routed gateway performs circuit admission
and records the selected source outcome. No health operation adds a second
network request to an ordinary read.

## Read failover

Read failover may use ordered alternatives when semantics are compatible.

Example:

```text
cast.read
  → Hypersnap
  → self-hosted
  → compatibility provider
```

## Write failover

Write failover is not equivalent to read failover.

If a submission times out, the provider may have accepted it. Blindly submitting the same logical command through another provider may duplicate or conflict.

Every writable capability must define:

- idempotency semantics;
- canonical message identity where applicable;
- ambiguous-success detection;
- reconciliation strategy;
- retry/fallback policy.

## Provider-affine cursors

Provider cursors must never be treated as portable domain pagination tokens.

Conceptual local representation:

```cpp
struct ContinuationToken {
    CapabilityId capability;
    ProviderId provider;
    std::string opaque_token;
};
```

If a route changes, continuation may require restart from a normalized anchor rather than passing a foreign cursor to another provider.

## Capability availability, not provider checks

Bad:

```cpp
if (provider == Provider::ManagedBackend) {
    showLegacyInbox();
}
```

Correct concept:

```cpp
if (capabilities.has(Capability::LegacyMessaging)) {
    showLegacyInbox();
}
```
