# 44 — Provider-Neutral Media Upload Gateway

This feature gate records the production media-upload boundary.

```text
Responsibility:        allow authenticated Genycaster users to attach local images/videos to
                       casts without exposing storage credentials or binding the desktop client
                       to qStorage, S3, R2, or another object-storage vendor.
Capability origin:     Extension/Derived infrastructure. Farcaster stores media URLs as embeds;
                       object storage itself is not a Farcaster protocol capability.
Mandatory or optional: Optional product composition. Guest/read-only builds and provider-free
                       core builds remain valid without media upload.
Public contract:       Application accepts a local file path plus the verified client session,
                       runs upload work on an I/O executor, and returns one stable public Asset URL
                       plus an opaque local-only management token. Confirmed Cast deletion releases
                       locally-associated managed assets through an authenticated lifecycle gateway.
Canonical data/invariants:
                         - storage access/secret keys never ship in the desktop binary;
                         - casts receive only stable Genyleap public asset URLs;
                         - QStorage write credentials remain server-side;
                         - upload chunks are bounded and exact-size checked;
                         - the gateway verifies the completed object before returning success;
                         - one media upload never blocks the Qt GUI thread;
                         - cancellation aborts the provider multipart session and never inserts a
                           partial media block into the composer;
                         - management tokens are local lifecycle credentials, never Farcaster embeds;
                         - QStorage deletion occurs only behind authenticated gateway ownership checks;
                         - a managed asset referenced by another locally tracked Cast is retained.
Dependencies:            domain.media, capability.media, application.media.upload, application
                         executor/session, isolated protocol signer authority, Qt Network gateway
                         adapter, Genyleap media service, Quilibrium SDK server addon.
Expected failures:       SignInRequired, SignerUnavailable, InvalidMedia, TooLarge, Cancelled,
                         GatewayUnavailable, RateLimited, UploadRejected, VerificationFailed.
Local-state representation: rich drafts/published Cast sidecars persist URL -> opaque management
                         token associations in QSettings. A durable pending-cleanup token list survives
                         transient gateway/offline failures until deletion succeeds.
Offline behavior:        no new upload/delete is attempted while unavailable; an already uploaded
                         stable URL remains a normal rich-media embed. Cleanup intent stays durable and
                         retries when signer/network readiness returns.
Provider policy:         qStorage is the initial backend behind the Genyleap gateway. Provider
                         semantics never cross the media capability boundary.
Failover semantics:      the gateway owns storage-provider selection. Client contracts and stored
                         cast URLs remain unchanged when the backend changes.
Tests proving boundary:  application upload/delete lifecycle tests; gateway auth/multipart/delete
                         ownership tests; presentation sidecar/ref-count tests; native SDK signing tests;
                         architecture/localization/QML guards; live QStorage multipart + DeleteObject
                         acceptance before production rollout.
```

## Public deployment shape

```text
UPLOAD

Genycaster Desktop
    -> POST https://uploads.example.com/v1/uploads
    -> gateway starts provider multipart upload
    -> PUT /v1/uploads/<signed-ticket>/parts/<part-number>
       (default 8 MiB bounded parts)
    -> gateway signs each QStorage UploadPart with normal SigV4 Authorization
    -> POST /v1/uploads/complete with ordered provider ETags
    -> gateway completes + verifies object

DELETE AFTER CONFIRMED CAST REMOVAL

Genycaster Desktop
    -> DELETE https://uploads.example.com/v1/assets
       body: opaque managementToken + fresh Farcaster signer proof
    -> gateway verifies token owner FID
    -> normal SigV4 DeleteObject
    -> QStorage object is removed (404 is treated idempotently)

DOWNLOAD / CAST EMBED

https://media.example.com/m/<opaque-key>
    -> short-lived presigned GET/HEAD redirect
    -> private qStorage object today, another object store later
```

The public media URL is provider-neutral. The production QStorage bucket must be private; no bucket-wide
public ACL is required. If QConsole shows **Public Bucket** enabled, disable it after confirming the
stable `media.example.com` read path, because public-bucket access would bypass the gateway.

## Why uploads are relayed

Live production diagnostics established the following QStorage behavior with the same valid
credentials, region and bucket:

```text
normal SigV4 Authorization-header PUT   -> HTTP 200
presigned GET                            -> HTTP 200
presigned PUT (Quilibrium SDK)           -> HTTP 403 AccessDenied
presigned PUT (AWS botocore) path-style  -> HTTP 403 AccessDenied
presigned PUT (AWS botocore vhost-style) -> HTTP 403 AccessDenied
presigned PUT with explicit payload hash -> HTTP 403 AccessDenied
```

Therefore the gateway does not modify/hack the generic SigV4 presigner. QStorage writes use the
normal `Authorization`-header path that has live interoperability evidence. Reads keep direct signed
redirects because presigned GET/HEAD works.

The relay is multipart rather than whole-file buffering. Genycaster reads one part at a time; the
Node gateway verifies the Farcaster proof, buffers at most that bounded part for payload hashing, and
forwards it to QStorage. Nginx request buffering is disabled to avoid temporary-file spooling.

## Quilibrium SDK integration boundary

The media service pins Quilibrium-SDK revision
`3599f81f6284797692dedc096f6ba0f35fe317a2`. `services/media-gateway/CMakeLists.txt` fetches that
revision or accepts `GENYLEAP_QUILIBRIUM_SOURCE_DIR` for a reviewed local checkout.

`native/quilibrium_storage_addon.cpp` is the Node-API compatibility boundary. It imports
`quilibrium.core`, `quilibrium.qstorage` and `quilibrium.sigv4` and exposes only fixed QStorage
operations:

- normal SigV4 signed `CreateMultipartUpload`;
- normal SigV4 signed `UploadPart`;
- normal SigV4 signed `CompleteMultipartUpload`;
- normal SigV4 signed `AbortMultipartUpload`;
- normal SigV4 signed `DeleteObject`;
- presigned GET/HEAD for private reads and verification.

There is no JavaScript SigV4 implementation. The SDK's generic `presign_put_object()` remains
available but is not used by the QStorage production write path while live interoperability fails.

The SDK publishes C++23 BMIs. This standalone server addon is an explicit C++23 external module/BMI
compatibility exception. Genycaster desktop/core remains C++26 and does not link the SDK.

## Authorization and ticket model

`POST /v1/uploads` requires a fresh Ed25519 proof from the active Farcaster signer. The gateway
verifies the signer against Snapchain with CastAdd scope before allocating provider storage.

The returned HMAC-protected v2 ticket binds:

```text
FID
signer public key
stable object key
provider multipart upload ID
content type
exact total byte size
part size
stable public asset URL
expiration
```

Every binary part request is itself Ed25519-signed over method, exact pathname and SHA-256 of the
part body. The ticket binds it to the already-authorized user/signer without performing another
Snapchain lookup for every 8 MiB chunk. Nonces are single-use. Completion is signed and must contain
exactly one ordered provider ETag for every expected part.

After verified completion the gateway returns a separate HMAC-protected managed-media token containing
only the stable object identity and owner FID. It has no QStorage credential and is never published to
Farcaster. `DELETE /v1/assets` requires a fresh signer proof, validates that proof against Snapchain,
requires the token owner FID to match the authenticated FID, and only then issues provider DeleteObject.

## Storage migration rule

Desktop/core/QML must never contain QStorage endpoints, access keys, secret keys, provider upload
IDs, or provider-specific response DTOs. The only persisted media locator in a cast is the stable
Genyleap URL. A later R2/S3/B2 adapter may use direct presigning or another multipart mechanism
without changing the capability/domain model or old Cast URLs.
