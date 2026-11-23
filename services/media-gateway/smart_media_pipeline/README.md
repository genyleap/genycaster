# Smart Media Pipeline

Smart Media Pipeline is the backend image-ingress and image-delivery engine in
`services/media-gateway`. It is not a protocol fork, browser plugin, or QML
feature. The authority boundary remains the existing signer-verified Media
Gateway and QStorage adapter.

## Primary goal: lower durable QStorage storage cost

**Auto is the default for images**, including older clients that do not send
`optimizationMode`. The gateway accepts bounded, signer-authorized multipart
chunks and temporarily stages image bytes in its own memory; it does **not**
create a QStorage multipart upload for the original image.

Once all chunks have been verified, the engine checks the actual image,
orientation, pixel count, transparency and animation, prepares a smaller
same-MIME image, and uploads **only those output bytes** to QStorage. The
published URL is the same stable `/m/v1/.../<uuid>.<extension>` previously
reserved for the upload.

The original source leaves the gateway's memory after success, abort or
expiry. No original copy, sidecar, extra thumbnail or variant is stored
durably in QStorage. This is also true for small source files whose final
output is unchanged because recompression would make them larger.

### Upload policy

| `optimizationMode` | Backend behavior | QStorage |
|---|---|---|
| absent | default `auto` (configurable rollback) | optimized |
| `auto` | max long edge 1600 px; JPEG/WebP/AVIF quality policies; lossless PNG | optimized |
| `high` | max long edge 1600 px, higher quality | optimized |
| `original` | preserve source bytes, existing QStorage multipart path | original |

The Original option is *supported by the backend but not yet surfaced in the
UI*. Video upload uses the unchanged direct-storage path. Animated GIF,
WebP and APNG are kept byte-identical when frame-safe transcoding is not
available: NEVER silently truncate a multi-frame image to its first frame.

The source and optimized outputs use the **same MIME** for compatibility with
existing native/WASM clients and third-party Farcaster/HyperSnap crawlers.
In a future version, a codec-negotiated WebP storage profile can be added
after client compatibility validation. The current version never changes the
published file extension behind a signed upload ticket.

### API compatibility

Endpoints are unchanged:

- `POST /v1/uploads` accepts optional `optimizationMode` in its signed body.
- `PUT /v1/uploads/<ticket>/parts/<n>` retains bounded signer proofs and ETags.
- `POST /v1/uploads/complete` writes the final optimized object to QStorage,
  verifies its size and content type, and returns the existing managed token.
- `DELETE /v1/uploads` discards an unfinished staged image.
- `DELETE /v1/assets` deletes the canonical final object and clears the
  in-memory variant cache.

New signed ticket version 3 identifies the staging session. Version 2,
already-issued tickets still complete against QStorage normally. Session
reservation is fixed at creation, capped at three concurrent uploads and
64 MiB combined source bytes, with a 30-minute default expiry derived from
the gateway's ticket TTL. Staging is **volatile by design**: an unfinished
upload after a gateway restart must be retried rather than being persisted
as source material on the server. A process shutdown has no QStorage
original to clean up. Successful completion retains only a tiny, expiring
receipt (not image bytes) so resending the same signed completion after a
lost HTTP acknowledgement does not write a second cloud object. A signed
abort after finalization removes the newly created canonical cloud object;
removal through the managed-token endpoint invalidates these receipts.

For compatibility with already-shipped clients, completion response
`sizeBytes` means **source bytes received**. New fields are
`sourceSizeBytes` and `storedSizeBytes` (the actual durable object size).
Updated clients use `storedSizeBytes` for display/accounting. Other
response fields (`assetUrl`, `contentType`, `managementToken`) retain
their existing meaning.

### Upload progress shown by the clients

- **Uploading:** measured multipart transfer bytes. A determinate progress bar
  is shown only while those bytes are actually being transferred.
- **Compressing and saving:** begins when the final signed request is sent
  *and* the server's upload ticket explicitly advertises `optimizationMode:
  auto|high`. The backend may be decoding, compressing or writing to QStorage;
  without a server progress endpoint the client displays an indeterminate
  indicator, **never a fabricated percentage**.
- **Finalizing:** the gateway response has been received and the client
  validates the canonical asset URL, stored object size and management token.
- **Complete:** the attached image shows the actual stored size. If the
  gateway reports `storedSizeBytes < sourceSizeBytes`, Composer also shows
  the percentage of bytes saved.

A deployed legacy/v2 gateway does not advertise optimization and therefore
skips the Compressing stage rather than claiming a compression occurred. The
future Original quality setting uses the same bypass.

### On-demand delivery

An optional variant request uses the same stable asset identifier:

```text
GET /m/v1/YYYY/MM/<uuid>.png?variant=thumbnail
GET /m/v1/YYYY/MM/<uuid>.png?variant=feed
GET /m/v1/YYYY/MM/<uuid>.png?variant=full
GET /m/v1/YYYY/MM/<uuid>.png?variant=original
```

Thumbnail = max 320 px, Feed = 960 px, Full = 1600 px.
Variant format `auto` prefers WebP (lossless for graphics/alpha);
explicit `format=jpeg|png|webp|avif` and `mode=auto|high` are bounded.
`variant=original` means the **canonical object stored in QStorage**,
not a lost, pre-optimization camera original.

Variants are generated from the already-optimized canonical image on
demand and cached briefly in a size-bounded, in-process cache. They are not
extra persistent objects. Normal `/m/...` GET requests have unchanged byte
and MIME semantics. No arbitrary remote image URLs or image dimensions can
enter this transform path.

### Security and failure behavior

- All writes retain the existing Ed25519 proof verification and replay
  protection; authorization is revalidated for each part and completion.
- Fixed limits: source <=25 MiB, decode <=32 million pixels, at most 300
  animated pages, output <=8 MiB for optional public delivery variants.
- CPU-heavy transforms are serialized and queued with a small bounded
  backlog. MIME is checked against actual bytes, not just an extension.
- Invalid input is rejected BEFORE durable QStorage upload. If the codec
  is unavailable, Auto/High **fail closed** rather than silently storing a
  large original.
- An unchanged or larger re-encoding is discarded in favor of the original
  *only for that image*. This prevents increased storage cost.
- No arbitrary URLs, user paths, signer keys, uploaded source image bodies
  or provider credentials are recorded in logs.
- Temporary buffering is process-bound, and each upload slot reserves
  its total declared bytes to keep memory predictable.
- Optimization metrics can be aggregated from response
  `sourceSizeBytes` and `storedSizeBytes` without logging individual
  media contents or FIDs.

### Development checks

```sh
npm ci --prefix services/media-gateway
node --test services/media-gateway/test/smart_media_pipeline.test.mjs
node --test services/media-gateway/test/gateway.test.mjs
node --test tests/wasm/browser_media_upload.test.mjs
```

No Qt, Android, desktop or WebAssembly build is required for backend unit
and integration testing. Production deployment is a separate release gate.
