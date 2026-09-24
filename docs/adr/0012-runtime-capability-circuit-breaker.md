# ADR 0012 — Runtime Capability Circuit Breaker

## Status

Accepted for Phase 4.5B.

## Context

Routing already stores health by provider/capability pair, but source outcomes
do not update it. A repeatedly timing-out optional feed can therefore consume
its entire provider deadline on every navigation and Retry. Provider-global
health would incorrectly disable unrelated identity, profile, Following, or
Channel capabilities.

## Decision

- `HealthRegistry` owns process-local circuit state keyed by stable
  `(ProviderId, CapabilityId)`.
- The circuit opens after a configurable number of consecutive retryable
  failures and uses an injected monotonic clock for cooldown decisions.
- Timeout, transport failure, unavailable source, and rate limit are retryable
  health failures. Other result errors do not add circuit strikes.
- `CapabilityRouter` remains synchronous and side-effect free. It consumes a
  current health snapshot and excludes open circuits during cooldown.
- `RoutedReadGateway` owns attempt admission and records latency/outcomes around
  the selected capability source call.
- A normal request after cooldown is a single half-open probe. A user-initiated
  Retry carries a provider-neutral Application recovery option and may request
  one half-open probe before cooldown expiry.
- Successful probes close the circuit; failed probes renew cooldown.
- Each half-open admission owns a process-local probe lease. Cancellation or a
  terminal non-retryable outcome releases only that lease, restores the prior
  Open state without adding a health observation, and preserves the existing
  cooldown. A cancelled probe therefore cannot strand the circuit in HalfOpen
  or release a newer probe for the same capability. Outcome recording is
  admission-aware, so older in-flight normal requests cannot resolve an active
  recovery probe.
- Circuit state is not persisted. Durable cached domain data remains available
  through the existing local-first path.

## Consequences

- A failing Trending capability cannot alter Following health.
- Repeated failures fail fast or route to an alternative provider.
- Tests use fake time and never sleep.
- A process restart resets circuit memory, avoiding stale persisted health while
  durable read data remains intact.
- Runtime diagnostics can name capability, selected provider, latency, result,
  endpoint, deadline, request limit, and transport stage without exposing auth.
