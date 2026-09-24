# ADR 0006 — Route Operations, Not Providers

## Decision

Provider selection occurs independently for each operation-level capability.

## Consequences

- no global backend switch exists;
- health is per provider/capability pair;
- routing policy may prefer different providers for different operations;
- capability quality and privacy requirements participate in selection;
- provider adapters remain replaceable without changing application semantics.
