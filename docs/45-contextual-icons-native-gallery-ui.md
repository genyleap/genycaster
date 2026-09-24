# Contextual Icons, Native Media Gallery, and Social UI Refinement

Date: 2026-09-02

## Feature gate

Responsibility: Replace presentation glyphs with the approved Genyleap SVG icon system, resolve one contextual presentation glyph for the existing Like action, present remote images in an in-app swipe gallery, and refine Home/Profile interaction hierarchy.

Capability origin: Local for icon selection, gallery state, layout, greeting period, menu availability, and presentation caching. Existing protocol Like/Unlike and provider-neutral profile reads remain unchanged.

Mandatory or optional: Mandatory for the shipping Qt composition; absent from provider-free/headless builds.

Public contract: QML consumes approved resource paths through `Icon`, asks `PresentationBridge` for a contextual Like glyph, and opens `MediaGallery` with already-normalized presentation media. `PresentationBridge::accountProfile` exposes the authenticated account's cached profile snapshot without changing the active navigation surface.

Canonical data/invariants: Contextual glyph selection never creates a new reaction kind. Like/Unlike remains the sole protocol action. Short keywords match complete Unicode-aware tokens. User-generated text is not rewritten. Profile/avatar/banner image sources continue through the existing C++ remote-image boundary.

Dependencies: QML -> Qt presentation adapter -> Application. SVG resources remain under `ui/assets/icons/genyleap`. No provider, HTTP, cursor, or storage implementation enters QML.

Expected failures: Unknown icon name falls back to the existing embedded font glyph; unmatched content uses `actions/heart`; unavailable media shows a local failure state; absent profile quality score renders an unavailable value without fabricating one.

Local-state representation: Process-local gallery state and presentation cache only. Existing SQLite profile `quality_score` remains the durable canonical projection.

Offline behavior: Embedded icons and cached account/profile/media presentation continue to render. The gallery can show any already-cached image and otherwise exposes the existing bounded remote-image failure state.

Provider policy and disablement: None for icon, gallery, or layout behavior. Profile quality remains provider-neutral and optional; its current source label is presentation provenance only.

Failover semantics: Existing routed profile/feed reads and cache-first behavior are unchanged. Missing quality data never triggers an ad-hoc QML request.

Tests proving the boundary: Deterministic contextual-reaction token/priority tests, provider score normalization coverage, localization catalog guard, architecture guard, strict QML lint, warning-fatal smoke, and compact/standard/wide LTR/RTL rendered inspection.

## Layout contract

- Compact: 360-599 px, mobile gutters, one reading column, bottom navigation limited to Home/Profile/Settings, full-window gallery.
- Standard: 600-999 px, bounded reading surface, compact rail where applicable, horizontally scrollable content tabs.
- Wide: 1000 px and above, persistent navigation plus bounded reading and context surfaces; content does not stretch indefinitely.
- Profile chrome follows the UI language direction. In Persian, the avatar overlap and primary identity alignment start at the right edge. Bio, handles, URLs, and Cast text preserve content-sensitive direction.
- Post surfaces do not change fill or border merely because the pointer hovers over the row. Interactive controls retain their own explicit hover/focus feedback.
- Gallery owns media navigation and dismissal. Tiles never delegate image clicks to the operating-system browser.
