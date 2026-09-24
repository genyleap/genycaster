# Genycaster Visual System 3.0 — Alpha 1.0 Review and Implementation

## Scope and source integrity

This pass is applied directly to `genycaster-alpha-1.0.zip`, SHA-256 `725a57d11bc9079e42a81c3ed35c23c2ecee1005f2c66766cc5a6f9767031404`. It preserves the existing application architecture, navigation destinations, signals, authentication gates, localization behavior, cast semantics, and C++/QML boundaries. The work is a visual-system and shared-control redesign, not a backend or product-flow rewrite.

## Reference analysis

The supplied references share a consistent visual grammar even though they come from different product categories. Their quality comes less from decorative color and more from disciplined geometry and optical hierarchy:

- Large neutral canvases make the interactive objects easy to parse.
- White or graphite material surfaces are separated with extremely light borders and broad, low-opacity shadows.
- Controls have generous internal spacing and visually continuous corners rather than generic rounded rectangles.
- Selected controls often rise one optical level inside a quiet segmented container.
- Primary actions are saturated but scarce. Most secondary actions remain neutral.
- Icon buttons use a clear hit area and often a small local plate instead of a bare glyph floating in space.
- Pressed motion is short and physically plausible; elements compress slightly instead of jumping.
- Bottom navigation is treated as a material object, with a stronger central/create affordance when creation is a primary product action.
- Information hierarchy is created with weight, spacing, and surface contrast rather than excessive separators.
- Shadows are broad and soft. Hard drop shadows or large ambient blur are absent.

The references do **not** justify turning every content item into a card. For a social client, that would reduce scan density and break Genycaster's messenger/content-first contract. Visual System 3.0 therefore applies depth primarily to controls, navigation, popovers, modal containment, composer affordances, and utility surfaces while keeping the timeline itself reading-first.

## Problems in the Alpha baseline

The Alpha design system already had semantic tokens and superellipse geometry, but many controls still rendered as variations of `fill + 1 px border`. That produced several weaknesses:

1. Surface hierarchy was too flat. A modal, a field, a secondary button, and a passive content surface could appear to occupy nearly the same optical plane.
2. The previous blue-gray palette tinted too much of the interface, which made the product feel closer to a generic dashboard than the clean references.
3. Buttons lacked a consistent material model across prominent, secondary, quiet, selected, icon-only, and destructive variants.
4. Fields and combo boxes read as generic SaaS controls rather than native-quality desktop controls.
5. Switches and tabs had functional states but limited tactile feedback.
6. Desktop side navigation and compact navigation were visually structural, but not compositional objects.
7. Mobile bottom navigation was functional but did not express the create action strongly enough.
8. Focus, hover, and pressed feedback were not always optically related to the same control anatomy.
9. The composer toolbars behaved correctly but looked like separate buttons rather than a cohesive editing instrument.
10. Existing shadow tokens were effectively not part of the component language.

## Visual System 3.0 decisions

### Neutral material palette

The day canvas is now `#F1F1F3` with white raised/elevated materials. Dark mode is neutral graphite (`#0B0C0F` canvas, `#121317` surface, `#1B1D23` raised, `#202229` elevated). Brand presets remain available, but chroma is restricted to interaction, focus, selection, and semantic status.

The default Geny interaction accent is `#315CF5` in light mode and `#829AFF` in dark mode. This produces a cleaner relationship between neutral content and active actions while preserving obvious Farcaster-client identity.

### Real superellipse elevation

`Superellipse.qml` now owns optional optical elevation. It uses Qt Quick `MultiEffect` on the exact rendered superellipse layer, so the shadow follows the n=4 geometry. Three levels are exposed through `Theme.qml`:

- `elevationControl`: local tactile controls and small utility surfaces.
- `elevationPopover`: floating controls, dropdowns, media affordances, mobile navigation.
- `elevationModal`: modal/drawer containment.

Elevation is disabled by default and instantiated only when requested. Cast/timeline surfaces stay flat for density and rendering cost.

### Button anatomy

`ActionButton.qml` is now the canonical action surface. It preserves the existing public API while normalizing height, padding, type weight, focus, hover, pressed, disabled, selected, destructive, icon-only, and prominent states. Press feedback uses a subtle `.985` scale rather than a large animated displacement.

Prominent actions use the primary fill. Normal actions use raised neutral material with a quiet border and optional control elevation. Quiet actions stay flat until interaction. Selected actions use an accent-tinted surface instead of becoming another filled primary button.

### Inputs and selection controls

`DirectionAwareTextField.qml` and `DirectionAwareTextArea.qml` now use clean raised materials, stronger focus borders, and focus elevation/halo. Their text-direction behavior remains unchanged.

`GenyComboBox.qml` uses the same material language, a dedicated chevron affordance, and a floating elevated popup with explicit selected-row feedback. The existing natural-width and RTL popup contracts remain intact.

