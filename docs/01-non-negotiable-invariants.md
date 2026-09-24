# 01 — Non-Negotiable Architectural Invariants

These rules are stronger than implementation convenience. Violating one requires an ADR and explicit architectural review.

## Provider invariants

1. Replacing or removing any managed backend must not require redesigning the client.
2. Replacing Hypersnap must not require redesigning the client.
3. A strict self-hosted/private deployment must remain architecturally valid.
4. No provider-specific SDK type may cross the provider boundary.
5. No provider-specific response model may enter domain/application/UI code.
6. No provider-specific cursor may become an application pagination contract.
7. No provider-specific authentication token may become a domain concept.
8. No provider-specific error code may become application control flow.
9. No provider hostname or endpoint path may exist outside its adapter/configuration package.
10. Provider selection is per capability/operation, not global.

## Feature invariants

1. DMs are optional.
2. Channels are optional.
3. Mini Apps are optional.
4. Wallet functionality is optional.
5. Search is optional.
6. Notifications are optional.
7. Legacy compatibility features are optional.
8. Secure messaging extensions are optional.
9. Removing an optional feature must not break core social capabilities.
10. Feature availability is determined by capability availability, not by provider identity.

## Domain invariants

1. Canonical Farcaster entities are provider-neutral.
2. Protocol-authenticated/canonical data must not be mixed with derived ranking/reputation values.
3. Derived data must be modeled separately from canonical entities.
4. Product-private data must be modeled separately from protocol data.
5. Provider provenance, when useful, is metadata — not canonical domain identity.

## Local-first invariants

1. UI rendering never depends directly on a provider request.
2. Network, database, JSON parsing, crypto, media decode, and persistence are never performed on the UI thread.
3. Valid cached state is rendered immediately.
4. Sync is incremental where possible.
5. Optimistic/local operations survive process restart when persistence is required.

## Write-path invariants

1. Protocol-native writes should be represented as canonical commands/messages before transport selection.
2. Provider methods must not define domain write semantics.
3. Write failover must account for ambiguous success and duplicate submission risk.
4. Idempotency/reconciliation strategy must be explicit for every write capability.

## Product-moat invariant

> A product moat must never silently become an architectural dependency.

If a feature exists only behind a flagship/backend service, it must be classified as `Compatibility` until it is replaced, standardized, or independently implemented.
