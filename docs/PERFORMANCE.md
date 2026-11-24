# Performance and benchmarking

GenyCaster is optimized for responsiveness and predictable resource use, but **design goals are not benchmark results**. A credible public comparison requires recordings from the same conditions and a reproducible workload. This page defines what to measure, how to report it, and what claims are permitted.

## Current evidence status

| Claim | Evidence |
|---|---|
| A shared native C++26 application core exists | Source modules and CMake targets in `src/` |
| Local-first caches and durable write state exist | `src/storage/`, `src/application/` and contract tests |
| Feed model reuse and scroll stabilization have regression coverage | `tests/wasm/` and Qt UI tests |
| GenyCaster starts faster than a named competitor | **Not yet established** |
| GenyCaster uses less RAM than a named competitor | **Not yet established** |
| GenyCaster renders more frames per second than a named competitor | **Not yet established** |

Passing tests or compiling with an optimizing compiler is not a substitute for measuring end-user latency. No speedup, FPS, battery-life or memory superiority number may be added to the README until the dataset and procedure below are published.

## The benchmark suite

| ID | Workload | Reported metric | Measurement boundary |
|---|---|---|---|
| P01 | Cold launch, no cache | Time to interactive shell; time to usable first feed | Process start → first usable UI / first feed |
| P02 | Warm launch with cached feed | Time to cached first cast | Process start → first fully rendered cached cast |
| P03 | Feed scroll through 100 comparable casts | Frame time p50, p95, p99; dropped-frame share | Continuous gesture → end of sample |
| P04 | Media-heavy timeline | Time to first visible image; decode stalls | Viewport admission → decoded displayed content |
| P05 | Idle and steady-state timeline | RSS / PSS / private footprint; GPU memory if supported | After consistent warmup and stable idle interval |
| P06 | Feed refresh and pagination | User-visible latency and duplicates/viewport movement | Refresh request → reconciled visible content |
| P07 | Switching feed → cast → back | Return latency and cast/offset error | Navigation input → stable restored viewport |
| P08 | Browser WASM first visit | Transfer bytes, download time, compile/start time | HTTP navigation → interactive Qt canvas |

Define `time to interactive` and `first cast` separately. A shell that draws a placeholder is not a loaded feed. Similarly, a cached cast must not be counted as a fresh network fetch.

## Controlled workload

1. **Same machine:** record CPU, GPU, RAM, OS, power mode and display refresh rate.
2. **Same test account/feed:** capture a deterministic set of public casts with similar text, embeds, image sizes and order. Never redistribute private account data.
3. **Same cache condition:** measure a genuinely cold install, a process-cold cached launch, and a same-session warm navigation separately.
4. **Same network:** record latency/bandwidth, connection mode, DNS/cache and whether reads use the same underlying protocol node.
5. **Same run boundaries:** define the click/launch timestamp and the event that ends each sample; do not mix different notions of loaded.
6. **Same capture method:** use one frame-time/memory measurement technique for both clients where possible and disclose discrepancies.
7. **Repeat independently:** at least 10 timed runs per case, discard warmup only under a documented predeclared rule; report distributions rather than cherry-picked best runs.
8. **Separate targets:** desktop/native, Android/iOS and WebAssembly are **different benchmark categories**; do not claim one native result proves browser performance.
9. **Report failures:** crashes, timeouts, rate limits, missing features and incomplete renders remain in the sample log, not silently excluded.
10. **Pin software:** record GenyCaster commit/tag, Qt version, release/debug configuration, competing app version, driver and browser.

An improvement caused by an empty feed, missing media, suppressed animation or disabled feature is not a comparable performance win.

## Measurement guidance

### macOS

Prefer Instruments (Time Profiler, Allocations, Metal System Trace/Core Animation as appropriate) and OS process metrics. Record whether GenyCaster is using Metal or a fallback. Obtain startup event timestamps from instrumented app events, not a stopwatch alone.

