# 24 — Production UI Foundation

## Status

Implemented as the post-Phase-4 presentation checkpoint. This change does not
add protocol write operations and does not alter the provider/domain/application
boundaries.

## Goal

Replace the Phase 4 validation shell with a production-oriented Qt Quick
foundation while preserving the existing local-first read contract.

The UI must present a social client first. Cache/network state remains visible,
but architecture terminology and development-console framing are removed from
the product surface.

## Implemented structure

The previous monolithic `AppShell.qml` was split into reusable QML components:

- `ActionButton.qml` — shared primary/secondary/quiet action treatment;
- `AppHeader.qml` — compact product header and identity access;
- `SideNavigation.qml` — persistent wide-layout navigation;
- `BottomNavigation.qml` — compact mobile Home, Explorer, Create, Apps, and Settings navigation;
- `FeedHeader.qml` — feed selection and explicit channel opening;
- `FeedSurface.qml` — the single list scroll owner for feed/social lists;
- `CastItem.qml` — canonical cast row composition;
- `ProfileListItem.qml` — follower/following row composition;
- `DetailSurface.qml` — scrollable cast/profile detail surface;
- `StatusStrip.qml` — local-first lifecycle and retry status;
- `Avatar.qml` — deterministic textual avatar fallback with a local image-provider hook;
- `EmptyState.qml`, `SectionHeader.qml`, and navigation primitives.

`AppShell.qml` now composes those parts and retains the existing presentation
bridge as the only QML-facing state/intent boundary.

## Data contract improvement

The Qt composition root now passes canonical `Profile::avatarUrl` values into
feed rows, social rows, cast detail, and profile detail. QML does not parse
provider DTOs and does not know provider-specific field names. Remote values
remain presentation data and are not fetched by QML; `Avatar.qml` loads only
`image://` or `qrc:/` sources until a C++ media cache/transport boundary owns
remote retrieval.

## Responsive behavior

- Compact (`< 600 px`): top product header, status strip, bounded content,
  bottom navigation.
- Standard (`600–999 px`): bounded reading surface with bottom navigation.
- Wide (`>= 1000 px`): persistent navigation rail plus a maximum 720 px reading
  surface.
- Feed/social lists remain the single scroll owner for list screens.
- Detail screens use one bounded `Flickable`.

## Interaction constraints

Only capabilities that exist in the current C++ presentation boundary are
exposed. Search, notifications, reactions, composer, and write actions are not
shown as inert controls. The Phase 5 composer should enter this shell only after
the write service/outbox/signing path exists.

## Verification performed in the current checkpoint

Verified:

- architecture guard passes;
- macOS SDK include guard passes against the existing core compile database;
- all QML delimiters are structurally balanced;
- every QML source is registered in `ui/CMakeLists.txt`;
- raw UI colors are centralized in `Theme.qml`;
- smoke-test object names (`appShell`, `readList`, sidebar actions) are retained.

Not verified in the current Linux container:

- Qt `qmllint` and runtime smoke, because Qt 6 is not installed in the
  container;
- rendered Qt screenshots on the target macOS/Qt 6.11.1 toolchain.

Target-device acceptance still requires a clean `ui-debug` configure/build,
strict QML lint, UI smoke/runtime smoke, and compact/standard/wide screenshot
review.

## Next UI work

Do not expand navigation with placeholder features. The next production UI
slice should be driven by Phase 5 and canonical read-model expansion:

1. complete Phase 5 Publish Cast vertical slice;
2. add a real composer surface backed by the Application write service;
3. expose canonical engagement/reply/embed metadata in the presentation model;
4. extend `CastItem` and `DetailSurface` with real actions and thread context;
5. add dark semantic tokens only after daylight target screenshots pass;
6. perform keyboard/focus/accessibility verification on macOS.

## Read-state corrective iteration

The follow-up read/UI iteration is documented in
`docs/25-read-reachability-and-interaction.md`. Provider unavailability is no
longer labeled as device Offline, stale is asserted only when usable cache
exists, debug-only failure diagnostics are interactive, and reconnect performs
one bounded retry.
