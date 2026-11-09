# C++20/23/26 Undefined Behavior and Qt Lifetime Audit — 2026-11-09

## Status and coverage (do not interpret this as a complete proof of safety)

- **Baseline:** `main` at `8cdeca54`, before these changes; repository was clean.
- **Configured project:** C++26, Qt **6.12**, CMake/Ninja, GCC **16.2.0** on one-vCPU Linux. Main `CMakePresets.json` includes native, Android and WASM configurations. This session did **not** perform a full product build, mobile build or deployment.
- **Inventory:** a repository-wide, read-only pattern inventory scanned **351 first-party C++ files** (including the new safety header) under `src/`, `ui/`, and `services/`; ~4.25 MB, excluding `build/` and downloaded `_deps/`. An initial 395-file count included 44 generated/vendor artifacts and must not be used as the first-party count. The scan found 212 `reserve/resize` occurrences, 154 front/back calls, 244 raw `data/constData` calls, 27 `reinterpret_cast` occurrences, 89 reference captures, 324 explicit `this` captures, and 18 Qt model-mutation operations. These are **search hits, not defect counts**.
- **Manual call-chain review:** prioritized `ui/app/social/feed_rows.cpp/.hpp`; `ui/render/stroke.cpp/.hpp`, `superellipse.cpp/.hpp`, `sparkline.cpp`; `src/execution/{thread_pool,serial,weighted_thread_pool}.cpp`; `src/transport/qt_network/{http,async_http}.cpp`; `src/application/{read_async_service,read_service}.cpp`; `src/routing/read_gateway.cpp`; `src/providers/{hypersnap/async_gateway,emoji/emoji_cache}.cpp`; `src/platform/custody_recovery.cpp`; and UI miniapp/media/network callback sites. An automated scan of a file is **not** equivalent to a full ownership/flow audit of that file. Many modules remain outside the deep reviewed set.
- **Baseline tests:** `ui.feed_rows_model` and `ui.masonry_window_runtime` both passed before any changes.
- **Toolchain feature probe (actually compiled and run):** GCC 16.2 reports `__cplusplus=202400`, `__cpp_lib_expected=202211`, `__cpp_lib_ranges_to_container=202202`, `__cpp_lib_containers_ranges=202202`, `__cpp_lib_inplace_vector=202603`, and `__cpp_lib_mdspan=202406`. This proves these macros for the *installed GCC/libstdc++ pair only*. Neither AppleClang nor Qt Android/WASM standard-library support was established. No C++26 container was introduced solely because it was available.

## Findings register

