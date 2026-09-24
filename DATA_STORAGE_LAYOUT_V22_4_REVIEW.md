# Data & Storage Layout v22.4 Review

## Scope

This pass addresses the visual defects visible in the September 12, 2026 macOS screenshots without changing Data & Storage policy semantics or the storage-controller ABI.

## Screenshot-driven findings

1. The desktop Data & Storage body used a `GridLayout` for cards with very different intrinsic heights. Grid rows therefore shared the height of their tallest card, vertically centering shorter peers and creating the large dead regions visible between Cache Policy / Cellular and Media Data Usage / Download Behavior.
2. The donut charts were oversized for the available card width. Their 24–28 px round-ended strokes made adjacent small segments overlap at the 12-o'clock seam and visually exaggerated the chart relative to the surrounding metrics.
3. Section cards mixed saturated accent separators with nested white rows. This created too many competing edges in the light theme.
4. The Content & loading heading used title/body typography that was too large relative to the Settings modal header and the surrounding control density.
5. Read-range and toggle cards used larger padding than the rest of the Settings surface, and the read-range slider duplicated a one-off visual implementation instead of the shared Genycaster slider.
6. The provider-safety note was inside the scroll column with a trailing spacer. This left a visibly loose band above the Settings footer and made the note look like an oversized tinted card rather than contextual guidance.

## Corrections

### Data & Storage

- Replaced the synchronized primary `GridLayout` with two independent desktop `ColumnLayout` stacks inside a `RowLayout`; cards now follow their own intrinsic heights and no longer inherit dead space from the opposite column.
- Raised the two-column threshold to 840 logical px. Narrow drawers reflow to one column instead of squeezing two information-dense cards side by side.
- Made compact sizing depend on the drawer's own width rather than the host window width.
- Reduced section padding from 16 to 12 logical px, section icon boxes from 42 to 36, and metric pills from 58 to 52 high.
- Replaced colored section separators with the neutral divider token and moved repeated setting rows onto the muted surface role.
- Reduced donut diameter to 156/172 logical px and stroke width to 18/20, added an inset, and disabled round caps for donut segments so adjacent categories meet cleanly without a seam bulge.
- Preserved category color semantics, keyboard behavior, RTL mirroring, cache actions, retention controls, auto-download policy, and usage accounting.

### Content & loading

- Reduced the section heading from title-2/body to title-3/meta typography and changed the vertical rhythm from 16 to 12 logical px.
- Changed the Data & Storage navigation entry from a persistent accent-tinted card to a neutral muted surface with accent reserved for the icon/value and hover state.
- Reduced read-range and toggle-card padding from 12 to 8 logical px.
- Replaced the local hand-built range slider with `GenySlider` so track/thumb sizing and interaction states match Data & Storage and the rest of the product visual system.
- Moved the provider-safety note outside the scrolling content into a compact footer-adjacent neutral strip; removed the trailing spacer that caused the highlighted loose band.

## Verification

Passed in the review container:

- `python3 tools/check_data_storage_contract.py`
- `python3 tools/check_architecture.py`
- `python3 tools/check_async_resource_contract.py`
- `python3 tools/check_media_upload_contract.py`
- `python3 tools/check_r5_release_contract.py`
- `python3 tools/check_r7_release_contract.py`
- targeted QML delimiter-balance checks for `DataStoragePopup.qml`, `LocaleSettingsPopup.qml`, and `VectorStroke.qml`
- targeted check confirms the Data & Storage primary layout no longer contains the old `contentColumns` / `Layout.columnSpan` synchronized-grid contract
- targeted check confirms no raw `Slider`, `ProgressBar`, `Switch`, `ComboBox`, or `Button` declarations exist in `DataStoragePopup.qml`

Baseline-only failures remain byte-for-byte unchanged:

- `tools/check_ui_design.py` reports the same existing issues in `GenyVideoPlayer.qml`, `CastItem.qml`, `AppsSurface.qml`, `DetailSurface.qml`, `TokenAddressPreview.qml`, and `HomeDashboard.qml`.
- `tools/check_localization_catalog.py` produces the same output as v22.3; this pass adds no new source-text translation calls or catalog debt.

Qt runtime limitation:

- `cmake --preset ui-debug` reaches `find_package(Qt6)` and stops because this container does not provide Qt 6 / `Qt6Config.cmake`. The final visual acceptance remains a macOS Qt 6.11.1 build/run using the supplied screenshots as the regression reference.
