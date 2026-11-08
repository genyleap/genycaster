# 14 — Performance and UI Runtime Contract

## Goal

The product target is a native, highly responsive social client with Telegram-class interaction quality. C++ is not itself a performance guarantee; the architecture must keep expensive work off the UI path and use local-first state, deterministic layouts, incremental updates, and efficient rendering.

## UI architecture

Preferred long-term model:

```text
QML application shell
    │
    ├── navigation
    ├── sheets/dialogs
    ├── settings
    ├── profile/detail surfaces
    ├── ordinary forms/controls
    └── motion/orchestration

C++ presentation layer
    │
    ├── application-facing presentation models
    ├── immutable/incremental snapshots
    └── UI-safe commands

Hot timeline rendering
    ├── Feed
    └── Chat
```

For hot timelines, the architecture must permit migration to custom `QQuickItem` / Qt Quick Scene Graph rendering without changing domain/application/timeline engines.

### UI source ownership

`ui/app/main.cpp` is the only file at the application root. All other
Qt adapters belong to shared capability groups: `bootstrap/`, `runtime/`,
`social/`, `browser/`, `miniapp/`, `identity/`, `network/`,
`data/`, `media/`, `settings/`, `update/`, `platform/` and
`diagnostics/`. A directory owns cooperating sources, never just one
`.cpp/.hpp` pair. See `docs/15-ui-app-boundaries.md`.

The scene-graph renderers share `ui/render/` as `stroke`,
`superellipse` and `sparkline`. QML source files live in feature
groups beneath `ui/components/`; the physical source paths are registered
in `ui/components/source_paths.json`. Public QML component names,
singleton names and their `Farcaster.Client` resource aliases stay stable,
even if a physical source filename is shortened.

Browser-visible files in `ui/wasm/` retain their public URL filenames:
changing them requires a separate installer, service-worker and deployment
migration. Always update CMake, tests, localization checks and architecture
guards when moving any UI source. Keep provider-neutral domain code out
of the Qt presentation boundary.

## Do not prematurely custom-render everything

The first implementation may use optimized Qt Quick delegates where they meet performance targets.

Custom scene-graph rendering should be introduced when benchmarks demonstrate a meaningful bottleneck.

The key requirement in Phase 0 is **renderer replaceability**, not premature renderer complexity.

## Timeline separation

```text
FeedEngine / ChatTimelineEngine
          ↓
      Snapshot/Diff
          ↓
Presentation Model
          ↓
Renderer
```

The renderer must not own network, persistence, ranking, or synchronization logic.

## Thread/executor model

Conceptual responsibilities:

```text
UI/Main thread
  input, tiny state commits, layout scheduling

Qt Render/Scene Graph thread
  GPU rendering

Network executor
  HTTP/WebSocket/coroutine I/O

Database executor
  SQLite reads/writes/migrations

Sync/model executor
  reconciliation, timeline updates, normalization

Media worker pool
  image/video metadata/decode/preprocessing

Crypto worker
  signing/encryption/decryption where appropriate
```

Exact executor topology may evolve, but expensive work must not drift onto the UI thread.

### Shutdown ownership

Responsibility: the native composition root stops network admission and drains
background work before Qt application destruction disables event delivery.
Capability origin: Local. Mandatory for the native UI; absent from the
provider-free core. Public contract: an application-scoped shutdown owner and a
Qt transport request scope, both idempotently stoppable. Dependencies: Qt Core
and the Qt HTTP adapter only. No new persisted or local domain state is created;
offline/cache behavior is unchanged. Shutdown reports `Unavailable` for stopped
HTTP work, does not retry or fail over, and never treats an interrupted write as
proof that the server rejected it.

Every background job must either own its inputs or have an owner that joins it
before releasing borrowed dependencies. A `QPointer` or cancelled observer alone
does not cancel network work. Blocking Qt HTTP requests must register with the
application request scope: stopping it rejects subsequent requests and queues
abort on each reply's owning thread. The global Qt pool is drained while
`QCoreApplication` is still alive, including early-return cleanup. Do not rely
on the Qt application destructor to drain event-driven workers, detach them, or
force-terminate them to hide a lifecycle defect. Dedicated executors/reactors
remain RAII-owned and must finish before application destruction.

