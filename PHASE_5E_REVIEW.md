# Phase 5E Review — Social Writes and App-FID Authorization

Date: 2026-08-28

## Scope reviewed

Phase 5E extends the existing protocol-native durable write architecture to:

- `CAST_REMOVE`;
- `REACTION_ADD` / `REACTION_REMOVE` for Like and Recast;
- `LINK_ADD` / `LINK_REMOVE` for Follow;
- App-FID `SignedKeyRequest` attestation so a production user custody wallet
  signs only the final gasless `KEY_ADD` request;
- local sliding-TTL refresh after authoritative write confirmation;
- Qt Feed/Detail interaction wiring and presentation reconciliation.

## Architecture result

PASS — no write action bypasses Application, signer scope enforcement,
canonical protocol encoding, the durable outbox, or authoritative
reconciliation.

Read behavior remains independent of write readiness. Hypersnap remains the
read provider policy; protocol writes are canonical Farcaster messages submitted
through the Snapchain write adapter.

## Security result

PASS in source design:

- SIWF and protocol write authority remain separate concepts;
- device Ed25519 private key remains isolated behind `INativeSigner`/secure vault;
- production App-FID custody key is not embedded in Genycaster;
- App-FID attestation receives only public key/FID/deadline material;
- user wallet approval is still required for the exact scoped `KEY_ADD`;
- the approval account must match the FID's authoritative `IdRegistry.custodyOf(fid)` address; SIWF AuthAddress and custody authority are not conflated;
- signer scopes are least privilege (`CAST_*`, `REACTION_*`, `LINK_*` only);
- retries reuse the exact signed envelope;
- signer replacement/revocation/expiry is re-gated before submission;
- production App-FID configuration fails closed when only half configured.

## Durable-state result

PASS in tested core layers. The SQLite outbox now persists operation kind,
author, optional target Cast, optional target FID and optional optimistic Cast.
Schema migration preserves earlier Cast outbox rows while allowing reaction and
link records to survive restart. Migration v14 additionally materializes the
latest account-scoped Like/Recast desired state so reaction affordances survive
provider refresh and application restart instead of existing only in memory.

## Presentation result

Source wiring is complete:

- Like/Unlike and Recast/Undo actions are available on Cast rows/details;
- Follow/Unfollow is available on profile detail;
- Delete is available only for the authenticated user's own canonical Cast;
- confirmed mutation state updates the local presentation snapshot;
- durable Like/Recast intent is re-applied after feed/detail refresh and session restore;
- interaction controls require authenticated + write-ready state.

Qt runtime acceptance is not claimed in the Linux verification container because
Qt6 is unavailable there. The normal macOS Qt/Clang runner remains the product
release gate.

## Follow-up

Phase 5F should harden write behavior under restart/offline/revocation/duplicate
stress and run full platform-native live acceptance with a deployed App-FID
attestation service and Snapchain mainnet/test account.