| ID / category | Location and severity | Evidence, trigger and confidence | Remediation and language choice | Verification / performance |
|---|---|---|---|---|
| **UB-01: signed arithmetic overflow** | `ui/app/social/feed_rows.cpp:FeedRowsModel::move` (around line 104); **Medium** | **Reproduced on the unmodified baseline under UBSan**: original line 107 attempted `2147483647 + 2147483647`, emitting `runtime error: signed integer overflow` when called with `INT_MAX`. | Check individual nonnegative values and `amount <= count - from/to` before computing sums; preserve all valid moves. **C++20/Qt 6**. | Boundary cases added to `tests/ui/feed_rows_model_tests.cpp`; native test and bounded combined ASan/UBSan fixture passed with the corrected implementation. Normal input performance not measured. |
| **UB-02: floating-to-integer conversion outside the range** | `ui/app/social/feed_rows.cpp:MasonryWindowModel::laneAt/indexAt` (around lines 478–491); **Medium** | **Confirmed by C++ conversion rules:** `static_cast<int>(floor(...))` ran before `std::clamp`. Large finite coordinates or infinities can be outside `int`. A NaN also violates assumptions of lane lookup. | Clamp in floating-point space before casting; `indexAt` rejects non-finite coordinates. Preserve lane choice for ordinary coordinates. **C++20/Qt 6**. | C++ model tests cover `1e300`, `-1e300`, infinity, NaN and ordinary indexes; native and ASan/UBSan fixtures passed. Runtime frame impact unmeasured. |
| **UB-03: tessellation segment conversion** | `ui/render/stroke.cpp` (around 111, 250, 303); **Medium** | **Confirmed by control flow:** `static_cast<int>(ceil(physicalSize))` preceded integer clamping for circular caps, arcs and rounded rectangles. A large QML size/radius can exceed the representable integer range. | Shared header `ui/render/geometry_safety.hpp` checks floating bounds and NaN before conversion. Retains the previous 4/10/12-to-144/512/64 segment limits at ordinary sizes. **C++20/Qt 6**. | `tests/ui/render_geometry_bounds_tests.cpp` compiles **real production scene-graph code**; tests 1e12-pixel geometry plus NaN/infinity bounds; native and combined ASan/UBSan pass. |
| **UB-04: superellipse segment conversion** | `ui/render/superellipse.cpp:superellipseCornerSegments` (around line 152); **Medium** | **Confirmed by control flow:** same cast-before-clamp defect for continuous-corner scene geometry. | Use the same bounded conversion helper; preserve the previous corner quality limit. **C++20/Qt 6**. | Same real renderer regression and sanitizer fixture passed. |
| **REL-01: uncaught executor exception (not language UB)** | `src/execution/thread_pool.cpp:ThreadPoolExecutor::Impl::run` (around line 48); **High (availability)** | **Confirmed C++ behavior:** a throwing job could escape its worker thread and call `std::terminate`, preventing drain accounting and terminating the process. Other executors already catch user callback exceptions. | Mirror the existing serial/weighted executor exception boundary when exceptions are enabled, then restore active-job accounting and continue. No new threads or synchronization. **C++20**. | Added a single-worker throwing-then-successful-job scenario in `tests/unit/execution_tests.cpp`; native `execution.bounded_read_pool` passed. No-throw hot-path cost not benchmarked. |

**Classification:** 4 vulnerable integer/floating conversion sites/groups representing actual language-level UB *when invalid inputs reach them*, and 1 confirmed C++ exception-handling reliability defect (**not UB**). The current evidence does **not** establish that those bad inputs previously occurred during normal production usage. No actual use-after-free or data race has been confirmed in this pass.

## Open investigations, deliberately not labeled confirmed UB

| ID | Location / concern | Confidence and required follow-up |
|---|---|---|
| **OPEN-01: callback lifetime** | `src/application/read_async_service.cpp` (e.g., `openIdentity` lines 326–415), `src/providers/hypersnap/async_gateway.cpp` callbacks | **Needs investigation.** Many long-lived asynchronous functions capture raw `this`. Their safety depends on the composition-owned executor/gateway shutdown order, cancellation and drain guarantees. Audit with controlled teardown races and compatible TSan; a capture by itself proves nothing. |
| **OPEN-02: oversized stroke input** | `ui/render/stroke.cpp` near `pointCount * 72U` and `QSGGeometry::allocate(static_cast<int>(vertices.size()))` | **Needs investigation.** Unsigned multiplication wraps by definition, not UB. With unrealistically huge point counts, estimate overflow, allocator exhaustion or Qt's integer vertex count may become problematic. Establish maximum accepted sample count, decimation and realistic memory limits before introducing a cap that changes visuals. |
| **OPEN-03: model width constraints** | `ui/app/social/feed_rows.cpp` `qsizetype` to `int` roles and `beginRemoveRows` | **Needs investigation, unlikely at realistic loads.** QAbstractItemModel uses integer row indexes. Conversion warnings predate this audit; a tested maximum row-policy should be explicit before supporting >`INT_MAX` elements. |
| **OPEN-04: platform/thread matrix** | Android, macOS, iOS if supported, WASM; raw network callbacks, QML object destruction, embedded mini-apps | **Not verified.** Run platform-specific lifecycle tests and stress checks with actual target toolchains. Qt6Keychain >=0.17 is missing in this Linux environment; the current project deliberately builds without persistent signer secrets (fail-closed). |

### Suspicious constructs cleared by reviewing local invariants

