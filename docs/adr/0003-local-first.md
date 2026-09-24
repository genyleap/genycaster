# ADR 0003 — Local-First Application State

- Status: Accepted for Phase 0

## Context

A high-performance mobile/desktop social client cannot make ordinary navigation dependent on backend round trips when data is already known locally.

Provider failure must not blank the product.

## Decision

The UI consumes canonical local state. Network providers synchronize that state asynchronously.

SQLite/WAL plus in-memory hot state will form the persistence/cache foundation.

## Consequences

- more synchronization/reconciliation complexity;
- substantially better offline/resume behavior;
- provider failover becomes less visible to the UI;
- feeds/chat can support optimistic and durable local state.
