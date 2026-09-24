# Genycaster V10 — Localization, Shell Status, Drawer Blur, Profile Flip, and Rich Cast Previews

## Scope

This pass is a targeted correction of the V8/V9 shell and content presentation based on runtime screenshots and direct Qt 6.11 feedback. It does not change Farcaster write serialization or invent unsupported protocol attachment capabilities.

## 1. Calendar localization and numeral shaping

`LocalizationController` now owns numeral-glyph normalization for UI presentation. Persian, Arabic, Hindi/Devanagari, and Latin UI languages receive their corresponding visible numeral glyphs independently of the selected regional locale.

Calendar month data continues to use `QCalendar` for arithmetic and conversion. Month titles are now generated through the selected UI locale with the selected calendar and normalized afterward. This fixes Persian Jalali presentation from year-first/grouped output such as `۱،۴۰۵ شهریور` to a month-first ungrouped title such as `شهریور ۱۴۰۵`. Calendar day cells and weekday labels are emitted by the same localized model.

Persian/Arabic decimal and grouping separators are normalized with the numeral style as well, so values such as the temporary Neynar Trust Score do not fall back to Latin punctuation.

## 2. Adaptive desktop shell and persistent status

The desktop navigation rail and context rail now scale within bounded widths instead of leaving large dead gutters around the reading column. The left rail uses a minimum design width and may expand to 18% of the desktop window up to 420 px. The right context rail may expand to 20% up to 520 px.

The previous top status/notification treatment is no longer part of feed layout. `SystemStatusCard` lives in `SideNavigation` and continuously summarizes live, saved/stale, syncing, offline, source, setup, or data-error state. Diagnostics and retry are progressively disclosed inside the same rail surface, so state changes do not shift the feed vertically.

## 3. Trust/Profile card interaction and loading

`ProfileTrustCard` is a true two-face card:

- tapping the front toggles to the back;
- tapping the back toggles to the front;
- both faces remain instantiated during the Y-axis rotation so reverse interaction is deterministic;
- the card's implicit height is the maximum required by either face, avoiding clipped biography/location/account content;
- the back face centers a larger profile image and additional identity information;
- unloaded profile metrics show loading affordances rather than valid-looking zeroes;
- Trust Score is the canonical normalized profile quality score currently populated from Neynar's 0.0–1.0 score and labeled `Preview · Neynar`.

All user-visible score/count formatting goes through localization helpers so Persian UI receives Persian numeral glyphs.

## 4. Drawer composition

`GenyDrawer` keeps its own surface opaque. Context separation is moved to `DrawerBackdrop`, which captures the shell behind the drawer using `ShaderEffectSource` and applies `MultiEffect` blur/scrim there. The drawer itself therefore no longer looks faded or semi-transparent.

## 5. Rich cast/embed presentation

Cast surfaces now treat media and links as structured visual content in both feed and detail views.

### Reusable previews

- image collections: `GalleryLayout` / `GalleryTile`
- video: `VideoPreview`
- audio: `AudioPreview`
- generic files: `FilePreview`
- websites/services: `LinkPreview`
- protocol dispatcher: `EmbedPreview`

### Link metadata resolver

`LinkPreviewResolver` is a Qt/C++ presentation adapter. It performs bounded asynchronous metadata retrieval outside QML, follows a limited redirect chain, parses Open Graph/Twitter-card metadata and favicon hints, and exposes remote imagery through the existing C++ image-provider path.

Provider-aware seeds improve immediate presentation for:

- YouTube — video identity and thumbnail
- GitHub — repository identity/OpenGraph visual
- Apple Music — provider identity, enriched by fetched Open Graph metadata
- generic HTTP(S) pages — title, description, image, favicon/domain

Cast feed and detail surfaces also infer one HTTP(S) URL from cast text when the protocol payload did not already provide the equivalent embed. This is presentation-only; safe/published serialization is not rewritten.

## 6. Recent-profile/story placeholder behavior and Community Pulse

The home avatar strip remains based on distinct authors of recently loaded casts rather than fabricated Story accounts. The current Community Pulse treatment uses activity-derived contribution cells instead of demo social messages.

## 7. Scene-graph scope

The `ui/render` QQuickItems remain reusable rendering infrastructure, not calendar-specific code. Custom scene-graph geometry is used for Genycaster-owned arcs, strokes, sparklines, dashed geometry, and superellipse surfaces. Standard text, image, video, layout, and input items remain native Qt Quick primitives so Qt can preserve its own scene-graph batching and rendering behavior.

## Verification evidence

Static release guards run on this exact V10 work tree:

- `tools/check_ui_design.py` — PASS, 86 QML files audited
- `tools/check_localization_catalog.py` — PASS, 520 referenced IDs / 523 catalog IDs / 12 locales
- `tools/check_architecture.py` — PASS
- `tools/check_media_upload_contract.py` — PASS
- `tools/check_r7_release_contract.py` — PASS
- `tools/check_r5_release_contract.py` — PASS (`R6 release contract`)

`cmake --preset core-debug` configured successfully in the sandbox. A subsequent GCC 14.2 build progressed through the changed domain/profile work and then terminated with a GCC internal compiler error in the existing `src/application/session.cppm` module (`cp/module.cc`), not with a project diagnostic attributable to this UI pass.

`cmake --preset ui-debug` cannot configure in this sandbox because the Qt 6 SDK/`Qt6Config.cmake` is not installed here. Runtime validation of the blur effect, multimedia surfaces, exact font fallback, and flip pointer behavior therefore remains a required Qt 6.11.1 macOS verification step.

## Runtime checks recommended on the target Qt build

1. Persian UI + Jalali calendar: confirm `شهریور ۱۴۰۵`, Persian weekdays, and Persian day digits.
2. Switch UI language while keeping the same regional/calendar selection and confirm numeral glyphs follow UI language.
3. Tap Profile Trust card front/back repeatedly and resize the window at each face.
4. Open every drawer and confirm only the environment behind it blurs while the drawer surface remains fully opaque.
5. Verify System Status changes do not move the home feed vertically.
6. Test cast feed/detail previews for image galleries, MP4/MOV, YouTube, Apple Music, GitHub, and a generic Open Graph page.
7. Confirm published cast text/bytes and protocol embeds remain unchanged by presentation inference.
