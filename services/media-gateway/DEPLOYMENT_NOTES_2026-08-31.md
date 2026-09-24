# Media Gateway deployment notes — 2026-08-31

This source tree consolidates the production fixes validated against live QStorage.

## Fixed invariants

- Multipart upload writes use normal Authorization-header SigV4.
- Completion verification uses a presigned GET whose canonical payload is `SHA256("")`.
  QStorage returned HTTP 403 for the otherwise-standard `UNSIGNED-PAYLOAD` form in live tests.
- Public media GETs redirect to the same QStorage-compatible hashed-payload presigned GET form.
- Public media HEADs are answered by the gateway from authenticated GET metadata, avoiding QStorage HEAD interoperability problems.
- Managed-media deletion reaches `DELETE /v1/assets`, validates the management token / signer proof,
  and issues QStorage `DeleteObject`.
- Clean native builds export `signDeleteObject`. The JavaScript adapter retains an Authorization-header
  SigV4 fallback for rolling deployments where an older already-built addon is still running.
- Removing a managed-media block in the desktop composer queues cleanup immediately only after the
  final local draft reference is removed and never deletes an asset still referenced by a published cast.

## Clean production deployment

From `services/media-gateway`:

```sh
cp .env.example .env   # first deployment only; then fill server-only secrets
chmod 600 .env
docker compose build media-gateway
docker compose up -d --force-recreate media-gateway
node --check storage/quilibrium_sdk.mjs
```

Do not copy a stale `.node` addon from an old container into a new deployment. The Dockerfile builds
`genyleap_quilibrium_storage.node` from this source tree and runs the gateway, adapter, and native-addon tests during the builder stage.

## Live acceptance smoke test

After deployment, upload a fresh media object from Genycaster, confirm completion returns HTTP 200,
then remove it from the composer and confirm the object is no longer retrievable from QStorage.
