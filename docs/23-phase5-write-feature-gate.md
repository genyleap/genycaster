# Phase 5 — Protocol Write Feature Gate (recorded before implementation)

This document records the mandatory feature implementation gate from `AGENTS.md` for
the first Phase 5 write slice. It is written before production code so the boundary,
semantics, expected failures, and tests are explicit.

## Capability origin and classification

```
Responsibility:        safe local-first publication of a canonical protocol-native write,
                       retained in a durable outbox with deterministic idempotency and
                       reconciliation against a routed submit capability.
Capability origin:     Protocol (cast publication is a canonical Farcaster protocol op).
Mandatory or optional: Optional product composition; the write slice must not be required
                       by the provider-free core or by read-only profiles.
Public contract:       Application enqueues a canonical write intent; the intent is built
                       into canonical protocol message bytes, signed through a local signer
                       boundary, durably recorded in the outbox alongside optimistic
                       canonical local state, and later submitted through a routed
                       provider-neutral submit port; confirmation/failure reconciles state.
Canonical data/invariants:
                         - one write intent ⇒ one durable outbox record;
                         - one stable idempotency key ⇒ at most one effective submit;
                         - signatures and message bytes never leak raw private keys;
                         - outbox records survive process restart and offline periods;
                         - no duplicate submits, no silent rollback, no unbounded retry.
Dependencies:            domain.write (intents/messages), capability.write (submit port),
                         capability.signer (signing port), local_state.write (outbox port),
                         application.write (orchestration), storage.write (SQLite outbox).
Expected failures:       SigningUnavailable, OutboxUnavailable, SubmitConflict,
                         SubmitUnsupported, TransportOffline, RateLimited.
Local-state representation: optimistic canonical entity (e.g. cast) write-through + outbox
                         record with status (pending / submitted / confirmed / failed).
Offline behavior:        enqueue and optimistic write succeed while offline; submit is
                         deferred until a routed submitter is available; retry is bounded.
Provider policy and disablement:
                         no write operation is advertised before canonical contract + signer
                         port + outbox semantics + a submit port are real; the concrete Phase 5C
                         Snapchain adapter uses binary HTTP `/v1/submitMessage` plus
                         `/v1/castById` reconciliation. When no write submitter is composed, the
                         signed durable record remains queued rather than weakening the gate.
Failover semantics:      routing resolves each submit operation independently through the
                         provider-neutral write gateway; a failing submitter does not change
                         canonical domain models or application use cases.
Tests proving the boundary:
                         canonical protocol fixtures; outbox record durability across store
                         reopen; idempotency/account isolation; optimistic snapshot + failure
                         preservation; application retry/reconciliation lifecycle; concrete
                         Snapchain binary submit + castById contract behavior.
```

## Phase ordering note

This gate describes the first durable protocol-write slice, delivered as Phase 5C. Phase 5A establishes guest-first account authentication and Phase 5B establishes real signer provisioning/readiness. The existing `capability.signer` port remains the stable raw-key-isolation boundary, but Phase 5C must consume a signer that Phase 5B has already verified as authorized, unexpired, unrevoked, and sufficiently scoped for the requested message type.

## Scope delivered for this slice

- **Implemented in Phase 5C:** canonical root PublishCast intent → CAST_ADD codec, an already
  provisioned/authorized Phase 5B signer authority, durable SQLite outbox, optimistic
  canonical cast materialization, deterministic protocol-hash idempotency, provider-neutral
  submit/reconcile capability, and a concrete Snapchain HTTP adapter.
- **Implemented in Phase 5D:** reply publication as the same PublishCast/CAST_ADD operation
  with canonical parent `CastReference { fid, hash }`, the same durable outbox,
  exact-envelope retry, reconciliation, signer gate and account isolation.
- **Implemented in Phase 5E:** Cast removal, Like/Unlike, Recast/Undo Recast,
  Follow/Unfollow, exact per-operation signer scopes, restart-safe social targets,
  desired-state reconciliation, and App-FID `SignedKeyRequest` attestation.
- **Still out of scope after Phase 5E:** mentions authoring UI, media upload,
  long/10k cast product eligibility, profile/verification writes and derived
  infrastructure.

Root cast publication was the first write operation; Phase 5D/5E prove that later
operations extend rather than bypass the same authority/outbox/reconciliation
invariants.
