# GenyCaster Data & Storage / Media Policy v22 Review

## Release scope

This review documents the Data & Storage subsystem added on top of the GenyCaster alpha9.6 / v21.5 context-loading, YouTube and semantic-tab candidate. The goal is a Telegram-class media storage experience adapted to GenyCaster: predictable cache behavior, explicit automatic-download policies, category-aware retention, user-visible storage accounting, media data-usage accounting, and a stable tap-to-load interaction on both desktop and mobile.

The implementation deliberately keeps policy and accounting out of individual QML views. Desktop and mobile surfaces consume one shared Qt presentation controller and one shared asynchronous remote-image provider. Project-owned non-Qt policy logic is implemented as a C++26 module; the QObject header remains a narrow textual Qt/MOC boundary as permitted by the repository architecture rules.

## User-visible behavior

### Adaptive Data & Storage surface

`ui/components/DataStoragePopup.qml` provides one adaptive surface for both form factors:

- mobile: bottom-sheet presentation with a single-column content flow suitable for touch;
- desktop/tablet: full-height drawer/panel, up to 880 px wide, with a two-column content grid where space permits;
- explicit layout direction is used for horizontal layouts so RTL and LTR catalogs behave consistently;
- storage and usage summaries use donut charts and category rows rather than exposing raw settings alone;
- Media Data Usage can be filtered by All, Wi-Fi or Cellular;
- returning from Data & Storage reopens the normal Settings surface instead of losing navigation context.

The same `DataStorageController` instance backs both layouts. There is no separate desktop-only or mobile-only policy implementation.

### Storage Usage

The surface exposes:

- total remote-media cache size;
- free filesystem capacity at the cache location;
- per-category cached byte totals;
- percentage/category visualization;
- clear-one-category actions;
- Clear Entire Cache;
- an explicit disk-cache enable/disable control;
- a configurable global cache budget from 64 MiB through 8192 MiB.

Cache categories are stable IDs shared by policy, on-disk paths, provider requests and accounting:

- `photos`
- `video-previews`
- `nfts`
- `miniapps`
- `link-previews`
- `avatars`
- `animated-emoji`
- `other`

The cache root is `QStandardPaths::CacheLocation/remote-media`. Legacy v21.5 entries under `remote-images` are read and lazily migrated only while disk caching is enabled.

### Automatic removal / retention

Each supported media category has an independent retention window. The public UI supports:

- Never (`0` days)
- 1 day
- 3 days
- 1 week
- 1 month
- 3 months
- 1 year

Persisted settings are validated when restored. Unsupported or corrupted retention values are not allowed to leak into runtime policy.

Maintenance performs retention expiry and global-budget eviction asynchronously. Entries are evicted oldest-used first after retention processing, approximating LRU with filesystem timestamps. Cache hits touch timestamps coarsely instead of writing metadata on every image access.

### Automatic Media Download

Wi-Fi/Ethernet and Cellular have independent rules for each media category. Each rule contains:

- enabled/disabled;
- maximum automatic download size.

The configurable per-file limit is bounded to 1–64 MiB. Default cellular policy is intentionally more conservative for heavy media such as NFT artwork, Mini App media and video previews. Ethernet is treated as the Wi-Fi/unmetered class by the Qt network adapter. Unknown transports use the conservative `Other` profile.

Avatar and animated-emoji defaults remain usable because blocking small identity/UI media aggressively harms feed scanning. The policy is nevertheless category-aware, so those classes still participate in retention, cache accounting and network accounting.

### Cache-first tap-to-load semantics

Turning off automatic download does **not** mean cached media disappears.

The provider always follows this order:

1. decoded in-memory image cache;
2. category-aware disk cache;
3. automatic-download policy;
4. network request;
5. decode worker;
6. optional disk write.

`SuperellipseImage.qml` separates `automaticPolicyBlocked` from `manualLoadRequired`. A policy-blocked image first attempts memory/disk retrieval. The Tap to load affordance appears only after that automatic cache lookup misses. This avoids a brief false “Tap to load” flash for media that is already local.

