# Context Loading, Rich Media, Semantic Tabs & Buffered Refresh — v21.5 Review

## Scope

This candidate addresses the eight feed/profile UX defects reported against alpha 9.6:

1. Hydrate the original cast behind profile replies and preserve engagement metadata on the reply itself.
2. Provide persisted, bounded content-loading controls instead of hard-coded feed budgets.
3. Make animated emoji resilient when the preferred Lottie renderer or an individual asset is unavailable.
4. Replace jagged/duplicated preview loading indicators with a single smooth scene-graph spinner.
5. Recognize YouTube watch, short, embed, live and short-link forms and route playback through the isolated media host.
6. Hydrate reply and quote context for all authors instead of leaving generic conversation placeholders.
7. Make Visuals/NFTs, Conversations and Devs semantic projections select relevant reply/media/developer rows.
8. Buffer new first-page feed rows behind a `Show N posts` affordance so refresh does not jump the viewport.

## Feature gate

Responsibility: Presentation-safe feed refresh, referenced-cast context hydration, profile-reply fidelity, media preview routing and user-tunable read budgets.

Capability origin: Protocol reads for casts/replies/reactions; Derived presentation classification for semantic tabs; Local persisted preferences for read budgets and refresh presentation.

Mandatory or optional: Core Farcaster reads remain mandatory; link metadata prefetch, inline YouTube playback, animated artwork and buffered refresh presentation are optional presentation behaviors.

Public contract: Existing provider-neutral `FeedQuery`, `CastState` and presentation bridge contracts. New Qt presentation properties expose bounded local read preferences and pending-new-post count without provider-specific DTOs or cursors.

Canonical data/invariants: Cast IDs remain canonical references. A successfully hydrated referenced cast is marked `resolved=true`; ID-only placeholders are not resolved. Feed provider order is preserved. Pagination appends/reconciles rather than replacing the visible timeline. New refresh rows may be held locally until explicitly revealed.

Dependencies: Application read service, provider-neutral domain models, Qt presentation bridge/QSettings, existing link preview resolver, media classifier, media web host and emoji controllers.

Expected failures: Referenced cast unavailable/deleted, provider request failure, link metadata unavailable, web-media host unavailable, unsupported graphics renderer, offline state. These degrade to an actionable open-context/external-link path or Unicode emoji instead of blocking the feed.

Local-state representation: Read preferences are stored under `read/*` in QSettings. Pending first-page rows are transient presentation state and are not persisted as protocol state.

Offline behavior: Cached visible feed remains usable. Referenced context that is already hydrated is preserved across pagination/cache snapshots. Unresolved references eventually show an open-original/quoted-cast fallback instead of an infinite spinner.

Provider policy and disablement: No provider URL or provider DTO is introduced into QML/domain/application. Hypersnap-specific reply metric hydration stays inside the Hypersnap adapter and is best-effort.

Failover semantics: Context hydration failure does not fail the containing feed. Link preview/YouTube host failure retains the external URL path. Emoji artwork failure retains the Unicode glyph until fallback artwork is actually ready.

Tests proving the boundary: Presentation tests cover bounded persisted preferences, buffered refresh/reveal behavior, referenced reply/quote metric hydration, explicit resolved markers and preservation of resolved media-only context across pagination.

## Implementation map

### Replies and referenced context

- `ui/mobile/main.cpp` discovers missing parent and quote cast IDs up to the user-configured hydration budget and resolves them through the application read service.
- Same-page and opened-cast previews carry an explicit `resolved` marker plus author, text, embeds and engagement counts.
- `PresentationBridge::applyReferencedCastPreview` patches visible rows, rows held behind the new-post banner and detail state; referenced revisions include engagement and media structure so stable delegates update.
- `CastItem.qml` and `EmbedCard.qml` distinguish ID-only placeholders from resolved content, show compact loading state, expose engagement counts and fall back after a bounded wait.
- Hypersnap `UserReplies` keeps provider-specific bulk engagement rehydration inside the adapter.

### Content & loading settings

