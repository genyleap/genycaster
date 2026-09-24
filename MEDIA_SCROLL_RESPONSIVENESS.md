# Media scrolling responsiveness

## Geometry follow-up gate

Responsibility: reserve media and link-card layout before asynchronous metadata
or pixels arrive. Origin: Local; mandatory shared feed presentation. Contract:
known initial dimensions shape the reserved frame; unknown dimensions use a
stable fallback, with loading/error content inside it. Dependencies: existing
presentation metadata and async image provider only. No domain or provider
policy change, network request, or new feature target. Transient state is a
per-URL layout reservation, reset on reuse; cached/offline content follows the
same rules. Missing metadata and image failures retain the frame. Tests cover
the actual feed panel, delayed link metadata, image completion/failure/timeouts,
initial known dimensions, URL reuse and responsive widths.

The follow-up also owns reuse of existing feed presentation objects: keep image
and gallery pools separate, and update same-size supporting embed slots in place.
This is transient, bounded ListView/Repeater state; no persistence, provider
selection, offline contract, or domain API changes. Tests exercise the production
FeedSurface and verify both delegate identity and updated URL after reuse.

## Scope and engineering authority

This repair owns presentation work when media-bearing feed rows enter the viewport.
The existing network reactor, bounded image decode pool, domain contracts, provider
policy, and stable feed geometry remain the owners of their current behavior.
Pre-existing working-tree changes are preserved.

Engineering authority loaded from `thecompez/ai-for-modern-cpp` at
`326cd850c2cb5e39264b7d2ccf2c29fba6627380`: `AGENTS.md` and the Architecture,
Modules, Naming, Syntax and Style, API Design, Errors and Resources, CMake and
Toolchains, Testing and Verification, Qt Quick UI, and Project CMake Baseline guides. The mandatory local
architecture documents and ADRs were also read.

## Implementation gate

- Responsibility: keep image presentation and video-preview construction from
  blocking feed gestures; preserve already-visible media while scrolling.
- Capability origin: Local presentation of existing media capabilities.
- Mandatory or optional: shared graphical media presentation; no new feature target.
- Public contract: existing image loading/failure/manual-download states and video
  actions; a decoded image is presented only when its scroll host has settled.
- Canonical data/invariants: no canonical model change; recycled rows cannot show
  the previous media; media completion does not resize feed rows.
- Dependencies: existing Qt Quick image provider, components, and canonical player.
- Expected failures: image decode/network failure and cancelled/recycled requests
  keep the existing placeholder/retry behavior.
- Local-state representation: transient presentation admission and player activation.
- Offline behavior: cached images use the same non-blocking presentation path.
- Provider policy and disablement: unchanged C++ storage/download policy.
- Failover semantics: unchanged provider-owned retry; cancellation is not a URL failure.
- Tests proving the boundary: delayed image completion during motion, source
  recycling, retained visible media, stable geometry, and lazy video activation.

## Findings

`RemoteImageProvider` already performs disk reads and image decode on its bounded
worker pool. However, `SuperellipseImage` immediately reveals an in-flight image
when it becomes `Image.Ready`, including during a fling. This also allocates its
mask layers and activates the ambient blur. Deferring only new requests does not
cover completions of requests started before the gesture.

`VideoPreview` constructs the complete `GenyVideoPlayer` for every idle video row,
including its multimedia objects and full-screen controls. An empty playback
source prevents decoding but does not prevent that construction cost. Its ambient
backdrop also requests a second copy of the poster independently.

The focused GPU regression reproduced a further detail: hiding a completed QML
Image does **not** prevent Qt Quick from calling its texture factory. Presentation
visibility alone was insufficient. Clearing the unfinished source when motion
starts makes Qt cancel/detach that response before a late completion can prepare
its texture. The regression observes `QQuickTextureFactory::createTexture`, not
just QML visibility.

## Final implementation

