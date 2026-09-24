# ADR 0005 — Protocol Write Semantics Are Owned by the Core, Not Vendors

- Status: Accepted for Phase 0

## Context

If application code models a protocol write as `Provider.createCast()`, the provider becomes part of the domain semantics and safe transport failover becomes difficult.

## Decision

Where protocol semantics permit, application commands are converted into canonical protocol messages and signed locally before submission through a provider/node transport capability.

## Consequences

- signer/key management is a first-class security subsystem;
- provider adapters become transport/submission implementations rather than domain owners;
- duplicate/ambiguous-success handling can be based on canonical message identity;
- compatibility-only writes may still require provider-specific adapters but remain isolated.
