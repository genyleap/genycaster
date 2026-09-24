# Genycaster Native UI/UX Redesign — Final Integration Report

Date: 2026-08-31

## Scope

This redesign was implemented directly in the existing Genycaster Qt Quick/C++ codebase. Existing presentation, authentication, guest browsing, write gating, localization, rich-composer, profile, and feed bindings were preserved rather than replaced with a mock application.

The implementation follows the Native UI/UX Redesign & Implementation Brief with Home as the master composition.

## Foundation

- Central semantic theme/token system retained and expanded.
- Mathematical superellipse identity uses `n = 4.0` through the reusable native QML `Superellipse` primitive.
- Spacing follows the 4px scale: 4, 8, 12, 16, 20, 24, 32, 40, 48.
- Light and Dark themes use independent semantic values rather than inversion.
- Primary adaptive thresholds are 600 / 900 / 1200 / 1440 px.
- Desktop target widths are 232px navigation, controlled 720–800px reading canvas, and 304px context rail.

## Product shell

Desktop navigation uses the required destination order:

1. Home
2. Explorer
3. Channels
4. Notifications
5. Store
6. Business
7. Messages
8. Bookmarks
9. Profile
10. Settings

Desktop includes the rich sidebar profile summary. Compact desktop and tablet widths use a compact navigation rail rather than shrinking the full sidebar. Mobile uses Home / Explore / Create / Notifications / Profile bottom navigation.

At 900–1199px, Home keeps peripheral awareness available through a collapsible context drawer rather than permanently consuming reading width.

## Master Home

Home now establishes the social design language through:

- compact personal greeting;
- progressive composer connected to the real `PresentationBridge` write/auth flow;
- Genycaster story strip with unread, partial, viewed, special and live states;
- presentation tabs for For You, Visuals, Conversations, Devs and Following;
- semantic For You sections derived from the real feed rows;
- desktop context rail ordered as Token Pulse, Calendar, Activity and Suggested for You.

Guest users can continue reading and browsing public content. Identity-requiring interactions open the existing authentication flow instead of blocking the whole application.

## Semantic content presentation

The feed is no longer one generic geometry for every mode.

### Conversations

Compact, reading-first cast composition with flat timeline treatment, reply context, subtle separators, engagement controls and right-click contextual actions.

### Visuals

Spacious composition with quieter chrome and reusable editorial gallery behavior. Image URL embeds are recognized when their URL provides a usable media extension.

### Multi-image media

`GalleryLayout` supports distinct one-, two-, three-, four-, and 5+ image compositions, including a dominant-image layout and `+N` continuation treatment.

### Video

`VideoPreview` provides thumbnail, play affordance, duration, watched progress, mute state and graceful unavailable-media presentation.

### Links

`LinkPreview` supports favicon, domain, title, summary, strong preview image, author/date/reading-time metadata and external navigation without reducing every URL to a tiny generic card.

### Collectibles

`CollectiblePreview` keeps artwork primary and accepts optional collection, edition, chain, price/last-sale and creator metadata without turning the social post into a trading panel.

### Developer content

Developer presentation remains social and reuses the rich composer/read renderer. Code blocks retain native syntax highlighting, dedicated monospace typography and copy behavior.

## Mobile creation

The central Create destination now expands into a Genycaster-specific creation surface with Cast, Photo, Story and Space paths. Unsupported remote behavior is not faked; existing auth/write gates remain authoritative.

## Search and native interaction

- Global `⌘K` on macOS and `Ctrl+K` on Windows/Linux opens the search palette.
- Core controls preserve focus, hover, pressed and selected states.
- Casts expose a desktop right-click contextual action surface in addition to the explicit more control.
- Popups/drawers use Escape-to-close through Qt Quick Controls behavior.

## Demo-data policy

Temporary social/context presentation data is isolated in `ui/demo/DemoSocialData.qml`.

It is used only where the backend does not yet provide the corresponding product model (stories, token pulse, calendar events, compact activity and suggestions). Real feed, profile, authentication and write models are not replaced by demo data.

## Localization

All production UI remains ID-based and translation-safe. The redesign passes the project localization guard for all 12 required locale catalogs. Layouts use implicit sizing and semantic direction instead of English-only fixed label widths.

## Validation performed in this container

PASS:

- UI design guard — 77 QML files audited.
- Localization catalog guard — 519 referenced IDs / 520 catalog IDs / 12 locales.
- Architecture guard.
- R6 release contract.
- R7 release contract after aligning the contract check with the lifecycle-safe `insertManagedMedia` path already used by the composer.
- `core-debug` CMake configuration.

Environment/toolchain blockers:

- GCC 14.2 reproducibly hits an internal compiler error in the C++ Modules writer while compiling `src/application/read_service.cppm`, including with `-j1`. The failure occurs before the Qt/QML target is built and does not provide a semantic source diagnostic for the redesign.
- The container does not include the Qt 6 development package, so `ui-debug` configuration stops at `find_package(Qt6 6.6)` and runtime/qmllint screenshot validation must be performed on the normal Genycaster Qt development environment.

The media-gateway contract guard currently reports unrelated storage-adapter drift in `services/media-gateway`; this redesign intentionally does not rewrite that backend subsystem as part of a visual UI delivery.

## Final engineering state

The UI is structured as a real application surface, not a screenshot mockup. Missing backend capabilities have stable presentation boundaries, existing working behavior remains connected, and the major social presentation systems are reusable QML components that can receive richer production metadata later without another architectural UI rewrite.