- `ui/components/SuperellipseImage.qml`: detach unfinished requests when motion
  starts, defer new requests until 48 ms after settling, and gate first image/mask
  presentation. Preserve already-presented images and terminal failure states.
  Clear recycled source identities synchronously. Resolve the scroll ancestor
  after delegate attachment using a component-owned zero-interval Timer. Skip
  invisible border geometry.
- `ui/components/AmbientMediaBackdrop.qml`: admit the first capture at rest and
  leave its source item detached until then. Keep existing captures visible but
  frozen during motion.
- `ui/components/VideoPreview.qml`: show a lightweight poster and accessible Play
  and Open Original buttons. Construct the canonical player through an
  asynchronous Loader only on explicit playback. Release it when the source is
  recycled. The ambient capture shares the poster's image instead of requesting
  a second one. Idle rows create zero `QMediaPlayer` objects; activation creates
  exactly one. Existing reserved row geometry is retained.
- `tests/ui/media_scroll_tests.cpp` and `ui/CMakeLists.txt`: add deterministic
  software and Metal regressions using production QML and a controlled async
  image provider, including a genuinely moving Flickable with late-attached
  descendants.

The C++ provider's existing asynchronous I/O, bounded decoder pool, cache and
download policy are unchanged. Suspending an incomplete remote transfer may
require restarting that transfer after scrolling stops; cache reuse follows the
existing provider policy. Already-visible images remain available during motion.
No broad shape-renderer rewrite or application/domain change is included.

## Verification

Environment: macOS ARM64, Qt 6.11.1, Homebrew Clang 23.1.1, CMake 4.4.3,
Ninja 1.13.2; Debug UI/test configuration in `build/media-scroll-verify`.
Qt Quick Controls uses Basic. Both new runtime tests enable `QT_FATAL_WARNINGS=1`.

Commands executed from the repository root:

```sh
cmake -S . -B build/media-scroll-verify
cmake --build build/media-scroll-verify --parallel 6
cmake --build build/media-scroll-verify --target farcaster_client_qmllint_strict
ctest --test-dir build/media-scroll-verify -R 'ui.media_scroll|media_playback_coordinator|presentation.media|async_resource|ui_design|architecture_guard' --output-on-failure --no-tests=error
python3 tools/check_ui_design.py
python3 tools/check_data_storage_contract.py
git diff --check
ctest --test-dir build/media-scroll-verify --output-on-failure --no-tests=error
```

- Configure and complete default build: PASS; executable at
  `build/media-scroll-verify/bin/farcaster_client`.
- Strict module QML lint (`--bare --max-warnings 0`, `-M Farcaster.Client`): PASS,
  no diagnostics.
- Final focused CTest run: **7/7 PASS**, 3.30 seconds: architecture guard, async
  resource contract, software media scrolling, Metal media scrolling, media asset
  lifecycle, classification, and playback coordinator.
- UI design guard: PASS, 120 QML files audited. Data & Storage contract guard:
  PASS, including stable media geometry. `git diff --check`: PASS for tracked
  changes; the new/untracked files were reviewed directly.
- The media regressions verify zero new texture-factory uploads from a late
  completion during motion, actual GPU upload after settling, preserved ready
  images/masks, recycled-source clearing, ambient admission/freeze, stable gallery
  and video height, one shared poster request, lazy keyboard-activated playback,
  player destruction on reuse, and continued real Flickable movement while a
  nested image remains deferred.
- Six Metal screenshots were generated and visually inspected: light/dark at
  logical viewport widths 360, 640, and 1024. Artifacts are
  `build/media-scroll-verify/screenshots/video-{light,dark}-{360,640,1024}.png`.
  These use synthetic blue artwork and validate preview layout, fit, controls and
  ambient appearance, not the full live feed.

The full suite is **32/41 passing**, not clean. Its nine failures are:

