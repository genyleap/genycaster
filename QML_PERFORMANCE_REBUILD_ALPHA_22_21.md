# Genycaster alpha.22.21 — QML Performance & UI Runtime Rebuild

## Scope

This review and rebuild targets `genycaster-src-alpha.22.21` with emphasis on Qt Quick/QML runtime cost, UI-thread work, hot timeline delegates, geometry ownership, adaptive layout behavior, offscreen rendering, effects, startup construction, hidden-surface activity, and repository UI contracts.

The source was changed directly; this is not a review-only report.

## Authority and inputs

The implementation followed the repository's own authority chain first, then the supplied QML reference companion:

1. `AGENTS.md` and repository-local contracts/checkers.
2. Existing Genycaster architecture/performance/UI documentation.
3. Supplied `QML_REFERENCE.md`, which summarizes upstream Qt Quick/QML guidance.

Repository note: `AGENTS.md` routes Qt Quick work through `docs/agent/QT_QUICK_UI.md`, but that file is not present in the alpha.22.21 package. No replacement policy was invented. Where the routed guide was unavailable, explicit repository rules/checkers and the supplied reference were used conservatively.

The project currently requires Qt 6.6 in the root `CMakeLists.txt`. The supplied QML reference was last reviewed against Qt 6.11.2, so no change in this rebuild depends on a newly introduced 6.11-only QML API.

## Primary findings

### P0/P1 — Hot feed delegate construction

The feed delegate constructed both the semantic cast row and profile row object trees and then selected one using visibility. In a virtualized timeline this multiplies delegate object count and binding work.

**Rebuild:** `FeedSurface.qml` now uses a branch `Loader` so only the required row implementation is instantiated. `SemanticFeedItem` consumes the feed's already-computed presentation classification rather than rescanning the cast.

### P1 — Repeated feed/embed classification on the GUI thread

Feed and cast presentation logic repeatedly traversed embeds to infer media/art/developer classifications. Preview-ready bursts could also trigger repeated semantic synchronization.

**Rebuild:** classification is consolidated, presentation kind is propagated with stable rows, preview synchronization is debounced/coalesced, and inactive feed surfaces do not continuously resynchronize hidden presentation state.

### P1 — Heavy subtrees eagerly created inside `CastItem`

`RichCastRenderer` and several `CastEmbedsPanel` branches were created even when their visual branch was inactive.

**Rebuild:** the large mutually-exclusive branches are lazy-instantiated. Embed partitioning is performed once and then split into visual/supporting lists.

### P1 — Offscreen blur remained live during scrolling

`AmbientMediaBackdrop` exposed `deferWhileScrolling`/`scrollHost`, but the expensive path did not actually use that information. `BottomNavigation` also had a blur/offscreen path that did not consume the adaptive rendering budget.

**Rebuild:** backdrop blur/offscreen capture suspends during scroll when configured, and expensive blur paths honor `RenderingController.effectScale`. The low-cost surface remains visible, so disabling the expensive path does not remove navigation/background structure.

### P1 — Startup constructed large hidden routes and overlays

Several route surfaces and very large settings overlays were constructed at shell startup even though they were hidden.

**Rebuild:** route and overlay construction now follows use frequency:

- large infrequent routes/overlays are lazy;
- feed/profile/detail are lazy on first use and retained afterward where preserving navigation/scroll state is valuable;
- settings/data-storage overlays are created only on first request;
- contextual side content is created only when its layout/route needs it.

This uses `Loader` for genuinely substantial optional surfaces, not as a blanket wrapper around tiny repeated elements.

### P1 — Hidden feed continued doing work

Keeping a feed surface alive preserves useful scroll/state, but hidden state still received preview/model events and could prime media or schedule pagination.

**Rebuild:** `presentationActive` gates semantic resync, art preview priming, metadata-driven refreshes, and load-more scheduling while the retained surface is not active. On reactivation, pending synchronization resumes.

### P1/P2 — Unnecessary offscreen layers in reusable visual components

`SuperellipseImage` could keep mask-related layer work active outside the precise renderable/visible state. `TokenSparkline` used an explicit multisampled offscreen layer despite already being rendered by a `Shape` geometry path.

**Rebuild:** superellipse mask layers are conditional on actual visibility/renderability, and the sparkline draws directly without a dedicated offscreen multisampled layer.

