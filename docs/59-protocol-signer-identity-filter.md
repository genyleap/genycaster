# Signed-message signer identity and selective client-key visibility

## Why a display name is not app provenance

A Farcaster `Message` contains a `signer` public key and a signature;
an application display name is not a protocol field. A signed key request can
attribute a delegated signer to the requesting application FID only after
validating the corresponding signer-registration metadata and current state.
It is unsafe to infer the client from cast author FID, read node, the
`sourceClient` label supplied by a hosted index, or text inside a post.

Authoritative references:
- https://github.com/farcasterxyz/protocol/blob/main/docs/SPECIFICATION.md
- https://github.com/farcasterxyz/protocol/discussions/103
- https://github.com/farcasterxyz/protocol/discussions/266
- https://github.com/farcasterxyz/snapchain

## Implemented now

- Canonical `domain::cast::Cast::signerPublicKeyHex` represents an **observed
  protocol public key** (lowercase 0x-prefixed 32-byte Ed25519 key). It is
  deliberately separate from `sourceClient`, the unverified display label.
- The raw-node decoder accepts the two documented byte representations:
  base64 protobuf JSON and exact 32-byte 0x hex; malformed, absent or
  non-Ed25519-sized values are **unknown**. It never substitutes a guessed
  app name for missing identity.
- SQLite migration 27 adds `casts.signer_public_key_hex` non-destructively.
  Cached root casts and direct reply trees retain the observed key on restart.
  Subsequent partial data from a compatible indexed provider cannot clear
  an already-observed key. No private signer material is stored.
- Qt presentation carries the observed key into feed row data, cache
  revisions and reply maps, without elevating hosted `sourceClient` to proof.
- The feed's optional **Signer key (app unverified)** selector offers
  *All*, *Only selected*, and *Exclude selected*. The candidate list comes
  only from canonical observed signer keys in the currently loaded feed.
  Include matches only exact key equality; missing keys are excluded.
  Exclude removes only the matching key while keeping unknown posts visible.
  Labels and controls are translated in all 12 locales.
- Filtering is presentation-only, preserving the provider's feed order and
  avoiding changes to canonical source data, signer authorizations or
  durable write routing. No network API key or provider-specific filter
  schema is introduced.

## Important limitations

**This is not a verified app-name or app-FID Include/Exclude filter.** The
public signer key tells us which delegated key produced a canonical message
as reported by the selected Snapchain node. The mapping between that key and
a named app still requires authoritative key-registration metadata, active
registration checks, and (where appropriate) validation of `requestFid`
against the signed key request. No locally verified signature check was added;
a protocol-validating node is the current source of message authenticity.

The selector is scoped to available feed rows. A key absent from cached
posts is unknown, not proof that the author used a particular app. The
available keys are not a network-wide client directory. A revoked signer or
a changed app authorization must be handled through the future verified
registry enrichment layer, not by guessing.

Balaji's requirement to include/exclude casts *by verified application*
remains **PARTIAL/NOT SATISFIED** until those registry checks and a real
self-hosted end-to-end run are implemented.

## Regression

```sh
cmake --build build/secure-signer --target farcaster_snapchain_read_contract_tests farcaster_read_pipeline_tests -j 2
ctest --test-dir build/secure-signer -R 'provider.snapchain.raw_read.contract|application.local_first_read' --output-on-failure
node --test tests/wasm/raw_signer_client_filter.test.mjs
```

Tests cover canonical signer normalization, invalid/absent signer handling,
non-destructive SQLite upgrade and restart, sparse provider merge,
actual extracted QML filter behavior (including unknown identity), and
translation presence. These do not establish a live cryptographic
registry-attested app identity.