### Windows

Use Windows Performance Recorder/Analyzer, ETW and GPUView/PresentMon where appropriate. Report backend (for example D3D11), process working set and the chosen frame metric.

### Linux

Use `perf`/system process accounting, compositor-appropriate frame tracing and GPU telemetry where available. Record the actual graphics API and desktop session type (X11/Wayland); background systems and GPU memory accounting vary.

### Android and iOS

Use Android Perfetto/FrameTimeline and Android Studio Profiler or Xcode Instruments on a real device. Keep thermal state and refresh rate explicit. Emulator results must never be presented as native-device results.

### WebAssembly

Use the browser Performance panel, `PerformanceObserver`/resource timing where appropriate, network request data and browser memory tools. Record browser + version, WebGL availability, WASM cache state and Service Worker state; cold download and post-cache startup are different experiments.

## Comparable products

A comparison is meaningful only if both implementations provide the tested feature with substantially equivalent content and UX. Do not label a competitor "slower" without recording which version, runtime, cache state and media implementation was used.

Publish a result table with a separate column for each tested client and append the full evidence links. Until then, the README intentionally says **not measured**.

### Result template

| Date | Device / OS | Browser or graphics backend | Workload ID | Client + exact version | Samples | p50 | p95 | p99 | Unit | Evidence |
|---|---|---|---|---|---:|---:|---:|---:|---|---|
| _Pending_ | — | — | P01–P08 | GenyCaster / reference client | — | — | — | — | ms / MiB | — |

Include raw per-run records and a clear explanation for any excluded sample. For screenshots/traces, redact personal information and access tokens.

## Acceptance rules for published claims

A comparison can be promoted to the README only when:

- the methodology and workload are published;
- exact build identifiers and devices are recorded;
- raw results or verifiable traces are available for inspection;
- the compared versions are both reasonably current and configured for feature parity;
- the summary distinguishes statistically meaningful improvement from run-to-run noise;
- platform and security caveats are stated next to the result.

Performance regression tests, profiler captures and source-level optimizations are valuable engineering work even before there is a competitive leaderboard. A fair benchmark is a measurement of the *whole user journey*, not just the fastest C++ function.

## Engineering audit — 9 November 2026 (baseline `c8100e51`)

This is a **bounded, evidence-based investigation**, not a claim that every
subsystem has been profiled. The full application and its user-visible FPS,
GPU workload, mobile energy consumption and real protocol latency were **not**
measured by these model tests. The local source was fast-forwarded from
`c7c63fc4` to `c8100e51` before taking the baseline. The performance
comparison uses an independently compiled copy of the *unaltered*
`c8100e51:ui/app/social/feed_rows.cpp`, not retrospectively inferred results.

### A. Architecture and measurement boundaries

The inspected read path is:

```text
Eligible HyperSnap/Snapchain/compatible provider
    → transport adapters (Qt network; protocol-specific parsing)
    → canonical application/routing and local-first storage/synchronization
    → PresentationBridge / runtime data bindings
    → FeedSurface or ProfileSurface (stable-key payload store)
    → FeedRowsModel (primitive-role Qt model)
    → MasonryWindowModel (native geometry and bounded viewport model)
      OR Timeline ListView (recycled delegates)
    → CastItem / media and interaction QML components
    → Qt Quick scene graph / Qt RHI → platform graphics API → GPU
```

Relevant source anchors: `src/transport/qt_network/async_http.cpp`,
`src/routing/`, `src/application/`, `src/storage/`,
`src/presentation/qt/presentation_bridge.cpp`,
`ui/app/runtime/bindings.cpp`,
`ui/components/feed/FeedSurface.qml`,
`ui/components/profile/ProfileSurface.qml`,
`ui/app/social/feed_rows.cpp`,
`ui/components/feed/FeedMasonryView.qml`, and
`ui/components/media/GalleryLayout.qml`.

