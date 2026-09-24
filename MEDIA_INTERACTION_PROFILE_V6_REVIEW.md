# Genycaster — Media Interaction & Profile Widget V6 Review

## Scope

This pass addresses the runtime issues reported against the V5 composer and the requested profile-summary redesign.

## Media upload/runtime fixes

### 1. Preview classification is explicit

`MediaUploadController` now classifies a local selection before handing it to the native upload pipeline. Supported local media is mapped to a canonical MIME type and a preview kind (`image` or `video`). Unsupported or unreadable files never receive a preview URL.

This prevents `SuperellipseImage` from being asked to decode MP4, MP3, ZIP, or other non-image inputs. In particular, video preview is no longer implemented by trying to pass a movie URL to `QQuickImage`.

### 2. Video preview uses Qt Multimedia

`MediaPreview.qml` provides a shared attachment-preview surface:

- images -> `SuperellipseImage`
- videos -> `MediaPlayer` + `VideoOutput`
- local and stable remote URLs are both accepted
- video thumbnails can play/pause where interaction is enabled
- failed/unsupported previews fall back to a bounded file state rather than emitting image-decoder noise

The UI module now links `Qt6::Multimedia`.

### 3. Valid MP4/MOV MIME detection is more robust

The native upload transport now uses `QMimeDatabase::MatchDefault` rather than content-only matching. This lets Qt combine filename and content evidence and avoids the macOS/platform MIME-database case where valid ISO-BMFF/QuickTime files can otherwise be reported as `application/octet-stream` before the gateway request.

The transport retains a strict canonical allowlist for the media formats currently supported by the Genycaster media gateway.

### 4. Unsupported files fail locally and once

MP3/ZIP and other unsupported extensions are rejected in the presentation adapter before a native upload request is emitted. Their URL is never fed to an image component. This eliminates the repeated `Unsupported image format` log spam and avoids a needless gateway 422 for known-unsupported local files.

Generic files/audio are not silently relabeled as Farcaster image/video media. Extending the gateway/domain contract to audio or arbitrary attachments should be a separate capability change.

### 5. Real attachment previews and metadata

The upload panel now preserves current-session metadata for each successfully uploaded asset before transient upload state is cleared:

- original local filename
- local preview URL
- MIME type
- byte size

Attached rows and thumbnail strips render the real image/video preview. Restored drafts fall back to the stable managed asset URL.

### 6. Composer scrolling

The composer drawer now owns a top-level `ScrollView` with a native Genycaster scrollbar. The editor/preview region has a stable minimum/preferred height so expanding upload content increases scrollable content height rather than squeezing the editor into an unusable static layout.

The drawer's own drag interaction is disabled while open so the bottom-sheet swipe recognizer does not compete with the scroll view on touch devices.

The attachment thumbnail strip is independently horizontally flickable.

## Profile widget redesign

A reusable `ProfileTrustCard.qml` now implements the requested hierarchy:

- **Trust Score** as the primary circular metric
- **Following** and **Followers** as the secondary split metrics
- avatar, display name, username and verification in the lower identity strip
- click/tap opens the profile/account destination
- the trust ring uses `VectorStroke` with `Shape.CurveRenderer`, sharing the same analytic curve-AA path as the rest of the visual system

`ProfileTrustCard` is used by both `SidebarProfileSummary` and the right `ContextRail`.

### Trust data semantics

The canonical Farcaster profile model currently contains follower/following counts but does not contain a trust score. The UI therefore accepts an optional `trustScore` from a future trust/reputation provider and renders an em dash when no such value exists. It does **not** derive a fake trust score from follower count or other unrelated canonical fields.

## `ui/render` scope

The files under `ui/render` are **shared module-level rendering primitives**, not calendar-specific code.

- `VectorStroke` is the active high-frequency visual primitive for arcs, sparklines and dashed rounded outlines, rendered through `Shape.CurveRenderer`.
- `Superellipse` is the active continuous-corner visual primitive, also rendered through `Shape.CurveRenderer`.
- `SceneGraphSuperellipse` remains registered only as the non-visual containment helper.
- `SceneGraphStroke` remains registered for compatibility but is not instantiated by production QML.

These custom primitives do not replace every `Text`, `Image`, `Rectangle`, `VideoOutput`, etc. Built-in Qt Quick items already render through the retained scene graph and keep Qt's normal antialiasing path.

## Verification

Passed locally in the sandbox:

- `tools/check_ui_design.py` — 81 QML files audited
- `tools/check_localization_catalog.py` — 519 referenced IDs, 523 catalog IDs, 12 locales
- `tools/check_architecture.py`
- `tools/check_media_upload_contract.py`
- `tools/check_r5_release_contract.py` (R6 contract)
- `tools/check_r7_release_contract.py`
- `node --test services/media-gateway/test/gateway.test.mjs` — 10/10 tests passed

Two additional presentation tests were added for:

- MP4 classification as a video preview before native upload
- MP3 rejection before preview/native transport

Full Qt configure/build cannot run in this sandbox because no Qt6 package configuration is installed here. `cmake --preset ui-debug` reaches `find_package(Qt6)` and stops on missing `Qt6Config.cmake`. Runtime verification should therefore be repeated on the project's Qt 6.11.1 macOS toolchain.