| Test(s) | Observed failure |
| --- | --- |
| `localization_catalog_guard` | Catalogue consistency/unreferenced translation IDs. |
| `ui.smoke`, `ui.localization_en_smoke`, `ui.localization_fa_smoke`, `ui.appearance_dark_smoke`, `ui.appearance_light_smoke` | App smoke readiness/action checks return exit code 2 without QML warning output. The generic smoke failure also reproduces in the existing QtCreator build. |
| `ui.runtime_smoke` | Following-feed request assertion expects limit 10; the app issues limit 20. |
| `ui.emoji_runtime` | Special search does not cover the full provider catalogue. |
| `ui.emoji_runtime_metal` | The same search assertion plus animation resume, cached/local URL and bundled emoji rendering assertions. |

These failures are outside the changed media components; their tests and owning
implementation were not modified to make the suite green. This is not evidence
that every failure was reproduced on a pristine pre-change checkout.

Evidence: `build/media-scroll-verify/configure.log`, `build.log`,
`qmllint-strict.log`, `ctest-media-final.log`, and `ctest.log`.

No production feed FPS or frame-time percentile improvement is claimed. The
tests establish the scheduling/resource behavior responsible for the identified
hitches. Live-network fast scrolling across a large feed and other OS/GPU
backends still require device-level profiling.

## Follow-up: invalid delegate context (2026-09-14)

The user supplied runtime errors from the QtCreator build: delayed evaluation of
`initializePresentation` / `initializeEffect` attempted to call `resolveScrollHost`
after the delegate's QML context became invalid. This was a defect in the initial
scroll repair. Its tests kept creation contexts alive and missed this lifecycle.

The new regression creates the actual media components in a temporary
`QQmlContext`, destroys that context before the queued initialization executes,
and deliberately keeps the visual QObject alive. Before the fix it reproduces
both exact errors and fails twice; see `build/media-scroll-verify/lifecycle-before.log`.

All newly introduced `Qt.callLater` callbacks in `SuperellipseImage.qml` and
`AmbientMediaBackdrop.qml` now use component-owned zero-interval Timer signal
handlers. These preserve deferred ancestor discovery and presentation updates,
while QML disables signal execution when the context is invalidated. The existing
48 ms settling timers, request cancellation, retained images and lazy player
behavior remain in place. The regression also invalidates an already-initialized
component with a source-change update queued, covering both scheduling paths.

Follow-up commands:

```sh
cmake -S . -B build/media-scroll-verify
cmake --build build/media-scroll-verify --parallel 3
cmake --build build/media-scroll-verify --target farcaster_client_qmllint_strict
ctest --test-dir build/media-scroll-verify -R 'ui.media_scroll|media_playback_coordinator|presentation.media|async_resource|architecture_guard' --output-on-failure --no-tests=error
cmake -S . -B build/Qt_6_11_1_macOS-Debug
cmake --build build/Qt_6_11_1_macOS-Debug --parallel 3
ctest --test-dir build/Qt_6_11_1_macOS-Debug -R 'ui.media_scroll' --output-on-failure --no-tests=error
```

Verification build configure/build and strict module QML lint: PASS. Final
focused tests: **7/7 PASS**, 5.83 seconds, including software and Metal with
`QT_FATAL_WARNINGS=1`. Both UI design and Data & Storage guards also pass.
The full suite was not repeated for this follow-up; its earlier failures above
remain unresolved. Evidence: `configure-lifecycle.log`, `build-lifecycle.log`,
`qmllint-lifecycle.log` and `ctest-lifecycle-final.log` under
`build/media-scroll-verify`.

The user's QtCreator build directory was also configured and fully rebuilt:
PASS, including relinking `build/Qt_6_11_1_macOS-Debug/bin/farcaster_client`.
Its software and Metal media regressions both pass (**2/2**, 4.91 seconds).
Evidence in that directory: `configure-media-lifecycle.log`,
`build-media-lifecycle.log`, and `ctest-media-lifecycle.log`. A currently running
app process must be relaunched to use the rebuilt executable.

## Follow-up: late geometry and delegate reconstruction (2026-09-15)

The actual `CastEmbedsPanel` still opted single-image galleries into decoded
aspect discovery. Testing the standalone gallery's default had missed that
caller override. Feed panels now disable it, retain initially known dimensions,
and reserve a 4:3 fallback when dimensions are absent. Late metadata keeps the
reservation for the same media identity; the image fits inside that frame.

