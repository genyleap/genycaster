# QML Reference

> Official Qt Quick/QML reference companion for **AI for Modern C++**.
>
> **Status:** reference material, not canonical repository policy
> **Primary upstream baseline:** Qt 6.11.2 documentation
> **Last reviewed:** 2026-09-11

## Purpose

This document collects the most relevant official Qt guidance for designing,
implementing, styling, profiling, and verifying professional Qt Quick/QML user
interfaces.

It is intentionally a **reference companion** rather than a second policy
document. It must not duplicate, replace, weaken, or silently extend the
canonical rules already defined by this repository.

Use it to answer questions such as:

- Which official Qt page supports a QML engineering decision?
- What does Qt recommend for layouts, controls, responsive behavior, focus,
  accessibility, RTL, high-DPI rendering, effects, or performance?
- Which Qt-specific pitfalls should an agent check before calling a UI polished?
- Where should an agent look when a Qt API or style behavior is version-sensitive?

---

## Repository Authority And Precedence

The truth hierarchy of `ai-for-modern-cpp` remains unchanged:

```text
Higher-priority human and platform instructions
    ↓
AGENTS.md
    ↓
docs/agent/QT_QUICK_UI.md and other routed docs/agent guides
    ↓
this QML_REFERENCE.md
    ↓
examples, external guidance, and implementation notes
```

If this document appears to conflict with `AGENTS.md`, follow `AGENTS.md`.

If this document appears to conflict with `docs/agent/QT_QUICK_UI.md`, treat
`QT_QUICK_UI.md` as the repository interpretation and investigate whether the
difference comes from:

1. a Qt-version change,
2. a product-specific exception,
3. stale reference material, or
4. an actual repository-policy inconsistency.

Do not invent new `GUI-*` rule identifiers in this file.

Normative language in quoted or summarized Qt guidance describes the upstream
Qt recommendation. It does **not** create a new repository-wide invariant by
itself.

Durable repository-wide corrections belong in `AGENTS.md`, reviewable
conditions belong in `docs/REVIEW.md`, task-specific engineering decisions
belong in the relevant `docs/agent/*` guide, and enforceable invariants belong
in the repository's tests.

---

# 1. Essential Qt Quick / QML References

These are the highest-value pages to read before making broad Qt Quick
engineering decisions.

## 1.1 Best Practices for QML and Qt Quick

Official Qt documentation:

https://doc.qt.io/qt-6/qtquick-bestpractices.html

Qt covers:

- preferring built-in controls before inventing replacements,
- avoiding unsupported customization of native styles,
- layout usage,
- scalable UI guidance,
- anchors and layout ownership,
- reusable components,
- resource handling,
- QML/C++ integration considerations.

This is the best general upstream companion to:

- `AGENTS.md` `GUI-*`,
- `docs/agent/QT_QUICK_UI.md`,
- `docs/agent/ARCHITECTURE.md`.

---

## 1.2 Qt Quick Performance Considerations and Suggestions

Official Qt documentation:

https://doc.qt.io/qt-6/qtquick-performance.html

This page should be considered required reading for performance-sensitive Qt
Quick work.

Important upstream guidance includes:

- use asynchronous and event-driven programming,
- move significant processing away from the GUI thread,
- avoid manually spinning the event loop,
- avoid expensive per-frame JavaScript,
- keep bindings simple and declarative,
- avoid complex view delegates,
- avoid unnecessary clipping,
- avoid unnecessary opacity,
- avoid overusing `Loader`,
- avoid giant catch-all singleton objects,
- do not put everything into QML,
- measure GPU limits rather than assuming rendering is cheap,
- use the Qt Quick Compiler,
- profile real behavior.

Qt explicitly describes clipping as a visual effect rather than a rendering
optimization.

---

## 1.3 Qt Quick Controls Guidelines

Official Qt documentation:

https://doc.qt.io/qt-6/qtquickcontrols-guidelines.html

Provides practical usage guidance for:

- buttons,
- containers,
- delegates,
- indicators,
- inputs,
- menus,
- navigation,
- popups,
- separators.

Before creating a bespoke control, check whether an existing Qt Quick Control
already provides the required semantics and interaction behavior.

---

## 1.4 Qt Quick

Official Qt Quick module:

https://doc.qt.io/qt-6/qtquick-index.html

Topic index:

https://doc.qt.io/qt-6/qtquick-toc.html

QML types:

https://doc.qt.io/qt-6/qtquick-qmlmodule.html

Use these when a task requires exact behavior of a visual type rather than a
high-level design recommendation.

---

# 2. Qt Quick Controls, Styling, And Custom Design Systems

## 2.1 Qt Quick Controls

https://doc.qt.io/qt-6/qtquickcontrols-index.html

Module topics:

https://doc.qt.io/qt-6/qquickcontrols-toc.html

Qt Quick Controls provide semantic UI controls that already implement large
parts of expected interaction, focus, accessibility, and platform behavior.

A project-owned design system should normally customize or style controls
rather than replace semantic controls with arbitrary visual primitives.

For example, a project button should normally remain a `Button`-based control
rather than becoming:

