# ADR 0008 — Preserve Blocking Capability Ports Behind a Serialized Read Executor

- Status: Superseded in production composition by ADR 0014; retained as the Phase 4 baseline

## Context

Provider read interfaces are blocking off-GUI-thread ports. Phase 4 needs real local-first orchestration without prematurely converting every capability contract to a framework-specific future or coroutine type.

SQLite/entity/timeline work, provider I/O, parsing, and reconciliation must never run on the Qt GUI thread, while cached state must be delivered before refresh completion.

## Decision

Add a small provider-neutral executor port accepting owned jobs and one RAII serialized worker implementation for Phase 4 reads. Application read use cases post their complete cache-then-refresh lifecycle to this executor. Observers run on the worker; presentation adapters are responsible for marshaling small commits to their owning UI thread.

Capability interfaces remain blocking. Each application request owns a cancel handle that suppresses stale observer delivery; cancellation does not claim to interrupt an already-blocking third-party call.

## Consequences

- No network, SQLite, JSON parsing, or reconciliation is required on the GUI thread.
- Deterministic manual executors can prove cache-before-refresh behavior in tests.
- One serialized lane avoids concurrent SQLite/list reconciliation races in the first vertical slice.
- Long provider calls can temporarily delay other refresh jobs; later profiling may justify separate database/network/model executors without redesigning capability contracts.
- The worker and queued jobs are RAII-owned and shut down deterministically.