Link previews reserve their artwork and text slots from their initial URL/cached
metadata. Generic links, video-site previews, and Mini Apps no longer grow when
artwork or long titles arrive, or collapse on timeout/failure. Collectible cards
reserve artwork and bounded text/chip slots as well. Direct video previews keep
their initial aspect. A link subsequently recognized as a direct image/video
retains its original height while changing to the correct renderer. URL reuse
resets the reservation; responsive width changes still recompute layout.

The native image surface supplies loading, failure and manual-download states;
the gallery's overlapping placeholder that hid those controls was removed.

A full application fixture with 60 alternating direct images and links exposed
another cost in `sample`: `QQuickListView` reuse triggered synchronous QML
construction in the animation step. Two related changes address this:

- `FeedSurface` has distinct delegate choices/pools for single images, galleries
  and other rows. Model reconciliation and provider order remain unchanged.
- `CastEmbedsPanel` uses count-based supporting slots, binding each slot to its
  current embed. Assigning a fresh JS-array model previously regenerated every
  preview. `EmbedPreview` retains a compatible Loader renderer on URL reuse;
  it no longer toggles it inactive before reclassification. Promotion height is
  retained only for classification of the same URL.

The new supporting-link identity regression failed before these latter changes
and passes afterwards: the original QObject survives and displays the new URL.
Evidence: `ctest-retention-red.log` / `ctest-retention-green.log`. The production
FeedSurface test also scrolls through 40 mixed rows, observes actual pooled
object reuse, and checks that reused objects retain their media layout family.

Geometry regressions cover the actual feed panel, cold metadata followed by long
text and portrait artwork, image completion, timeout, missing art, provider
failure, URL reuse, known initial dimensions and late direct-media promotion.
Link and collectible layouts are exercised at widths 360/640; screenshots cover
generic links, video links, Mini Apps and collectibles in both themes. Artifacts
are under `build/media-scroll-verify/screenshots`.

The full-app HTTP fixture uses an isolated database/settings file and synthetic
images. The production resolver rejects loopback link metadata by design, so
that run exercises actual image downloads and unresolved link cards. Delayed
resolved link metadata is covered by controlled regression inputs instead.
Public guest discovery was unavailable during this session. These are explicit
limits of the end-to-end verification, not a live-feed FPS guarantee.

### Hidden controls found by QML profiling

An instrumented full-app trace (`scroll-native-qml.qtd`) showed synchronous
creation of hidden cast menus and button tooltips while new feed delegates were
being constructed. The trace contained 720 Superellipse creations, 37 ActionButton
creations and 84 Popup creations in that scrolling capture. These counts include
nested controls; they are not separate frame-time totals or an FPS measurement.

`CastItem` now creates its action menu on explicit interaction and releases it
when the cast identity changes. `ActionButton` creates its tooltip after the hover
delay. The tooltip also uses the actual attached Window and the button's Overlay,
fixing invalid window/overlay lookups exposed by the interaction regression.
No scroll buffer enlargement is included: the existing bounded cache contract
remains intact. Temporary profiling code and its loopback debug listener were
removed from the application before the final builds.

Tests verify absent idle menus/tooltips, menu opening, correct cast action routing,
destruction on reuse, and tooltip opening/closing on real mouse hover. The Metal
fixture now exposes its window and waits for actual scroll movement and layout
polish rather than assuming both happen within 100 ms. Images are still held
pending while reserved geometry is checked.

The shared image surface now also creates its superellipse mask only after the
decoded image is admitted. Pending cards therefore retain a plain stable
placeholder without constructing hidden shape/layer geometry. Border geometry
and policy-blocked manual-download controls are likewise instantiated only when
visible. Regressions verify that the mask is absent while pending, appears after
settling, and that the lazy manual-download control still accepts a real tap.

The final visual run keeps one fixed test window while rendering the responsive
cards. This avoids changing the native Metal window repeatedly during capture.
The resulting light/dark screenshots were visually checked for containment,
reserved media size, controls, loading state and text elision.