```qml
Rectangle {
    MouseArea { /* ... */ }
    Text { /* ... */ }
}
```

unless there is a concrete architectural reason for the lower-level
implementation.

---

## 2.2 Customizing Qt Quick Controls

https://doc.qt.io/qt-6/qtquickcontrols-customize.html

Key upstream points:

- controls can be customized without reimplementing their entire behavior;
- complete custom styles can be created using Qt Quick Templates;
- a custom style is a QML module;
- declarative assignment is preferred for custom visual items;
- style implementation has its own performance considerations;
- Windows and macOS native styles are not appropriate foundations for arbitrary
  visual delegate replacement.

For a consistent product-owned appearance across platforms, Qt recommends
building on a customizable style available across platforms, such as:

- Basic,
- Fusion,
- Imagine,
- Material,
- Universal,

or creating a custom style.

This directly supports the style-selection contract already documented in
`docs/agent/QT_QUICK_UI.md`.

---

## 2.3 Styling Qt Quick Controls

https://doc.qt.io/qt-6/qtquickcontrols-styles.html

Use this page to decide between:

- Basic,
- Fusion,
- Imagine,
- Material,
- Universal,
- FluentWinUI3,
- platform-native styles,
- custom styles.

### Basic Style

Qt describes Basic as a simple, lightweight, all-round style that offers high
performance and is suitable as a cross-platform customization foundation.

### Compile-Time Versus Run-Time Style Selection

Qt supports both strategies.

Compile-time style selection can provide the QML compiler with more specific
type information.

Run-time style selection enables one binary to select among styles at run time.

Do not casually mix both approaches. Qt documents ordering and theming
limitations when they are mixed.

Repository-specific style policy remains owned by
`docs/agent/QT_QUICK_UI.md`.

---

## 2.4 QQuickStyle

https://doc.qt.io/qt-6/qquickstyle.html

Important:

When run-time style selection is used, configure the style **before** loading
QML that imports Qt Quick Controls.

Example ordering only:

```cpp
QQuickStyle::setStyle(QStringLiteral("Basic"));
QQmlApplicationEngine engine;
```

The repository's C++ architecture, composition root, module policy, and Qt
registration rules still come from `AGENTS.md` and the routed agent guides.

---

## 2.5 Qt Quick Controls Configuration File

https://doc.qt.io/qt-6/qtquickcontrols-configuration.html

Useful for:

- preferred style,
- fallback style,
- style-specific theme configuration,
- palette configuration.

---

## 2.6 Supported Qt Quick Controls Environment Variables

https://doc.qt.io/qt-6/qtquickcontrols-environment.html

Useful primarily for development, verification, and controlled deployment.

Important variables include:

- `QT_QUICK_CONTROLS_STYLE`,
- `QT_QUICK_CONTROLS_FALLBACK_STYLE`,
- `QT_QUICK_CONTROLS_CONF`,
- `QT_QUICK_CONTROLS_HOVER_ENABLED`.

Do not let test-only environment configuration accidentally become the
unverified production style-selection architecture.

---

# 3. Experimental Styling: Qt Labs StyleKit

## 3.1 StyleKit Features Overview

https://doc.qt.io/qt-6/qtlabsstylekit-overview-features.html

## 3.2 Style QML Type

https://doc.qt.io/qt-6/qml-qt-labs-stylekit-style.html

Qt Labs StyleKit can describe control visuals centrally, including:

- colors,
- dimensions,
- radii,
- borders,
- shadows,
- themes,
- hovered states,
- pressed states,
- disabled states,
- transitions.

However, in Qt 6.11 it is a **Technology Preview**.

Therefore:

- it is useful as design-system research and experimentation;
- it must not silently replace a stable production styling architecture;
- adoption requires an explicit project decision and version-risk review;
- production code must not assume Technology Preview API stability.

The stable repository guidance in `QT_QUICK_UI.md` remains authoritative.

---

# 4. Layout And Geometry

## 4.1 Qt Quick Layouts

Overview:

https://doc.qt.io/qt-6/qtquicklayouts-overview.html

QML module:

https://doc.qt.io/qt-6/qtquick-layouts-qmlmodule.html

Relevant types include:

- `RowLayout`,
- `ColumnLayout`,
- `GridLayout`,
- `StackLayout`,
- `FlexboxLayout`,
- `LayoutItemProxy`,
- `Layout` attached properties.

---

## 4.2 Layout Ownership

Qt's best-practices documentation warns against mixing layout management with
anchors on an immediate child managed by a layout.

Avoid competing geometry owners.

Problematic shape:

```qml
RowLayout {
    Item {
        anchors.fill: parent
        Layout.fillWidth: true
    }
}
```

Prefer one clear ownership model.

For a layout-managed child:

```qml
RowLayout {
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
```

For geometry internal to that component, anchors or bindings can still be used
inside the component where they do not compete with the parent layout.

---

## 4.3 Layouts Are Not Free

Qt also notes that layouts and anchors have creation and memory cost.

Do not interpret "use layouts correctly" as "wrap every small delegate in
multiple nested layouts".

Especially inside hot `ListView` or table delegates, simple geometry bindings
can be appropriate when they are clearer, cheaper, and acyclic.