Workers must not use `BlockingQueuedConnection` to wait on the GUI during
shutdown. Platform launch requests use a shutdown-aware dispatch gate: pending
calls are rejected and their waiters wake without another GUI event. Dispatch
also has a deadline while the application is running; timed-out callbacks must
not launch a browser later or capture references to expired caller stack data.

Tests proving this boundary use a local server that deliberately withholds a
response, exercise GET/POST, queued/follow-up requests, normal quit and scope
destruction, and enforce a process timeout. Smoke modes disable live insights
and therefore cannot alone prove shutdown with active market requests.

For native UI shutdown, the composition root also has an explicit QML
teardown phase **after** `QGuiApplication::exec()` returns and **before**
the runtime/controller owners are destroyed. It stops admitting new
background requests, joins the bootstrap worker, releases the scene graph,
synchronously destroys QML root objects while the GUI dispatcher is still
installed, drains `DeferredDelete` events, and collects JS garbage.
This addresses the lifetime gap behind warnings about incomplete QML object
creation and attempts to start timers after an event dispatcher has gone.
The same sequence applies to macOS, Windows, Linux, Android and iOS;
WebAssembly retains its browser event-loop lifetime.

Qt Creator's `QML debugging is enabled` is a **Debug-build** diagnostic.
It is not a production logging feature. The macOS Text Services
`TSM AdjustCapsLockLEDForKeyTransitionHandling` line originates outside
GenyCaster and is not a QML teardown error. Qt engine/timer warnings are
not hidden through message handlers or logging flags: the native shutdown
sequence must be verified by a real close-after-use test on each platform.

## Hard UI-thread prohibitions

Never perform on the UI thread:

- network I/O;
- database I/O;
- JSON parsing of provider payloads;
- cryptographic operations;
- image/video decode;
- large persistence serialization;
- complete timeline rebuilds for incremental changes;
- large provider-to-domain normalization passes.

## Deterministic timeline layout

### Explorer magazine presentation

Responsibility: the Explorer presentation can arrange existing cast rows in
independent, variable-height columns. Capability origin: Local; optional visual
composition, with the existing timeline available. Public contract: a saved
magazine/timeline choice and target card width (420, 500 or 640 logical pixels).
The default targets two roughly 500-pixel columns; small cards may use three.
Narrow windows fall back to one column. Home and both shell rails retain their
existing composition.

Canonical data/invariants: casts, provider ordering, pagination, actions and
stable identities remain owned by the existing feed. New cards are assigned in
source order to the shortest estimated column; their assignment stays fixed
while metadata loads. Reordering or changing the column count recomputes lanes.
Dependencies: existing QML cast presentation and feed model only. Expected
failures: missing metadata/images keep the existing fallback; pending delegates
use estimated heights. Local state: display preferences in the existing
Explorer QSettings category; measured heights are transient per item/width.
Offline behavior, provider policy/disablement and failover semantics are
unchanged: this presentation consumes cached rows and introduces no requests.

Only near-viewport cast trees are instantiated; offscreen slots retain measured
geometry. Layout work is coalesced, and idle geometry changes preserve a visible
cast anchor. Required coverage: mixed heights, column containment, bounded live
delegates, append/reorder/reuse, resize, return from detail, RTL, keyboard scroll
and existing pagination/interaction paths. Source implementation is separate
from Qt runtime qualification; do not infer rendering or frame-time results
from a source review.

### Native viewport-window Magazine (November 2026)

`FeedRowsModel::reconcile()` accepts an ordered snapshot of primitive, stable
roles from Explorer and Profile, then emits Qt model insert/move/remove/change
notifications in C++; changes with almost no shared IDs use one bounded reset.
Heavy cast payloads stay keyed in the presentation surface and never become
variable-typed `QAbstractItemModel` roles. The Qt model is GUI-thread-owned;
network, decode and persistence still execute through the existing read workers.