For a genuine cache miss:

- first tap/click is an explicit manual download request and may bypass the automatic-download enable switch and the short automatic-failure backoff;
- after the image is ready, the next interaction performs the normal action, such as opening the media gallery or following the appropriate detail interaction.

This applies to mouse and touch because the interaction belongs to the shared QML component rather than a platform-specific branch.

### NFT media behavior

NFT/collectible artwork is treated as media first and a marketplace link second:

- artwork uses the `nfts` category;
- the preview reserves stable geometry before image decode;
- artwork uses `PreserveAspectFit`, avoiding destructive square cropping;
- tapping/clicking blocked artwork requests the image instead of launching the marketplace;
- tapping/clicking ready artwork opens the Media Gallery;
- the details/metadata area remains the explicit external marketplace/link action;
- full-gallery retrieval is an explicit manual request and is still accounted to the NFT category.

This lets users inspect artwork before deciding to navigate to an external NFT destination.

### Mini App, link and video preview media

Media call sites were classified so that policy actually affects the intended traffic:

- cast/gallery images: `photos`;
- NFT artwork: `nfts`;
- Mini App artwork/posters: `miniapps`;
- YouTube/video posters: `video-previews`;
- OpenGraph/rich-link art: `link-previews`;
- profile media: `avatars`.

Mini App and rich-preview poster geometry is reserved before the image arrives. This prevents the common small-placeholder-to-large-card jump during decode. Decode targets are bounded so feed scrolling does not eagerly decode full-resolution assets that are only being shown as previews.

Link-preview interaction distinguishes the image region from the text/domain region. If artwork is policy-blocked, an artwork tap is consumed by loading; the rest of the card can still remain a valid link target.

## Architecture

### C++26 storage policy module

`src/presentation/media/storage_policy.cppm` exports `farcaster.presentation.media.storage_policy` and owns platform-independent concepts:

- `MediaCategory`
- `NetworkClass`
- `DownloadRule`
- `PolicySnapshot`
- `UsageDeltas`
- thread-safe `RuntimeState`

Policy snapshots are protected by a mutex. Usage counters use atomics so network/decode workers do not access QSettings or GUI-owned QObjects.

`storage_policy.cpp` contains the default policy and stable ID mapping. A missing `<atomic>` / `<mutex>` implementation-unit dependency discovered during standalone module verification was corrected rather than relying on accidental transitive visibility from the module interface.

### Qt presentation boundary

`ui/mobile/data_storage_controller.hpp/.cpp` is the intentionally textual QObject/MOC boundary. It owns:

- QSettings persistence;
- conversion between QML strings and policy enums;
- network-class observation through `QNetworkInformation`;
- storage/usage data exposed as Q_PROPERTY and Q_INVOKABLE methods;
- periodic usage-counter draining/persistence;
- asynchronous cache scans, category clears, retention and budget maintenance.

The filename is inherited from the existing UI composition area; the controller itself is shared by the desktop and mobile QML paths.

### Remote image provider

`ui/mobile/remote_image_provider.hpp/.cpp` implements category-aware request descriptors using a `v22` provider key. Automatic and manual requests are distinct. Legacy request forms remain supported for compatibility.

Network accounting captures `NetworkClass` **when the request starts**. If connectivity changes while a request is in flight, the completed bytes remain attributed to the transport on which the request began.

Manual user retry does not inherit the short negative-cache suppression intended for automatic retries. Transport-admission/backpressure failures are likewise not stored as URL content failures.

### Threading and lifetime

Potentially expensive filesystem work is scheduled with `QtConcurrent::run`; image decode is performed outside the GUI thread by the existing bounded decode path. Cache maintenance does not synchronously wait on the GUI thread.

The controller uses a shared lifetime token and an owning QObject context for continuations so asynchronous maintenance cannot publish state through a destroyed presentation object during shutdown.