The design target is:

```text
clear geometry ownership
+ maintainability
+ correct resizing
+ measured performance
```

not dogmatic use of one geometry mechanism everywhere.

---

# 5. Implicit Size And Component Sizing

## 5.1 Item QML Type

https://doc.qt.io/qt-6/qml-qtquick-item.html

Important properties:

- `implicitWidth`,
- `implicitHeight`,
- `width`,
- `height`,
- `focus`,
- `focusPolicy`,
- `visible`,
- `opacity`,
- `layer.enabled`.

Qt describes implicit size as the preferred size of an item.

Layouts use an item's implicit dimensions when determining preferred sizes.

Reusable visual components should therefore expose meaningful intrinsic sizing
when practical.

Example:

```qml
Control {
    implicitWidth: Math.max(
        120,
        contentItem.implicitWidth + leftPadding + rightPadding
    )

    implicitHeight: Math.max(
        36,
        contentItem.implicitHeight + topPadding + bottomPadding
    )
}
```

Do not force arbitrary explicit dimensions merely because a component has no
useful intrinsic size.

Also note Qt's warning that some text sizing combinations can cause text layout
to happen more than once. Measure before applying expensive sizing patterns to
large delegate sets.

---

# 6. Responsive UI

## 6.1 Qt Quick Responsive Layouts

https://doc.qt.io/qt-6/qtquicklayouts-responsive.html

Responsive design is more than scaling controls.

Qt explicitly discusses situations where the UI should:

- reorganize,
- hide or reveal elements,
- alter layout properties,
- use states,
- adapt to device class or orientation,
- move items through supported responsive mechanisms.

Think in product compositions rather than device labels alone.

A useful design vocabulary is:

```text
compact
standard
wide
```

or another product-specific set of compositions.

Breakpoints should follow usability and content constraints rather than being
chosen only because a popular CSS framework uses the same number.

The repository's exact layout contract belongs in `QT_QUICK_UI.md`.

---

# 7. Right-To-Left And Bidirectional Interfaces

## 7.1 Right-to-left User Interfaces

https://doc.qt.io/qt-6/qtquick-positioning-righttoleft.html

## 7.2 LayoutMirroring

https://doc.qt.io/qt-6/qml-qtquick-layoutmirroring.html

Qt's RTL guidance distinguishes between content and layout.

General upstream principles include:

- mirror interface layout when appropriate;
- do not mirror arbitrary content such as photos, videos, or maps merely
  because the locale is RTL;
- directional icons may require mirroring;
- non-directional icons normally do not;
- use application layout direction;
- test animations and transitions after mirroring;
- remember that raw `x` coordinate logic is not automatically mirrored.

Typical root-level binding:

```qml
LayoutMirroring.enabled:
    Qt.application.layoutDirection === Qt.RightToLeft

LayoutMirroring.childrenInherit: true
```

Do not assume this solves every bidirectional-content problem.

Examples of content that frequently remains LTR inside an RTL application:

- source code,
- URLs,
- wallet addresses,
- hashes,
- protocol identifiers,
- version strings where the product requires stable LTR presentation.

Content direction, application direction, and directional iconography must be
reviewed separately.

---

# 8. Keyboard Focus And Input

## 8.1 Focus Management in Qt Quick Controls

https://doc.qt.io/qt-6/qtquickcontrols-focus.html

## 8.2 Item Focus APIs

https://doc.qt.io/qt-6/qml-qtquick-item.html

Qt Quick Controls use the Qt Quick focus system and provide `focusPolicy`
support.

A desktop-grade interface should be checked for:

- Tab navigation,
- Shift+Tab navigation,
- Enter/Return activation where appropriate,
- Space activation where appropriate,
- arrow-key navigation where appropriate,
- Escape behavior,
- platform shortcuts,
- visible keyboard focus.

For controls, prefer the semantic focus information supplied by the control
APIs.

`Control.visualFocus` is particularly useful because it distinguishes keyboard
focus visualization from pointer/touch focus behavior.

Control reference:

https://doc.qt.io/qt-6/qml-qtquick-controls-control.html

---

# 9. Accessibility

## 9.1 Accessibility for Qt Quick Applications

https://doc.qt.io/qt-6.11/accessible-qtquick.html

Qt Quick Controls already provide useful accessibility behavior.

Custom items may need explicit accessibility metadata.

The most important metadata described by Qt includes:

- accessible name,
- accessible description,
- accessible role.

Example shape:

```qml
Item {
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Open settings")
}
```

A visually custom interaction is not complete merely because pointer input
works.

Review:

- semantic role,
- accessible name,
- state,
- focus path,
- keyboard operation,
- contrast,
- non-color cues,
- error feedback,
- translated accessibility strings.

Repository acceptance requirements remain defined by `GUI-*` and
`QT_QUICK_UI.md`.

---

# 10. High-DPI And Scalable Rendering

## 10.1 High DPI

https://doc.qt.io/qt-6/highdpi.html

Qt provides device-independent coordinates through its higher-level UI APIs,
including Qt Quick.

Application code should not manually double all UI dimensions for Retina or
other high-density screens.

Instead:

