# Phase 4.5B — Capability Health and Circuit Breaking

## Feature implementation gate

```text
Responsibility: Turn provider/capability runtime outcomes into deterministic health and circuit state so repeated optional-capability failures fail fast without degrading unrelated reads.
Capability origin: Local routing infrastructure observing Protocol, Derived, Compatibility, Extension, or Local capability outcomes without redefining their semantics.
Mandatory or optional: Generic health/circuit infrastructure is mandatory for routed reads; each product capability remains independently mandatory or optional according to its own contract.
Public contract: Provider-neutral Application ReadOptions marks an explicit manual recovery probe; Routing owns per-(ProviderId, CapabilityId) health, admission, cooldown, and diagnostics.
Canonical data/invariants: Health never exists at provider-only granularity; router selection remains operation-level and side-effect free; only retryable source failures affect the circuit; cooldown uses monotonic time.
Dependencies: Presentation intent → Application read option → routed gateway → capability router/health registry → provider-neutral capability source. Concrete endpoint and Qt transport details remain in adapters.
Expected failures: Timeout, transport failure, unavailable source, and rate limit are retryable circuit failures; open-circuit admission fails fast; invalid input, not-found, auth, conflict, protocol, storage, and internal failures do not accumulate retryable circuit strikes.
Local-state representation: Circuit state is process-local routing state. Canonical cached entities/timelines remain durable in SQLite and usable while a circuit is open.
Offline behavior: OS reachability remains the only source of device Offline. An open capability circuit is source degradation and preserves cached data.
Provider policy and disablement: Policy evaluates each provider/capability health snapshot independently. Administrative Unavailable remains distinct from runtime Degraded/Open.
Failover semantics: Normal routing excludes an open circuit during cooldown and may select another compatible provider. After cooldown one normal attempt is an automatic probe. Manual Retry may request one bounded probe before cooldown expiry.
Tests proving the boundary: Fake-clock unit tests prove Healthy → Suspect → Degraded/Open, fast rejection, manual and automatic probes, recovery, retryable classification, per-capability isolation, and Trending failure with healthy Following.
```

## State machine

```text
Unknown/Healthy
  → first retryable failure: Suspect / Closed
  → repeated retryable failure: Degraded / Open / cooldown
  → manual Retry or cooldown expiry: HalfOpen probe
  → probe success: Healthy / Closed
  → probe failure: Degraded / Open / renewed cooldown
  → probe cancellation/neutral terminal: Degraded / Open / preserve cooldown
```

The default policy opens after two consecutive retryable failures and cools
down for 30 seconds. Tests inject a fake monotonic clock; production uses
`std::chrono::steady_clock`.

Half-open admission is represented by a process-local probe lease. Cancellation
or a terminal non-retryable outcome releases only that exact lease, restores the
pre-probe Open state, and does not fabricate a success/failure health sample.
This prevents stale request cancellation from leaving a capability permanently
HalfOpen or from releasing a newer recovery probe. Completion attribution is also
admission-aware: an older in-flight normal request may contribute telemetry, but
it cannot resolve or overwrite the active recovery probe.

## Transport ownership correction

The Qt Network implementation directly implements `farcaster.transport.http`.
It therefore belongs to the Transport subsystem as
`farcaster.transport.qtnetwork`, in its own Qt-linked target. Moving it does
not permit Qt dependencies in the provider-neutral `farcaster.transport.http`
port or the libcurl adapter.
