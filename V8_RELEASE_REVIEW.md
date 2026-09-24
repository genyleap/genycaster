# Genycaster V8 — Calendar, Profile, Recent Posters, Activity Heatmap, Drawer and Preview Pass

This release is the first packaged artifact containing the V8 changes that were previously present only in the working tree.

## User-visible changes versus V7

- Right context rail widened from 304 px to 384 px.
- Left navigation widened from 264 px to 280 px.
- Calendar switched from JavaScript/Gregorian-only month construction to a C++ `QCalendar`-backed month model driven by localization/calendar preferences.
- Calendar presentation simplified to month navigation + weekday/day grid; demo event rings/progress details were removed.
- Profile Trust Score now consumes normalized provider quality score `[0, 1]`, with Neynar score support in the HyperSnap codec and an explicit `Preview · Neynar` label.
- Profile Trust card has loading states and a two-sided flip interaction. The back face prioritizes a larger centered profile avatar and additional identity details.
- Fake Story entries were removed. The horizontal strip now derives unique recent posters from actual feed rows.
- Community Pulse became a GitHub-style 52-week × 7-day contribution heatmap based on canonical feed timestamps.
- Top status notification became a floating animated island so appearance/disappearance does not relayout the feed.
- Shared drawers support a frosted backdrop via `ShaderEffectSource` + `MultiEffect` instead of exposing an opaque drawer background.
- Composer tab behavior was made exclusive and Farcaster Preview was redesigned as a cast-like preview rather than a raw URL panel.
- Semantic theme tokens were added to prevent `undefined` QColor assignments in `FeedPulseCard` and `ProfileTrustCard`.

## Backend/model changes

- `domain::Profile` includes optional `qualityScore`.
- HyperSnap profile decoding reads `score` and falls back to legacy `experimental.neynar_user_score`, accepting finite values in `[0, 1]`.
- Profile quality score is persisted in SQLite entity storage.
- `LocalizationController::calendarMonthModel()` exposes localized calendar month metadata/cells to QML.

## Verification performed in this environment

- `tools/check_ui_design.py` — PASS (82 QML files)
- `tools/check_localization_catalog.py` — PASS (520 referenced IDs, 523 catalog IDs, 12 locales)
- `tools/check_architecture.py` — PASS
- `tools/check_media_upload_contract.py` — PASS
- `tools/check_r7_release_contract.py` — PASS

Qt runtime/build validation must still be performed on the target Qt 6.11.x installation because this sandbox does not provide the Qt 6 SDK required by the UI preset.

## Important correction

The prior assistant response described these V8 changes but linked the V7 archive. Consequently, installing that archive could not show the calendar/profile/story/heatmap changes. This archive contains the actual V8 working-tree changes listed above.