- design in Qt's logical coordinate system;
- provide appropriate high-resolution assets where raster assets are required;
- prefer suitable vector assets for small scalable icons;
- verify rasterization quality on representative scale factors.

Qt's best-practices page also recommends scalable UI resources and highlights
SVG as useful for small icons.

---

# 11. Icons And Visual Assets

## 11.1 Icons in Qt Quick Controls

https://doc.qt.io/qt-6/qtquickcontrols-icons.html

Qt Quick Controls support icon properties such as:

- icon name,
- icon source,
- icon width,
- icon height,
- icon color.

The repository's application icon, launcher icon, packaging, and branding rules
remain owned by:

`docs/agent/APP_ICONS_AND_BRANDING.md`

Do not treat a launcher icon and an in-application glyph as the same artifact.

For in-application icons, review:

- consistent visual weight,
- optical alignment,
- consistent bounding boxes,
- RTL directional behavior,
- crisp high-DPI rendering,
- enabled/disabled/selected states.

---

# 12. Animation And Motion

## 12.1 States And Animations

https://doc.qt.io/qt-6/qtquick-statesanimations-topic.html

Motion should communicate:

- state transition,
- continuity,
- hierarchy,
- navigation,
- feedback.

It should not exist merely because animation is available.

Avoid expensive calculations inside per-frame JavaScript.

---

## 12.2 Animator Types

https://doc.qt.io/qt-6/qml-qtquick-animator.html

Animator-derived types include:

- `OpacityAnimator`,
- `RotationAnimator`,
- `ScaleAnimator`,
- `XAnimator`,
- `YAnimator`.

Animator types operate directly on the Qt Quick scene graph and can execute on
the rendering thread when the active render loop supports it.

They are useful for appropriate visual animation, but they are **not** a license
to block the GUI thread.

Qt still recommends asynchronous architecture rather than relying on render
thread animation to hide expensive synchronous work.

---

# 13. Effects, Blur, Shadow, And Glass-Like Surfaces

## 13.1 MultiEffect

https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html

`MultiEffect` supersedes common Qt 5 Graphical Effects use cases and can combine
multiple effects in one effect item and shader.

Relevant capabilities include:

- blur,
- shadow,
- colorization,
- brightness,
- contrast,
- saturation,
- masking.

Qt performance guidance for MultiEffect is especially important:

- enable only effects that are actually used;
- hide unused effect items;
- blur and shadow are among the heavier effects;
- minimize the processed pixel area;
- avoid animating properties that force expensive effect resizing or shader
  changes;
- avoid repeatedly regenerating blur for an animated source unless the visual
  requirement justifies the cost.

Therefore, a design system should not equate "premium" with pervasive blur and
shadow.

Use effects where they communicate surface hierarchy or product identity, then
profile them.

---

# 14. Clipping, Layers, And Offscreen Rendering

## 14.1 Performance Guidance

https://doc.qt.io/qt-6/qtquick-performance.html

Qt states that clipping is disabled by default and should only be enabled when
required.

Important implications:

- clipping increases renderer complexity;
- clipping is not an optimization;
- clipping inside delegates is particularly costly;
- setting `clip: true` everywhere is an anti-pattern.

---

## 14.2 Item Layers

https://doc.qt.io/qt-6/qml-qtquick-item.html

`layer.enabled` renders an item subtree into an offscreen surface.

That capability is useful for specific effects and rendering strategies, but it
has memory and rendering implications.

Do not enable layers as a generic "make rendering smoother" switch.

Use them because the rendering design requires an offscreen texture, and
measure the result.

---

# 15. Scene Graph And GPU-Aware UI Design

## 15.1 Qt Quick Scene Graph

https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html

## 15.2 Default Scene Graph Renderer

https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html

Qt Quick uses a retained scene graph and can render through platform graphics
APIs such as:

- Metal,
- Vulkan,
- Direct3D,
- OpenGL,

depending on platform and configuration.

Important rendering concepts:

- batching,
- state changes,
- geometry uploads,
- opaque versus blended content,
- render-thread behavior,
- overdraw,
- offscreen surfaces.

For performance investigations, Qt documents renderer diagnostics such as:

```text
QSG_RENDERER_DEBUG=render
```

Do not tune rendering based only on aesthetic intuition.

Measure representative screens and scrolling surfaces.

---

# 16. Views, Delegates, And Virtualized Content

## 16.1 Performance Reference

https://doc.qt.io/qt-6/qtquick-performance.html

Qt specifically warns against complex delegates for views.

This matters for:

- `ListView`,
- `GridView`,
- table-like views,
- feeds,
- chats,
- timelines,
- large model-driven surfaces.

A hot delegate should not casually contain:

```text
multiple nested Layouts
multiple Loader instances
unnecessary clip
multiple blur/shadow effects
large ShaderEffectSource surfaces
heavy JavaScript bindings
repeated parsing
network logic
domain logic
large object graphs
```

Delegate simplicity is both an engineering and UI-design constraint.

If a visual design cannot scroll smoothly on representative hardware, the
visual design is not finished.

---

# 17. Loader And Lazy Construction

## 17.1 Loader