`GenySwitch.qml` now has a compact track, raised white thumb, restrained checked fill, and smooth positional feedback. `GenyTabStrip.qml` and `GenyTabButton.qml` support a segmented-material mode in which the active tab lifts from the muted container. Profile content tabs explicitly remain low-weight/non-segmented to preserve reading hierarchy.

### Navigation

Desktop `SideNavigation.qml` and `CompactNavigationRail.qml` now sit as inset superellipse surfaces on the app canvas rather than reading as full-height rectangular slabs. The navigation item itself gains a local icon plate and refined selected/hover/press feedback.

`BottomNavigation.qml` is now a floating superellipse dock. The central Create destination is treated as the primary mobile action with a stronger primary plate while the other destinations retain compact selection feedback. No route or signal behavior was changed.

### Composer and editing

`HomeComposer.qml` is promoted to a clean raised control surface without turning the feed into a card stack. `ComposerToolbar.qml` is now a coherent segmented mode selector. `FormattingToolbar.qml` and `ComposerInsertPanel.qml` use the same raised/elevated language, making editor controls visually related instead of incidental.

`FloatingComposeButton.qml` and the media play affordance use popover elevation. This gives high-priority floating actions the stronger optical depth seen in the supplied references without applying that depth everywhere.

### Popups, drawers, status, and contextual surfaces

`PopupSurface.qml` defines modal elevation. `GenyDrawer.qml` selects popover or modal elevation based on presentation. Status surfaces, sidebar profile summary, context-rail widget surfaces, and pull-to-refresh utility controls gain only control-level depth where containment benefits from it.

## Files changed

The visual implementation touches the theme plus shared/high-leverage QML components only. It deliberately avoids rewriting business-specific content renderers or application services.

- `ui/theme/Theme.qml`
- `ui/components/Superellipse.qml`
- `ui/components/ActionButton.qml`
- `ui/components/DirectionAwareTextField.qml`
- `ui/components/DirectionAwareTextArea.qml`
- `ui/components/GenyComboBox.qml`
- `ui/components/GenySwitch.qml`
- `ui/components/GenyTabButton.qml`
- `ui/components/GenyTabStrip.qml`
- `ui/components/NavItem.qml`
- `ui/components/BottomNavButton.qml`
- `ui/components/BottomNavigation.qml`
- `ui/components/SideNavigation.qml`
- `ui/components/CompactNavigationRail.qml`
- `ui/components/PopupSurface.qml`
- `ui/components/GenyDrawer.qml`
- `ui/components/FloatingComposeButton.qml`
- `ui/components/EngagementButton.qml`
- `ui/components/HomeComposer.qml`
- `ui/components/ComposerToolbar.qml`
- `ui/components/FormattingToolbar.qml`
- `ui/components/ComposerInsertPanel.qml`
- `ui/components/AppHeader.qml`
- `ui/components/HomeFeedHeader.qml`
- `ui/components/ExplorerFeedHeader.qml`
- `ui/components/FeedHeader.qml`
- `ui/components/FeedSurface.qml`
- `ui/components/ContextRail.qml`
- `ui/components/SidebarProfileSummary.qml`
- `ui/components/ProfileSurface.qml`
- `ui/components/StoryItem.qml`
- `ui/components/StatusStrip.qml`
- `ui/components/VideoPreview.qml`
- `DESIGN.md`

## Deliberate non-changes

`CastItem.qml` is intentionally not converted to an elevated card. Timeline content should remain dense, conversational, and easy to scan. Profile/detail content also retains a flatter hierarchy. This prevents the reference aesthetic from becoming indiscriminate "cardification" and preserves the project's documented messenger discipline.

No authentication, Farcaster protocol, media upload, localization, persistence, routing, or C++ service behavior is modified in this pass.

## Verification

Static project gates after the redesign:

- `tools/check_ui_design.py` — PASS, 78 QML files audited.
- `tools/check_localization_catalog.py` — PASS.
- `tools/check_architecture.py` — PASS.
- `tools/check_r5_release_contract.py` — PASS (reports the repository's R6 contract label).
- `tools/check_r7_release_contract.py` — PASS.

`cmake --preset ui-debug` was also attempted. Configuration reaches the project normally but stops at `find_package(Qt6)` because this sandbox does not contain a Qt6 SDK (`Qt6Config.cmake` / `qt6-config.cmake` is unavailable). Therefore the final release gate still requires a native Qt build plus rendered screenshot review on the target workstation. In particular, validate `QtQuick.Effects.MultiEffect` rendering, light/dark shadow strength, macOS/Windows/Linux font metrics, keyboard focus, Persian RTL, and compact/desktop breakpoints.

## Runtime visual QA matrix

The recommended visual pass is:

- 390×844 compact light and dark.
- 760×900 standard light and dark.
- 1280×900 wide light and dark.
- Persian RTL and English LTR.
- Feed, Explorer, profile, cast detail, settings, composer, combo popup, modal/drawer, offline/error status, and media preview.
- Mouse hover, keyboard focus, pressed, selected, disabled, destructive, loading, and reduced-motion states.

The goal of that pass is optical tuning only. It should not require another architectural redesign.