Static literal `layer.enabled: true` occurrences in `ui/components` were reduced from 10 to 7. This is a structural indicator only; it is not a measured GPU-time result.

### P2 — Unnecessary clipping

`TokenAddressPreview` used rectangular clipping despite its composition not requiring that clipping boundary.

**Rebuild:** the clip was removed. Total literal `clip: true` usage in `ui/components` changed from 40 to 39. Remaining clipping was not removed blindly because clipping can be semantically required.

### P2 — Loader initialization caused transient wrong branch creation

`EmbedPreview` defaulted to a link classification before async/imperative classification completed. That could transiently instantiate a heavy `LinkPreview` even for image/video/token/collectible embeds, then replace it.

**Rebuild:** the content loader remains inactive until classification is ready, metadata refreshes are coalesced, and `previewReady` updates classification directly without an extra re-query path.

### P2 — Layout/RTL/design-contract failures

The alpha.22.21 baseline UI checker reported missing explicit layout directions, legacy radius usage outside the project's shape system, a timeline divider contract mismatch, mobile navigation contract mismatches, and two raw-delimiter false positives caused by regex text.

**Rebuild:** these were corrected while preserving presentation semantics. The final UI checker passes all 120 audited QML files.

## Files changed

- `ui/components/AmbientMediaBackdrop.qml`
- `ui/components/AppShell.qml`
- `ui/components/AppsSurface.qml`
- `ui/components/BottomNavigation.qml`
- `ui/components/CastItem.qml`
- `ui/components/DetailSurface.qml`
- `ui/components/EmbedPreview.qml`
- `ui/components/FeedSurface.qml`
- `ui/components/GenyVideoPlayer.qml`
- `ui/components/HomeDashboard.qml`
- `ui/components/SemanticFeedItem.qml`
- `ui/components/SuperellipseImage.qml`
- `ui/components/TokenAddressPreview.qml`
- `ui/components/TokenSparkline.qml`

No C++ source was changed in this pass.

## Structural before/after indicators

For the 117 QML files under `ui/components`:

| Static indicator | Original | Rebuilt | Interpretation |
|---|---:|---:|---|
| `clip: true` | 40 | 39 | One unnecessary clip removed; required clips retained |
| literal `layer.enabled: true` | 10 | 7 | Fewer unconditional offscreen layers |
| `Loader` blocks | 13 | 28 | Intentional increase to replace eager heavy branches/routes |
| `MultiEffect` blocks | 7 | 7 | Effects not mechanically deleted; activation is more selective |
| `ShaderEffectSource` blocks | 3 | 3 | Existing effects retained but expensive live paths are gated |
| `ListView` blocks | 12 | 12 | No virtualization regression |
| `Repeater` blocks | 53 | 53 | No broad structural expansion |

These are static source metrics, not FPS/GPU measurements. The important runtime change is that many expensive trees represented by those existing components are no longer simultaneously instantiated or continuously active.

## Repository verification

Final repository checks run on the rebuilt tree:

```text
UI design guard passed: 120 QML files audited.
Architecture guard passed.
Async resource contract guard: PASS
Data & Storage contract guard: PASS
Explorer semantic/source contract guard: PASS
Media upload contract guard: PASS
vNext release contract: PASS
R7 release contract: PASS
```

The localization checker is **not clean in the original package** and remains unchanged by this rebuild. Original and rebuilt outputs are byte-identical: 135 pre-existing diagnostics after the `FAILED` header, including source-text `qsTr` usage and untranslated/unreferenced translation IDs. This pass deliberately did not hide or rewrite that unrelated baseline.

## Qt toolchain limitation in this environment

The repository already declares the QML module using `qt_add_qml_module(...)` and provides `farcaster_client_qmllint_strict`, which is the correct integration path for the Qt Quick Compiler/QML tooling.

However, this execution environment does not contain a Qt SDK:

```text
qmllint: unavailable
qmlformat: unavailable
qt-cmake: unavailable
qmake: unavailable
```

A CMake configure attempt cannot resolve Qt 6 (`Qt6Config.cmake` / `qt6-config.cmake` is unavailable). Therefore this report does **not** claim that Qt's parser/compiler, `qmllint`, runtime smoke tests, QML Profiler, or device/GPU frame measurements were executed here.

