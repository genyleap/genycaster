# 36 — Phase 5D Reply and Shared Composer

Phase 5D is **implementation-final in source** as of 2026-08-28.

This slice extends Phase 5C without creating another write stack. A reply is the
same Farcaster `MESSAGE_TYPE_CAST_ADD` operation with a canonical
`CastAddBody.parent_cast_id`:

```text
Canonical cast in Presentation
  -> Reply intent
  -> parent CastReference { parent FID, parent 20-byte CastId }
  -> same session + current CAST_ADD signer gate
  -> same canonical codec
  -> same atomic optimistic projection + durable outbox
  -> same reconcile-before-retry loop
  -> same Snapchain submit/reconcile adapter
```

Guest/public/cached reads remain independent of this path.

## Protocol contract

The current Snapchain protobuf defines reply parent context as:

```text
CastAddBody.parent_cast_id = field 3
CastId.fid                = field 1
CastId.hash               = field 2 (20 bytes)
```

Genycaster models the protocol input as `domain::cast::CastReference`, which
contains both the parent author FID and canonical CastId. `CastId` alone is not
sufficient to encode the wire parent.

Protocol validates before signing:

- parent author FID is non-zero;
- parent hash is exactly 20 bytes of hexadecimal data;
- ordinary CAST_ADD validation still applies, including the standard 320 UTF-8
  byte text limit and conservative embed compatibility limit.

The deterministic root CAST_ADD fixture from Phase 5C is unchanged. A reply
fixture additionally pins the nested parent CastId bytes, proving that extending
PublishCast did not alter root serialization.

## One write use case

`PublishCastIntent` and `PublishCastRequest` now carry an optional canonical
parent. There is deliberately no `ReplyService`, reply outbox, reply submitter or
reply-specific provider API.

`WriteService::publish()` remains the only path:

1. require an authenticated, unexpired session;
2. require the current network-verified signer with `CastAdd` scope;
3. prepare canonical MessageData using the optional parent;
4. sign the exact 20-byte BLAKE3 protocol hash;
5. atomically enqueue optimistic Cast + exact signed Message envelope;
6. submit the first attempt directly;
7. validate the successful submit response and confirm it without a redundant
   read-after-write lookup;
8. after an ambiguous prior attempt, reconcile by canonical CastId before any
   re-submission, then retry, block or fail according to the durable Phase 5C
   policy.

Retry never reconstructs the reply and never asks the signer to sign again.
The exact signed envelope committed before network I/O remains the retry payload.

## Durable reply state

No SQLite schema fork is required. The existing optimistic Cast representation
already persists its optional parent CastId and the signed envelope already
contains the parent FID/hash wire data.

The Phase 5D storage regression proves that a reply can be enqueued, the store
closed, and the same canonical parent recovered after reopen.

FID isolation remains unchanged: ready-list queries are scoped by authenticated
author FID, so switching accounts cannot resume, reconcile or project another
account's queued reply.

## Presentation boundary

QML still supplies **intent only**. It never sees or constructs protobuf,
MessageData bytes, BLAKE3 hashes, signatures, outbox records or provider
requests.

The reply action passes a cast's string CastId to `PresentationBridge`.
`PresentationBridge::prepareReply()` resolves the parent author FID from the
current canonical C++ presentation state and retains the protocol parent target
inside the bridge.

This is intentional: Farcaster FIDs are unsigned 64-bit values and are not sent
through QML/JavaScript numeric representation for protocol construction.
QML keeps only display context such as parent CastId, name and text preview.

When `publishReply(text)` is invoked, the bridge emits the already-resolved
parent FID + CastId into the C++ composition root. Application/Protocol validate
again before signing.

## Shared composer UX

Root publish and reply use the same Qt popup and lifecycle state.

Reply mode adds:

- explicit `Reply` title/action;
- parent author/context preview;
- reply-specific placeholder/status copy;
- the same 320 UTF-8 byte preflight;
- the same queued/submitting/retry/blocked/failed/confirmed lifecycle.

The composer closes on authoritative confirmation. It remains available during
retry, blocked or failure states so the user can understand the durable local
state without removing the authoritative parent cast from view.

Closing the composer, signing out or switching accounts clears the retained
reply target. Late write callbacks remain author-FID gated by the existing
Phase 5C presentation isolation.

## Official client alignment

The pinned Farcaster client snapshot
`b6922e24036cac6f5e6d51904a59ff7cfcdd8483` also treats reply context as an
optional parent on the ordinary composer/create-cast flow:

- `apps/farcaster-mobile/src/utils/CastComposerIntentUtils.ts` carries
  `parentCastHash` in the same composer intent;
- `packages/farcaster-client-hooks/src/hooks/data/mutations/useCreateCast.ts`
  adds an optional parent to the ordinary create-cast request.

Genycaster keeps the same product shape while using the protocol-native
`CastReference { fid, hash }` required for direct Snapchain submission rather
than a private backend contract.

## Cross-platform requirement

Phase 5D adds no OS-specific code to Domain, Application, Protocol or durable
storage. It reuses the Phase 5B signer abstraction, Phase 5C SQLite/HTTP write
pipeline and Qt presentation boundary.

A release still requires a normal supported Qt/Clang build and live signer +
Snapchain acceptance on every platform that advertises write support.

## Deferred after Phase 5D

The next write slice may add reactions, recasts and follow state over the same
authority/outbox/reconciliation principles. Phase 5D does not implement those
operations, mention authoring UI, media upload or long/10k cast eligibility.
