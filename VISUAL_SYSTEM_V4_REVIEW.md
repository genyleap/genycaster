# Genycaster Visual System V4 — Flat Context Rail Pass

## Scope

This pass responds to the September 1, 2026 art-direction correction:

- remove project-owned drop shadows globally;
- keep the existing superellipse geometry and control APIs intact;
- redesign the desktop right context rail around the supplied clean habit/calendar reference;
- make the calendar the primary right-rail widget;
- use large white/graphite superellipse cards on a cool neutral rail canvas;
- add a reference-like compact weekly activity widget beneath the calendar.

## Shadow removal

`ui/components/Superellipse.qml` no longer imports or instantiates `QtQuick.Effects.MultiEffect` for shadows. Existing `elevation`, `shadowColor`, and `shadowOpacityScale` properties remain accepted only for source compatibility; `shadowEnabled` is fixed to `false` in this pass.

The `MultiEffect` instances that remain in `Avatar.qml` and `SuperellipseImage.qml` are image masks, not drop shadows.

## Right context rail

`ui/components/ContextRail.qml` was rebuilt as a cleaner stacked widget surface:

- dedicated cool-neutral rail canvas;
- borderless large superellipse cards;
- calendar card moved to the top and promoted to the dominant widget;
- localized Monday-first weekday labels;
- fixed six-week calendar grid with adjacent-month dates;
- previous/next month controls;
- selected/current-day states;
- partial accent event rings;
- compact calendar progress strip;
- reference-like weekly Activity card with mint interaction color and circular day states;
- Token Pulse and Suggested for You retained below using the same card anatomy.

## Theme additions

`ui/theme/Theme.qml` adds dedicated right-rail materials and semantic icons:

- `contextRailCanvas`
- `contextRailCard`
- `contextRailInset`
- `contextActivityAccent`
- `contextActivitySurface`
- `iconCalendar`
- `iconUsers`
- `iconChart`

No new user-facing translation IDs were introduced.

## Verification

Passed in the sandbox:

- `python3 tools/check_ui_design.py`
- `python3 tools/check_localization_catalog.py`
- `python3 tools/check_architecture.py`
- `python3 tools/check_r5_release_contract.py`
- `python3 tools/check_r7_release_contract.py`

`cmake --preset ui-debug` was attempted and reached the expected environment limitation: the sandbox does not contain a Qt6 SDK / `Qt6Config.cmake`, so QML runtime rendering and a full native build cannot be claimed here.