That distinction matters: the changes remove known structural costs and pass the repository's own guards, but a numeric FPS/frame-time claim requires a Qt-enabled build on representative hardware.

## Required device/toolchain qualification

On the normal Qt development machine, perform the following before merging:

```bash
cmake --preset ui-debug
cmake --build --preset ui-debug
cmake --build --preset ui-debug --target farcaster_client_qmllint_strict
ctest --preset ui-debug --output-on-failure
```

Then profile representative Home/feed/detail/navigation interactions with Qt Creator's QML Profiler. Inspect component creation, JavaScript/binding time, animations, and scene-graph frames during fast flicking and route transitions.

For renderer diagnosis, use the repository's normal debug launch path with Qt scene-graph diagnostics such as:

```bash
QSG_RENDERER_DEBUG=render <application>
```

Do not ship renderer-debug variables as production configuration.

Qualification should cover at least:

- cold launch and warm resume;
- Home first paint;
- fast timeline flick with mixed text/image/video/token/collectible embeds;
- repeated Home → Profile → Detail → Home navigation;
- opening/closing Search, Composer, Settings, Data & Storage, Notifications, and Apps;
- adaptive efficiency/fidelity transitions;
- compact/standard/wide window widths;
- LTR and RTL UI direction;
- 1x/2x high-DPI paths;
- 60 Hz and, where product hardware supports it, 120 Hz frame stability.

## Residual performance decisions

Some expensive primitives remain intentionally:

- Superellipse image masking still uses an offscreen path when an actual clipped image needs that shape; it is now inactive when not needed.
- `VectorStroke` retains its explicit layer path because it is not a primary timeline hot path and should be changed only with visual/GPU evidence.
- `AccentColorStudio` retains its effect/layer design because it is an infrequent settings surface and repository checks require its current shell contract.
- Composer/Search are not blindly unloaded; first-interaction latency and state retention matter for frequent actions.

The next optimization pass should be profiler-led rather than based on deleting every effect, clip, layer, or loader mechanically.

## Result

This rebuild attacks the highest-confidence Qt Quick costs in alpha.22.21:

- smaller hot delegate object graphs;
- fewer repeated JavaScript scans;
- less hidden-surface work;
- less startup construction;
- selective offscreen/effect activation;
- no transient wrong preview branch;
- reduced unconditional layer use;
- corrected RTL/layout/navigation/design contracts;
- repository guards clean for the touched UI/runtime contracts.

Actual frame-time and startup-time deltas remain to be measured with a Qt-enabled build on target hardware.

## Qt 6.11.1 runtime qualification hotfix

A real macOS Qt 6.11.1 launch exposed a QML type-load failure in `CastItem.qml`:

```text
Invalid property assignment: "implicitHeight" is a read-only property
```

The rebuild had added five bindings of the form `implicitHeight: item ? item.implicitHeight : 0` directly to `Loader`. `Loader` is implemented through Qt's implicit-size item machinery, where `implicitWidth` and `implicitHeight` are read-only and are maintained from the loaded item by `QQuickLoader` itself. Those bindings were therefore both invalid and redundant.

The correction removes all five direct Loader implicit-height assignments and relies on Loader's native intrinsic sizing together with `Layout.fillWidth`/margins. `tools/check_ui_design.py` now rejects direct `implicitWidth` or `implicitHeight` assignments on a `Loader`, so this failure class is caught before runtime.

Post-hotfix repository gates:

```text
UI design guard passed: 120 QML files audited.
Architecture guard passed.
Async resource contract guard: PASS
Data & Storage contract guard: PASS
Explorer semantic/source contract guard: PASS
Media upload contract guard: PASS
vNext release contract: PASS
R7 release contract: PASS
```

The macOS Qt 6.11.1 runtime launch remains the authoritative next smoke test because this execution environment still does not provide a Qt SDK.

## R3 visual-fidelity and interaction follow-up

The R3 follow-up was driven by real macOS/Qt 6.11.1 interaction screenshots after the R2 runtime hotfix. The goal is not to trade visual quality for lower GPU cost; it preserves the product's media depth while removing avoidable work and geometry ambiguity.

### Full-screen media gallery

`MediaGallery.qml` now treats the entire popup content viewport as the media stage. The previous top and bottom reserved bands were removed, and counter/close/navigation/thumbnail chrome is composited above the image instead. Full media uses `Image.PreserveAspectCrop`, so the image covers the viewport without aspect-ratio distortion and the close control remains physically over the media.

