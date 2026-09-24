# ADR 0011 — Provider-Neutral Idempotency Key Gates One Effective Submit

- Status: Accepted for Phase 5

## Context

Duplicate submission on retry, restart, or failover is a real risk. A submit transport
may accept the message and drop the response, or the client may crash after send but before
persisting the confirmation. A client-owned idempotency strategy must prevent "at most one
effective submit" for a given logical intent without depending on provider-specific
conflict semantics alone.

## Decision

Give each canonical write intent a stable, provider-neutral **idempotency key** derived
from the signed message identity (the canonical message bytes/signature hash). The outbox
treats this key as unique: enqueue is a no-op if an identical key already exists, and a
submit is performed only for records currently in `pending`. A never-attempted record goes
directly to the canonical submit endpoint after durable enqueue; it does not pay a redundant
preflight lookup. A successful submit response is validated against the exact message
hash/type/FID and is sufficient confirmation. If an attempt may already have reached the
network but its result is ambiguous, the next retry/restart reconciles authoritative state
before any re-submission. Retry/bounded backoff are driven by an attempt counter with an
explicit maximum; a conflict is reconciled before it can become a second effective submit.
Application never sees provider cursors or provider error enums, only canonical submit
results.

## Consequences

- no unbounded retries, no duplicate submits, no silent rollback;
- one key ⇒ at most one effective submit across retry/restart/offline gaps;
- retry classification is explicit (retryable vs confirmed vs permanently failed);
- deterministic tests prove duplicate enqueue dedups and a restarted store never re-submits
  a confirmed record.