- `ui/app/social/feed_rows.cpp`: `moved.reserve(amount)` is followed by **`append` for each element** before indexed access. `reserve` alone would not create elements, but here the code does create them; **not UB**.
- `ui/render/sparkline.cpp`: samples are validated to have at least 2 points; `points` and `differences` are filled before `front()`, `back()` and indexing; no confirmed reserve/size confusion in this path.
- `src/platform/custody_recovery.cpp`: BIP39 word views reference `normalized.value` while its owning sensitive buffer remains alive and is not mutated after the views are created in the reviewed loop.
- `src/transport/qt_network/http.cpp`: synchronous `QNetworkReply` signal lambdas capture local references but supply the local `QEventLoop` as QObject connection context. `RequestRegistration` is destroyed **before** that loop by reverse local destruction order. This local path does not itself demonstrate a dangling capture.
- `src/providers/emoji/emoji_cache.cpp`: observed `reinterpret_cast<const std::byte*>` is used to view live byte character storage for immediate hashing; not evidence of invalid lifetime or aliasing.

## Validation evidence and limitations

| Test | Result | Coverage |
|---|---|---|
| GCC 16.2 + Qt 6.12 C++26 focused native compile | **Pass** | Production model, production scene-graph primitives, executor module |
| `ui.feed_rows_model` | **Pass** | QAbstractItemModel moves, invalid indexes, stable rows and large values |
| `ui.masonry_window_runtime` | **Pass** | QML model and actual Qt Quick layout |
| `ui.render_geometry_bounds` | **Pass** | Production C++ renderers, extreme dimensions and non-finite segment limits |
| `execution.bounded_read_pool` | **Pass** | Worker survives exception and processes next job |
| Combined **AddressSanitizer + UndefinedBehaviorSanitizer** | **2/2 pass** | Small instrumented out-of-tree model and scene-graph targets; `-fsanitize=address,undefined -fno-omit-frame-pointer -O1`; `UBSAN_OPTIONS=halt_on_error=1`; LSan not enabled (`detect_leaks=0`) |
| **Pre-fix UBSan negative control** | **Expected failure reproduced** | Isolated original `8cdeca54` translation unit compiled with the same combined sanitizers: the `INT_MAX` move test exited with status 1 and emitted `signed integer overflow` at the original `feed_rows.cpp:107`. The same test passed after remediation. The negative control is intentionally **not** a production or CI target. |
| Full existing WASM JavaScript contract suite | **1,185/1,185 pass** | Static contracts and JS behavior, **not** a WebAssembly compilation or browser test |

The native CMake configure emits pre-existing Qt6TaskTree/WrapVulkanHeaders discovery warnings and records absent optional Qt6Keychain support. The relevant test targets still compiled. Clang 18, `clang-tidy` and `scan-build` were located on the server, but their **full-project analyses have not run**. **ThreadSanitizer, LeakSanitizer, AppleClang, Android toolchains, macOS Instruments, WebAssembly production builds and real GPU profiling have not run in this audit.** A passing bounded sanitizer fixture does not establish that the remaining codebase is UB-free.

### Resource and performance discipline

Normal `std::vector::reserve()`, `QList`, Qt model roles and public QML interfaces remain unchanged. Numeric overflow checks add only bounded comparisons, and the render segment caps are identical for ordinary finite inputs. The thread-pool exception handler runs only on the exceptional path. **No before/after CPU, memory or GPU benchmark was obtained for these safety changes**; no speedup or energy improvement is claimed.

### Next pass, prioritized

1. Trace all `[this]` async callbacks and service destruction paths through `ui/app/runtime/composition.cpp` and provider shutdown; create deterministic cancellation/destruction stress tests.
2. Run TSan separately from ASan, then Clang Static Analyzer/clang-tidy with a C++26-compatible compilation database (avoid silently substituting C++20 flags).
3. Audit remaining QML-exposed C++ integer conversions, image-buffer limits, graph allocations and raw Qt pointer ownership with targeted hostile-input fixtures.
4. Validate GCC/Clang/AppleClang standard-library macro support and Qt module compatibility per target; retain C++20/23 facilities where C++26 is absent.
5. Run full Linux/Windows/macOS/Android/WebAssembly CI and real rendering tests before declaring this release cross-platform safe.