The full-resolution gallery path is also lazy. Only the current page and one neighbor on each side instantiate the full-screen `Image`; thumbnails remain independently bounded. This avoids eagerly decoding every gallery item into a full-screen texture when a multi-image cast is opened. Gallery open/close opacity/scale motion now uses `OpacityAnimator` and `ScaleAnimator`, while reduced-motion behavior remains unchanged.

### Stable chromatic media shadow while scrolling

R2 incorrectly treated `AmbientMediaBackdrop.deferWhileScrolling` as permission to hide the expensive effect. That reduced work but caused the colorized media shadow to visibly disappear during a fling.

R3 keeps the effect visible. During scrolling, the existing source/layer texture is frozen (`live: false`) and moves with its delegate; when scrolling settles, capture resumes. Adaptive rendering now scales only the offscreen sampling resolution. This preserves visual continuity while avoiding continuous source recapture and blur regeneration during fast scrolling.

The mobile navigation blur follows the same principle: adaptive quality scales its capture texture rather than removing the glass effect. Both existing `ShaderEffectSource` instances that do not have a recursive source dependency now explicitly use `recursive: false`.

### Mobile Mini Apps destination

The compact mobile dock is now:

```text
Home | Explorer | Create | Apps | Settings
```

The previous Profile destination was replaced with the existing Apps route and `Theme.iconApps`. Desktop/profile navigation remains intact. The release/UI contract checks and production UI documentation were updated to match the intended mobile route contract.

### Settings horizontal-drift correction

The Settings screenshot showed vertical pages shifted/cropped horizontally after left/right trackpad or wheel input. The root cause was a vertical `ScrollView` configuration that hid the horizontal scrollbar while leaving `contentWidth` implicit. That allowed oversized implicit content to create a horizontal scroll range with no horizontal affordance.

All four Settings vertical pages now bind `contentWidth: availableWidth`. The same correction was applied to other unambiguously vertical views (`DataStoragePopup`, the account popup scroll in `AppShell`, `SocialSidebar`, and `ContextRail`). `tools/check_ui_design.py` now rejects a vertical `ScrollView` that permanently hides its horizontal scrollbar without explicitly constraining `contentWidth`; viewers that deliberately switch to `ScrollBar.AsNeeded` remain allowed.

### R3 static effect indicators

Compared with the R2 tree:

| Static indicator | R2 | R3 | R3 intent |
|---|---:|---:|---|
| `recursive: true` in QML | 2 | 0 | Remove unnecessary recursive ShaderEffectSource backing textures |
| `Loader` blocks in `ui/components` | 28 | 29 | One targeted lazy full-screen gallery page loader |
| `clip: true` in `ui/components` | 39 | 39 | No mechanical clip removal; semantically required clips retained |
| `layer.enabled` assignments | 13 | 13 | No visual layer removed without runtime evidence |
| `ShaderEffectSource` blocks | 3 | 3 | Same effects, cheaper/non-recursive capture behavior |

These remain structural indicators, not substitutes for QML Profiler or GPU frame measurements.

### R3 verification

Repository gates after the R3 changes:

```text
UI design guard passed: 120 QML files audited.
Architecture guard passed.
Async resource contract guard: PASS
Data & Storage contract guard: PASS
Explorer semantic/source contract guard: PASS
Media upload contract guard: PASS
vNext release contract: PASS
R7 release contract: PASS
```

The localization checker still fails on the same pre-existing catalog diagnostics as R2; baseline and R3 checker output are byte-identical. No new localization diagnostic was introduced by replacing the mobile Profile destination with the already-existing Apps translation id.

This environment still has no `qmllint`, `qmlformat`, `qt-cmake`, or `qmake`, so macOS Qt 6.11.1 runtime + `farcaster_client_qmllint_strict` remain mandatory qualification steps for this R3 package.

## R4 image-fit and Home-layout correction

R4 supersedes one R3 visual decision after macOS/Qt 6.11.1 runtime screenshots showed that a full-screen media viewer must prioritize complete-image visibility over viewport coverage. R3 used `Image.PreserveAspectCrop` for the full-screen gallery. R4 changes the viewer to `Image.PreserveAspectFit`: user media is never cropped, aspect ratio is preserved, and any remaining letterbox area belongs to the gallery background rather than to an enlarged/cropped image.

