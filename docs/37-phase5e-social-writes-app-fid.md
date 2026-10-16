# Phase 5E — Social Writes and App-FID Authorization

Phase 5E extends the existing Phase 5C/5D durable write pipeline instead of
adding provider-specific mutation APIs. Like/Unlike, Recast/Undo Recast,
Follow/Unfollow and Cast removal are canonical Farcaster protocol messages,
use the same secure Ed25519 signer, and pass through the same SQLite outbox,
retry and authoritative reconciliation rules as Cast publication.

## Delivered protocol operations

| Product action | Farcaster message | Required signer scope |
|---|---|---|
| Publish / Reply | `CAST_ADD` | `CAST_ADD` |
| Delete own Cast | `CAST_REMOVE` | `CAST_REMOVE` |
| Like / Recast | `REACTION_ADD` | `REACTION_ADD` |
| Unlike / Undo Recast | `REACTION_REMOVE` | `REACTION_REMOVE` |
| Follow | `LINK_ADD` | `LINK_ADD` |
| Unfollow | `LINK_REMOVE` | `LINK_REMOVE` |

The default Genycaster signer policy requests only those six social scopes.
`KEY_ADD` and `KEY_REMOVE` are never delegated signer scopes.

## Durable-write invariant

Every implemented write follows one path:

```text
Qt intent
  -> Application scope gate
  -> canonical MessageData protobuf
  -> BLAKE3-20 message hash / idempotency identity
  -> Ed25519 signature through the isolated signer handle
  -> exact signed Message envelope persisted in SQLite
  -> reconcile desired state
  -> submit exact envelope when required
  -> authoritative reconcile
  -> confirmed / bounded retry / failed
```

Retries reuse the exact signed envelope. They do not reconstruct an intent and
do not re-sign it. Reaction and Link writes store their canonical target in the
same outbox record shape used by Cast writes.

## App-FID signer authorization

SIWF establishes account authentication. It is not write authorization.
Genycaster provisions a separate scoped Ed25519 signer with Snapchain `KEY_ADD`.

For a production App-FID flow, the App FID signs `SignedKeyRequest` metadata and
the user custody wallet signs only the `KeyAdd` authorization. The App FID
custody private key must never ship inside the Qt application.

```text
Genycaster device signer public key
  -> Genycaster attestation endpoint
  -> App-FID SignedKeyRequest proof
  -> user custody wallet signs KEY_ADD once
  -> Snapchain KEY_ADD
  -> GetSignersByFid authoritative polling
  -> Ready
```

After the signer is Ready, ordinary Cast/Like/Recast/Follow operations use the
local isolated Ed25519 signer and do not reopen the custody wallet.

## Production configuration

Set both values together:

```text
GENYCASTER_APP_FID=<Genycaster App FID>
GENYCASTER_SIGNER_ATTESTATION_URL=https://auth.example.com/v1/farcaster/signer-attestation
```

Equivalent command-line options are:

```text
--app-fid=<fid>
--signer-attestation-url=<https-url>
```

Configuration is fail-closed: setting only one of the two values makes signer
setup unavailable instead of silently weakening attribution.

The shipping Genycaster UI does not permit self-client attribution. If the App FID
or attestation endpoint is unavailable, signer provisioning remains unavailable
instead of substituting the signed-in user's FID as `requestFid`. Lower-level
SignerService tests may still exercise self-attribution as a protocol compatibility
case, but product wiring never enables that path.

The App FID must also be distinct from the currently signed-in user's FID. Farcaster
does not accept an arbitrary client display-name field for this attribution: clients
resolve the verified `requestFid` to its Farcaster profile and render that identity
in surfaces such as `Sent from …`. Therefore a dedicated Genycaster Farcaster FID
whose profile represents the client is required for the external label to read as
Genycaster. A product-name string in the desktop binary cannot substitute for that
protocol identity.

## Attestation endpoint contract

The desktop client sends a bounded HTTPS JSON request containing only public
request material:

```json
{
  "requestFid": "123456",
  "publicKey": "0x<32-byte-ed25519-public-key>",
  "deadline": 123456789
}
```

The service MUST validate the configured App FID and sign this exact EIP-712
payload with the App FID custody authority:

```text
Domain:
  name: Farcaster KeyAdd
  version: 1
  chainId: 1

Primary type: SignedKeyRequest
Fields:
  requestFid uint256
  key        bytes
  deadline   uint256
```

