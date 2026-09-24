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

Feed plays once; detail loops; picker cells always present Noto Animated artwork and advance
it only on selection/hover. Composer and picker-preview loop policies are covered natively;
the existing TextArea editor still displays Unicode. Reduced motion pauses provider artwork on
visible picker/rich-text surfaces; hidden/clipped items and background windows release renderers. Text layout owns
inline bounds, including RTL and repeated identical emoji; proportional line-height ratios are
converted to QText percentages and overlay rectangles are clamped to their owning text line.
A transparent animation replaces its glyph instead of painting over it.
Existing theme, keyboard navigation, picker selection and published Unicode remain intact.