Type reference:

https://doc.qt.io/qt-6/qml-qtquick-loader.html

Performance guidance:

https://doc.qt.io/qt-6/qtquick-performance.html

`Loader` is useful for genuine lazy construction, such as:

- application pages,
- expensive secondary panels,
- infrequently opened dialogs,
- large optional content.

Qt warns against treating `Loader` as generic performance magic.

For tiny frequently-created delegate elements, the extra item and QML context
can make the design worse rather than better.

---

# 18. Bindings And JavaScript

## 18.1 Performance Guidance

https://doc.qt.io/qt-6/qtquick-performance.html

Prefer:

```text
small
declarative
readable
acyclic
cheap-to-reevaluate
```

bindings.

Avoid moving authoritative domain behavior into QML JavaScript.

The repository ownership model remains:

```text
QML
    presentation
    layout
    animation
    visual state
    input forwarding

C++
    domain rules
    validation
    persistence
    networking
    application orchestration
    authoritative state
    expensive processing
```

See:

`docs/agent/QT_QUICK_UI.md`

and:

`docs/agent/ARCHITECTURE.md`

for repository policy.

---

# 19. QML Compiler

## 19.1 Qt Quick Compiler

https://doc.qt.io/qt-6/qtqml-qtquick-compiler-tech.html

Qt Quick Compiler processes QML and JavaScript at build time to improve startup
and binding/function execution.

When QML modules are declared using the supported Qt CMake integration, much of
this compilation happens as part of the normal build pipeline.

Do not manually invoke internal compiler tools as a substitute for correct Qt
CMake integration.

Repository CMake policy remains documented in:

- `docs/agent/PROJECT_CMAKE_BASELINE.md`,
- `docs/agent/CMAKE_AND_TOOLCHAINS.md`.

---

# 20. QML Static Analysis And Formatting

## 20.1 qmllint

https://doc.qt.io/qt-6/qtqml-tooling-qmllint.html

`qmllint` validates QML syntax and reports a range of QML anti-patterns and
type/API issues.

The repository already requires strict QML validation for Qt Quick deliverables.

Important principle:

```text
A window opening is not proof that the QML is correct.
```

Lazy components, alternate responsive branches, popups, delegates, and dialogs
must also be instantiated or otherwise exercised by verification.

---

## 20.2 qmlformat

https://doc.qt.io/qt-6/qtqml-tooling-qmlformat.html

`qmlformat` formats QML according to Qt QML coding conventions.

Formatting should be automated rather than manually approximated.

---

## 20.3 QML Language Server

https://doc.qt.io/qt-6/qtqml-tooling-qmlls.html

Provides editor integration for:

- completion,
- lint diagnostics,
- navigation,
- symbol usage,
- rename,
- formatting,
- documentation help.

Tooling diagnostics are useful evidence, but they do not replace build, runtime,
or visual verification.

---

## 20.4 Qt QML Tooling Index

https://doc.qt.io/qt-6/qtqml-tooling.html

Useful entry point for:

- `qmllint`,
- `qmlformat`,
- QML Language Server,
- QML Profiler,
- preview tooling,
- compiler-related tooling,
- type registration tooling.

---

# 21. Profiling

## 21.1 Profiling QML Applications

https://doc.qt.io/qt-6/qtquick-profiling.html

The QML Profiler can inspect runtime behavior including expensive QML activity.

Use profiling when:

- scrolling stutters,
- animation skips frames,
- startup is slow,
- bindings reevaluate excessively,
- component creation is expensive,
- models or JavaScript appear hot,
- UI changes introduce measurable regressions.

Do not optimize arbitrary code paths because they merely look suspicious.

Measure first.

---

# 22. Frame Budget And Responsiveness

Qt's performance documentation commonly uses 60 FPS as a practical reference,
which leaves roughly one frame interval for all GUI-side and rendering work.

The exact target depends on:

- display refresh rate,
- platform,
- product requirements,
- hardware class.

A 120 Hz display has a substantially smaller frame interval than a 60 Hz
display.

Therefore, avoid encoding "60 FPS means finished" as a universal design rule.

Instead:

1. define the target hardware and refresh expectations;
2. test the actual interaction;
3. inspect frame stability;
4. profile regressions;
5. keep the GUI thread responsive.

---

# 23. Professional Visual-System Guidance Compatible With Qt

The following design-system practices are compatible with Qt guidance and the
repository's existing Qt Quick policy.

They are reference guidance, not new `GUI-*` rules.

## 23.1 Use Semantic Design Tokens

Prefer semantic roles such as:

```text
surface
surfaceRaised
surfaceOverlay

textPrimary
textSecondary
textDisabled

accent
accentHover
accentPressed

border
borderStrong

success
warning
danger
```

over duplicated literal values spread through screens.

Also centralize:

```text
spacing
radius
typography
icon sizes
control heights
motion durations
borders
elevation/effects
```

The actual token values are product decisions.

---

## 23.2 Use A Small Spacing Scale

A consistent scale is easier to reason about than arbitrary per-component
nudging.

Example only:

```text
2
4
8
12
16
24
32
48
```

Do not treat these exact values as repository policy.