Final verification after all follow-ups: the focused suite is **7/7 PASS** in
`build/media-scroll-verify` (software and Metal media scrolling, async-resource
and Explorer guards, media lifecycle/classification/playback), and both media
scroll tests are **2/2 PASS** in the user's `build/Qt_6_11_1_macOS-Debug`
directory. The complete builds and strict QML lint pass. UI design and Data &
Storage guards pass. Evidence: `build-final.log`, `qmllint-final.log`,
`ctest-final.log`, and the matching `build-final.log` / `ctest-final.log` in the
Qt Creator build directory.

The final isolated full-app run scrolled across delayed direct images and link
cards with no QML `ReferenceError`, `TypeError`, invalid-context diagnostic or
other runtime warning. The visible loading frame and the decoded portrait image
occupied the same reserved card area. The test server and application were
stopped after verification.

## Follow-up: stable loading identity (2026-09-21)

Fast scrolling exposed a visual state transition that the earlier geometry and
upload tests did not cover. `SuperellipseImage` deliberately replaced its
spinner with an image glyph while the scroll host was moving. It also stopped
reporting loading between the moving signal becoming false and the 48 ms source
admission timer firing. Link previews had the same gap before their debounced
metadata request began, and their metadata timeout could expire during a fling.

The image surface now treats every non-empty source with no admitted active
source as waiting for admission. The large-media spinner remains the visible
placeholder through motion and the settle delay; its rotation stops during the
gesture so it does not schedule animation frames. The fallback image glyph is
reserved for compact/reduced-motion presentation or a terminal link-preview
state and is no longer selected merely because scrolling is active.

Link metadata waiting, including scroll deferral and request debounce, is one
continuous loading state. Metadata and artwork deadline timers stop during
motion and resume afterwards. Metadata source handlers now observe the writable
external/resolved properties directly, eliminating a `previewData` binding loop
found by the fatal-warning regression.

`ui.media_scroll` now asserts the visible spinner, hidden fallback glyph and
stopped animation during motion, then checks the identical placeholder in the
post-fling admission/debounce interval. It also exercises an unresolved link
created while its scroll host is moving.

Final verification for this follow-up: strict QML lint passes; the focused suite
is **7/7 PASS** in `build/media-scroll-verify`; the rebuilt Qt Creator directory
passes both software and Metal media-scroll tests (**2/2**). The UI design guard
audited 120 QML files and the Data & Storage contract guard also passes.

## Follow-up: failed and stalled images (2026-09-21)

Responsibility: keep terminal image failures quiescent and genuine pending
requests visibly active. Capability origin: Local, mandatory shared media UI.
The public loading/failure/manual-download contract and per-URL layout remain
the same. Dependencies are the existing QML surface and provider. Expected
HTTP/decode errors become stable placeholders after provider retry; slow images
keep progress until the provider completes. State remains transient per source;
offline cache, download policy, provider routing and failover are unchanged.
The regression boundary is a batch of failed sources, held image responses and
actual Flickable movement with no provider response available.

The previous follow-up still stopped progress animation during motion and kept
a redundant QML retry timer. The provider already retries transient reads and
negatively caches terminal URL failures for 30 seconds. The surface's 420 ms
retry timer recreated requests inside that backoff window and repeatedly switched
failure back to loading. The new batch regression reproduced these transitions
for 12 failed images before the fix. QML no longer retries terminal failures;
source changes, policy changes and explicit manual download retain their existing
admission behavior. The provider still owns its bounded transport retries.

The image and link spinners now continue their render-thread RotationAnimator
during a fling. This supersedes the stopped-animation behavior described above.
Link artwork also no longer has a separate five-second visual deadline: that
deadline replaced progress with a static image glyph while the ten-second HTTP
request and its retry could still be pending. Actual image completion/failure
now owns the transition. Metadata without an image URL retains its bounded
deadline, which does not start when the card is created during motion.

