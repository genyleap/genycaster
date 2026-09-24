# Genycaster Phase 5F Source-Hardening Delivery

Checkpoint: Phase 5F — Write Hardening
Date: 2026-08-29

Implemented in source:

- event-driven durable-outbox recovery from network-online and signer-ready
  transitions, with a 60-second safety fallback;
- concurrent `WriteService::resume()` wakeup coalescing;
- reconcile-before-resubmit handling for transient/timeout ambiguity;
- durable Conflict reconciliation-only mode that never blindly POSTs again;
- final authoritative reconciliation before maximum-attempt failure;
- SQLite migration v15 for account-scoped, restart-safe Follow/Unfollow desired
  state, complementing v13 Cast-removal and v14 Like/Recast projections;
- refresh/restart overlay of durable Follow state in Qt presentation;
- terminal invalidation of all queued writes on authoritative signer expiry or
  revocation, including future-backoff records;
- terminal rejection of queued envelopes sealed by a replaced signer key;
- authoritative signer restore winning over optimistic sliding-TTL checkpoints;
- operation-sensitive reconciliation where absent `RemoveCast` confirms deletion;
- Phase 5F architecture/review/verification documentation.

Verification in the delivery environment:

- clean Linux/GCC 14.2 Phase 5F configure: PASS;
- architecture guard: PASS;
- `farcaster_write_storage_tests`: BUILD/RUN PASS;
- v15 Follow/Unfollow migration/durability/account-isolation tests: PASS;
- `farcaster_protocol_write_contract_tests`: BUILD/RUN PASS.

Environment limitations:

- GCC 14.2 C++ Modules ICEs while serializing several higher-level exported
  module interfaces (`cp/module.cc:16297`), before useful semantic diagnostics;
- Clang 17 is present but the runner lacks `clang-scan-deps`, so it cannot be
  used as a module-build fallback;
- Qt6/qmllint are unavailable in this Linux container;
- macOS Clang 22 + Qt 6.11.x build/lint/smoke, deployed App-FID acceptance, all
  live writes and ambiguity/offline/signer fault injection remain release gates.

Phase 5F is therefore **source-hardening complete, release acceptance pending**.
Do not advance to Phase 6 until the closure matrix in
`PHASE_5F_VERIFICATION.md` passes.

See `docs/39-phase5f-write-hardening.md`, `PHASE_5F_REVIEW.md`, and
`PHASE_5F_VERIFICATION.md`.

## Profile/read acceptance support added after Phase 5F hardening

To make final platform acceptance materially testable, the delivery now also
includes the provider-neutral Profile read expansion documented in
`docs/40-profile-read-expansion.md`:

- Profile Posts, Replies and Popular feeds with independent local-first cache
  keys and opaque continuation pagination;
- author-profile navigation from Cast and Followers/Following rows, plus cast-detail parent traversal;
- profile/social target isolation from the Home viewing identity;
- Follow/Unfollow plus normal reply/reaction/delete controls from profile rows;
- `hasMore` pagination state independent of `busy`, deferred QML load-more
  dispatch and constant footer geometry, removing the reported `canLoadMore`
  binding graph;
- 5-second Following and 6-second derived/profile read budgets;
- non-warning Saved degradation when cached rows remain usable;
- a 35-second reachable-network source-recovery probe aligned behind the
  30-second capability-health cooldown.

Additional container evidence:

- provider-enabled UI-disabled clean configure: PASS;
- architecture guard: PASS;
- changed Hypersnap feed/registration objects: BUILD PASS;
- changed Hypersnap contract-test translation unit: BUILD PASS;
- no `canLoadMore` reference remains in source/QML/tests.

Qt/qmllint/runtime acceptance of the exact binding-loop regression and the new
Profile interaction surface remains **NOT VERIFIED** in this container because
Qt6 is unavailable. The full read-pipeline executable is additionally blocked
by the existing GCC 14.2 C++ Modules serializer ICE. These are explicit macOS
closure gates, not inferred passes.