For writes, authentication and signing remain separate, capability-specific
operations. The investigation did **not** modify provider choice, signers,
credentials, HTTP semantics, private data, or cache authorization.

The isolated measurement environment was a **one-vCPU x86-64 Linux VPS**
(Linux 6.8, approximately 4.1 GB RAM), Qt **6.12.0**, a native Release
CMake/Ninja build using the installed GCC toolchain, and `QElapsedTimer`.
The benchmark isolates CPU-side Qt model operations, not an event-driven
full application frame. Its model comparison uses equal source rows, fixed
identities, fixed column widths, and matched counts, with five warmup cycles,
45 recorded samples for 1,500 rows and 30 for 4,000 rows. Samples are sorted
for nearest-rank percentiles. Test inputs use generated public-free fixture
identities and contain no real account data.

The reproducible benchmark lives at
`tests/ui/feed_performance_benchmark.cpp`, in the optional
`farcaster_feed_performance_benchmark` target. An independent baseline
binary was built from the exact old translation unit outside the source tree,
then tested back-to-back with the modified binary.

### B. Model baseline and optimized results

The paired samples below were recorded from the original matched
**C++20 smoke-target binaries** (both baseline and optimized used the same
compiler, Qt 6.12 and Release flags), before the benchmark fixture was moved
into the C++26 project target. These values are **one controlled run**, not a
confidence interval; the one-vCPU server exhibited noticeable high-percentile
interference between independent runs. Units are milliseconds; smaller values
are better. The checked-in optional target now explicitly requires C++26.

| Native operation | Rows | Baseline p50 | Updated p50 | Delta (p50) | Verification |
|---|---:|---:|---:|---:|---|
| Reconcile unchanged snapshot | 1,500 | 0.7702 | 0.5038 | −34.6% | Two separate Release binaries, 45 samples each |
| Reconcile unchanged snapshot | 4,000 | 2.3499 | 1.6410 | −30.2% | Two separate Release binaries, 30 samples each |
| Reconcile 90-row order change | 1,500 | 0.9397 | 0.7962 | −15.3% | Same generated rows and benchmark harness |
| Reconcile 90-row order change | 4,000 | 2.9242 | 2.6208 | −10.4% | Same generated rows and benchmark harness |
| Full native Magazine relayout | 4,000 | 2.2723 | 2.3390 | +2.9% | Not a target of the optimization; within observed noise |
| Native viewport admission | 4,000 | 0.0209 | 0.0204 | −2.4% | Not a target of the optimization; within observed noise |

Previous paired repeats showed unchanged-snapshot p50 for 4,000 rows:
baseline **2.3479/2.4096 ms** and optimized **2.1318/1.9258 ms**, before
the additional preallocation change. A subsequent, idle-CPU repeat after
finishing the C++26 benchmark refactor measured **2.2643 → 1.4603 ms**
(4,000 unchanged rows, p50) and **0.7429 → 0.4754 ms** (1,500 rows, p50).
Thus the stable-order optimization helps the isolated model workload, but
the exact reduction depends on server noise. For the 4,000-row earlier
paired run, p95 of unchanged
reconciliation was **6.1180 ms baseline** versus **2.1365 ms updated**;
other earlier paired runs produced larger variance, so no robust tail
latency improvement is claimed yet.

**Unavailable / not measured:** actual cold startup, warm first cast,
real user gesture-to-frame latency, real feed P95/P99 frame times,
application CPU utilization, idle wakeups, RSS/heap growth, GPU time,
texture memory, draw calls, data-transfer counts, backend request latency,
battery drain and mobile thermal state. Synthetic model microbenchmarks
cannot replace any of these measurements.

### C. Bottleneck and risk register