Regression coverage now includes zero extra QML requests and zero loading-state
changes for 12 failed images across repeated scroll/idle cycles; all three link
layouts retain animated progress after 5.2 seconds with responses held pending;
a real Flickable advances more than 100 pixels with those responses still held.
Before the fix, the new failure/progress assertions reported four failures.

An isolated full-app HTTP fixture served 60 image posts alternating HTTP 404,
18-second delayed responses and two-second successful images. Scrolling reached
post 37 across failed and pending cards without a QML warning. A 12-second macOS
sample captured image decoding on worker threads rather than the UI thread;
this is not a full frame-time or live-network performance guarantee. Evidence:
`build/media-scroll-verify/scroll_failure_fixture.py`,
`fixture-failures-runtime.log`, and `scroll-failures-sample.txt`. The fixture app
and server were stopped after inspection.

Configure and build succeeded in both `build/media-scroll-verify` and
`build/Qt_6_11_1_macOS-Debug`. The verification build passed strict module QML
lint and all seven focused tests, including Software and Metal scrolling with
fatal QML warnings enabled (34.65 seconds total). UI design, Data & Storage
guards and tracked-diff whitespace checks pass. Logs use the suffix
`-failed-images.log` in the corresponding build directories. No full-suite
pass is claimed; the earlier unrelated failures remain outside this repair.
The Qt Creator build also passes both scrolling tests (**2/2**, 35.44 seconds),
and its `bin/farcaster_client` was relinked with the final QML changes.


## Follow-up: pointer interaction and ambient capture (2026-09-21)

Responsibility: keep the static artwork shadow resident across scroll/idle and
pointer transitions. Capability origin: Local; mandatory shared media surface,
with the existing optional ambient decoration. Public contract: existing source,
active, geometry and appearance properties. Canonical invariant: a shadow matches
its current static image and refreshes for source/geometry changes, while motion
alone cannot restart its capture. Dependencies: existing Qt Quick effects and
image surface. Missing/failed images have no active shadow. State is transient;
offline cache, provider policy/disablement, routing and failover are unchanged.
Tests exercise deferred admission, GPU pixels, source replacement, resizing and
repeated pointer/scroll transitions.

`GalleryTile` does not start work on hover. The live ambient source and blur layer
previously toggled their capture state on every scroll stop. The new regression
failed against that behavior before the change (`ctest-hover-red.log`).
`AmbientMediaBackdrop` now keeps its media snapshot non-live, coalesces explicit
refreshes for source/geometry changes, and retains the snapshot across scrolling.
The small blur canvas retains its normal dirty-content updates without toggling
its layer's live state. Both offscreen surfaces used for the blur use one sample rather
than inheriting the fidelity profile's multisampling budget. Blur geometry and
colour falloff remain unchanged.

The Metal regression checks actual blue initial pixels and red replacement
pixels, not only QML readiness. It also checks resize refresh, zero capture-live
transitions, zero scheduled recaptures and no new provider requests during four
scroll/settle/pointer cycles. A real web LinkPreview also receives a mouse-enter
event: its hover colour must appear without another artwork capture or image
request. Failed-image cycles now include pointer movement as well.
GPU assertions explicitly render a frame through
`QQuickWindow::grabWindow()` because a desktop test can be occluded while its QML
event loop keeps running. Earlier passive waits intermittently received no GPU
frames (`ctest-hover-repeat.log`); those runs were failures, not successful
verification. Qt documents that window grabbing can render a non-visible window:
https://doc.qt.io/qt-6/qquickwindow.html#grabWindow

An isolated full-app HTTP fixture was scrolled and sampled on macOS before the
change. The sample shows UI/render synchronization and worker image decoding,
but does not establish the user's severe pointer-triggered hang. In particular,
Metal drawable/vsync waits are not evidence of a deadlock. This repair removes a
verified redundant rendering path; no claim is made that it conclusively fixes
every reported live-feed freeze. Sampling evidence is in
`hover-active-before-sample.txt` and `fixture-hover-runtime.log` under the
verification build directory. A separate fixture shutdown wait in the live
market worker was observed and is outside this media rendering change.

