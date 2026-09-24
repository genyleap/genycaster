# ADR 0001 — Capabilities, Not Providers, Are the Core Abstraction

- Status: Accepted for Phase 0

## Context

A Farcaster client may obtain similar data from Hypersnap, a self-hosted service, a flagship-managed backend, or future infrastructure.

Selecting one global backend couples unrelated features and leaks vendor semantics into the application.

## Decision

Application/domain code depends on provider-neutral capability contracts.

Provider selection occurs per capability/operation through a routing layer.

Hypersnap is a preferred default policy for suitable protocol-native capabilities, not a core dependency.

## Consequences

Positive:

- provider replacement becomes localized;
- self-hosted mode is possible;
- compatibility providers remain optional;
- per-capability failover/privacy/cost policies become possible.

Cost:

- normalization layer required;
- capability semantics must be designed deliberately;
- pagination/error/retry behavior cannot simply expose vendor contracts.
