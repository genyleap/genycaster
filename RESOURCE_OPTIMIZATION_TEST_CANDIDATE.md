# Genycaster Alpha5 Resource-Optimized Test Candidate

Date: 2026-09-03

## Scope

This candidate completes the alpha5 read/media resource optimization pass. The
primary Hypersnap read path is asynchronous end to end from the application read
service through the provider adapter and the Qt Network reactor. Network waiting
does not consume application worker threads. CPU-heavy JSON and image decoding
are separately bounded, persistence remains serialized, and QML feed work has
been reduced without changing product semantics.

## Runtime topology

```text
Qt/QML
  -> AsyncReadService
      -> cache / SQLite serial lane
      -> AsyncRoutedReadGateway
          -> capability router + per-capability circuit breaker
              -> async Hypersnap capability adapters
                  -> primary Qt Network reactor
                  -> bounded in-flight / per-origin admission
                  -> timer-driven timeout and retry
                  -> QNetworkReply cancellation
              -> weighted JSON decode pool
  -> QQuickAsyncImageProvider
      -> isolated bounded image-traffic reactor
      -> two-worker weighted image decode pool
```

Protocol writes, signer provisioning, and compatibility authentication retain
their own explicit semantics. Generic retry is not applied to arbitrary writes.

## Resource envelope

Primary async network reactor defaults:

- 96 active requests globally.
- 32 active requests per origin.
- 64 non-interactive requests globally and 24 per origin, preserving headroom
  for interactive and visible work.
- 320 accepted active/queued requests.
- 64 MiB accepted request-body memory.
- 96 MiB buffered response memory.
- 256 KiB QNetworkReply read buffer per request.

Hypersnap JSON decode:

- 2-4 workers depending on hardware.
- 48 queued decode jobs maximum.
- 64 MiB retained queued response data maximum.

Remote image path:

- QQuickAsyncImageProvider; no QQuickPixmapReader network wait.
- 32 active image-network requests, 12 per origin.
- 96 accepted image jobs.
- 48 MiB image-network response budget.
- 12 MiB maximum encoded image body.
- 2 image decode workers, 24 queued decode jobs and 32 MiB queued retained data.
- 48 MiB decoded CPU image cache.
- 20M decoded-pixel safety ceiling and 96 MiB QImageReader allocation limit.
- 256 MiB bounded disk image cache with maintenance outside the UI path.

## Major corrections

- Removed nested `QEventLoop::exec()` from the primary async HTTP path.
- Added real cancellation down to `QNetworkReply::abort()`.
- Added shared-deadline, timer-driven retry with `Retry-After` handling for
  transient idempotent reads.
- Preserved HTTP status semantics instead of collapsing HTTP 4xx/5xx into a
  generic transport failure.
- Added bounded global/per-origin network admission and byte budgets.
- Preserved capability routing, recovery-probe admission, per-capability health
  observation and circuit breaking around the new asynchronous provider path.
- Added byte-weighted decode admission so async I/O cannot create an unbounded
  JSON memory queue.
- Changed User Likes hydration from one cast request per reaction to Hypersnap
  bulk-cast lookup.
- Changed viewer interaction hydration to batch cast interactions rather than
  reading up to 10,000 likes and 10,000 recasts for a feed.
- Kept compatibility endpoint fallback restricted to explicit unsupported/not
  found cases; transient failure does not fan out across unrelated endpoints.
- Made feed/social SQLite append incremental with identity indexes instead of
  deleting and rewriting the entire stored list on every page.
- Replaced repeated linear cast/profile scans in feed presentation with indexed
  lookup.
- Removed `JSON.stringify`-based feed row revision work from QML and throttled
  scroll bookkeeping.
- Replaced the remote image provider's blocking pixmap-loader path and private
  media network threads with QQuickAsyncImageProvider plus the async reactor.
- Moved image disk-cache maintenance outside startup/UI execution.
- Routed media upload HTTP through the shared async network transport while
  preserving the bounded dedicated media write executor and write-specific
  retry rules.
- Bounded SerialExecutor, ThreadPoolExecutor, link-preview work, and other
  queues changed by this pass.

## Verification collected in this environment

PASS:

- `python3 tools/check_architecture.py .`
- `python3 tools/check_async_resource_contract.py .`
- `python3 tools/check_media_upload_contract.py .`
- `python3 tools/check_ui_design.py .` (98 QML files)
- `python3 tools/check_localization_catalog.py .` (638 references, 642 IDs,
  12 locales)
- `python3 tools/check_r5_release_contract.py .`
- `python3 tools/check_r7_release_contract.py .`
- `git diff --check`
- Clean CMake configure/generate with GCC 14.2, Ninja, UI disabled and tests
  enabled.
- `farcaster_storage_sqlite_boundary` compile and link.
- Media gateway JavaScript tests: 16/16 PASS.

BLOCKED BY TOOLCHAIN:

- Full C++ module build on the available GCC 14.2. The compiler ICEs in its
  module/IPA implementation (`ipa_comdats` / `cp/module.cc`) while compiling
  project module implementation units. The same class of ICE is reproducible on
  existing unchanged module code, and no semantic source diagnostic is emitted.

NOT VERIFIED IN THIS CONTAINER:

- Qt6 GUI compile/link, strict qmllint, runtime smoke, and macOS Instruments
  profiling because Qt6/macOS are not available here.
- Live Hypersnap/QStorage network smoke because this execution environment does
  not provide the required external runtime environment.

This archive is therefore a **test candidate**, not a claim that the graphical
release has passed the project's macOS/Qt acceptance gate. The next useful
verification is to build it with the project's supported macOS Qt toolchain and
measure CPU, memory, network, frame time, and cancellation behavior with
Instruments.

## macOS Clang build correction R2

The first test-candidate archive declared `UploadProgress` after `Request` in
`src/transport/async_http.cppm`, while `Request` used that alias as a member
type. Clang correctly rejected the interface with `unknown type name
'UploadProgress'`. R2 moves the alias before `Request`; no API semantics change.
The corrected `farcaster_transport` target was then compiled and linked
successfully with GCC 14.2 in the available Linux verification environment.