Persisted presentation preferences:

- posts per request/refresh: 1..100 (default 20)
- reply/quote context hydration limit: 1..100 (default 40)
- buffer newly refreshed posts: enabled by default
- prefetch link-preview metadata: enabled by default
- inline YouTube playback: enabled by default

The settings UI explicitly describes these as read-cost/latency controls; it does not expose provider credentials, endpoints or provider-private cursors.

### Animated emoji

`RichInlineText.qml` keeps Lottie as the preferred renderer, starts the existing sprite renderer after renderer failure or an 850 ms readiness deadline, and hides the Unicode glyph only when actual artwork is ready. `SpriteEmoji.qml` now safely tolerates an unavailable emoji controller.

### Preview spinner

`GenySpinner.qml` uses Qt Quick Shapes with `Shape.CurveRenderer`, rounded caps and a single continuous rotation. `LinkPreview.qml` disables duplicate image-frame loading indicators and uses this spinner consistently.

### YouTube

The existing preview resolver recognizes `youtu.be`, `watch?v=`, `/shorts/`, `/embed/` and `/live/`, seeds YouTube artwork, and keeps YouTube pages in the web-hosted media family rather than treating them as raw video files. The media host now accepts `/live/` too and canonicalizes supported links to the privacy-enhanced `youtube-nocookie.com/embed/<id>` player. If the inline setting is disabled or the host is unavailable, the external URL path remains available.

### Semantic tabs

- Visuals/NFTs: no longer discards replies; recognizes media, collectible kinds, contract/token metadata, CAIP/EIP-155 locators and common NFT marketplace links.
- Conversations: accepts reply/thread rows and root casts with replies or explicit discussion signals.
- Devs: accepts roots and replies and inspects the hydrated parent context as well as the cast itself for Git hosting, repository/release/API/package/programming/smart-contract signals.
- Existing filtered-tab pagination prefetch remains responsible for fetching additional provider pages when the current materialized page does not produce enough semantic rows.

### Buffered new-post UX

A refresh of an already-visible feed reconciles updated metrics/content into rows already on screen while holding genuinely new cast IDs in a pending buffer. `FeedSurface.qml` renders a full-width `Show N posts` control directly below the overlay header. Revealing prepends unique pending rows and reapplies viewer reaction/follow projections before moving the view to the beginning.

## Verification evidence

### Passed static/contract guards

- `python3 tools/check_architecture.py` — PASS
- `python3 tools/check_async_resource_contract.py` — PASS
- `python3 tools/check_media_upload_contract.py` — PASS
- `python3 tools/check_r5_release_contract.py` — PASS
- `python3 tools/check_r7_release_contract.py` — PASS

### Baseline-only guard failures

`tools/check_ui_design.py` and `tools/check_localization_catalog.py` still return non-zero because alpha 9.6 already contains unrelated guard debt. Their output for this working tree is byte-for-byte identical to the untouched original tree, so this change set introduces no additional finding in either guard.

### Configure/build constraints in this environment

- `cmake --preset core-debug` configures successfully with UI/providers disabled.
- A full core build progressed without diagnostics before the execution window interrupted it; this is not recorded as a pass.
- The focused Hypersnap build cannot reach the changed `async_gateway.cpp`: GCC 14.2 ICEs first while compiling the unchanged `src/providers/hypersnap/async_request.cppm` at `cp/module.cc:16297`.
- `cmake --preset ui-debug` cannot configure because Qt 6.6 development CMake package files (`Qt6Config.cmake` / `qt6-config.cmake`) are not installed in this container.
- Consequently the new Qt presentation unit tests cannot be executed here. They are included for a Qt-capable CI/desktop toolchain and no full-build-pass claim is made.

## Release assessment

The source changes are internally reviewed and static architecture/release contracts pass. This package should be treated as a source candidate requiring normal Qt 6.6+ and a module-capable compiler CI run before release-signing, because this container cannot compile the Qt target and GCC 14.2 encounters a pre-existing modules compiler ICE on the Hypersnap dependency path.
