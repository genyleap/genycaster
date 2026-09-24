# Genycaster Interaction & Layout V7 Review

## Scope

This pass responds to runtime screenshots from Qt 6.11.1 on macOS and intentionally changes interaction/layout behavior rather than adding more decorative treatment.

## 1. Profile placement and navigation width

- The detailed `ProfileTrustCard` is owned by the desktop left navigation only.
- The right `ContextRail` no longer duplicates the account/profile card.
- Desktop navigation width is now 264 logical pixels (8-point aligned), up from 232.
- The sidebar Trust/Following/Followers values expose a loading state while the authenticated account profile snapshot is not yet available. Valid zero values remain distinguishable from unavailable data.

## 2. Useful right-rail replacement

`FeedPulseCard.qml` replaces the duplicate profile widget.

It uses the currently loaded canonical feed rows and never invents historical analytics:

- Likes, Recasts and Replies are selectable metrics.
- The large value is the total for the loaded rows for the selected metric.
- The dot field is a logarithmically normalized visualization of the per-cast engagement counts from those same loaded rows.
- An explicit spinner is shown when the feed has not produced rows yet and the read pipeline is busy.

The visual hierarchy intentionally follows the supplied compact analytics reference without copying its revenue semantics.

## 3. Composer interaction / apparent application freeze

The desktop composer is no longer modal.

- Desktop: `modal: false`, no dimming, no forced popup focus. The rest of the shell remains interactive while the editor is open.
- Compact/mobile: the composer remains a modal bottom sheet because that composition occupies the primary task surface.
- Escape is an application shortcut for closing the composer.
- Mobile keeps drawer gestures; desktop does not compete with the editor ScrollView for vertical drag gestures.

This specifically addresses the runtime behavior where opening the composer made the rest of the window appear hung.

## 4. Media preview density

The upload surface now has one clear preview responsibility per region instead of rendering the same media at several large sizes.

- Once attachments exist, the drop zone collapses to a compact add-more target.
- Attachment rows use a compact media-kind icon instead of starting another image/video decoder.
- The dedicated thumbnail strip owns visual thumbnails and remains horizontally flickable.
- Thumbnail geometry is larger and cleaner while row height is reduced.
- Rich editor media blocks are compact attachment rows rather than large duplicate video/image cards.
- The Farcaster Preview visually renders up to the protocol's first two media embeds and removes exact media URL-only lines from the *visual preview only*. Publishing still uses the unchanged safe serialization and protocol embed contract.

## 5. Video thumbnails

`MediaPreview.qml` no longer routes `VideoOutput` through the previous hidden-source `MultiEffect` path.

- `VideoOutput` renders directly through Qt Multimedia/Qt Quick.
- A newly loaded video is briefly advanced muted and then paused so the decoded first frame remains available as a thumbnail/poster.
- User play/pause takes ownership from the automatic poster priming path.
- Tiny previews suppress verbose decoder error text and retain a media fallback icon.

This keeps image/video decoding under Qt Multimedia while reducing duplicate active players in the composer.

## Verification performed in this environment

Passed:

- `tools/check_ui_design.py` — 82 QML files
- `tools/check_localization_catalog.py` — 519 referenced IDs / 523 catalog IDs / 12 locales
- `tools/check_architecture.py`
- `tools/check_media_upload_contract.py`
- `tools/check_r5_release_contract.py`
- `tools/check_r7_release_contract.py`

Qt configure was attempted with `cmake --preset ui-debug` and reaches the project Qt dependency gate. This sandbox does not contain a Qt6 SDK / `Qt6Config.cmake`, therefore full QML type creation, `qmllint`, application build and runtime screenshot verification are **NOT VERIFIED here** and must be rerun with the project's Qt 6.11.1 macOS kit.

## Runtime acceptance checklist for the macOS kit

1. Open composer at a wide desktop size and confirm Feed/sidebars remain clickable.
2. Close via the visible close action and Escape.
3. Add JPG/PNG and MOV/MP4 together; confirm no duplicate large media preview appears inside the visual editor.
4. Confirm image thumbnails render immediately and video tiles settle on a decoded poster frame.
5. Horizontally scroll the attachment thumbnail strip with trackpad and drag.
6. Confirm Farcaster Preview shows at most two media previews while the published byte count remains unchanged.
7. Start with a cold profile cache and verify sidebar metrics show loading rather than `0`; after the own-profile snapshot arrives, verify real zero/non-zero values replace loading.
8. Confirm the right rail contains Feed/Community Pulse, Calendar, Activity and Token Pulse with no duplicate profile card.