The media provider preserves request descriptors until transport/decode admission is known. This avoids using moved-from URL/category state during retry/backpressure paths.

## Media Data Usage

The controller persists successfully downloaded remote-media bytes by network class and media category. The Data & Storage UI exposes:

- All media usage;
- Wi-Fi/Ethernet media usage;
- Cellular media usage;
- per-category breakdown for the selected scope;
- percentage/category visualization;
- usage epoch (“since” time);
- Reset Statistics.

The UI intentionally calls this **Media Data Usage**, not Total Network Usage. v22 does not claim to account for every byte produced by Farcaster API calls, uploads, Mini App WebViews, or full audio/video streaming transports. Those transports require their own instrumentation before a Telegram-style application-wide total can be truthful.

## Persistence and defaults

Preferences are stored below the `dataStorage/...` QSettings namespace. Numeric inputs are clamped or validated before entering the runtime snapshot. Reset Auto-download Settings restores the code-defined defaults rather than requiring each QML control to know a default.

Usage counters are drained from worker-safe atomics and persisted periodically. Reset Statistics clears persisted media counters and starts a new usage epoch.

## Localization

The Data & Storage surface is represented in all 12 catalogs present in this source tree:

- Arabic
- German
- English (US)
- Spanish
- Persian
- French
- Hindi
- Japanese
- Korean
- Portuguese (Brazil)
- Russian
- Simplified Chinese

The dedicated Data & Storage contract verifies 56 storage translation IDs across the catalog set.

While touching `CollectiblePreview.qml`, four source-text/hard-coded NFT UI strings were converted to translation IDs. The intentionally universal `NFT` acronym is explicitly allowed by the localization guard, while human-readable accessible text is localized.

## Verification evidence

Verification was run on Debian with GCC 14.2.0, CMake 3.31.6 and Ninja 1.12.1.

| Check | Result | Evidence / interpretation |
| --- | --- | --- |
| `tools/check_architecture.py` | PASS | Architecture guard passed. |
| `tools/check_async_resource_contract.py` | PASS | Async resource contract passed. |
| `tools/check_data_storage_contract.py` | PASS | 56 translation IDs; category-aware cache; adaptive UI; stable media geometry. |
| `tools/check_media_upload_contract.py` | PASS | Existing media upload contract remains intact. |
| `tools/check_r5_release_contract.py` | PASS | Existing release contract remains intact. |
| `tools/check_r7_release_contract.py` | PASS | Existing R7 release contract remains intact. |
| standalone C++26 `storage_policy` compile/link/run | PASS | Module interface, implementation and deterministic unit test compile and run with GCC 14.2. |
| `cmake --preset core-debug` | PASS configure | Core build tree generated. |
| `cmake --preset strict-self-hosted` | PASS configure | Strict self-hosted build tree generated. |
| `cmake --preset minimal-core` | PASS configure | Minimal-core build tree generated. |
| `cmake --preset provider-hypersnap` | PASS configure | Hypersnap-enabled core build tree generated. |
| `cmake --build --preset core-debug -- -j1` | BLOCKED BY BASELINE GCC ICE | GCC 14.2 ICE at `src/application/read_async_gateway.cppm:7`, `cp/module.cc:16297`. The unmodified v21.5 baseline reproduces the same failure at the same module/location. |
| `cmake --preset ui-debug` | NOT VERIFIED IN THIS ENVIRONMENT | Qt 6.6 development package/configuration is absent (`Qt6Config.cmake` / `qt6-config.cmake`). Full Qt/QML compile and runtime tests therefore cannot be claimed here. |
| global localization guard | BASELINE DEBT, NO NEW FINDINGS | v21.5: 137 findings; v22: 135 findings; 0 new, 2 resolved. No current finding belongs to `genycaster.storage.*`. |
| global UI design guard | BASELINE DEBT, NO NEW FINDINGS | v21.5: 12 findings; v22: 10 findings; 0 new, 2 resolved. No current finding belongs to `DataStoragePopup.qml`. |