## Profile fidelity/media finalization

The final Profile-read candidate additionally includes `docs/41-profile-fidelity-media.md`:

- canonical public product label `Genycaster`; protocol App FID `866408` is the
  dedicated shipping identity and still requires the real custody-backed
  signer-attestation service before writes are enabled;
- Profile PFP/banner/URL/location/social-count persistence plus protocol-native
  UserData fallback;
- one-time migration v19 clearing polluted historical social-count snapshots;
- feed/social author summaries can no longer overwrite dedicated Profile detail;
- asynchronous C++ `image://remote` media loading for PFP, cover and image embeds;
- Posts / Quotes / Replies / Likes / Popular are semantically separate;
- provider `replies_and_recasts` results are filtered so Replies contains only
  parented Casts;
- authenticated header removes the duplicate adjacent identity control;
- reaction-overlay Cast identifiers are canonicalized across refresh.

Container re-check: architecture guard PASS, `git diff --check` PASS, clean
UI-disabled configure PASS, Hypersnap codec/feed/profile implementation objects
BUILD PASS, and the updated Hypersnap contract-test translation unit BUILD PASS.
GCC 14.2 again ICEs in unchanged/higher-level C++ module serialization before
the full linked test matrix can run; Qt runtime validation remains a macOS gate.


## Provider-neutral media upload finalization

The delivery now includes the complete source implementation for local Composer media upload behind a storage-neutral Genyleap gateway:

- native image/video selection through Qt `FileDialog`, with image preview, upload progress, cancel, retry, and media-only Cast support;
- a C++26 module path from `domain.media` through provider-neutral capability/application boundaries into the Qt Network transport;
- a Qt MOC-only presentation controller; QML contains no storage/network credentials or provider I/O;
- stable upload/control origin `storage-farcaster.genyleap.com` and stable Cast asset origin `media-farcaster.genyleap.com`;
- private qStorage objects with bounded gateway-relayed multipart writes using normal SigV4 Authorization headers, plus short-lived signed GET/HEAD redirects for delivery;
- a provider-neutral storage adapter backed by the pinned Quilibrium SDK: normal SigV4 header signing for multipart writes and presigned GET/HEAD for private reads;
- signer-backed media allocation authorization, nonce replay protection, bounded JSON bodies, per-client rate limiting, upload-ticket HMAC integrity, completion `HEAD` verification, and bounded Qt response handling;
- Docker Compose and nginx deployment examples, plus a credential-safe `.env.example`;
- provider-neutral media architecture guard and deterministic SigV4 regression vectors independently cross-checked against botocore;
- cross-platform preservation of Unicode local filesystem paths at the Qt/filesystem boundary.

Final container evidence on 2026-08-30:

- architecture guard: PASS;
- UI design guard: PASS (55 QML files);
- localization guard: PASS (493 referenced IDs, 494 catalog IDs, 12 locales);
- media-upload contract guard: PASS;
- R6 release contract: PASS;
- R7 release contract: PASS;
- media gateway Node tests: 13/13 PASS;
- Node and Python syntax checks: PASS;
- `core-debug` CMake configure with `FARCASTER_ENABLE_MEDIA_UPLOAD=ON`: PASS;
- registered CTest architecture/localization/media guards: 3/3 PASS.

The GCC 14.2 target build remains blocked by the toolchain's C++ Modules serializer ICE at `cp/module.cc:16297` while compiling `farcaster.application.media.upload`; it emits no semantic source diagnostic. The available Clang runner lacks `clang-scan-deps`, and Qt6 is not installed in this container. Therefore the exact Qt runtime/build acceptance and one real qStorage multipart create/part/complete/HEAD/GET smoke test remains an explicit deployment gate rather than inferred passes. See `MEDIA_UPLOAD_VERIFICATION.md`.