It returns:

```json
{
  "requestFid": "123456",
  "publicKey": "0x<same-public-key>",
  "deadline": 123456789,
  "requestSignerAddress": "0x<20-byte-app-custody-address>",
  "signedKeyRequestSignature": "0x<65-byte-eip712-signature>"
}
```

The client rejects a response whose FID, public key or deadline differs from
its request. It also enforces exact 20-byte address and 65-byte signature
lengths before constructing `SignedKeyRequestMetadata`.

The endpoint never receives the user's custody key, the user's custody
signature, or the device signer private key. The only private key owned by the
service is the App FID custody key, which should be held in a KMS/HSM or another
server-side secret boundary appropriate to deployment.

## Sliding TTL

The default gasless signer TTL is 30 days. A network-confirmed write advances
the locally cached `lastUsedAt`/verification timestamp so the local gate does
not falsely expire an actively used signer while Snapchain's sliding TTL is
being refreshed.

## Local-first removal semantics

`RemoveCast` enqueue atomically records a durable local cast tombstone beside
the signed outbox operation. Entity and timeline reads exclude tombstoned casts
immediately and continue excluding them across refresh/restart, including when
the network removal is retrying or terminally rejected. This follows the
no-silent-rollback local-first policy while retaining the exact signed remove
message for explicit retry/recovery.

## Local-first reaction semantics

Like/Unlike and Recast/Undo-Recast enqueue atomically update an account-scoped
`cast_reaction_local_state` projection beside the signed outbox operation. Feed
and Cast-detail presentation overlays that durable desired state after every
provider refresh, so a successful local interaction is not erased merely
because the provider feed payload carries aggregate engagement counts but no
authenticated viewer-reaction state. The projection is rehydrated off the GUI
thread after session restore and survives restart.

Migration v14 backfills the newest reaction intent already present in older
Phase 5 outboxes. Re-enqueueing an older idempotency key does not overwrite a
newer inverse intent. When the locally active viewer reaction is known but a
stale provider snapshot still reports an impossible zero aggregate count,
presentation clamps that count to at least one until the authoritative metrics
refresh catches up.

## Reconciliation endpoints

A fresh write is confirmed from the validated successful `/v1/submitMessage`
response, so normal interaction does not pay an immediate read-after-write lookup.
The Snapchain write adapter uses these endpoints only to reconcile retry/restart
ambiguity before re-submission:

- Cast presence: `/v1/castById`
- Reaction presence: `/v1/reactionById`
- Follow presence: `/v1/linkById`

For remove operations, authoritative absence is the confirmed desired state.
This prevents a timeout or duplicate response from causing a second effective
mutation.

## UI boundary

QML sends user intents only. It does not construct protocol identities or
protobufs. Feed/detail controls forward canonical cast/FID values to the C++
presentation bridge, which invokes Application. Interaction controls remain
disabled until both account authentication and verified signer readiness are
true.

## 2026-10-16 attestation routing correction

A public Snapchain read/write gateway does NOT supply
`POST /v1/farcaster/signer-attestation`. It explicitly returns
`404 {"error":"route_not_available"}` for unknown routes.
Previously the desktop client invented this path by appending it to the
Snapchain base URL when the attestation URL was unset. That produced a
misleading signer error even though no signing authority was available.

The client now **never synthesizes an attestation URL** from Snapchain.
`GENYCASTER_SIGNER_ATTESTATION_URL` must point to a **separately deployed
trusted signer service** that can produce a verifiable
`SignedKeyRequest` signature for the configured Genycaster App FID,
bound to the exact `requestFid`, public key and deadline. The private
App-FID signing key must remain server-side in a restricted signing
service; it must never be bundled with the Qt app or stored in the
Snapchain gateway. With no configured service, write enablement fails
closed with a setup message instead of issuing repeated HTTP 404 calls.
Explicitly misconfigured endpoints returning HTTP 404 are classified as
`NotFound` and mapped to the attestation-configuration message, rather
than a generic Snapchain signer rejection.

**Deployment prerequisite still open:** no such signing endpoint or
App-FID signing credentials were established by this UI/client fix.
Consequently read-only login works, but real protocol publishing
requires provisioning and validating that secure server service and
an end-to-end wallet approval. Do not report writer activation as done
until KEY_ADD is observed on Snapchain and signed casts are accepted.

