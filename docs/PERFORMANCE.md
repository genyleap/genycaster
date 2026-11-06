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

See also [Architecture](ARCHITECTURE.md), [UI performance contract](14-performance-and-ui-runtime.md), and [Contributing](../CONTRIBUTING.md).
