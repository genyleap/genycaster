# Media Upload Verification — Gateway Multipart Relay

Date: 2026-08-31

## Current production contract

Genycaster media upload remains provider-neutral at the desktop/application boundary. The current
QStorage deployment uses this write path:

```text
Genycaster Qt client
    -> POST /v1/uploads
    -> bounded PUT /v1/uploads/<ticket>/parts/<n>
    -> POST /v1/uploads/complete
    -> stable https://media-farcaster.genyleap.com/m/... URL

Media Gateway
    -> QStorage CreateMultipartUpload / UploadPart / CompleteMultipartUpload
       signed with normal SigV4 Authorization headers
    -> normal SigV4 Authorization-header HEAD for completion verification

Confirmed Cast removal
    -> durable local managed-media cleanup token
    -> DELETE /v1/assets with fresh Farcaster signer proof
    -> normal SigV4 DeleteObject through the gateway

Stable media URL
    -> short-lived presigned GET/HEAD redirect to private QStorage
```

No QStorage endpoint, access key, secret key, provider upload ID, or provider-specific DTO crosses
into QML/domain/application contracts. Storage access/secret keys never ship in the desktop binary.

## Why direct presigned PUT was disabled for QStorage

Live diagnostics against the configured QStorage account established:

```text
ListBuckets with header-auth SigV4                         -> HTTP 200
normal SigV4 Authorization-header PutObject                -> HTTP 200
presigned GET                                              -> HTTP 200
Quilibrium-SDK presigned PUT                               -> HTTP 403 AccessDenied
AWS botocore presigned PUT, path-style                     -> HTTP 403 AccessDenied
AWS botocore presigned PUT, virtual-host-style             -> HTTP 403 AccessDenied
AWS botocore presigned PUT with explicit payload SHA-256   -> HTTP 403 AccessDenied
```

The same valid credentials, region (`q-world-1`) and bucket (`genycaster`) were used for the
successful header-auth PUT and failed presigned PUT probes. Therefore the production adapter does
not patch the generic SDK presigner or expose storage credentials to the desktop. QStorage writes
use the normal header-authenticated path that has live interoperability evidence.

## Source changes in this revision

### Media gateway

- `POST /v1/uploads` now starts a provider multipart upload and returns a signed v2 ticket.
- The ticket binds FID, signer key, object key, provider upload ID, exact file size, content type,
  part size, stable asset URL and expiration.
- `PUT /v1/uploads/<ticket>/parts/<partNumber>` accepts exactly one bounded binary part.
- Each part request carries a fresh Farcaster Ed25519 proof over method/path/body SHA-256.
- Part byte count is validated before forwarding to storage.
- `POST /v1/uploads/complete` accepts exactly one ordered ETag for every expected part.
- `DELETE /v1/uploads` aborts an allocated provider multipart session.
- Verified completion returns an opaque `managementToken` that binds the object key to its owner FID.
- `DELETE /v1/assets` requires a fresh signer proof, validates current signer authority and owner FID,
  and then performs idempotent provider `DeleteObject`.
- Only upload creation performs the Snapchain signer-status lookup for multipart transfer; subsequent
  part/complete/abort requests are cryptographically bound to the signer already captured in the HMAC ticket.
- A separate part-rate limiter avoids the former 60-control-request ceiling for legitimate large
  uploads while still bounding authenticated chunk traffic.
- Provider failures are logged server-side without signed URLs or credentials and surface to clients
  as gateway/storage availability failures.

### QStorage adapter / native addon

- JavaScript still contains no independent SigV4 implementation.
- The Node-API addon imports `quilibrium.sigv4` and exposes fixed normal-header signing operations for
  CreateMultipartUpload, UploadPart, CompleteMultipartUpload, AbortMultipartUpload and DeleteObject.
- Binary part bodies are passed into the SDK signer so `x-amz-content-sha256` and Authorization are
  computed from the exact forwarded bytes.
- Presigned GET/HEAD remains the read/verification mechanism.
- Generic `presign_put_object()` remains available in the pinned SDK/native boundary for provider
  portability but is not used by the QStorage live upload path.

### Qt desktop transport

- The desktop no longer consumes a provider `uploadUrl`.
- It reads the local file in the exact `partSizeBytes` returned by the gateway.
- Every chunk is PUT only to `storage-farcaster.genyleap.com` and receives a provider-neutral ETag
  token from the gateway.