The two resolved localization findings are the source-text translation call and hard-coded `NFT` finding in `CollectiblePreview.qml`. The two resolved UI findings are missing explicit horizontal-layout directions in the same component.

The remaining global localization/UI findings predate this v22 feature and are retained as baseline debt rather than being hidden or reported as a passing global guard.

## Release limitations

The following are explicitly **not** claimed as verified in the current container:

1. Full Qt 6.6 desktop/mobile compilation and QML runtime behavior, because the Qt 6.6 development SDK is not installed.
2. A full GCC core build, because GCC 14.2 crashes in an unchanged pre-v22 C++ module and the same crash reproduces in the v21.5 baseline.
3. Application-wide total network accounting. v22 accounts for the remote visual-media provider; API, upload, Mini App WebView and streaming byte accounting remain separate future work.
4. OS-specific cellular behavior on every target device. Network policy uses Qt's reported transport/metered state and shares the same policy semantics across desktop and mobile.

These limitations do not change the static/standalone verification results above, but they are retained to avoid overstating release qualification.

## Baseline delta

Relative to the v21.5 source candidate, the final implementation has 48 source-tree path-level deltas when build directories and checksum manifests are excluded:

- 40 existing files modified;
- 8 files added;
- 0 files removed.

The additions are the storage policy module/implementation, deterministic policy test, Data & Storage contract guard, Data & Storage QML surface, and the Qt controller boundary. Existing media/QML files are modified primarily to assign media categories, route provider requests, preserve geometry, enforce two-stage tap-to-load interaction, integrate navigation and add localization/CMake registration.

## Qualification conclusion

Within the capabilities of this environment, the v22 Data & Storage implementation is release-packaging ready: its dedicated contract and all applicable architecture/release guards pass, its non-Qt C++26 policy module compiles and runs deterministically, baseline failures are explicitly reproduced and isolated, and the feature introduces no new global localization/UI guard findings.

A production release on macOS/iOS/Android/Linux/Windows should still run the repository's complete Qt 6.6+ build, QML runtime test suite and platform network/storage smoke tests before shipping binaries.

## v22.1 macOS Clang/Qt build amendment

A subsequent real Apple Silicon build with Qt 6.11.1 and Clang 22 exposed an ADL ambiguity in
`ui/mobile/data_storage_controller.cpp`: Qt conversion wrappers reused the exported module names
`mediaCategoryId` and `networkClassId`. Unqualified calls with storage-policy enum arguments
therefore considered both the local wrapper and the imported namespace function.

v22.1 renames only the Qt adapters to `qtMediaCategoryId` and `qtNetworkClassId`, updates every
controller call site, and extends `check_data_storage_contract.py` so this exact collision cannot
be reintroduced. Stable IDs, settings keys, disk layout and user-visible behavior are unchanged.
See `DATA_STORAGE_V22_1_MACOS_CLANG_FIX.md` for the detailed compiler analysis and verification.

## v22.2 macOS/Clang qualification note

A subsequent real Qt 6.11.1 Apple Silicon build exposed a named-module ABI boundary issue after
all source files had compiled successfully. `RuntimeState`, which belongs to
`farcaster.presentation.media.storage_policy`, had been forward-declared through Qt textual
headers. Clang could compile those translation units but the final executable link expected a
constructor symbol that did not match the definition emitted from the module-aware implementation
unit.

v22.2 removes `RuntimeState` from the textual Qt ABI. `DataStorageController` and
`RemoteImageProvider` exchange only an opaque `std::shared_ptr<void>` ownership handle across the
textual boundary; `remote_image_provider.cpp` recovers the concrete module type only after importing
the storage-policy module. See `DATA_STORAGE_V22_2_CLANG_MODULE_ABI_FIX.md` for the exact root cause,
change set and verification evidence.
