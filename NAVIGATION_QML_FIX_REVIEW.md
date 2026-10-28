# Navigation and QML diagnostics repair

Source changes, 2026-10-29. Runtime acceptance is **NOT VERIFIED**.

The follow-up screenshots showed that the first repair was insufficient: sharp
content remained visible through the glass, labels overflowed their selected
buttons, and the scroll-to-top button sat beneath the dock.

The dock now captures a bounded rectangle from an unlayered page subtree that
includes the opaque Theme.canvas background. Blurring transparent glyphs alone
cannot obscure the original sharp glyphs beneath them. The dock stays outside
the captured subtree; hidden routes stay hidden. The extra full-viewport texture
and intermediate clipped layer were removed. Overscan and the rounded mask
remain, and an owned timer requests a repaint after appearance changes.
Unsupported graphics backends receive an opaque theme surface.

Destination widths use measured caption text and explicit padding. A label is
shown only when the whole capsule has room for it and four other icon targets;
layout width is no longer animated. Feed/Profile scroll-to-top controls receive
the visible dock's height as a bottom overlay inset, plus a gap above it.

The three AppShell transforms reference their owning loaders explicitly.
The Apps topic selector takes its width budget from the outer viewport rather
than the RowLayout whose size hints it changes. LinkPreview reads a committed
layout snapshot; catalogue promotion is reconciled at scroll idle instead of
inside the geometry binding. Lottie pause/resume checks each generated
NumberAnimation's running state and leaves completed one-shot animations alone.
The follow-up removes the recursive callback and Qt.callLater discovery path:
an owned, single-shot Timer performs iterative discovery and is cancelled when
the renderer is destroyed. This addresses the newly reported invalid QML context.

Node probes no longer read failed/closed replies. Both Qt transport paths avoid
reading an unreadable or empty reply. HTTP status/error handling remains intact;
the reported "Host requires authentication" response is not claimed resolved.
The supplied log does not identify which endpoint requested authentication.

These are repairs to existing Local presentation and reply handling. They add no
feature target, provider policy, network operation, persistence or domain contract.
Existing navigation signals, cached/offline content and theme selection remain
the owners of their respective behavior.

Engineering authority read: `thecompez/ai-for-modern-cpp` revision
`326cd850c2cb5e39264b7d2ccf2c29fba6627380`, including AGENTS.md and the required
ARCHITECTURE, MODULES, NAMING, SYNTAX_AND_STYLE, API_DESIGN,
ERRORS_AND_RESOURCES, CMAKE_AND_TOOLCHAINS, TESTING_AND_VERIFICATION and
QT_QUICK_UI guides. Applied skill: `uiux-master:qt-qml-platform`.

Qt references inspected:

- [MultiEffect source visibility](https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html)
- [ShaderEffectSource](https://doc.qt.io/qt-6/qml-qtquick-shadereffectsource.html)
- [VectorImage animation controls](https://doc.qt.io/qt-6/qml-qtquick-vectorimage-vectorimage.html)
- [Qt 6.12 generated animation bindings](https://github.com/qt/qtdeclarative/blob/6.12/src/quickvectorimage/generator/qquickqmlgenerator.cpp)

Verification: supplied screenshot inspection, source and final diff review only.
The checkout has no `.git` directory; changed sources were compared with copies
saved before editing. Existing dock source-contract assertions were updated but
not executed.
Configure, build, lint, tests, application launch and rendered inspection were
deliberately not run, following the user's explicit instruction. No claim of
warning-free execution or visual acceptance is made.