The methodology follows the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) resource, bounds and concurrency principles. This is a **verified high-risk first pass**, not a claim of exhaustive manual analysis or proof of total absence of undefined behavior.


## Continuation — worker construction and untrusted Media Upload (2026-11-09)

**Inventory update:** after adding the narrowly scoped safety headers, the read-only
first-party C++ pattern survey covers **353** files in `src/`, `ui/` and
`services/` (excluding `build/` and downloaded `_deps/`). This is still an
automated *inventory*, not an exhaustive manual review of all call chains.
For this pass, the detailed reviewed paths are
`src/execution/thread_pool.cpp`,
`src/execution/weighted_thread_pool.cpp`,
`src/execution/thread_startup.hpp`,
`src/transport/qt_network/media_upload.cpp`,
`src/transport/qt_network/async_http.cpp`,
`src/providers/hypersnap/async_request.cpp`,
`src/application/read_async_service.cpp`,
`ui/app/runtime/composition.cpp/.cppm`, and
`ui/app/social/channels.cpp`.

### Additional findings register

| ID / category | Location / severity / confidence | Root cause and trigger | Repair / standard | Verification and performance |
|---|---|---|---|---|
| **UB-05 — untrusted JSON floating-to-unsigned cast** | `src/transport/qt_network/media_upload.cpp` (ticket fields `partSizeBytes`, `partCount`; part response `partNumber`, `sizeBytes`; completion `sizeBytes`). **High**, **confirmed conditional UB**. | Network-returned JSON numbers were converted with `static_cast<std::uint64_t>(value.toDouble(...))` **before** validating bounds/integrality. Negative and out-of-range doubles violate C++ floating-to-integer conversion requirements; fractional positive values could silently truncate. A standalone pre-fix-shaped GCC 16.2 probe with `1e100` generated `-fsanitize=float-cast-overflow`: “outside the range of representable values of type 'long unsigned int'”. | `src/transport/qt_network/media_json_numbers.hpp` validates numeric type, exact integer conversion via Qt 6.12 `QJsonValue::toInteger(-1)`, nonnegative value and a caller-provided maximum **before** converting to unsigned. Validate multipart sizes/counts before division; compute ceiling quotient with division+remainder; preserve the optional completion-size fallback. **C++20/Qt 6.12**, no C++26 dependency. | `ui.media_json_numbers` tests negative, fractional, oversized, nonnumeric, NaN/infinity and valid boundary values. **`ui.media_gateway_protocol` invokes the real media adapter** with an in-memory fake HTTP transport and fake public signer; verifies accepted three-step upload and fail-closed malformed ticket, part and completion responses. The real `farcaster_transport_qt_media` library compiled with GCC16/Qt6.12. All tests passed. No measured runtime delta: metadata parsing is not a per-frame hot path. |
| **REL-02 — partially started worker group can terminate process** | `src/execution/thread_pool.cpp` and `src/execution/weighted_thread_pool.cpp` constructors. **High availability**, **confirmed by standard C++ destructor contract and control flow**, not memory UB. | Each constructor created multiple `std::thread` elements without rollback. If creating worker N throws `std::system_error` after earlier workers have started, unwinding destroys joinable threads; `std::thread::~thread` calls `std::terminate`. No actual OS resource-exhaustion event was observed. | Both native pools now call `src/execution/thread_startup.hpp::startWorkerGroup`. The helper requests synchronized stop, wakes blocked workers and joins all started threads **before rethrowing the original exception**; the rollback callback is compile-time required to be `noexcept`. **C++20**, zero extra work during normal job execution. | `tests/unit/execution_tests.cpp` deliberately throws during worker creation after one worker was started, proving that it is awakened and joined and the exception is propagated. Native `execution.bounded_read_pool` passed. No full-platform fault-injection coverage yet. |
| **REL-03 — weighted queue budget charged before an allocation succeeds** | `src/execution/weighted_thread_pool.cpp`, **Medium reliability**, **confirmed exception-safety ordering issue**. | `queuedWeightBytes += retainedBytes` ran before `std::deque::push_back`. On a `bad_alloc` or another enqueue exception, no item would be inserted, but the resource budget would remain charged. Affects native and cooperative-WASM implementations. | Insert the nothrow-movable job first, then commit the byte counter; no additional allocation or synchronization. **C++20**. | Native unit test verifies bounded admission, rejection under full budget, drain and successful subsequent full-budget admission. **Actual allocator-failure injection for the deque has not been run**; the exceptional path is source-proven only. |