Choose a scale appropriate to the product and verify it visually.

---

## 23.3 Preserve Visual Hierarchy

Prefer hierarchy through:

- spacing,
- typography,
- surface contrast,
- alignment,
- content density,
- primary/secondary action treatment.

Avoid solving every hierarchy problem with:

- more borders,
- more shadows,
- more gradients,
- more blur,
- more cards.

---

## 23.4 Keep Component Semantics Stable

Prefer names describing UI role:

```text
PrimaryActionButton
IconButton
SearchField
NavigationRail
StatusBanner
ProfileHeader
```

instead of names describing incidental appearance:

```text
BlueButton
DarkBox
RoundedThing
FancyPanel
```

Repository naming requirements remain defined by `AGENTS.md` and the routed
guides.

---

## 23.5 Treat Interaction States As A System

Reusable controls should account for relevant states such as:

```text
normal
hovered
pressed
focused
checked
selected
disabled
busy
error
```

Not every control needs every state.

The important point is that state styling is deliberate and consistent rather
than being added ad hoc screen by screen.

---

## 23.6 Visual Size And Hit Target Are Different Concerns

A small icon can live inside a larger interactive target.

Do not enlarge the glyph merely to make interaction easier.

Use control padding and hit geometry to preserve both:

- visual balance,
- usable interaction.

Exact target dimensions are product/platform decisions and should be validated
against the relevant platform guidance.

---

# 24. Common QML Design And Engineering Failure Modes

| Failure | Why it is risky | Preferred investigation |
|---|---|---|
| `Rectangle + MouseArea + Text` used as every control | Reimplements semantics, focus, accessibility, and states | Check Qt Quick Controls first |
| Customizing macOS/Windows native control internals | Native styles reject unsupported customization | Use documented native surface or a customizable cross-platform style |
| `clip: true` added everywhere | Increases rendering complexity | Enable only where visual clipping is actually required |
| `layer.enabled: true` used as a performance fix | Forces offscreen rendering | Use only for a measured rendering requirement |
| Blur/shadow on every card | Expensive pixels and weak hierarchy | Reserve effects for meaningful surfaces and profile |
| Heavy `ListView` delegates | Creation and scrolling regressions | Flatten and simplify hot delegates |
| `Loader` around tiny repeated elements | Adds object/context overhead | Lazy-load genuinely expensive/rare surfaces |
| Complex JS in bindings | Expensive reevaluation and poor ownership | Keep bindings simple; move application logic to C++ |
| JavaScript during animation | Can block the GUI thread | Use declarative animation and asynchronous architecture |
| Parent and child sizing each other | Binding loops | Establish one geometry owner |
| Anchors on immediate Layout children | Competing geometry systems | Use `Layout.*` for the layout-managed child |
| Explicit dimensions everywhere | Breaks intrinsic sizing and responsiveness | Use meaningful implicit sizes and layout constraints |
| Hard-coded LTR geometry | Breaks RTL | Prefer mirroring-aware layouts and directional review |
| Mirroring every asset | Corrupts non-directional content | Mirror layout and only appropriate directional graphics |
| UI logic inside C++ rendering code | Blurs presentation boundaries | Keep domain/application state separate from QML presentation |
| Domain logic inside QML | Duplicates authoritative behavior | Route intents to C++ presentation/application layers |
| Ignoring lazy components in tests | Hidden runtime QML failures | Instantiate primary popups/dialogs/delegates/branches |
| Trusting only one screenshot size | Hides clipping and dead-space issues | Verify compact/standard/wide representative states |
| Trusting lint alone | Lint cannot prove runtime interaction | Build, run, exercise, and inspect |
| Trusting runtime alone | Runtime may miss static issues | Run strict lint and compile generated QML sources |

---

# 25. Reference Verification Checklist

This checklist does not replace `docs/REVIEW.md`.

Use it as an upstream Qt-specific supplement.

## Architecture

- [ ] QML owns presentation rather than authoritative domain behavior.
- [ ] Expensive or blocking work is not performed on the GUI thread.
- [ ] The QML-facing C++ surface is intentionally small.
- [ ] Models are exposed through suitable Qt model contracts rather than
      arbitrary object graphs.

## Controls

- [ ] Existing Qt Quick Controls were considered before creating custom
      interaction primitives.
- [ ] The effective Qt Quick Controls style is explicit.
- [ ] Native styles are not customized through unsupported delegate
      replacement.
- [ ] Custom controls preserve semantics, focus, and accessibility.

## Geometry

- [ ] Each important size relationship has one clear owner.
- [ ] Immediate Layout children are not simultaneously anchor-managed.
- [ ] Reusable components expose useful intrinsic sizes when appropriate.
- [ ] There are no binding loops.
- [ ] Compact and wide layouts preserve primary actions.

## Responsive Behavior

- [ ] Responsive design reorganizes content where scaling alone becomes
      unusable.
- [ ] Breakpoints are justified by content/task behavior.
- [ ] Long text and translated expansion have been exercised.
- [ ] Popups and dialogs remain inside safe bounds.

## RTL / Bidirectional UI

- [ ] Application direction follows the selected locale or explicit product
      decision.