| ID | Subsystem and exact location | Evidence / root cause | Priority | Action and status |
|---|---|---|---|---|
| PERF-01 | `FeedRowsModel::reconcile` in `ui/app/social/feed_rows.cpp` | Every unchanged-order hydration still built old-key membership, counted displacement and walked the generic move algorithm; model timing scaled with input rows | High | **Changed, compiled, measured:** stable-identity-order branch emits only changed primitive roles; pre-reserves incoming-key hash |
| PERF-02 | `MasonryWindowModel::synchronizeWindow` and `FeedMasonryView.qml` | Native per-lane binary search and bounded visual window already in use; 4,000-row viewport admission ~0.02 ms in the synthetic test | Low | **Retained**, as no evidence supports a rewrite |
| PERF-03 | `FeedMasonryView.qml` `cacheBuffer` and `retentionBuffer` | Incoming revision increased admission range; this may trade fewer incubation gaps for more live delegates and textures | Medium | **Unverified runtime trade-off:** measure FPS, delegate count, RSS, and texture uploads under fast image-heavy scroll before changing it |
| PERF-04 | `GalleryLayout.qml` / `GalleryTile.qml` and `SuperellipseImage.qml` | Current single-photo mask behavior requests an image layer on relevant platforms; offscreen effects can consume render passes and texture memory | Medium | **Static risk only:** capture Qt Quick scene graph/RHI/GPU traces before effect changes; preserve existing image and corner fidelity |
| PERF-05 | `FeedSurface.qml::synchronizeStableModel` and `ProfileSurface.qml::synchronizeStableModel` | QML creates primitive-role arrays and tracks keyed payloads before native reconciliation; repeated large hydration can still cross JS/C++ boundaries | Medium | **Unmeasured:** instrument end-to-end JS-to-model transfer cost and model notification fan-out |
| PERF-06 | `ui/app/media/remote_image.cpp`, `ui/app/network/link_preview.cpp`, `src/transport/qt_network/async_http.cpp` | Asynchronous image/provider/network and cache machinery exist, but no request, cache-hit or decode trace was captured for realistic scrolling | High to measure | **No code change** without network and GPU evidence; check bounded concurrency, cache eviction and obsolete-request cancellation |
| PERF-07 | Qt UI and WASM contract tests | Release smoke fixture used `assert(writeFile(...))`, whose side effect disappears under `NDEBUG`; this caused a misleading missing-QML-file and crash | High (correctness) | **Fixed:** explicit fatal runtime checks replace side-effectful asserts; Release Qt smoke tests re-run |
| PERF-08 | Static JS tests after QML/native layout migration | Initial full suite: 1,169/1,182 pass, 13 failures expecting deleted flexible spacers, old JS Masonry placement or synchronous Loader construction | High (regression gate) | **Fixed and verified:** assertions now match native layout, viewport handoff and responsive utilities; full 1,182-test suite passes |

