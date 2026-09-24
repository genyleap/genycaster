# Phase 4.5B Review — Capability Health and Circuit Breaking

## Status

Phase 4.5B is implemented. Routed read outcomes now drive runtime health and a
deterministic circuit breaker keyed by `(ProviderId, CapabilityId)`. A failing
optional capability such as Trending does not change the health or admission of
Identity, Profile, Following, Channel, Cast, or social reads.

The feature gate is recorded in
`docs/31-phase4.5b-capability-health-and-circuit-breaking.md`; ADR 0012 records
the runtime state-machine decision.

## Runtime health model

`farcaster.routing.health` now owns:

- `Unknown`, `Healthy`, `Suspect`, `Degraded`, and administrative `Unavailable`
  health states;
- `Closed`, `Open`, and `HalfOpen` circuit states;
- observed latency, error-rate, consecutive-failure, last-error,
  last-successful-at, and cooldown metadata;
- an injected monotonic clock and configurable failure threshold/cooldown;
- normal versus explicit manual-recovery admission.

The default circuit opens after two consecutive retryable failures and uses a
30-second cooldown. Timeout, transport failure, unavailable, and rate-limit
results are retryable. Other failures neither add a strike nor falsely reset
the circuit as a successful operation.

The registry is intentionally serialized by the existing Phase 4 read executor.
The router remains side-effect free: it filters and scores current snapshots,
while `RoutedReadGateway` owns admission and outcome recording around the actual
capability-source call.

## Recovery behavior

```text
successful read
  → Healthy / Closed

first retryable failure
  → Suspect / Closed

repeated retryable failure
  → Degraded / Open / cooldown

ordinary navigation during cooldown
  → fail fast or select another eligible provider

manual Retry during cooldown or normal read after expiry
  → one HalfOpen probe
  → success closes; failure renews cooldown
```

Presentation emits only a provider-neutral recovery intent. Application carries
that intent as `ReadOptions`; provider identities and circuit types do not cross
into Presentation or QML. Automatic reconnect uses a normal retry and therefore
does not bypass cooldown.

## Runtime diagnostics

Failure context is composed at the layer that owns each fact:

```text
Capability: feed.trending.read       routing gateway
Provider: hypersnap                  routing gateway
Endpoint: /v2/farcaster/feed/trending provider adapter
Limit: 10                            provider adapter
Deadline: 12000ms                    provider adapter
Latency: 12004ms                     routing gateway
Result: Timeout                      routing gateway
Transport: QtNetwork                 transport adapter
Stage: WaitingForHeaders             transport adapter
Elapsed: 12004ms                     transport adapter
Host / Proxy                         transport adapter
```

Open-circuit fail-fast errors also name capability, provider, result, and
remaining cooldown. Authentication data, headers, and secrets are not included.

## Network ownership correction

The Qt Network implementation now lives at `src/transport/qt_network/` and
exports `farcaster.transport.qtnetwork`. It directly implements the
provider-neutral `farcaster.transport.http` port in a separate Qt-linked target.

`src/platform/` now contains only OS-adapter placeholders. The obsolete
`src/platform/qt_network/` target, module, and root CMake inclusion were removed.
The architecture guard rejects reintroducing that misplaced directory and
continues to forbid Qt throughout the provider-neutral transport/core layers;
only the concrete Qt transport boundary is exempt.

## Deterministic coverage

Core tests use a fake monotonic clock and prove:

- Healthy/Suspect/Degraded/Open/HalfOpen recovery behavior;
- no wall-clock sleep;
- fast rejection without another source invocation;
- successful and failed manual probes;
- automatic probe after cooldown expiry;
- cooldown renewal after probe failure;
- explicit retryable-error classification;
- Trending degradation while Following remains healthy and callable;
- structured first-failure and open-circuit diagnostics.

Provider contracts additionally lock endpoint, limit, and deadline diagnostic
metadata, including the 12-second Trending budget. The existing UI runtime smoke
continues to prove Identity → Profile → Following without any Trending request.

## Deliberate limits

- Circuit state is process-local; canonical cached entities and timelines remain
  durable in SQLite.
- The current serialized executor is the concurrency boundary. Splitting network
  and storage execution later requires an explicit synchronization design for
  health state.
- No background synthetic probe is added. A real user read after cooldown is the
  automatic probe, avoiding hidden network work.
- No write, signing, outbox, or Composer behavior is part of this iteration.
