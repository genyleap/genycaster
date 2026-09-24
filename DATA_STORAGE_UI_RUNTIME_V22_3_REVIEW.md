# Data & Storage UI / Runtime v22.3 Review

## Scope

This patch hardens the Data & Storage surface and its remote-media pipeline without changing the storage-policy ABI.

## UI / UX

- Rebuilt `DataStoragePopup.qml` around Genycaster surfaces, superellipse geometry, theme spacing, accent hierarchy and explicit RTL-aware layouts.
- Added product-owned `GenySlider.qml` and `GenyProgressBar.qml`; the popup no longer renders raw Qt slider/progress visuals.
- Reworked settings, retention, auto-download, network and usage rows into consistent Genycaster setting surfaces.
- Removed the auto-download delegate's dependency on an undeclared `index`, eliminating the reported QML `ReferenceError`.

## Remote image hardening

- `RemoteImageProvider` now validates the payload before image decode. Recognized image signatures and `image/*` responses remain eligible for compatibility decode/retry; known markup/non-image payloads are rejected immediately.
- Mini App metadata no longer publishes a preview image when the declared image URL resolves to the Mini App page itself.
- Cancelled Qt async HTTP responses are drained/discarded instead of aborting an active TLS socket from the normal caller-cancellation path. Timeout, shutdown and body-limit teardown remain bounded separately.

## Verification

Passed:

- `python3 tools/check_data_storage_contract.py`
- `python3 tools/check_architecture.py`
- `python3 tools/check_async_resource_contract.py`
- `python3 tools/check_media_upload_contract.py`
- `python3 tools/check_r5_release_contract.py`
- `python3 tools/check_r7_release_contract.py`
- Targeted QML delimiter/syntax-balance check for the modified QML files
- Targeted check confirms no raw `Slider`, `ProgressBar`, `Switch`, `ComboBox`, `Button` or `ScrollBar` declarations remain in `DataStoragePopup.qml`

Environment limitation:

- `cmake --preset ui-debug` reaches Qt discovery but cannot configure in the review container because the Qt 6 SDK / `Qt6Config.cmake` is not installed. The target should therefore receive a normal Qt 6.11.1 build/run on macOS as the final integration verification.

Existing baseline debt, unchanged by this patch:

- `tools/check_ui_design.py` reports the same pre-existing issues in `GenyVideoPlayer.qml`, `CastItem.qml`, `AppsSurface.qml`, `DetailSurface.qml`, `TokenAddressPreview.qml`, and `HomeDashboard.qml` as the untouched alpha9.6.1 archive.
- `tools/check_localization_catalog.py` produces byte-for-byte identical failure output to the untouched alpha9.6.1 archive (pre-existing source-text translation calls and untranslated catalog entries).
