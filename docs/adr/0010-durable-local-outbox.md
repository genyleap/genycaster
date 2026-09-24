# ADR 0010 — Protocol Writes Persist in a Durable Local Outbox

- Status: Accepted for Phase 5

## Context

A local-first client must render user intents before confirmation. If optimistic state
lives only in memory, a restart or offline period silently discards user actions. A write
must therefore be durably recorded at the moment of intent, in the same transaction as the
optimistic canonical state it projects.

## Decision

Define a local outbox port and a SQLite-backed implementation:

```text
IOutboxStore.enqueue(WriteEnvelope) -> Result<OutboxId>
IOutboxStore.listPending()          -> Result<vector<WriteEnvelope>>
IOutboxStore.markSubmitted/Confirmed/Failed(...)
IOutboxStore.findByIntent(idempotency) -> Result<optional<WriteEnvelope>>
```

Enqueue opens a transaction: INSERT the outbox record (canonical signed message, intent,
status `pending`, attempt count, timestamps) and write the optimistic canonical entity in
one commit. Confirmation/failure update the record and reconcile projection. Records
survive store reopen.

## Consequences

- queued writes survive process restart and offline periods;
- confirmation path, retry classification, and failure state are explicit and recoverable;
- no silent rollback and no loss-on-reopen;
- the outbox is infrastructure, so provider cursors/signatures never leak to UI/Domain;
- deterministic tests prove a record and its optimistic entity both survive a reopen while
  the submitter is unavailable.