## Restoring direct custody-authorized writing without App-FID (2026-10-16)

Regression: Runtime previously required **both** a canonical App FID and a
SignedKeyRequest attestation URL before it instantiated `SignerService`. As a
result, an ordinary desktop build without deployment-only App-FID secrets
could no longer reach the existing wallet-based signer authorization flow.
The account popup then remained `unavailable`, which looked like a protocol
failure even though native sign-in worked.

Snapchain supports **self-requested KEY_ADD**: the verified user's custody
wallet signs both the `SignedKeyRequest` for their **own FID** and the
`KeyAdd` authorization. These are separate EIP-712 signatures, submitted
through the existing local browser-wallet approval and verified by Snapchain.
The secure Ed25519 signing key continues to live in the local OS key vault.
The signer is attributed to the **user's FID**, not to Genycaster's independent
App FID. The wallet approval page now states this clearly before consent.

Runtime chooses the strict externally-attributed path **only when both**
`GENYCASTER_APP_FID` and `GENYCASTER_SIGNER_ATTESTATION_URL` are provided.
Without a trusted attestation URL, it uses the self-requested custody flow
instead of disabling all writes. A stray attestation URL without its matching
App FID is rejected as a configuration error; a fully configured App-FID
path still fails closed if the attestor is invalid. There is **no** fallback
from a failed live App-FID signature to a self-attributed signature, and
no fabricated signature or bypassed custody validation.

This restores an actual route to permissioned publishing while preserving
the stronger App-FID attribution when available. A production App-FID signing
authority is only necessary when Genycaster must appear as the attributed
client, not for user-custody-authorized personal signer registration.

Tests exercise personal and externally attributed wallet approval forms,
requester-FID metadata, the signer-registration/restart lifecycle, and strict
denial for external attribution without a trusted attestor. A real user wallet
approval followed by Snapchain-verified KEY_ADD and a published cast must still
be tested in the shipped macOS application before marking end-to-end complete.

## 2026-10-16: custody-wallet approval usability and signer lifetime

Farcaster's finalized **FIP 2026 Snapchain Signers** uses `KEY_ADD` with
EIP-712 custody authorization; the custody wallet (FID owner) approves
a bounded-scope Ed25519 signer. The approved local key may then sign casts,
reactions and links without prompting the wallet per interaction. A new
authorization is required only when the signer is missing, revoked, expired,
or otherwise no longer eligible. The app retains signer material in the
platform key vault and verifies registration on restart; ordinary sign-in
is distinct from a new `KEY_ADD`.

The user-provided Hypersnap signer reference
(`https://hypersnap.org/docs/reference/reads/signers`) documents
read-only signer/on-chain event endpoints. Hypersnap's compatibility
`POST /v2/farcaster/signer*` routes return HTTP 501 and cannot provide
desktop custody authorization. Repointing KEY_ADD approval to Hypersnap
is not a valid shortcut. Existing native Snapchain registration stays in
use for Farcaster permissioned writes regardless of the read provider.

The desktop previously invoked `QDesktopServices::openUrl` for the
one-shot `http://127.0.0.1:<port>/approve/<capability>` URL, selecting
the macOS default browser (usually Safari), which does not expose an
injected EIP-1193 provider. The custody approval launcher now prefers
installed **Brave Browser**, then **Google Chrome** when no explicit
wallet URL template is configured. It does not change Mini App wallet
URL launching or override an explicitly configured template. If neither
wallet-capable browser can be found it retains the platform fallback,
and the local approval page explains the extension requirement.

Selecting a wallet other than the on-chain FID custody address is a
recoverable mismatch, **not** an authorization success. The page
shows the required custody address and lets the user switch wallet
accounts and retry without discarding the one-shot loopback ceremony.
The provider still refuses mismatched callbacks and never issues
`KEY_ADD` with the wrong address. Browser wallet errors/rejections no
longer automatically send the fatal `wallet_error` callback from
JavaScript; the form remains usable until success or timeout. The
backend still honors an explicit aborted/rejected callback when posted.

Reduced Motion suppresses decorative transitions/shimmer but now
retains essential progress-spinner movement at a slower 1.4-second
rotation. Image loading indicators do not become a static placeholder
solely because Reduce Motion is enabled.