Final verification for this follow-up: configure/build succeeded in both build
directories; strict module QML lint passed. The verification build passes all
seven focused tests (38.94 seconds, `ctest-hover-final.log`). The Qt Creator
build passes both software and Metal media-scroll tests (38.03 seconds,
`ctest-hover.log`), including actual link-hover feedback. Before adding that
last interaction assertion, the explicit-frame Metal pixel tests passed three
consecutive runs (58.64 seconds, `ctest-hover-frames.log`). UI design and Data &
Storage guards pass; the tracked diff and the three edited files have no
whitespace errors. Complete build logs are `build-hover-complete.log` and
`build-hover.log` in their respective build directories. The isolated fixture
app and HTTP server are stopped. No complete repository test-suite pass or
live-feed frame-time guarantee is claimed.

## Follow-up: asynchronous rows and scroll-safe media effects (2026-09-22)

Responsibility: keep feed delegate construction and decorative image effects out
of the active scroll frame. Capability origin: Local. This is mandatory for the
shared feed UI; ambient colour blur remains optional decoration. Public feed,
media, navigation and rendering-profile contracts remain unchanged. Canonical
invariants: an incubating row has non-zero reserved geometry; a ready row uses
its measured height; scrollable media does not allocate ambient blur/capture
textures; frame telemetry cannot resize window-wide image or icon resources.
Dependencies remain Qt Quick Loader/ListView, the shared media surface and the
Qt presentation rendering controller. Failed row incubation leaves its reserved
slot rather than collapsing the list. State is transient and device-local.
Offline behavior, provider policy, cancellation, retry and failover are
unchanged. Regression coverage uses the production feed and media components in
both software and Metal renderers.

`FeedSurface` now incubates non-section row subtrees asynchronously. Its outer
delegate calculates a bounded reservation from width, text, presentation kind
and declared media metadata, so the ListView never receives a zero-height row
while Qt spreads object construction across frames. When incubation completes,
the row switches to the component's real implicit height once the ListView is at
rest, so completion cannot move rows during a flick. Existing delegate
families, reuse pools and stable model identity are retained.

`AmbientMediaBackdrop` now detects an owning Flickable/ListView and does not
instantiate its `MultiEffect` or `ShaderEffectSource` tree in that context.
This removes both decorative offscreen passes from feed cards, including the
post-fling allocation burst. Static, non-scrolling media can still opt into the
existing frozen snapshot. The effect subtree itself is lazy and preserves the
original padded blur-canvas geometry.

Adaptive frame telemetry now has a narrow notifier for effective quality and
energy labels. Effect texture size, SVG raster size and geometry scale stay
fixed throughout an Adaptive session; only an explicit Efficiency/Fidelity
profile change can resize those resources. This prevents a slow-frame streak
from triggering a window-wide texture/icon rebuild while preserving visible
status and drawer-quality updates.

The regression asserts that production feed delegates are asynchronous, retain
positive reservation and row height, establish a usable scroll range and still
exercise pool reuse. It also asserts that scrollable media constructs no ambient
effect/capture objects. The static-effect branch continues to verify actual GPU
pixels, source replacement and geometry refresh. GPU waits explicitly request a
frame because asynchronous QML incubation does not advance in an occluded Metal
test window from event processing alone.

Configure, application/test build and strict QML lint pass in
`build/media-scroll-verify`. All seven focused tests pass (38.41 seconds),
including software and Metal media scrolling; evidence is
`ctest-two-fixes-final.log` and `build-two-fixes-final.log`. The Qt Creator build
was rebuilt and its two scrolling tests pass (35.93 seconds) in
`build/Qt_6_11_1_macOS-Debug/ctest-two-fixes.log`. UI design, Data & Storage and
tracked-diff whitespace guards pass. No complete repository-suite or live-device
frame-time claim is made.

## Follow-up: multi-image gallery frame admission (2026-09-23)

