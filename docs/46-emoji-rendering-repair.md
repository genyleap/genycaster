# Emoji rendering repair

Engineering authority: `thecompez/ai-for-modern-cpp` at
`326cd850c2cb5e39264b7d2ccf2c29fba6627380`; AGENTS and Architecture, Modules,
Naming, Syntax and Style, API Design, Errors and Resources, CMake and Toolchains,
Testing and Verification, Qt Quick UI and Project CMake Baseline guides applied.

## Feature gate

Responsibility: render canonical Unicode emoji with optional local Lottie animation.
Capability origin: Local presentation with optional Derived asset acquisition.
Mandatory or optional: Unicode is always available; animation is optional.
Public contract: provider-neutral Qt asset callbacks plus native motion policy.
Canonical data/invariants: published text stays Unicode; asset paths never enter casts.
Dependencies: presentation -> application; executable runtime -> provider/cache/transport.
Expected failures: missing renderer, unsupported emoji, network/cache/validation failure.
Local-state representation: bundled resources, bounded disk cache and memory registry.
Offline behavior: bundled/cached assets work; other emoji retain their Unicode glyph.
Provider policy and disablement: the executable composes the asset backend; a renderer-free
build never loads the optional QML renderer or requests network assets.
Failover semantics: preserve the glyph until the renderer is ready; restore it on unload/error.
Tests proving the boundary: real bundled JSON validation, policy tests, Qt asset URL and
inline-layout tests, QML playback/readiness/lifecycle regression tests.

## Structural decision

Keep the existing Unicode catalog, capability, repository, and motion-policy modules.
Move concrete provider/cache/network composition out of the presentation controller into
the executable's named runtime module. Confine repository mutations to the Qt thread;
serialize cache access and validation on a bounded worker. UI callbacks own no provider types.

## Interaction and layout contract

Feed/profile casts, detail, and active animated-picker cells loop continuously
while motion is allowed. Loaded artwork does not return to Unicode after one
iteration. Composer and picker-preview loop policies are covered natively;
the existing TextArea editor still displays Unicode. Reduced motion pauses provider artwork on
visible picker/rich-text surfaces; retained renderers pause while clipped or in background windows
and are released with their owning delegates. Text layout owns
inline bounds, including RTL and repeated identical emoji; proportional line-height ratios are
converted to QText percentages and overlay rectangles are clamped to their owning text line.
A transparent animation replaces its glyph instead of painting over it.
Existing theme, keyboard navigation, picker selection and published Unicode remain intact.

## Rendering lifecycle correction — 2026-11-05

Scope: existing local emoji presentation only; the feature gate above remains
unchanged. Qt 6.12 is already the project's minimum. The pinned engineering
authority and its required implementation/Qt guides were read for this change.

Source inspection of Qt 6.12 identified two invalid readiness assumptions:

- Lottie's generated `NumberAnimation on frameCounter` is a property value
  source, not an entry in `Item.data`. The previous recursive search could not
  observe or pause it. At `endFrame`, Qt makes the generated root invisible
  while its children and implicit dimensions remain present. The previous
  readiness expression therefore hid Unicode behind an invisible vector.
- A hidden Image preloading a sheet cannot report AnimatedSprite's texture
  readiness. Destroying that sprite during viewport changes, or stopping it
  on a multi-row sheet, also separates fallback state from actual pixels.

Lottie readiness now uses `VectorImage.status`, `generatedItem`, and the
generated timeline's visibility/end frame. Pause uses `animations.paused`,
guarded against completion; no recursive animation search, signal closures,
or delayed completion timer remains. Renderer errors activate the existing
sprite fallback; source replacement returns to Unicode while loading.

Sprite playback now moves one cached Image under a fixed frame-sized clip.
The same Image owns readiness and pixels, with explicit row/column addressing.
A NumberAnimation advances the frame index and pauses without releasing the
sheet or restarting a one-shot. Completed one-shots restore Unicode without
restarting the loading indicator. Inline sprites share the vector's viewport
and motion state and retain loaded sheets during short scroll transitions.

Verification: source/diff review only. Configure, build, lint, automated tests,
and runtime/visual checks were intentionally not run at the user's request.
Existing regression expectations were updated and the runtime pause/completion
case strengthened, but these changes remain unexecuted. Manual acceptance:
feed one-shot completion, pause mid-animation, repeated scroll/focus changes,
picker multi-row sprites, reduced motion, and missing/corrupt vector assets.

### Continuous playback policy

The user's follow-up supersedes the earlier feed/picker one-shot policy:
all visible animated surfaces now loop. Unicode remains the unavailable/error
fallback, not the resting state of a healthy animation. Reduced motion, focus,
and viewport changes pause the existing renderer without replacing its infinite
loop count with a finite one. The native policy and existing regression cases
now cover feed/detail parity, playback beyond multiple iterations, and pausing
after multiple iterations. These follow-up checks were also not executed;
configure/build/test remain deferred to the user.