`MasonryWindowModel` in `ui/app/social/feed_rows.{hpp,cpp}` owns numeric
heights, lane assignments, RTL coordinates, stable-ID lookup, cached samples
and per-lane source indexes. Its `QAbstractListModel` exposes **only** the
viewport and adjacent retention range to `FeedMasonryView.qml`. A Repeater
over that *windowed model* is intentional; using the full source model in a
Repeater is forbidden because that would instantiate one QML slot per cast.
Viewport admission is found by per-lane binary search, and adjacent delegate
identity is reconciled incrementally instead of resetting the whole window on
each scroll event. Gallery text and media remain Qt Quick delegates; only
layout geometry and stable-window indexing are native.

Source updates and geometry measurement are coalesced, and a same-cast viewer
reaction revision retains the previous painted height until new content
provides a real measurement. The cover/header instances are retained across
Magazine/Timeline toggles so the banner is not re-incubated on every switch.
Geometry estimates are width-bucketed; actual measured content always wins.

Conversation replies use the same native primitive-role reconciliation rather
than a per-hydration global revision counter. Only a cast whose visible text,
metadata, context, reaction, or engagement fields change invalidates its
delegate; unchanged sibling replies keep their QML item and measured height.
Opening another conversation explicitly clears the old conversation scope.

Verification: `tests/ui/feed_rows_model_tests.cpp` exercises real Qt model
signals and 600-row numeric geometry; `tests/ui/masonry_view_runtime_tests.cpp`
loads the production QML with a native 800-row model and checks viewport
admission and reverse scrolling; `tests/ui/feed_geometry_runtime.mjs`
tests real `ListView` delegate reuse and changing heights with Qt Quick.
These tests are **not** evidence of achieved frame-rate, production media
decode performance, or cross-device qualification. Record such metrics with
the benchmark methodology in `docs/PERFORMANCE.md`.

Feed/chat should avoid viewport instability caused by late media/reaction layout changes.

Long-term layout engine should support:

- cached text layout keyed by entity ID + width + font scale + theme-relevant parameters;
- known media aspect ratios before full decode where possible;
- stable variable-height estimation/calculation;
- incremental invalidation;
- exact viewport anchoring when older/newer content is inserted.

## Media pipeline

Conceptual pipeline:

```text
URL
 ↓
Cache lookup
 ↓
metadata/thumbnail
 ↓
worker decode
 ↓
GPU texture
```

Requirements:

- priority by viewport proximity;
- cancellation;
- request deduplication;
- memory-pressure handling;
- disk cache;
- reduced expensive work during high-velocity scroll where appropriate.

## Prefetch and navigation

Later phases may implement:

- adaptive prefetch based on direction/velocity/network/memory/battery;
- predictive navigation data preparation on pointer/touch-down with cancellation if navigation does not complete.

These are optimizations, not domain semantics.

## Engineering performance targets

Targets are budgets to test, not claims of achieved performance:

```text
touch → visible response       ≤ 1 frame where feasible
optimistic local insert        ≤ 16 ms target
cached conversation open       < 100 ms target
warm resume                    < 150 ms target
cached feed first paint        < 250 ms target
60 Hz flows                    near-zero dropped frames target
120 Hz                         first-class design target
UI-thread network/DB/JSON      0
```

All targets must be validated on a real device matrix before being treated as achieved.

## Device matrix direction

Performance testing should eventually cover:

- current iPhone flagship;
- older supported iPhone;
- current Pixel/Android flagship;
- mid-range Android;
- low-memory Android;
- Apple Silicon macOS;
- Windows x64;
- Linux desktop.

## Apple SDK build warnings and native Mini App integration

