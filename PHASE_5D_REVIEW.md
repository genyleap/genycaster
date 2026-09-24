# Phase 5D Review — Reply and Shared Composer

Date: 2026-08-28

## Decision

**Phase 5D is implementation-final in source.**

Reply publication now extends the Phase 5C PublishCast pipeline with canonical
parent CastId semantics. No parallel reply service, persistence path, signer
path or provider route was introduced.

## Implemented boundary

### Domain

`domain::cast::CastReference` owns the protocol-neutral pair required to refer
to a parent cast:

- parent author FID;
- parent CastId/hash.

`domain::write::PublishCastIntent` carries that reference optionally. Root casts
therefore remain the same operation with no parent.

### Protocol

`farcaster.protocol.message` validates and encodes the optional
`CastAddBody.parent_cast_id` using the current Snapchain schema. Parent FID zero,
wrong-length hash and non-hex hash fail before signer invocation.

`farcaster.protocol.write` keeps the exact Phase 5C identity rule:

```text
BLAKE3-20(MessageData) == canonical CastId == outbox idempotency key
```

The optimistic reply stores the parent CastId while the exact signed envelope
retains all canonical wire data required for retry.

### Application and storage

`WriteService::publish()` remains shared by root and reply. Session authority,
`CastAdd` scope, current signer public-key checks, atomic enqueue,
reconcile-before-submit, bounded retry and terminalization rules are unchanged.

The existing SQLite outbox schema is sufficient. Reply parent state survives
store reopen, and ready-list/account isolation remains author-FID scoped.

### Presentation and Qt composer

Feed and detail surfaces expose a Reply action only when a canonical cast ID is
available. Both use the existing composer popup.

QML passes the cast ID for selection and display context only. The C++
`PresentationBridge` resolves the parent author FID from canonical presentation
state and retains it outside QML. This avoids u64 precision loss through the
JavaScript numeric boundary and prevents QML from constructing protocol parents.

Root and reply share input validation and write lifecycle. Reply-specific copy
is limited to user-facing context/status.

## Regression and safety review

Verified invariants:

- existing root CAST_ADD fixture remains unchanged;
- reply has an independently pinned canonical protobuf fixture;
- invalid parent fails before signing or enqueue;
- reply survives SQLite reopen with parent intact;
- restart/retry submits the exact stored envelope without re-signing;
- account switch cannot resume another FID's reply;
- existing signer unavailable/revoked/replaced protections still apply;
- late write callbacks remain current-account gated;
- guest/public/cached reads remain independent of reply failures.

## Security review

Phase 5D introduces no new secret-bearing storage or UI surface. QML does not
receive private keys, seeds, custody secrets, signatures, signed message bytes or
provider credentials.

Parent author FID is resolved in C++ presentation state and then validated again
in Protocol. A forged or stale UI parent cannot bypass canonical validation or
the authenticated-author signer gate.

## Cross-platform review

No macOS-only, Windows-only, Linux-only, Android-only or iOS-only dependency was
introduced by reply support. Release acceptance remains per platform because
native Qt, secure-store/wallet and live Snapchain behavior must be proven on the
actual supported runner.

## Upstream alignment

Reviewed official Farcaster client snapshot:
`b6922e24036cac6f5e6d51904a59ff7cfcdd8483`.

Its mobile composer intent and create-cast mutation both model reply as optional
parent context on the ordinary cast flow. Genycaster follows that shared-composer
shape while constructing the full protocol-native `{ parent fid, parent hash }`
required by Snapchain directly.

Current Snapchain protocol reference remains:
`8f82bcc68dd4e5c75b4a4814bf6fc6b54215a73c`.

## Next slice

Phase 5E may implement Reaction/Recast/Follow writes, but must reuse the existing
session + signer authority, deterministic protocol identity, durable outbox,
reconciliation and account-isolation architecture instead of bypassing it.
