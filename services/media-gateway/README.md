# Genycaster Media Gateway

Provider-neutral upload authorization and stable media-delivery boundary for GenyCaster.

This GenyCaster-owned gateway also exposes bounded browser authentication flows
that verify linked identity via the separate [OpenProof `opp` service](https://github.com/genyleap/openproof/tree/main/apps/opp),
configured at `https://openproof.genyleap.com`. The gateway does **not** own
OpenProof accounts or its PostgreSQL database. The GenyCaster account API and
its independent PostgreSQL database live under `services/account-sync/`.
See [OpenProof and service boundaries](../../docs/OPENPROOF_AND_SERVICES.md).

**Example addresses:** `uploads.example.com` and `media.example.com` are placeholders; configure your own deployment origins.

The desktop client never receives reusable qStorage/S3/R2 credentials and never stores a provider
URL in a Farcaster cast. It talks only to:

- `https://uploads.example.com` for authenticated upload control and bounded multipart chunks;
- `https://media.example.com` for stable public asset URLs.

Production objects are expected to remain private. If QConsole shows **Public Bucket** enabled, disable
it after confirming the stable `media.example.com` read path. Playback/download uses a
short-lived QStorage-compatible presigned GET redirect. Upload writes use normal SigV4 `Authorization` headers behind the gateway because live
QStorage interoperability tests accept header-authenticated writes and hashed-payload presigned GETs but reject `UNSIGNED-PAYLOAD` read presigns and
presigned PUTs with `403 AccessDenied` (including AWS botocore reference presigning).

## Upload architecture

```text
Genycaster Desktop
        |
        | POST /v1/uploads (signed Farcaster proof)
        v
Media Gateway
        |
        | CreateMultipartUpload (normal SigV4 Authorization)
        v
QStorage

Genycaster Desktop
        |
        | PUT /v1/uploads/<ticket>/parts/<n>
        | one bounded chunk at a time
        v
Media Gateway
        |
        | UploadPart (normal SigV4 Authorization)
        v
QStorage

Genycaster Desktop
        |
        | POST /v1/uploads/complete + ordered ETags
        v
Media Gateway
        |
        | CompleteMultipartUpload -> hashed-payload presigned GET verification
        v
QStorage
```

The default part size is 8 MiB. Nginx request buffering is disabled, and the Node gateway buffers at
most one bounded part because normal SigV4 signing requires the payload hash before forwarding the
part to QStorage. Whole media files are never buffered in process memory or persisted on the VPS by
the application.

Cancellation uses `DELETE /v1/uploads` to abort the provider multipart session. The HMAC-protected
upload ticket contains the opaque provider upload ID, stable object key, exact expected file size,
part size, FID and signer key. Part requests are Ed25519-signed and bound to the exact binary body.

## Managed asset deletion

Successful completion returns an opaque `managementToken` beside the stable public asset URL. The
desktop persists that token only in local lifecycle metadata; it is never embedded in a Cast. After a
Cast removal is confirmed, Genycaster calls `DELETE /v1/assets` with a fresh Farcaster signer proof and
the opaque token. The gateway verifies signer state and owner FID before issuing normal SigV4
`DeleteObject` to QStorage. Missing provider objects are treated as already-deleted success.

The gateway does not expose the QStorage object key as deletion authority. Reusing a stable URL does
not grant delete permission.

## Read architecture

```text
Published cast embed
        |
        v
https://media.example.com/m/<opaque-key>
        |
        | 307 short-lived signed redirect
        v
private QStorage object
```

The stable public URL is storage-provider-neutral. Existing casts therefore survive a future
QStorage -> R2/S3/B2 migration.

## Storage boundary

The QStorage adapter keeps normal write signing behind a small Node-API addon that imports the
pinned Quilibrium C++ SDK. Multipart writes and normal object deletion use `sigv4_signer::sign()`.
For QStorage reads, the gateway deliberately uses a small compatibility presigner in JavaScript
because live QStorage rejects the SDK's default `UNSIGNED-PAYLOAD` query-auth form; the accepted
canonical payload is `SHA256("")`. Completion verification uses the same authenticated GET and
cancels the response body immediately after metadata arrives. `DeleteObject` also has an
Authorization-header JavaScript fallback so a rolling deployment with an older native addon cannot
turn client cleanup into HTTP 500; clean image rebuilds use the native `signDeleteObject` export.

`presign_put_object()` remains available as a generic S3-compatible SDK capability but is not used
for the live QStorage upload path because both SDK and botocore presigned PUT probes currently receive
`403 AccessDenied` from QStorage.

The SDK revision is pinned in `CMakeLists.txt`. The standalone Node addon is an explicit C++23 BMI
compatibility boundary; Genycaster desktop/core remains C++26.

## Server configuration

Copy `.env.example` to `.env` and keep it mode `0600`. At minimum configure:

```text
GENYLEAP_MEDIA_HOST=0.0.0.0
GENYLEAP_MEDIA_TICKET_SECRET=<server-only random secret>
GENYLEAP_MEDIA_STORAGE_PROVIDER=quilibrium-qstorage
GENYLEAP_MEDIA_STORAGE_ENDPOINT=https://qstorage.quilibrium.com
GENYLEAP_MEDIA_STORAGE_REGION=q-world-1
GENYLEAP_MEDIA_STORAGE_BUCKET=genycaster
GENYLEAP_MEDIA_STORAGE_ACCESS_KEY_ID=<server-only access key>
GENYLEAP_MEDIA_STORAGE_SECRET_ACCESS_KEY=<server-only secret key>
GENYLEAP_MEDIA_MULTIPART_PART_BYTES=8388608
```

Never place QStorage credentials in QML, CMake desktop settings, a desktop `.env`, or a shipped
binary.

## Build and run

Docker Compose is the production path:

```sh
docker compose up -d --build
```

The Dockerfile is multi-stage: CMake/GCC/Git and module BMIs stay in the builder image; the runtime
image receives only Node, required runtime libraries, gateway sources and the compiled addon.

Local native build:

```sh
npm run build:native
npm test
npm run test:native
```

Health check:

```sh
curl --fail https://uploads.example.com/healthz
```

## Reverse proxy

`deploy/nginx.conf.example` uses a 9 MiB body ceiling for the default 8 MiB parts and disables
`proxy_request_buffering` so Nginx does not spool chunks to temporary files. The Docker port remains
bound to `127.0.0.1:8790` on the host while Node listens on `0.0.0.0:8790` inside the container.

With Cloudflare proxying enabled, use Full (strict) TLS.

## Verification

Fast tests:

```sh
npm test
```

Native boundary after compiling the addon:

```sh
npm run test:native
```

Production acceptance requires one real QStorage multipart smoke test:

1. create a small image upload ticket;
2. confirm `CreateMultipartUpload` succeeds;
3. upload every part through the gateway and capture provider ETags;
4. complete the multipart upload;
5. confirm hashed-payload presigned GET verification matches exact byte size/content type;
6. open the stable `media.example.com/m/...` URL and confirm the signed GET redirect works;
7. delete the diagnostic object through normal SigV4 `DeleteObject` and confirm authenticated GET verification returns 404;
8. publish/delete a test Cast and confirm only the stable Genyleap URL is public while cleanup uses the local management token.

If QStorage later supports interoperable presigned PUTs, a direct-provider upload mode may be added
behind the same provider-neutral HTTP and C++ capability boundaries. Do not expose storage secrets to
restore direct uploads.
