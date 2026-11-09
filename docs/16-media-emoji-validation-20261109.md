# GenyCaster — Media Reveal & Animated Emoji Validation
**Date:** 2026-11-09
**Application runtime commit:** `4a86bbcfba6d685d65c8d65f263ee3a44beb534e`
**Qt:** 6.12.0 (native and Emscripten/WebAssembly)
**Status:** Published, integrity-verified, native/GPU-path tests passed; authenticated-feed device verification remains external.

## 1. Behaviour under test

- **Media:** `GalleryTile.qml` withholds its decoded image until the source's intrinsic dimensions have propagated into the final single-image `GalleryLayout.qml` geometry. `SuperellipseImage.qml` retains the placeholder during this interval; the handoff is an opacity fade, not a height animation. Very tall portraits use their natural aspect on mobile; desktop Magazine applies an intentional bounded editorial aspect. Recycling a delegate must not reuse a previous image's aspect.
- **Emoji:** the C++ motion policy provides one playback cycle; `AnimatedEmojiLottie.qml` and `SpriteEmoji.qml` stop their timeline after completion and keep a resting visual frame. Visibility/refresh can replay the loaded asset. `Theme.animateEmoji` is persisted separately from avatar animation; Power Save/Reduce Motion still take precedence.

## 2. Actual native Qt Scene Graph validation

The native `farcaster_emoji_ui_tests` was executed with `xvfb-run`, OpenGL and `LIBGL_ALWAYS_SOFTWARE=1`. Qt logged:

- `Creating QRhi with backend OpenGL`, `OpenGL VENDOR: Mesa`.
- `RENDERER: llvmpipe (LLVM 20.1.2, 256 bits)`, OpenGL 4.5.
- `Emoji UI regression failures: 0`; process exit **0**, about 35.2 seconds.

This exercises the real Qt Lottie scene graph and pixel-capture checks, **not** a GPU physically attached to the server. This finding does not establish Metal, Vulkan, iPhone Safari, Android GPU, or physical-device battery performance.

The native `application.emoji_platform`, `ui.emoji_runtime`, and `ui.mobile_portrait_gallery` tests also passed with Qt 6.12. `mobile_portrait_gallery` explicitly covers:
1. Source metadata tall-image natural-height layout on 390px devices.
2. Delayed decoded ratio and refitting of a tall portrait.
3. Responsive switching between desktop editorial and mobile natural ratios.
4. The **new** `awaitFinalAspect` and `fullBleedAspect` properties on the actual instantiated tile binding, using a delayed-ratio test tile stub.

The test's QML `GalleryTile` stub was updated to match the production properties; this **test-only** follow-up does not affect the published WASM binary.

## 3. Reproducible CPU measurement

Benchmark file on the remote QA host: `/opt/genycaster/caster-deployment/emoji_render_cpu_bench_20261109.py`
Raw observations: `/opt/genycaster/caster-deployment/qa-emoji-performance/cpu-results.json`

Test configuration: 48 instances of the **same bundled Lottie JSON asset**, 64×64 presentation, Qt 6.12 `VectorImage.CurveRenderer` through OpenGL/Mesa llvmpipe, one server, cold-process start, Linux `/proc` CPU time for the qmlscene/Xvfb process tree. Three separate runs, each with observations at 1, 3, 5, 8, and 11 seconds, for static, one cycle and unbounded loop modes, respectively.

| Mode | CPU time, 8–11s | Equivalent one-core usage, 8–11s |
|---|---:|---:|
| Static | 0.00 CPU seconds | **0.0%** |
| Play once, then rest | 0.00 CPU seconds | **0.0%** |
| Loop continuously | 2.56 CPU seconds | **85.3%** |

The one-shot mode initially uses CPU while playing, then stops its frame work once completed. Values are **one run per mode**, not a multi-run confidence interval; they are a scene-specific measurement, not a battery-life forecast or a global GenyCaster performance figure.

The host exposes **no** battery/power_supply metering and no usable `energy_uj` RAPL counters. Actual watt-hours/CPU-to-energy ratios on Android/iOS/macOS **could not be measured** and are therefore not claimed.

## 4. WebAssembly release validation

- Qt 6.12 Emscripten WebAssembly compiled and linked successfully from clean commit `4a86bbcf`.
- Immutable file: `genycaster.c48648878bfe8c9d1a02.wasm`.
- Version fingerprint: `774a3d2610a808ef57af`.
- Manifest: **41 files**, validated twice by `verify_release.py`; required hashes matched.
- Atomic production release directory:
  `/var/www/caster.genyleap.com/releases/4a86bbcfba6d-774a3d2610a808ef57af-web`.
- Previous release:
  `/var/www/caster.genyleap.com/releases/26a806f7c690-2ab8140f0bea35baf6b6-web`.
- Desktop browser boot smoke: HTTP 200, expected manifest WASM, **Canvas present**, `phase=ready`, no reported errors.
- Mobile boot smoke: 390×844 viewport, HTTP 200, expected manifest WASM, `phase=ready`, no required-asset errors.
- Three supplemental Chromium viewport captures (390px mobile DPR2, 390px desktop context DPR2, 1440px desktop) confirmed visible application UI and error-free boot. Some SwiftShader browser profiles do not expose a DOM-countable `canvas` despite visually rendered Qt UI. Raw screenshots and JSON observations retained under
  `/opt/genycaster/caster-deployment/qa-media-mobile-774a3d2610a808ef57af/`.
- Atomic release and both baseline browser smoke tests passed. Previous release and 118 immutable legacy assets were preserved for open browser tabs and rollback.

**Browser visual limitation:** the fresh anonymous browser session enters the application's first-run setup. The subsequent Terms & Privacy acceptance is not completed on behalf of a real user; the anonymous test therefore **cannot capture a real Explorer post image or quantify in-feed placeholder-to-image flicker**. Mobile geometry/placeholder contract tests and the native layout transitions are verified, but **not** an end-to-end authenticated media-video comparison on Safari/Chrome hardware.

## 5. Remaining physical-device acceptance (not claimed complete)

A real iOS/Android browser or native device with an authorized signed-in account is still needed for:
- Capturing the actual originally reported image transition in a populated Explorer/feed, including portrait, panorama, quote card and recycled delegate cases.
- Checking playback-on-enter, resting frame, replay-on-return and Settings switch in real scrolling conditions; comparing `reduceMotion` and Power Save.
- Measuring CPU time and energy with platform-native tools (Android Studio Energy Profiler, Instruments Energy Log/Time Profiler) and a repeatable workload.

The distinction between **verified native integration**, **web startup**, and **unverified authenticated-device visuals** is deliberate. A successful boot alone is not proof that the target image flicker is absent in all real feeds.