The macOS adapter at `ui/app/miniapp/browser_macos.mm` uses Apple's modern
`WKWebView`, **not** the deprecated `WebView`/WebKitLegacy class. Earlier
umbrella imports (`AppKit.h` and `WebKit.h`) pulled unrelated legacy SDK
symbols, including OpenGL/ColorSync declarations, into the translation unit.
The adapter now imports only its directly used AppKit, WebKit and
QuartzCore class headers. Apple Clang's subsequent C++26 build revealed
that `WKWebViewConfiguration.h` and `WKNavigationDelegate.h` forward-declare
`WKPreferences`, `WKNavigationAction` and `WKNavigationResponse` rather
than providing their property definitions. Each class's concrete WebKit
header is now imported directly, and the CMake SDK probe includes the
same declarations; restoring umbrella headers is not necessary.
The November 2026 macOS clean-build log confirmed
that this did **not** fully resolve the SDK problem: upstream LLVM Clang 23,
running in C++26 mode, diagnosed mixed-enum operations in Apple
`CGImage.h` and `ColorSyncDeprecated.h` as hard language errors. C++26
(P2864R2) removed these implicit mixed-enum operations, so this is not
a missing cast in GenyCaster's Mini App logic.

The macOS-specific SDK adapter now keeps `OBJCXX_STANDARD 26` (with the
requirement enabled) and selects **Apple Clang from the active Xcode SDK**
for this Objective-C++ compilation boundary, including when Qt Creator
prepopulated an LLVM compiler before the language was enabled. A previously
enabled, incompatible compiler in an existing CMake cache is rejected with
an explicit cache-reset instruction rather than silently reused.
The shared project C++26 compiler, named modules and Mini App runtime
remain unchanged. CMake verifies a small
Objective-C++26 AppKit/WebKit header compilation at configuration time
and fails with an explicit toolchain/SDK message if unsupported rather
than starting a long, doomed build. This is an SDK/compiler compatibility
boundary, not a downgrade to C++23. Because two compiler vendors may
share this C++ ABI boundary, the resulting `Genycaster.app` still requires
full macOS link and runtime verification. Linux cannot validate it.

**No `#pragma clang diagnostic ignored`, `-Wno-deprecated-declarations`,
or other diagnostic-suppression workaround is used.** A warning in Apple's
SDK headers is not evidence that GenyCaster calls the corresponding legacy
API; do not port absent speech or ColorSync features as a workaround. The only app-owned deprecated
call reported by the November 2026 build, `NSBox.borderType`, was removed:
the Mini App loading background is now drawn by an opaque `NSView` subclass
that repaints when the system appearance changes. Avoid replacing it with a
transparent `NSBox`, because that would reveal web content behind the
loading overlay.

Other actionable warnings from that build were fixed in their owners:
the custody phrase scan explicitly converts characters to unsigned before
`std::isspace`; the Mini App FID authorization comparison uses an exact
JSON integer instead of converting a `uint64` to `double`; unused lambda
captures and aliases were removed. QML's generated `InitialSetup_qml.cpp`
AOT integer-narrowing warning was caused by a QML `Array.indexOf()`
expression inferred as `qsizetype` before assignment to an `int` slot.
`InitialSetup.qml` now uses boolean `includes()` to avoid the narrowing
without touching the Qt-generated translation unit. If a new generated
warning appears, trace it back to its source QML expression instead of
silencing the compiler.

**SDK consistency:** the reported OpenGL `-Wavailability` warning includes
both `/Applications/Xcode.app/.../MacOSX.sdk` and
`/Library/Developer/CommandLineTools/SDKs/MacOSX27.sdk` in the same
diagnostic. That indicates inconsistent SDK headers/availability macros
during this invocation, not an OpenGL call made by GenyCaster. Verify:

```sh
xcode-select -p
xcrun --sdk macosx --show-sdk-path
xcodebuild -version
printf '%s\n' "$SDKROOT"
```

