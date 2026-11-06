# Navigation glass — 2026-11-06

## Contract and evidence

- Responsibility: the existing compact navigation material, edge coverage and
  foreground contrast over arbitrary media; Local, mandatory presentation.
- Public contract: existing routes, dimensions, action button, safe-area and
  user settings remain unchanged. No new feature target.
- Invariants: live backdrop blur excludes the dock itself; enabled destinations
  stay readable over black/white/colored media in Light and Dark modes; the
  capsule mask retains antialiased edge coverage.
- Dependencies/state: existing Qt Quick Effects, theme tokens and Superellipse
  precision renderer; no provider, I/O, domain or persistence changes.
- Failures/offline: existing opaque surface fallback when glass/source is absent;
  no provider policy or failover changes.
- Verification: palette contrast against arbitrary backdrop extremes, existing
  navigation contracts and source-QML smoke; no compile/configure per user request.

The supplied screenshot shows the Light-mode dock over very dark media. The
previous 40% white tint produces a gray surface close to the muted icon color.
The previous mask also thresholds its antialiased alpha at 0.5 with zero spread.
The updated material pairs stronger neutral tint with dedicated navigation ink;
blur remains live. Precision edges are used for the tint and selection capsule.
A nonzero mask spread preserves fractional coverage around the blurred capsule.
Qt's mask spread behavior is documented at
https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html#maskSpreadAtMin-prop
and was checked against the installed Qt 6.12 shader implementation.

## Verification results

- Node navigation/material selection: 17/18 passed. All 14 checks in
  `navigation_glass_contrast`, `mobile_glass_safearea_regression`,
  `mobile_floating_dock_motion` and `mobile_action_dock_config` passed.
  The extra mixed desktop suite fails its existing bookmark inset assertion;
  `BookmarksSurface.qml` is unchanged and the HEAD version also lacks the
  asserted formula. This is unrelated to navigation glass.
- New contrast regression evaluates the solid semantic foreground colors over
  125 RGB backdrops per theme, including selected washes (minimum 4.5:1).
  It does not claim every partially transparent duotone SVG pixel has that ratio.
- An isolated QML fixture ran on Qt 6.12 / Cocoa / Metal with fatal warnings:
  3/3 passed (one material test plus setup/cleanup), switching Light/Dark over
  black, white, saturated pink and text. Pixel checks verify the material's
  luminance bounds and retained backdrop variation. Both captures were inspected.
  The fixture copies production BottomNavigation and Icon, uses the production
  Superellipse precision path without its C++ renderer, and supplies minimal
  theme/localization fixtures. It does not launch the complete application.
  Artifacts: `/tmp/genycaster-nav-glass/light.png`, `dark.png`, `tst_Glass.qml`.
- Existing `ui.qml_import_smoke`: 1/1 passed with fatal warnings; no rebuild.
- `qmlformat` parsed both edited production QML files without in-place writes.
- Architecture guard and `git diff --check`: passed.
- No configure, build, qmlcachegen or deployment. Browser/device acceptance of
  a fresh complete build remains unverified per the no-compilation instruction.