- [ ] Directional layout mirrors correctly.
- [ ] Directional icons are reviewed individually.
- [ ] Non-directional media is not blindly mirrored.
- [ ] Code, URLs, hashes, addresses, and other LTR content remain readable.
- [ ] Mirrored transitions still behave correctly.

## Accessibility / Input

- [ ] Keyboard navigation works.
- [ ] Focus is visible when appropriate.
- [ ] Custom controls expose meaningful accessible metadata.
- [ ] Disabled, busy, selected, and error states remain understandable.
- [ ] Important meaning is not conveyed by color alone.

## Rendering

- [ ] `clip` is used only when required.
- [ ] Layers are enabled only for an intentional offscreen-rendering reason.
- [ ] Blur and shadow cover only justified pixel areas.
- [ ] Hidden expensive content is actually hidden.
- [ ] Hot delegates avoid unnecessary effects and object depth.
- [ ] High-DPI assets render crisply.

## Motion

- [ ] Motion explains feedback, state, hierarchy, or continuity.
- [ ] Heavy JavaScript does not run per frame.
- [ ] Animations do not hide blocking GUI-thread work.
- [ ] Reduced motion or product accessibility requirements are considered when
      applicable.

## Tooling

- [ ] `qmllint` is clean under the repository-required strictness.
- [ ] QML formatting follows the project's selected `qmlformat` policy.
- [ ] Generated QML sources compile.
- [ ] The graphical executable links.
- [ ] Relevant lazy components are instantiated.
- [ ] Runtime QML warnings are treated according to repository policy.
- [ ] QML Profiler is used when performance is in question.
- [ ] Representative visual states are inspected.

---

# 26. Qt-Version-Sensitive Notes

## 26.1 Read The Exact Version Documentation

Do not assume a property exists because it appears in a newer Qt documentation
page.

For every version-sensitive QML API:

1. identify the project's minimum Qt version;
2. inspect the exact QML type;
3. inspect inherited members;
4. validate imports;
5. run `qmllint`;
6. instantiate the component.

This is already consistent with `docs/agent/QT_QUICK_UI.md`.

---

## 26.2 StyleKit Is Not Yet A Stable Foundation

As of Qt 6.11, Qt Labs StyleKit is a Technology Preview.

Re-evaluate its maturity before promoting it into a production baseline.

---

## 26.3 Native Style Customization Has Constraints

macOS and Windows native styles are not general-purpose customizable style
foundations.

Use their documented surface as native controls, or deliberately select a
customizable cross-platform/product-owned style.

---

## 26.4 Qt Quick Uses RHI-Based Cross-Platform Rendering

Do not make application-level rendering architecture depend accidentally on one
graphics API unless the feature explicitly requires it.

Qt Quick may use different underlying graphics APIs across platforms.

Custom shaders and scene graph integrations must respect the active Qt Quick
rendering architecture.

---

# 27. Recommended Reading Order For Agents

For a new or redesigned Qt Quick interface:

```text
1. AGENTS.md
2. docs/agent/START_PROJECT.md                 # when starting a new project
3. docs/agent/QT_QUICK_UI.md
4. docs/agent/ARCHITECTURE.md                  # when ownership changes
5. docs/agent/PROJECT_CMAKE_BASELINE.md        # when creating/changing Qt CMake
6. QML_REFERENCE.md                            # upstream Qt evidence and lookup
7. exact Qt type documentation                 # for API-sensitive implementation
8. docs/REVIEW.md
9. verification guides required by change level
```

For a performance regression:

```text
AGENTS.md
    ↓
QT_QUICK_UI.md
    ↓
EXECUTION_DISCIPLINE.md
    ↓
Qt Quick Performance Considerations
    ↓
QML Profiler
    ↓
Scene Graph / MultiEffect docs if rendering-related
```

For an RTL bug:

```text
AGENTS.md
    ↓
QT_QUICK_UI.md
    ↓
Right-to-left User Interfaces
    ↓
LayoutMirroring
    ↓
exact affected control/view documentation
    ↓
runtime and visual verification
```

For a custom control system:

```text
AGENTS.md
    ↓
QT_QUICK_UI.md
    ↓
Qt Quick Controls Guidelines
    ↓
Customizing Qt Quick Controls
    ↓
Styling Qt Quick Controls
    ↓
exact control/template types
    ↓
lint + build + runtime + accessibility + visual checks
```

---

# 28. Integration With AI For Modern C++

This file is designed to be added as:

```text
docs/agent/QML_REFERENCE.md
```

That location preserves the existing knowledge architecture:

```text
AGENTS.md
    canonical policy

docs/agent/QT_QUICK_UI.md
    repository Qt Quick decision guide

docs/agent/QML_REFERENCE.md
    official upstream Qt reference index and engineering notes
```

A minimal routing addition to `docs/agent/README.md` can be:

```markdown
| Which official Qt/QML references support a UI decision? | `QML_REFERENCE.md`, after `QT_QUICK_UI.md` |
```

A minimal addition to the primary references section of
`docs/agent/QT_QUICK_UI.md` can be:

