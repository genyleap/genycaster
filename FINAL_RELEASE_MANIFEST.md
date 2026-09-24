# Genycaster consolidated source — 2026-08-31

This tree consolidates the production fixes validated during the QStorage media lifecycle rollout.

## Included fixes

1. QStorage multipart uploads remain server-relayed and Authorization-header SigV4 signed.
2. Upload completion verification uses a QStorage-compatible presigned GET with `SHA256("")`; it does not use `UNSIGNED-PAYLOAD` or QStorage HEAD.
3. Stable media GET redirects use the same compatible read signing.
4. Stable media HEAD responses are synthesized by the gateway from authenticated GET metadata.
5. QStorage `DeleteObject` uses the native addon on clean builds and has a JS Authorization-header SigV4 compatibility fallback for stale rolling-deployment addons.
6. Composer managed-media removal releases the management token, queues cleanup immediately after the last draft reference disappears, and protects assets still referenced by published casts.
7. Cleanup intent is persisted before asynchronous deletion so a restart cannot restore a media URL that was already scheduled for removal.
8. Docker builds compile the native addon from this tree and execute gateway, adapter, and native-addon tests in the builder stage.

## Verification performed while packaging

```text
node --check storage/quilibrium_sdk.mjs   PASS
node --check gateway.mjs                  PASS
node --test gateway + adapter             16/16 PASS
```

The Qt/C++ client lifecycle regression tests are included in `tests/unit/media_lifecycle_presentation_tests.cpp`; the packaging environment does not provide the project's Qt 6 development toolchain, so those binaries were not rebuilt here.

## Recommended media-gateway deployment

Keep the existing production `.env` outside source archives, then from `services/media-gateway` run:

```sh
./deploy.sh
```

The script validates required configuration, checks JS syntax, runs non-native Node tests, builds the Docker image (which also compiles/tests the native addon), recreates the service, and performs an in-container health check.
