# Hypersnap Read/Write Reliability Review

> **Current test-candidate note (2026-09-03):** the four-worker blocking read
> topology documented later in this historical review has been superseded by
> the true-async, bounded-resource architecture in
> `RESOURCE_OPTIMIZATION_TEST_CANDIDATE.md`. The root-cause analysis and
> Hypersnap endpoint/retry semantics below remain relevant; use the newer file
> for the current runtime topology and verification status.

## Feature implementation gate

```text
Responsibility: Remove request amplification and head-of-line blocking from live Hypersnap reads while preserving local-first cached state and protocol-correct writes.
Capability origin: Protocol/Derived read capabilities plus Local execution/routing health infrastructure.
Mandatory or optional: Reliable identity/profile/Following reads are mandatory for the primary client flow; Trending is optional discovery; write hardening is mandatory when writes are enabled.
Public contract: Existing provider-neutral capability ports, Application ReadService states, executor port, and protocol-write submitter contracts remain unchanged.
Canonical data/invariants: Exact capability semantics are never silently substituted; provider cursors stay provider-affine; cached canonical state remains usable through source failure; generic POST is never blindly retried.
Dependencies: Presentation → Application ReadService → routed gateway/health → Hypersnap adapter → provider-neutral HTTP transport. Protocol writes remain Application WriteService → durable outbox → Snapchain submit/reconcile adapter.
Expected failures: Timeout, transport failure, HTTP 408/429/5xx, endpoint compatibility 404/405/501, malformed payload, storage failure, authorization failure, and open capability circuit.
Local-state representation: Existing SQLite canonical read projections and durable write outbox remain authoritative local state.
Offline behavior: Saved data remains renderable; source failure is distinct from OS-level offline state; bounded retries occur only inside the total provider deadline.
Provider policy and disablement: Hypersnap remains capability-routed. Exact explicit feed endpoints are preferred; generic feed aliases are compatibility-only.
Failover semantics: Transient failure retries the same idempotent GET. Endpoint alias fallback occurs only for unsupported/not-found compatibility. Following never becomes Trending/Channels and Trending never becomes arbitrary Channels.
Tests proving the boundary: Hypersnap contract tests cover bounded GET retry, Retry-After, exact feed compatibility behavior, no transient alias fan-out, empty-feed semantics, endpoint/timeout contracts; execution test proves two live jobs can start concurrently in the bounded pool.
```

## Root cause found in alpha5

The screenshots are consistent with two source-level reliability problems rather
than a documented Hypersnap public-read rate limit:

1. `FeedSource::trending()` could execute up to five sequential 10-second feed
   requests during one outage (`/feed/trending`, generic `/feed`, `/for_you`,
   `/topic`, and a multi-channel fallback). `FeedSource::following()` could also
   substitute Trending and Channels after failure/empty results. This amplified
   load during an outage and could change feed semantics silently.
2. All live reads shared one `SerialExecutor`. Cancellation suppressed stale
   observer delivery but did not interrupt an already-blocking provider call.
   A long Trending chain could therefore delay Profile/User Posts/Search and
   make unrelated capabilities appear broken later.

The provider request client also had no bounded transient retry policy and no
`Retry-After` support. Each GET was a single transport attempt.

## Implemented read corrections

- Hypersnap GETs now use up to three attempts **inside one total deadline**.
- Retry is restricted to transport/timeout/unavailable/rate-limit failures and
  HTTP `408`, `429`, `500`, `502`, `503`, `504`.
- Numeric `Retry-After` is honored when it fits the remaining request budget;
  otherwise the current failure is returned without extending the deadline.
- HTTP response headers are preserved by Qt Network and libcurl transports.
- HTTP `400` maps to `InvalidArgument`; `405`/`501` map to `Unsupported` so
  endpoint compatibility is explicit.
- Feed alias fallback is allowed only for `NotFound`/`Unsupported`.
- Following no longer silently falls back to Trending or Channels.
- Trending no longer probes For You, Topic, or Channels after empty/transient
  failure. An empty successful Trending page remains a valid empty result.
- Following uses a 5-second total budget; Trending/channel/profile-feed reads
  use 6 seconds, matching the later project reliability contract.
- Production live refreshes use a fixed four-worker pool, while cache/session
  execution remains serialized. Capability health is internally synchronized.
- The Qt production transport is composed with HTTP/1.x forced for this path,
  matching the existing remote-image reliability policy and avoiding optional
  HTTP/2 negotiation as another edge/proxy variable.

## Write-path review

No generic POST retry was added. That is intentional: a transport retry cannot
assume an arbitrary POST is idempotent. Genycaster's Farcaster protocol writes
already use the correct separate path:

```text
WriteService
  → durable outbox + idempotency key
  → canonical protobuf codec
  → Snapchain /v1/submitMessage
  → reconcile authoritative state before retrying ambiguous operations
```

`WriteService` classifies retryable failures and schedules bounded durable
retries. Snapchain submission has its own finite 5-second transport deadline.
This is safer than hiding retries inside `IHttpTransport::post()`.

## Verification status in this container

Re-run on 2026-09-03 after the reliability changes:

- `cmake --preset provider-hypersnap`: **PASS**.
- `ninja -C build/provider-hypersnap farcaster_transport farcaster_transport_curl`: **PASS**.
- `python3 tools/check_architecture.py`: **PASS**.
- `python3 tools/check_ui_design.py`: **PASS** (98 QML files audited).
- `python3 tools/check_localization_catalog.py`: **PASS** (638 referenced IDs, 642 catalog IDs, 12 locales).
- `python3 tools/check_r7_release_contract.py`: **PASS**.
- `python3 tools/check_media_upload_contract.py`: **PASS**.
- Full provider build: **BLOCKED BY TOOLCHAIN**. GCC 14.2 ICEs while
  serializing existing high-level C++ modules at `cp/module.cc:16297`. A direct
  build of the new bounded executor also hits GCC 14.2 `ipa_comdats` ICE; the
  compiler emits no semantic source diagnostic.
- Clang fallback: **BLOCKED BY ENVIRONMENT** because the installed Clang 17
  lacks `clang-scan-deps`, which CMake requires for this module graph.
- Qt production target / qmllint / GUI smoke: **NOT VERIFIED** because Qt6 is
  unavailable in this container.
- Live `haatz.quilibrium.com` smoke: **NOT VERIFIED** because this execution
  container has no external DNS/network access.

Shipping verification still requires the supported macOS/Qt toolchain, the
linked provider contract tests, full UI target, QML lint/smoke, and a live-node
run. No release claim is inferred from the blocked layers.