### Ownership and thread-affinity audit observations

- `ui/app/runtime/composition.cpp:Runtime::~Runtime` cancels active Read requests
  before destruction and releases the asynchronous transport before draining
  Decode and Cache executors. The ReadService and routed adapters remain live
  until afterward. This is an intentionally safe ownership ordering for
  service/gateway callbacks **within this Runtime composition**.
- `src/transport/qt_network/async_http.cpp` uses a context-bound Qt worker
  `QObject`, `QPointer` handles and `BlockingQueuedConnection` shutdown
  before joining its `QThread`. Pending observer closures are abandoned
  rather than invoked during teardown. This is a source-level observation,
  **not** proof that every external caller respects object lifetime.
- `ui/app/social/channels.cpp` connects network reply handlers with the
  controller as QObject context; that context cancellation is safer than an
  unscoped `[this]` lambda. `ChannelListModel::synchronize` performs Qt
  begin/end insert, move and remove operations. This review did not establish
  a new model-contract violation.
- Remaining risk **OPEN-01** is not closed: teardown under actual simultaneous
  provider completion, executor rejection, cancellation and navigation has not
  been stress-tested across all target platforms.

### Additional verification

- GCC 16.2 / Qt 6.12 native compilation: **pass** for
  `farcaster_transport_qt_media`, the number decoder test and the existing
  execution library/unit target.
- Native regression: `ui.media_gateway_protocol`, `ui.media_json_numbers`,
  `execution.bounded_read_pool` — **3/3 passed**. The five focused native
  model/render/exec/media-number tests also passed.
- Instrumented out-of-tree test targets compiled with
  `-fsanitize=address,undefined -fno-omit-frame-pointer -O1`:
  `ubsan_feed_bounds`, `ubsan_scene_graph`, `ubsan_media_json_numbers`
  — **3/3 passed**. LeakSanitizer disabled; sanitizer scope remains narrow.
- Independent pre-fix-shaped `-fsanitize=float-cast-overflow` negative control
  exited nonzero on `1e100` before casting into `uint64_t`, as expected.
- A **separate** `-fsanitize=thread` worker-startup test successfully compiled,
  but the TSan runtime immediately exited **66** with
  `FATAL: ThreadSanitizer: memory layout is incompatible` on this Linux
  host. **No TSan race detection result is available.** Do not interpret this
  environment failure as a race or a passing analysis.
- Clang 18/clang-tidy is installed, but invocation was rejected by the
  restricted remote command policy; no clang-tidy or Clang Static Analyzer
  scan is claimed. AppleClang, Android and production WASM builds remain
  unverified in this pass.
- No new dependencies, public model roles, QML properties, authentication
  guarantees, cache ordering or provider-specific domain contracts were added.
  No reproducible benchmark or performance improvement claim is made.

**Next work:** continue Qt object-lifetime stress tests and provider-specific
buffer/path parsing audits, run TSan on a compatible host, and verify native
and WebAssembly builds in the normal CI matrix. Do not claim full repository
coverage on the basis of these targeted passes.

### Network shutdown verification update

The previously unbuilt native Qt shutdown-test executables were explicitly compiled
and executed after review of the Runtime/transport lifetime contract:

- `ui.background_shutdown.quit`, `close-window`, `early-return`, `success`,
  `timeout`, `gui-stop`, `gui-success`, and `scope-reuse`: **8/8 passed**.
- `ui.async_reactor_shutdown` (one active and two queued requests abandoned
  without stale observer delivery): **1/1 passed**.

Both were run with GCC16's `libstdc++` on the native Linux host. A first
CTEST invocation returned `Not Run` because the executables were not built,
not because the tests had failed; after compiling them, all nine passed.
This evidence covers the tested shutdown contracts, **not all possible
cross-thread schedules**.
