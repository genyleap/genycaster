# ADR 0007 — Provider Cursors Are Infrastructure-Private

## Decision

Public pagination uses client-owned opaque continuation tokens. Raw provider cursors remain in infrastructure storage with the provider and capability that issued them.

## Consequences

- provider cursors never leak into domain/application/UI;
- failover cannot incorrectly assume cursor portability;
- a later restart strategy can use normalized semantic anchors when provider continuity is unavailable.
