# Node-attested App FID attribution from raw Snapchain

## Contract and evidence

Protocol `Message.signer` identifies a 32-byte Ed25519 public key, not
an app display name. Snapchain provides the unified
`GET /v1/signer?fid=<author>&signer=<0x-public-key>` endpoint
(see upstream `src/network/http_server.rs`, `SignerHttpRequest`).
For `SIGNER_SOURCE_OFFCHAIN` (gasless KEY_ADD), `requestFid`
is the app FID bound to custody-authorized SignedKeyRequest metadata.
Snapchain validates the EIP-712 request-signer and requester custody FID
at registration time. This is **a claim validated by the selected
protocol-validating Snapchain node**, not an independent local EIP-712 check.

Official proposal: https://github.com/farcasterxyz/protocol/discussions/266

## Implemented behavior

- A strict `decodeSignerAppFid` boundary checks the **exact signer
  public key**, **cast author FID**, `keyType=1`, source
  `SIGNER_SOURCE_OFFCHAIN`, positive `requestFid`, positive bounded TTL,
  consistent `lastUsedAt+ttl=expiresAt`, current unexpired registration and
  (critically) **Farcaster-epoch seconds** for both network expiry and
  comparison clock; using Unix seconds marks valid registrations expired.
  `MESSAGE_TYPE_CAST_ADD` scope. Missing/legacy/expired/unauthorized records
  produce no App FID; mismatched data fails verification. No app FID is
  synthesized from the author, text, indexed `app.name` or read endpoint.
- The raw `snapchain-v1` async adapter enriches profile feed Casts after
  canonical decoding, using only the **same selected read-node origin and
  endpoint-specific credentials**. An optional lookup is bounded to eight
  *distinct (author FID, signer-key) pairs per page*. It groups repeated
  keys and never transfers credentials to another provider. Network errors,
  missing endpoints and corrupt registrations leave the canonical Cast
  available with **unknown** App FID.
- The same best-effort enrichment is used after the bounded raw Following
  merger. It is *not* a full directory lookup: if a page contains more than
  eight signer pairs, the remainder stay unknown rather than being guessed.
  The derived Following feed's existing author and history limits still apply.
- `domain::cast::Cast::sourceAppFid` is an optional canonical App FID,
  distinct from `sourceClient` (unverified display label) and
  `signerPublicKeyHex`. SQLite additive migration 28 stores it as nullable
  INTEGER; sparse provider results cannot erase previously recorded source
  FIDs. This is *last observed* registration evidence, not evidence that an
  app still controls a key after later revocation/re-registration.
- Qt maps the App FID as a **decimal string** to avoid JavaScript 64-bit
  integer precision loss. App FID Include/Exclude controls operate on exact
  decimal IDs, separate from signer-key filters and preserve provider order.
  Unknown App FIDs cannot match Include and remain visible in Exclude.
  The interface says **App FID confirmed by selected node; app name
  not verified**, translated into all 12 locales.

## Boundaries and remaining acceptance work

- Onchain legacy signers do not expose a trusted `requestFid` in this
  path; they remain unknown. A decorative hosted `app.name` badge is never
  substituted as security-grade proof.
- A signer may be revoked, expired or re-registered after an older Cast;
  an observed current association is not rigorous historical app ownership
  at Cast creation time. Correct historical attribution requires registration
  events and signer state at Cast time, not only the present snapshot.
- For now signer lookups are sequential and capped, and feed presentation
  scopes filters to available Casts; complete cross-network app filtering
  requires full indexing, evidence refresh/expiration and source-independent
  coverage. Selected node trust is not equal to a locally verified signature.
- No own live Snapchain node and no independent human operator test are
  available, so **Balaji acceptance remains PARTIAL, not qualified**.

## Verification

```sh
cmake --build build/secure-signer --target farcaster_snapchain_read_contract_tests farcaster_read_pipeline_tests -j 2
ctest --test-dir build/secure-signer -R 'provider.snapchain.raw_read.contract|application.local_first_read' --output-on-failure
node --test tests/wasm/raw_signer_client_filter.test.mjs
```

Tests exercise strict registration parsing, revoked/expired/legacy responses,
FID/key mismatch, same-node async enrichment and graceful unknown identity,
non-destructive SQLite migration from pre-App-FID snapshots, persisted
provenance after restart, and include/exclude matching semantics.
