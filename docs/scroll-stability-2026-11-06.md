# Feed and conversation scroll stability — 2026-11-06

## Scope and evidence

This change addresses unwanted upward scroll jumps during feed incubation,
conversation refresh and late media layout. The first supplied recording was absent. The follow-up recording
`2026-11-06 09-57-04.mp4` was available (1920×1080, 37.098 seconds).
Frames at two-second intervals from 0 through 36 seconds and a closer frame at
25 seconds show large empty regions, delayed media and abrupt content changes
in the existing desktop build. These are pre-fix visual observations, not visual
acceptance of the edited source.

Engineering authority read: `thecompez/ai-for-modern-cpp` revision
`326cd850c2cb5e39264b7d2ccf2c29fba6627380`, `AGENTS.md` and the required
Architecture, Modules, Naming, Syntax and Style, API Design, Errors and Resources,
CMake and Toolchains, Testing and Verification, and Qt Quick UI guides.
The user's explicit no-compilation instruction overrides build requirements for
this task. No configure, compilation, resource generation, or deployment ran.

## Presentation contract

- Responsibility/capability: local presentation geometry and stable reply order;
  existing feed and conversation surfaces, with no new product capability.
- Public contract: existing cast/reply actions and data remain unchanged.
- Invariants: retain reply delegates by normalized Cast ID within one parent
  conversation; input cancels stale viewport corrections; late image decode
  cannot resize a scrolling gallery; only actual geometry changes initiate
  compensation, never an estimated total content extent.
- Dependencies/state: existing `FeedRowsModel` and QML-owned ephemeral payload,
  layout and pending-intent state. No new C++/provider/storage boundary.
- Offline/failure behavior: an empty busy refresh preserves current replies;
  removed anchors do not force navigation; cast changes reset only that
  conversation's state. Existing provider policy/failover is unchanged.
- Layout: preserve compact/desktop widths, gutters, typography, action order,
  full-image fit and virtualized delegates. Unknown image dimensions keep the
  initial reservation, which can leave letterboxing until opened in the viewer.

## Corrections

1. `FeedSurface.qml`: stop treating estimated `contentHeight` changes as requests
   to restore an old viewport. Capture the visible row immediately before an
   actual measured-height commit at rest, and apply
   its measured displacement without clamping against an estimated extent.
   Wheel/browser/scrollbar changes to `contentY` cancel pending correction timers
   even when `moving` is false. Retained rebuild anchors restore directly instead
   of first snapping their top to the viewport for one frame.
2. `DetailSurface.qml`: replace the repeatedly assigned array model with the
   existing stable-ID Qt row model. Apply insertion, movement, deletion and payload
   revision updates without resetting the model on hydration/reaction updates.
   Preserve the visible reply during idle header/row height settlement. Measured
   height is authoritative during motion too: freezing the outer slot while
   its content changes creates empty bands or overlapping content.
   Confirmed-reply focus is one cancellable intent per reply, not a recurring
   content-height side effect. Pooled delegates become invisible.
3. `CastEmbedsPanel.qml`: decoded image dimensions no longer replace the reserved
   aspect ratio in any scrolling surface, including conversation replies.
4. Feed and detail row heights now follow loaded content during motion. The
   previous blanket movement guard retained oversized estimates or clipped
   growing content. Reused feed rows synchronize immediately, including when
   their measured height happens to equal the previous identity's height.
5. `CastItem.qml`, `GalleryLayout.qml`, `CastEmbedsPanel.qml`: keep existing
   supporting previews mounted across payload updates. Stagger tile construction
   without waiting indefinitely for movement to stop. Loaders/provider queues
   remain bounded by the virtualized viewport and existing cache/budgets.
6. `SuperellipseImage.qml`: normal image loading and frame-paced presentation
   continue while scrolling. Even explicitly deferred surfaces no longer detach
   an in-flight request on every gesture. Reused URLs still clear the previous
   image identity; data/download policy and asynchronous decoding are unchanged.
7. `WalletBrowserPicker.qml`: remove `LayoutMirroring` from `GenyModal`, which
   derives from `Popup`, not Item. Keep the existing direction/mirroring policy
   on its `ColumnLayout` and row content.

Qt documents that variable-height ListView delegates make total content extent
an estimate: <https://doc.qt.io/qt-6/qml-qtquick-listview.html>.

## Verification without compilation

Commands ran from the repository root. Qt tools used the existing 6.12.0 host
installation. Existing runtime tests used Basic Controls/minimal/software with
fatal warnings as configured in CTest. They were not rebuilt.

