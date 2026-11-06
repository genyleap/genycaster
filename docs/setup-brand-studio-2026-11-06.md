# Setup brand studio — 2026-11-06

## Presentation contract

- Responsibility: refine the existing desktop Setup brand rail; Local,
  decorative and optional. Setup actions, authentication and consent are unchanged.
- Public contract: preserve drag orbit, wheel zoom, keyboard reset/navigation,
  metadata links, responsive rail visibility and the canonical logo geometry.
- Invariants: bounded existing animation clock; no decorative input blockers;
  pause automatic motion when hidden, inactive, reduced-motion or power-saving.
- Dependencies/state: Qt Quick Shapes/Effects and existing shader; transient
  pointer/orbit/pulse state only. No network, storage or provider dependencies.
- Failure/offline: fully local assets; no changes to routing or failover.
- Verification: source-QML parsing, existing smoke/runtime tools, visual rail
  fixture using the existing QSB, plus motion lifecycle regressions.

Refinement mode: preserve the split layout and brand identity. Improve sculpture
contrast, frame it with faint orbital arcs, and connect a brief light response to
Setup stage changes and interaction. The right-hand form remains the primary
task. No configure, build or shader compilation is authorized for this task.

## Changes and motion contract

- A single Qt-provided color pass improves contrast and reduces the cyan cast;
  no blur, shadow pass, new texture assets or shader changes.
- Two fine arcs and one orbiting light use the existing 25 Hz orbit state. No
  added repeating timers. The default/reset view starts at -18 degrees so the
  front and extrusion read together.
- Hover parallax settles over 160 ms; direct drag stays immediate. Existing
  click/Enter feedback also acknowledges actual Setup stage changes. It never
  blocks navigation. Explicit drag/zoom/reset remain available in reduced motion.
- Automatic motion and pulses stop when the rail is hidden, the app is inactive,
  Reduced Motion is enabled, or media power saving is enabled. Static depth and
  framing remain visible.

## Actual verification

- `node tests/ui/setup_brand_runtime.mjs
  /Users/compez/Qt/Build/6.12.0/host/qtbase/bin/qmltestrunner
  /tmp/genycaster-setup-studio/final`: PASS, 5/5 (capture, lifecycle, reset/zoom,
  setup/cleanup). Qt 6.12, Cocoa/Metal, fatal warnings enabled.
  The fixture extracts the entire production rail, loads existing QSB/atlases
  directly, and stubs only Theme/LocaleFormat and its outer Setup state. Light
  and Dark captures were visually inspected. This is not a full-app launch.
- `ctest --test-dir build/Qt_6_12_0_macOS_Desktop_Debug
  -R '^ui.qml_import_smoke$' --output-on-failure --no-tests=error`: PASS, 1/1,
  existing executable loading source QML, fatal warnings enabled.
- `node --test tests/wasm/glass_g_union_geometry.test.mjs
  tests/wasm/setup_wireframe_flow.test.mjs
  tests/wasm/setup_persistence_loader.test.mjs`: 44/51. Seven existing failures
  concern removed studio control buttons, WASM download/splash markup, Home
  editing, Setup review and shell visibility. All seven reproduce using the
  HEAD version of InitialSetup (0/7), while other inputs remain unchanged.
  The completion-badge test now scopes its no-effects assertion to the badge;
  it also ensures the rail has one color pass without blur/shadow.
- `qmlformat` parsing, architecture guard and `git diff --check`: PASS.
- No compilation, configure, QSB generation or resource generation. The existing
  GLSL/QSB and canonical atlases are unchanged. Fresh full-app browser/mobile
  acceptance and GPU performance profiling remain unverified.