The counts from a static inventory of `ui/components/` were 171 QML files,
40 files containing a `Timer`, seven containing `MultiEffect` and
32 containing `Loader`. These are **source occurrence counts, not runtime
allocation counts**; they cannot justify removing those features without
profiling. The Qt Quick Profiler and scene graph diagnostics are appropriate
for follow-up. Consult the [Qt Quick performance profiler](https://doc.qt.io/qt-6/qtquick-profiling.html),
[scene graph renderer documentation](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html),
[ListView reuse and cache semantics](https://doc.qt.io/qt-6/qml-qtquick-listview.html),
and [graphics backend adaptations](https://doc.qt.io/qt-6/qtquick-visualcanvas-adaptations.html).

### D. Implemented changes, validation, and boundaries

- `FeedRowsModel::reconcile`: detect unchanged stable order in O(N) without
  rebuilding old-key membership or searching for moves; use the original
  incremental path for actual order changes, additions, removals or unrelated
  feed scopes. Preserve exact revision/presentation/media-role notifications.
  Pre-reserve the bounded `seen` map using the input size.
- `tests/ui/feed_rows_model_tests.cpp`: add a 2,048-row unchanged/hydration
  regression, asserting no reset/insert/move for stable order and exactly one
  changed `RevisionKeyRole` for one mutation.
- `tests/ui/masonry_view_runtime_tests.cpp` and feed model tests: use
  always-executed runtime assertions instead of `assert` with potentially
  side-effecting setup in Release.
- `tests/ui/feed_performance_benchmark.cpp`: optional opt-in native
  benchmark target, separate from standard CI success/failure thresholds.
- `tests/wasm/`: update obsolete source-structure assertions to the
  currently implemented native viewport handoff and responsive action
  layout. The native Qt fixture still verifies non-overlapping mixed-height
  cards, bounded viewport admission and reverse scrolling.

The isolated native test suite was run with **Qt 6.12 Release** on the server.
The affected three native smoke tests passed after the fix, the focused
76-case WASM contract subset passed, and the complete **1,182-case WASM test
suite passed (1,182 passed, zero failed)**. The latter validates source-level
contracts and unit behaviors, not interactive mobile rendering or real FPS.

An ASan/UBSan RelWithDebInfo build of the native feed-model fixture also
**compiled and linked successfully**. Its full 10,000-row runtime fixture did
**not** complete within the remote execution limit (the detached test was
terminated with SIGTERM after approximately 105 seconds). It is therefore
**not** reported as a clean sanitizer run; an instrumented, bounded fixture on
unconstrained CI remains to be completed.

The audit deliberately avoided changing media quality, blur, scroll physics,
provider routing, authentication, concurrency settings, compiler optimization
flags or cache capacities for which a repeatable benefit had not been
demonstrated. The additional `O(N)` identity comparison is inexpensive
relative to the previous generic reconciliation path but still scans keys;
an upstream C++ change-sequence/epoch contract could eliminate even that
work if real end-to-end data shows it dominates.

### E. Remaining independent platform qualification

Capture **ten or more reproducible runs** for each relevant P01–P08 workload,
using identical data and network/cache state. Profile Qt QML bindings,
QSG/RHI rendering phases, `ListView`/Masonry delegate lifetime, texture
uploads, hidden-view activity, image decode and signalled UI updates. Compare
native desktop, actual mobile hardware and browser WASM separately; do not
extrapolate from the synthetic Qt model benchmark. This server has one vCPU,
no instrumented physical Android/iOS device and no production user-gesture GPU
trace; the installed `perf` command was not executable through the current
restricted remote-operation policy. Other platform measurements remain
unavailable rather than fabricated.

## Explorer semantic feeds — 9 November 2026

An isolated, read-only public HyperSnap probe was made against the configured
default indexed provider (`https://haatz.quilibrium.com`), not the separate
raw Snapchain host. Results are sample observations rather than user-visible
end-to-end timings:

| Explorer source | Public probe | Observation |
|---|---|---|
| NFTs / text search | `cast/search?q=art` or `q=nft`, 40 rows | No embeds in sampled results; client-side art eligibility can reject the whole page |
| NFTs / channel feed | `feed/channels?channel_ids=nft,art`, 40 rows | 30 rows with embeds; endpoint responded in approximately 126 ms |
| Dev / text search | `cast/search?q=github`, 40 rows | All sampled results had no embeds and triggered an optional display-hydration path |
| Dev / channel feed | `feed/channels?channel_ids=dev,developers,build,programming`, 40 rows | 40 indexed channel casts; endpoint responded in approximately 158 ms |
| Conversations / native chain | `v1/info` plus 500-block event window on two shards, then `cast/bulk` | Valid replies and parent identities verified; public node was responding during the probe |
| Following / reply projection | `feed/following?fid=3&limit=50` | 34 of 50 public sampled posts were replies, proving that a source-name gate can wrongly hide real conversations |

**Corrections:** the first NFT source now requests indexed NFT/art channel
casts, with independent bounded market/mint search cursors for supplemental
pages. Dev prioritizes developer channels; compatible nodes without the
channel endpoint fall back to text search. A search cursor never enters the
unavailable channel endpoint on later NFT pages; existing v1 collectible
cursors are decoded conservatively and migrated to v2 without replaying
the old text-search cursor against the new channel endpoint. In
`ui/components/feed/FeedSurface.qml`, Following + Conversations filters
replies by actual parent identity rather than rejecting the Following
source, and image detection accepts established artwork-CDN URLs lacking a
file extension. This does not loosen the fungible-token exclusion gate.

**Limits:** these probes used public data, not the user's selected node or
credentials. A successful HTTP probe does not prove mobile FPS, complete
image decode, offline behavior, or installed-app startup. The native provider
contract test includes cursor routing, fallback and decoded-parent checks;
`tests/wasm/explorer_freshness_scroll_regressions.test.mjs` checks the
Following reply projection and extensionless image classification.

## Proposed feed fluidity and Neon integration — 24 November 2026

**Status: proposed, not implemented or benchmarked.** Source baseline:
[`d0cb948`](https://github.com/genyleap/genycaster/tree/d0cb9487c9dacc1ebd41cc2aa92943eac1055761).
The user-visible symptom is heavy scrolling between casts. The engineering
goal is Telegram Desktop-class responsiveness while preserving GenyCaster's
visual fidelity, content coverage and Qt Quick architecture.

### Existing strengths and remaining hypotheses

Timeline already uses a recycled ListView with a bounded cache; Magazine
uses a native viewport-window MasonryWindowModel. FeedRowsModel reconciles
stable identities incrementally. Remote images use bounded asynchronous
decode, decoded-image caching and frame-paced presentation; video players
are admitted on demand. Do not replace these foundations merely to introduce
another renderer. The CPU model microbenchmarks above are not GPU or
end-to-end scrolling measurements.

| Priority | Source evidence | Proposed action |
| --- | --- | --- |
| P0 | [SuperellipseImage.qml](../ui/components/visual/SuperellipseImage.qml) enables a persistent Image layer and MultiEffect mask for admitted clipped images; [Avatar.qml](../ui/components/content/Avatar.qml) uses the same path. | Profile offscreen passes, texture allocations and bandwidth. Prototype direct texture sampling plus analytic contour coverage in one QSG material, without a separate mask texture or per-image offscreen layer. |
| P0 | [CastItem.qml](../ui/components/feed/CastItem.qml) has a large rich delegate tree, nested layouts, loaders, contextual previews and engagement controls. | Count live objects and binding evaluations; load only applicable subtrees; cache presentation-ready metadata and preserve interaction/accessibility. Timeline already hides the decorative card surface, so Glass alone is not an adequate explanation. |
| P1 | [FeedSurface.qml](../ui/components/feed/FeedSurface.qml) commits resolved heights during flicking; [FeedMasonryView.qml](../ui/components/feed/FeedMasonryView.qml) coalesces layout but still receives late measurements. | Cache measured heights by content/layout revision and width; reserve known media aspect ratios; minimize reflow and preserve stable cast/offset anchors. Do not freeze incorrect heights or introduce gaps/overlap during scrolling. |
| P1 | [remote_image.cpp](../ui/app/media/remote_image.cpp) caches decoded CPU images, but a cached QImage does not guarantee a resident GPU texture. | Measure uploads, residency, repeated allocations and frame-admission queues; evaluate bounded per-window GPU texture reuse with render-thread ownership, invalidation and context-loss handling. |
| P1 | SuperellipseImage's GIF playback can continue during scrolling; pooled rows are hidden but near-viewport delegates may remain active. | Separate actual viewport visibility from delegate retention; budget decorative animation and pause offscreen work where appropriate without interrupting user-initiated playback. |
| P2 | [RichCastRenderer.qml](../ui/components/content/RichCastRenderer.qml) builds spans and rich blocks; embed classification and hydration can trigger presentation changes. | Profile parsing, shaping and binding work; move proven hot transformations to cached C++ presentation data. Consider a lightweight native cast renderer only if optimized Qt Quick delegates still miss the frame budget. |

### Planned integration of the new Neon design kit

GenyCaster is intended to integrate the redesigned standalone
[Neon](https://github.com/genyleap/neon) kit again. The current reference
baseline is [Neon 0.7.0 / `873bac8`](https://github.com/genyleap/neon/tree/873bac8fbbf52a65d2fc30925af7ff67fa288d50),
not a declaration that this version is already integrated or native-certified.
A shared native image renderer is a **proposed next Neon capability**, not
a feature claimed to exist in that baseline.

```text
GenyCaster feed models / stable viewport geometry / media policy
                         |
                Cast presentation
                         |
         Neon shared visual primitives
     (analytic shapes, materials, future image material)
                         |
              Qt Quick Scene Graph / RHI
                         |
           Metal / Vulkan / platform backend
```

Neon should own generic image geometry, analytic anti-aliasing, material
composition and reusable rendering primitives. GenyCaster should retain
protocol data, feed ordering, remote-image provider, download policy,
decode scheduling, media admission, playback and application state.
Avoid duplicating the provider or constructing a separate render engine.
Preserve avatar silhouettes, PreserveAspectFit/Crop behavior, transparency,
DPR, fallback rendering, GIF/video semantics, RTL and accessibility.
Validate texture lifetime and sharing on the owning render thread; account
for atlas coordinates, premultiplied alpha, source changes and device loss.
Use public Qt scene-graph APIs and QSB; no private backend shortcuts.

### Execution and acceptance gates

1. Establish a Release/Metal baseline on the user's iMac M1 for P03/P04/P05/P07.
   Compare Timeline and Magazine separately on the same fixture and cache
   state; capture p50/p95/p99 frame intervals, missed refresh deadlines,
   GUI-thread and render/GPU time, object counts, upload volume, memory
   and render passes. Use Instruments and Qt QML/scene-graph diagnostics.
2. Isolate the image-mask hypothesis with an A/B test (existing path versus
   a diagnostic unmasked path). The unmasked variant is diagnostic only,
   not a quality-equivalent production win. Repeat with static media,
   animated media and text-only fixtures; compare Efficiency and Adaptive
   as a secondary multi-variable diagnostic, not proof of one root cause.
3. Implement and qualify the shared Neon image primitive in Neon first.
   Compare it against the existing masked path with equivalent pixels,
   dimensions, sampling and feature behavior. Verify native builds,
   QSB, visual regression, resource ownership and supported backends.
4. Pin a verified Neon revision, migrate image/avatar rendering
   incrementally behind a reversible integration boundary, and remeasure.
   Migrate other shared visual controls only where the contracts match;
   avoid rewriting working feed models or interaction logic.
5. Optimize remaining measured delegate, geometry, upload and animation
   bottlenecks. Preserve scroll continuity, media readiness, click targets,
   accessibility, selection and return-from-detail behavior. Extend the
   existing media-scroll tests and add reproducible real-frame traces.

For a 60 Hz display the frame interval budget is about 16.67 ms; at 120 Hz
it is about 8.33 ms. These are targets, not achieved GenyCaster results.
Use the actual refresh rate and report distributions, not average FPS
alone. Follow the existing ten-run methodology and keep native desktop,
mobile and WASM qualification separate.

**Engineering principle:** avoid unnecessary work before optimizing
individual draws. Borrow Telegram Desktop's viewport-limited work,
incremental invalidation and bounded resource lifetime, not its QWidget
rendering architecture. Preserve visual quality and functional behavior.
No performance improvement or confirmed root cause is claimed until
measured on the real rendering path.

See also [Architecture](ARCHITECTURE.md), [UI performance contract](14-performance-and-ui-runtime.md), and [Contributing](../CONTRIBUTING.md).