### Full-screen media: complete image, image-relative chrome

`MediaGallery.qml` now uses `Image.PreserveAspectFit` for the full-screen source. Decode size remains bounded with `sourceSize` and the current adaptive media-decode scale, so preserving the complete source does not reintroduce unbounded texture allocation.

The overlay chrome is also aware of the actual painted image rectangle. `paintedWidth`/`paintedHeight` are converted into a `paintedRect`, and the counter/Close row is positioned against that rectangle when it is large enough. This keeps the Close button visually on the image when a wide or tall source produces letterboxing. The existing current/previous/next lazy full-resolution policy remains intact.

The project image-presentation contract is now semantic rather than global:

- full-screen media, readable Mini App artwork, poster-first link previews, and video posters use `Image.PreserveAspectFit`;
- avatars, profile/banner covers, tiny thumbnails, multi-image feed tiles, and ambient/effect filler surfaces may use `Image.PreserveAspectCrop` where intentional cropping is part of the component contract;
- user-content surfaces do not use `Image.Stretch`.

`tools/check_ui_design.py` now requires every project-owned `SuperellipseImage` call site to declare `fillMode` explicitly, preventing the component's legacy crop default from silently leaking into a content surface after future refactors.

### Mini App and poster preview fitting

`LinkPreview.qml` now derives `previewImageAspect` from preview metadata when available. Mini App discovery posters and rich poster-first link previews use that aspect to select a bounded frame height and render with `Image.PreserveAspectFit`. Their existing ambient backdrop fills unused frame area, so complete artwork/text remains readable without introducing a hard empty card surface.

`GenyVideoPlayer.qml` uses the same fit policy for its pre-playback poster, matching the player's existing aspect-preserving video output instead of showing a cropped poster that changes composition when playback starts.

Mini App/resource icon call sites in `AppsSurface.qml`, `TaskManagerPopup.qml`, and `ContextRail.qml` now declare `Image.PreserveAspectFit` explicitly. `AppsSurface` also drops a redundant outer `clip: true` around `SuperellipseImage`; the image component already owns its shape mask, so the delegate no longer adds a second renderer clip for the same visual boundary.

### Home wide-layout simplification

The duplicate Home `Recent Activity` widget column is removed. Runtime screenshots showed that it could collapse into a narrow strip between the main Home composition and the context rail. Removing it also deletes its feed scan, deduplication JavaScript, repeater, and row object graph. The right context rail remains a separate product surface, but Home now enables it only when the existing `contextRailFits` width calculation actually succeeds rather than for every desktop-class width.

The desktop composer toolbar now switches secondary actions to icon-only presentation before horizontal pressure can push the primary `New cast` action outside its card. The primary action keeps its text label and stays inside the toolbar/container.

### R4 structural indicators

Compared with R3 after these changes:

| Static indicator | R3 | R4 | R4 intent |
|---|---:|---:|---|
| `clip: true` in `ui` | 39 | 38 | Remove one redundant Mini App icon clip; preserve semantic clips |
| `Image.PreserveAspectFit` declarations in `ui` | 7 | 14 | Complete-content presentation for gallery/posters/app artwork |
| `Image.PreserveAspectCrop` declarations in `ui` | 14 | 10 | Crop remains only for cover/thumbnail/effect semantics |
| Home duplicate `Recent Activity` subsystem | present | removed | Eliminate overlap plus avoidable JS/object creation |

These are static source indicators, not frame-time measurements.

### R4 verification

Repository gates after the R4 changes:

```text
UI design guard passed: 120 QML files audited.
Architecture guard passed.
Async resource contract guard: PASS
Data & Storage contract guard: PASS
Explorer semantic/source contract guard: PASS
Media upload contract guard: PASS
vNext release contract: PASS
R7 release contract: PASS
```

`tools/check_localization_catalog.py` produces byte-identical output for the R3 baseline and R4 tree (136 output lines including the existing failure header), so this pass introduces no localization diagnostic.

The execution environment still does not provide `qmllint`, `qmlformat`, `qt-cmake`, or `qmake`. Therefore the macOS Qt 6.11.1 build/runtime remains the final authority for visual qualification, with particular attention to portrait/landscape gallery sources, Mini App cards with text-bearing artwork, borderline desktop Home widths, and the compacted composer action row.