```markdown
- Repository Qt/QML upstream reference index:
  [`QML_REFERENCE.md`](QML_REFERENCE.md)
```

Do not replace the existing `QT_QUICK_UI.md` route with this file.

`QT_QUICK_UI.md` explains what this repository expects.

`QML_REFERENCE.md` explains where the corresponding upstream Qt evidence and
technical reference material can be found.

---

# 29. Official Qt Reference Index

## General

- Best Practices for QML and Qt Quick
  https://doc.qt.io/qt-6/qtquick-bestpractices.html

- Qt Quick
  https://doc.qt.io/qt-6/qtquick-index.html

- Qt Quick Topic Index
  https://doc.qt.io/qt-6/qtquick-toc.html

- Qt Quick QML Types
  https://doc.qt.io/qt-6/qtquick-qmlmodule.html

## Controls

- Qt Quick Controls
  https://doc.qt.io/qt-6/qtquickcontrols-index.html

- Qt Quick Controls Guidelines
  https://doc.qt.io/qt-6/qtquickcontrols-guidelines.html

- Qt Quick Controls Topics
  https://doc.qt.io/qt-6/qquickcontrols-toc.html

- Customizing Qt Quick Controls
  https://doc.qt.io/qt-6/qtquickcontrols-customize.html

- Styling Qt Quick Controls
  https://doc.qt.io/qt-6/qtquickcontrols-styles.html

- QQuickStyle
  https://doc.qt.io/qt-6/qquickstyle.html

- Qt Quick Controls Configuration
  https://doc.qt.io/qt-6/qtquickcontrols-configuration.html

- Supported Qt Quick Controls Environment Variables
  https://doc.qt.io/qt-6/qtquickcontrols-environment.html

- Focus Management in Qt Quick Controls
  https://doc.qt.io/qt-6/qtquickcontrols-focus.html

- Icons in Qt Quick Controls
  https://doc.qt.io/qt-6/qtquickcontrols-icons.html

- Control QML Type
  https://doc.qt.io/qt-6/qml-qtquick-controls-control.html

## Layout / Responsive

- Qt Quick Layouts Overview
  https://doc.qt.io/qt-6/qtquicklayouts-overview.html

- Qt Quick Layouts QML Module
  https://doc.qt.io/qt-6/qtquick-layouts-qmlmodule.html

- Qt Quick Responsive Layouts
  https://doc.qt.io/qt-6/qtquicklayouts-responsive.html

- Item QML Type
  https://doc.qt.io/qt-6/qml-qtquick-item.html

## RTL

- Right-to-left User Interfaces
  https://doc.qt.io/qt-6/qtquick-positioning-righttoleft.html

- LayoutMirroring QML Type
  https://doc.qt.io/qt-6/qml-qtquick-layoutmirroring.html

## Accessibility

- Accessibility for Qt Quick Applications
  https://doc.qt.io/qt-6.11/accessible-qtquick.html

## Rendering / Effects

- Qt Quick Performance Considerations and Suggestions
  https://doc.qt.io/qt-6/qtquick-performance.html

- MultiEffect
  https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html

- Qt Quick Scene Graph
  https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html

- Qt Quick Scene Graph Default Renderer
  https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html

- Animator
  https://doc.qt.io/qt-6/qml-qtquick-animator.html

- States and Animations
  https://doc.qt.io/qt-6/qtquick-statesanimations-topic.html

## High DPI

- High DPI
  https://doc.qt.io/qt-6/highdpi.html

## Loading / Views

- Loader
  https://doc.qt.io/qt-6/qml-qtquick-loader.html

- ListView
  https://doc.qt.io/qt-6/qml-qtquick-listview.html

## Tooling

- Qt QML Tooling
  https://doc.qt.io/qt-6/qtqml-tooling.html

- qmllint
  https://doc.qt.io/qt-6/qtqml-tooling-qmllint.html

- qmlformat
  https://doc.qt.io/qt-6/qtqml-tooling-qmlformat.html

- QML Language Server
  https://doc.qt.io/qt-6/qtqml-tooling-qmlls.html

- Profiling QML Applications
  https://doc.qt.io/qt-6/qtquick-profiling.html

- Qt Quick Compiler
  https://doc.qt.io/qt-6/qtqml-qtquick-compiler-tech.html

## Technology Preview

- Qt Labs StyleKit Features Overview
  https://doc.qt.io/qt-6/qtlabsstylekit-overview-features.html

- Style QML Type
  https://doc.qt.io/qt-6/qml-qt-labs-stylekit-style.html

---

# 30. Maintenance

When Qt releases a new minor version:

1. verify all links still resolve;
2. review `qtquick-bestpractices.html`;
3. review `qtquick-performance.html`;
4. review Qt Quick Controls style/customization changes;
5. review responsive-layout changes;
6. review QML tooling changes;
7. review StyleKit status;
8. compare new upstream guidance against `AGENTS.md`;
9. update repository policy only when the upstream change represents a durable
   project decision;
10. run the repository knowledge-contract verification after documentation
    changes.

Do not silently modify repository policy merely because an upstream Qt page
changed.

The purpose of this file is to make upstream evidence easy to find while
keeping the repository's own authority structure explicit and stable.