Configure the active developer directory and Qt Creator's CMake kit to use
one consistent Xcode SDK, then rerun CMake with a fresh cache if the SDK path
changed. Do not disable the project's availability checks or switch to
deprecated OpenGL APIs. Official references:
[Apple NSBox borderType](https://developer.apple.com/documentation/appkit/nsbox/bordertype),
[WKWebView](https://developer.apple.com/documentation/webkit/wkwebview/),
[AVSpeechSynthesizer](https://developer.apple.com/documentation/avfaudio/avspeechsynthesizer),
[Xcode command-line tool settings](https://developer.apple.com/documentation/xcode/configuring-command-line-tools-settings).

## Qt Quick and C++ ownership — current audit

GenyCaster's UI is **one adaptive Qt Quick application**, not a separate
mobile application. Its cross-platform executable entry, Qt-facing controllers,
Mini App adapters, runtime composition and QML application window live in
`ui/app/`. Platform packaging properties are isolated in `properties/`; operating-system code adapters remain in `src/platform/` and narrowly owned `ui/app/` files,
and visual resources/components remain in `ui/render/`, `ui/components/`,
`ui/theme/` and `ui/assets/`. The retired demo data is not part of the QML
module.

A source inventory in November 2026 found **156 shipped component QML files**
with roughly **57.9k lines** and around **695 named JavaScript functions**.
These counts indicate where to inspect responsibilities; they do not measure
runtime performance, duplicated logic or how many lines should be removed.
The largest imperative surfaces include FeedSurface, ProfileSurface, AppShell,
GenyVideoPlayer, NotificationSurface and ComposerDrawer.

| Responsibility | Preferred owner | Why |
|---|---|---|
| Page layout, themes, accessibility, animation, declarative bindings | QML | Native reactive UI; translating it into C++ adds boilerplate and can weaken iteration |
| Canonical content normalization, security checks, token/URL parsers | C++ domain/application or bounded presentation adapter | Shared behavior, typed contracts, fuzzable tests and trusted input boundaries |
| Network, image/metadata fetching, cancellation, retry and caching | C++ provider/transport workers | No GUI-thread I/O; bounded memory and explicit ownership |
| Feed models, reconciliation, paging and stable item identity | C++ presentation/application | Incremental model updates avoid large JS list transformations |
| Playback state, Mini App permissions and signer transitions | C++ lifecycle/authority owners | Centralized state machines, scoped permissions and testable failure cases |
| Numerical chart sampling, decimation and coordinate generation | C++ rendering helper | Deterministic arithmetic outside reactive JavaScript; QML continues painting |

**First small migration:** `ui/components/content/TokenSparkline.qml` went from
**138 to 48 lines** by moving finite-value filtering, bounded decimation,
smoothing and point generation to `ui/render/sparkline.cpp`.
The Shape/PathPolyline and color behavior remain declarative. This is not
evidence of a frame-time improvement; `ui.token_sparkline_geometry` exercises
the C++ geometry and platform rendering still requires live tests.

### Candidates for incremental migration

1. **FeedSurface, ProfileSurface and AppShell** — inventory each JavaScript
   function. Move state reconciliation, repeated view-model transformations,
   navigation state machines and policy checks into existing C++ controllers;
   keep local UI focus, anchors, transitions and layout bindings in QML.
2. **ComposerDrawer and rich media surfaces** — move text validation,
   revision-aware draft persistence, media admission and parser work into C++.
   Existing C++ models and QML renderers must remain the single authorities;
   avoid new parallel implementations.
3. **TokenSemantic and locale helpers** — migrate *canonical* token locator
   parsing and input normalization to a typed C++ adapter, while preserving
   `qsTrId` lookups and locale-revision bindings where Qt translation updates
   need them.
4. **GenyVideoPlayer / Mini Apps** — review asynchronous state ownership,
   request lifetime and permission transitions. Keep interaction animations
   and platform-specific visual composition in QML. Never confuse visual
   component size with an automatically safe refactor.

**Migration gate:** preserve QML public properties and signals; add behavior
tests before replacing a JavaScript helper; check desktop/compact/RTL behavior,
native/WASM runtime and package resource aliases; benchmark hot paths; and
delete the old JS function only after the C++ implementation has matching
semantics. No goal of "zero JavaScript" is appropriate for a declarative UI.
