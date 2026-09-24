# Genycaster Visual System 2.0 — Review and Redesign Pass

## Reference-set findings

The supplied references consistently use a restrained, product-grade interaction language rather than decorative styling:

- neutral off-white / graphite canvases;
- crisp high-contrast typography with large hierarchy jumps;
- one strong accent color, isolated to state and primary actions;
- continuous-corner geometry rather than ordinary rounded rectangles;
- white/elevated controls separated by optical shadow before border;
- very soft multi-layer shadows with small vertical displacement;
- compact pill, segmented, and icon controls;
- pressed feedback through subtle scale rather than dramatic animation;
- sparse separators and low-contrast hairline borders;
- generous negative space around important content;
- navigation surfaces that feel like intentional objects instead of full-bleed bars.

## Baseline problems found in the source

The previous design foundation was structurally sound but visually under-resolved:

1. The light palette was blue-gray and read as generic SaaS chrome rather than a premium native social client.
2. `Theme.shadow` existed, but no reusable elevation model consumed it.
3. Most controls lived on the same z-plane. Inputs, popovers, modals, navigation, and buttons differed mostly by fill/border.
4. Standard buttons and fields relied on gray fills and visible borders instead of subtle surface contrast plus depth.
5. Desktop navigation and mobile bottom navigation were full-bleed structural rectangles, giving the shell a prototype feel.
6. Selected navigation states depended too heavily on accent-colored text/icon state.
7. Pressed states were mostly fill changes; tactile response was inconsistent.
8. Modal and popover geometry was correct but did not visually separate enough from the underlying application.
9. Composer formatting controls had the right behavior but lacked the elevated toolbar treatment visible in the reference set.
10. The design token system mixed good semantic roles with colors that were too tinted for the desired reference language.

## Implemented redesign

### 1. Neutral visual palette

`Theme.qml` now uses a neutral light canvas and graphite dark system. The Geny accent is more vivid, while large surfaces remain neutral.

The accent is no longer responsible for creating visual depth. Depth comes from surface hierarchy and elevation.

### 2. Optical elevation tokens

Added semantic elevation levels:

- `elevationNone`
- `elevationControl`
- `elevationPopover`
- `elevationModal`

Added three semantic shadow colors:

- `shadowLow`
- `shadowMid`
- `shadowHigh`

### 3. Superellipse-native shadow rendering

`Superellipse.qml` now supports semantic elevation directly. Shadow silhouettes use the exact same mathematical geometry as the rendered surface.

The shadow nodes are instantiated through a `Repeater` only when elevation is non-zero, so flat feed geometry does not pay for hidden shadow nodes.

This avoids adding Qt Graphical Effects / web-style shadow dependencies and keeps the UI architecture native.

### 4. Button family

`ActionButton` was rebuilt around the reference interaction language:

- raised secondary surface;
- low optical shadow for primary/secondary actions;
- cleaner borders;
- tactile pressed scaling;
- preserved primary, quiet, destructive, selected, icon-only, compact, and RTL behavior;
- optional pill geometry without changing the public component contract.

### 5. Inputs and combo boxes

Text fields and text areas now behave like clean white/graphite controls instead of permanently gray input wells.

Focus creates surface lift and a clear semantic focus ring.

`GenyComboBox` now uses raised closed-state geometry and an elevated popover with a softer hairline boundary.

### 6. Switches

`GenySwitch` now uses a wider continuous track, elevated circular thumb, tactile press feedback, and cleaner focus treatment.

### 7. Tabs and segmented controls

`GenyTabButton` now combines a quiet selected surface with a restrained accent indicator. This gives selection a tangible state without turning content tabs into heavy dashboard pills.

### 8. Navigation

Desktop `SideNavigation` and `ContextRail` now render as bounded elevated surfaces on the canvas instead of full-height flat panes.

Mobile `BottomNavigation` is now a floating elevated dock with continuous corners and soft separation from content.

`NavItem` and `BottomNavButton` now use cleaner selected geometry and tactile press feedback.

### 9. Composer chrome

`FormattingToolbar` now uses an elevated surface and larger continuous geometry. Existing editing behavior is unchanged.

The floating composer action now participates in the same elevation system.

### 10. Modal hierarchy

`PopupSurface` now owns modal elevation, softer border semantics, and the larger modal curve role. All existing `GenyModal` / `GenyDrawer` flows inherit it without backend changes.

## Design-system rules after this pass

1. **Content remains flat unless containment is necessary.** Cast rows are not converted into floating cards.
2. **Controls may elevate; content should not become dashboard chrome.**
3. **Accent communicates state/action, not canvas identity.**
4. **Borders are fallback separation. Surface contrast and elevation should do most of the work.**
5. **One control state must not change layout geometry.** Press feedback uses transform scale only.
6. **All branded contained geometry remains superellipse-based.**
7. **Elevation must use semantic levels, never arbitrary shadow literals in screen QML.**
8. **RTL/LTR contracts remain unchanged.** Visual polish must not reintroduce global mirroring.
9. **Reduced-motion mode still collapses interaction animations through existing motion tokens.**
10. **List-heavy surfaces remain effect-light.** Only explicitly elevated controls instantiate shadow geometry.

## Validation performed

Passed:

- `python3 tools/check_ui_design.py`
- `python3 tools/check_localization_catalog.py`
- `python3 tools/check_architecture.py`

Qt configure was attempted with `cmake --preset ui-debug`. The execution environment does not contain Qt6, so configuration stops at `find_package(Qt6)` before QML compilation/runtime validation.

## Required visual QA on a Qt 6.11 workstation

Render and compare at minimum:

- 390×844 light and dark;
- 760×900 light and dark;
- 1280×900 light and dark;
- Persian RTL and English LTR;
- hover / pressed / focus / disabled states;
- bottom dock with composer FAB;
- desktop sidebar + feed + context rail;
- composer drawer and formatting toolbar;
- account/settings drawers;
- long translated labels and mixed Persian/Latin content.

The next visual pass should be screenshot-driven and tune optical values (shadow strength, control spacing, and navigation density) against real Qt rendering rather than adding more structural effects.