Responsibility: prevent adjacent decoded images from entering one Qt Quick
scene-graph synchronization and stalling pointer/scroll processing. Capability
origin: Local. This is mandatory shared media presentation. The public cast,
gallery, loading, cache and navigation contracts remain unchanged. Canonical
invariants: gallery geometry is reserved before pixels arrive; multi-image tiles
do not allocate per-image offscreen mask/ambient effects; large decoded surfaces
become visible at no more than one per rendered frame; stale source generations
cannot make replaced media visible, and only the latest queued generation is
retained for a recycled delegate. Dependencies are Qt Quick's
asynchronous Loader/Image path, the remote-image provider and the presentation
rendering controller. Recoverable HTTP/decode failures keep the existing quiet
sentinel and stable placeholder. Local state consists only of bounded live QML
delegates and weak QPointer presentation requests. Offline/cache hits use the
same visibility pacing. Download policy, provider routing,
disablement and failover remain unchanged. Tests cover simultaneous adjacent
responses, actual render-pass separation, effect allocation and both
software/Metal.

The reproduced two-image cast had three compounding costs. `Repeater` completed
every `GalleryTile` synchronously; every ready tile created a full image layer,
mask layer and `MultiEffect`; even invisible loading/failure `Icon` objects owned
their own colourization effects. Two network or RAM-cache completions could then
publish together and cause both textures and effects to enter the same render
sync. The apparent mouse hang was the GUI/render synchronization, not a hover
handler in the tile.

Multi-image `GalleryLayout` delegates now keep their precomputed slots while
asynchronous Loaders incubate the heavy tile trees. Their requests retain a small
per-index start offset. `GalleryTile` uses the direct Image scene-graph texture
path for multi-image casts, with a vector inverse corner cover instead of a
full-size mask texture; ambient capture remains available only for a single
image. Loading and failure glyphs are native text glyphs, so hidden states no
longer instantiate SVG source textures or `MultiEffect` objects.

Request staggering alone cannot prevent two late responses from converging.
The provider therefore retains its two-worker bounded decode pool, while a
window-frame coordinator owns the decisive scene-graph boundary regardless of
where a response came from. Decoded large Images remain hidden, keep the stable
spinner visible, and receive one visibility grant at a time. Each grant publishes
a monotonically increasing presentation generation. A direct render-thread
`beforeSynchronizing` connection snapshots that generation when the scene graph
actually observes it; the following direct `afterRendering` connection queues an
acknowledgment carrying the synchronized generation back to the GUI thread. The
controller grants the next image only when that acknowledgment covers the
outstanding grant. Queued render notifications from older frames carry an older
generation and therefore cannot release another image. Delegate generations
also reject grants for replaced sources. This prevents a response backlog from
making several large texture nodes visible in one frame without serializing disk
reads or image decodes behind one worker.

The regression deliberately completes two adjacent 640x360 responses in one
turn. It asserts two asynchronous tile Loaders, stable geometry, direct texture
mode, zero active mask loaders, zero `MultiEffect` instances, and distinct
presentation sequence numbers for the two visibility grants. It also records the
window's actual `afterRendering` pass count and requires that count to increase
between the first and second grant. The Metal branch renders/grabs frames
explicitly so occlusion cannot turn a missing desktop frame into a false pass.

Final verification: complete application/test builds and strict zero-warning QML
lint pass in both `build/media-scroll-verify` and the user's
`build/Qt_6_11_1_macOS-Debug`. The verification directory passes the async
resource guard and software/Metal scroll regressions (**3/3**, 39.84 seconds).
The Qt Creator directory passes its software/Metal regressions (**2/2**, 39.28
seconds). UI design (120 QML files), Data & Storage, async-resource and
whitespace guards pass. The general architecture guard still reports the
pre-existing `CMAKE_CXX_STANDARD 23` in the unrelated untracked raremint-signer
icon asset; this media repair does not change that tool. The older full-app smoke
probe also returns its existing interaction-check code 2 in the pre-change Qt
Creator binary, so it is not counted as evidence for or against this repair. No
application or test process remains running.