- Completion sends the ordered ETag list to the gateway.
- Cancellation/failure before completion performs best-effort multipart abort.
- Upload progress remains total-file based while only one bounded part is resident in memory.
- QML and provider-neutral capability/domain/application interfaces do not acquire QStorage
  semantics. Upload completion carries only an opaque lifecycle token beside the stable media asset.
- Published Cast sidecars persist local URL-to-token associations; confirmed RemoveCast queues cleanup
  only when no other locally tracked Cast references the same token. Failed/offline cleanup stays durable
  in QSettings and is retried independently of durable write-service availability.

### Deployment

- Dockerfile is multi-stage so compiler/Git/module build trees are not retained in the runtime image.
- Compose retains `restart: unless-stopped` and publishes only `127.0.0.1:8790` on the host.
- Container Node binding example is `0.0.0.0:8790`.
- Nginx example uses a 9 MiB body limit for default 8 MiB parts and disables request buffering to
  avoid temporary-file spooling.
- `.env.example` uses the live bucket name `genycaster` and documents multipart/rate settings.

## Automated evidence executed in this environment

### Media gateway JavaScript tests

```text
cd services/media-gateway
npm test
```

Result: **15/15 PASS**.

Coverage includes authenticated multipart upload and stable read, managed-asset deletion, tampered-token
rejection, cross-FID ownership rejection, exact part sizing, abort, invalid proof rejection, nonce replay,
stable HEAD method preservation, credential isolation, SDK-signed multipart/DeleteObject adaptation,
Authorization-header HEAD verification, strict configuration parsing and provider isolation.

### Media upload architecture guard

```text
python3 tools/check_media_upload_contract.py
```

Result: **PASS**.

The guard now rejects:

- QStorage/provider semantics in desktop/core source;
- desktop consumption of provider `uploadUrl` values;
- live gateway use of direct QStorage `createUpload()` presigned PUT;
- missing multipart create/part/complete/abort/delete operations;
- a JavaScript SigV4 implementation beside the Quilibrium SDK;
- missing bounded multipart deployment configuration.

## Native addon build status in this environment

A full native addon build was attempted with:

```text
npm run build:native
```

CMake/GCC configuration started successfully, but FetchContent could not resolve `github.com` from
the container shell. The build therefore stopped before compiling the addon. This is an environment
network limitation; no native build pass is claimed here.

The source tree includes an expanded `test/native_addon.test.mjs` that verifies the multipart and
DeleteObject header-signing exports once the pinned SDK is available. The deployment Docker build must
run this test before producing the runtime image.

## Desktop build status in this environment

This container does not contain the user's Qt 6.11.1 macOS SDK/toolchain. A shipping Qt binary pass
is therefore not claimed. The desktop source change remains within the existing Qt Network adapter
boundary and keeps provider-neutral capability/application/QML APIs unchanged.

## Deployment acceptance gates

Before production acceptance, perform:

1. **Server Docker acceptance** — build the multi-stage image against the pinned Quilibrium-SDK and
   require gateway tests plus native-addon test to pass.
2. **Live QStorage lifecycle acceptance** — small real image: CreateMultipartUpload -> UploadPart(s)
   -> CompleteMultipartUpload -> Authorization-header HEAD verification -> presigned GET verification -> DeleteObject -> HEAD 404.
3. **Qt/macOS acceptance** — clean build, select image/video, publish, confirmed Cast deletion,
   durable cleanup retry, and verify the QStorage object disappears.

Do not restore direct QStorage uploads by embedding credentials in Genycaster. If QStorage later
supports interoperable presigned PUT, add that as a provider-adapter transport mode behind the same
public contracts.

## Targeted C++ media-upload test build attempted in this environment

The non-UI CMake configuration completed successfully with media upload enabled:

```text
cmake -S . -B build/multipart-verify -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFARCASTER_ENABLE_MEDIA_UPLOAD=ON \
  -DFARCASTER_BUILD_UI=OFF
```

The targeted `farcaster_media_upload_tests` build was started serially from that fresh configuration.
It progressed through the foundation/domain modules and into capability-module compilation, but did not
finish within the available sandbox execution window. No C++ semantic diagnostic was observed in that
run, and a target-build PASS is not claimed. The Qt presentation lifecycle test was not built because
this verification configuration has `FARCASTER_BUILD_UI=OFF`; the macOS Qt/Clang acceptance gate remains
required for the shipping desktop surface.