| Check | Actual result |
| --- | --- |
| `node --test tests/wasm/*scroll*.test.mjs tests/wasm/feed_*.test.mjs tests/wasm/miniapp_wallet_provider.test.mjs` | PASS, 86/86, including 18 new behavioral/source regressions |
| `node --test --test-name-pattern='confirmed reply returns' tests/wasm/reply_keyboard_navigation_logout.test.mjs` | PASS, 1/1 |
| `python3 ci/check_architecture.py` | PASS |
| `/Users/compez/Qt/6.12.0/host/bin/qmlformat ui/components/FeedSurface.qml ui/components/DetailSurface.qml ui/components/CastEmbedsPanel.qml ui/components/CastItem.qml ui/components/GalleryLayout.qml ui/components/SuperellipseImage.qml ui/components/WalletBrowserPicker.qml` | PASS, parsing/format output only; no in-place formatting |
| `ctest --test-dir build/Qt_6_12_0_macOS_Desktop_Debug -R '^ui.qml_import_smoke$' --output-on-failure --no-tests=error` | PASS, 1/1; existing executable loads source QML |
| `node tests/ui/feed_geometry_runtime.mjs /Users/compez/Qt/Build/6.12.0/host/qtbase/bin/qmltestrunner` | PASS, 3 behavioral scenarios plus setup/cleanup (5/5), Qt 6.12.0, zero warnings |
| `git diff --check` | PASS |
| Configure/full build/AOT | NOT RUN, user instruction |
| Visual/device scroll acceptance | NOT VERIFIED |

The behavioral regressions execute actual QML JavaScript functions with simulated
model/viewport/timer objects. They cover stale corrections, incorrect estimated
extents, direct retained-anchor restoration, inactive/moving guards, hydration,
append/reorder/delete/deduplication, parent navigation, busy-empty refreshes,
cancelled focus, decoded-media reservation and late header/row compensation.
The additional `feed_geometry_runtime.mjs` harness extracts the current
production `FeedRow` declaration and runs it with a real Qt `ListView`,
asynchronous `Loader`, reuse pool and kinetic flick. Heavy cast content is a
lightweight deterministic test double; this is not a full app render.
It proves actual height growth, oversized reservation release during motion,
and reused row identity/geometry. Injecting the previous movement guard into
a disposable copy of this harness reproduced both geometry failures (3 pass,
2 fail); the unmodified production-source harness passed all 5 checks.
The installed SDK runner was rejected by macOS for an invalid code signature;
the existing runner in the Qt build tree ran successfully without modification
or compilation.

Additional attempted checks and limits:

- An exploratory broader media-source test run reported 82/89 passing. One
  assertion required the old request-detachment behavior and was updated to
  require request continuity; it passes in the final focused run. The other six
  assertions concern unchanged LinkPreview layout formulas, setup power-save
  markup and locale coverage. Those unrelated surfaces were not edited.

- The broader initial Node selection including all reply/navigation tests had
  57/58 passing. Its unrelated `beta label is short and is translated in every
  locale` assertion expects obsolete Settings markup. Running that same test
  from the HEAD version reproduced the failure (0/1). It was not changed.
- `ctest --test-dir build/Qt_6_12_0_macOS_Desktop_Debug -R '^ui.media_scroll$'
  --output-on-failure --no-tests=error` failed (0/1) before the scrolling checks.
  The existing executable reports undefined `genyPulseActivity` in
  `GenyPulsePanel.qml` plus Home/footer fixture failures. This run cannot validate
  the changed scrolling behavior; no rebuild was attempted.
- Direct file `qmllint` with the existing generated import path reported unresolved
  `FeedRowsModel` context imports (including the pre-existing FeedSurface use)
  and an existing layout-owned height warning. It is not a clean lint result.
- Strict module lint (`qmllint --bare --max-warnings 0`, generated QML/UI import
  roots and the Qt 6.12.0 QML directory, `-M Farcaster.Client`) failed on unresolved
  `ProCastPolicy`. The generated module contains older copied QML, so this is also
  not evidence for strict lint acceptance of the edited source.

Remaining acceptance: a subsequent authorized fresh build and real desktop,
mobile and browser scroll runs with delayed images/link previews, append/refresh,
conversation navigation and interrupted gestures. No claim of device-level
stability or warning-clean full-app runtime acceptance is made by these checks.
The user explicitly authorized committing and pushing the complete task changes
after the follow-up video and wallet warning correction.
