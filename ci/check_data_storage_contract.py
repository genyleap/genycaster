#!/usr/bin/env python3
"""Guard Genycaster's Telegram-class Data & Storage and stable-media contracts."""

from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
errors: list[str] = []


def read(rel: str) -> str:
    path = root / rel
    if not path.exists():
        errors.append(f"missing required file: {rel}")
        return ""
    return path.read_text(encoding="utf-8")


policy = read("src/presentation/media/storage_policy.cppm") + read("src/presentation/media/storage_policy.cpp")
controller = read("ui/app/data_storage_controller.hpp") + read("ui/app/data_storage_controller.cpp")
provider = read("ui/app/remote_image_provider.hpp") + read("ui/app/remote_image_provider.cpp")
image_surface = read("ui/components/SuperellipseImage.qml")
storage_qml = read("ui/components/DataStoragePopup.qml")
geny_slider = read("ui/components/GenySlider.qml")
geny_progress = read("ui/components/GenyProgressBar.qml")
link_preview_resolver = read("ui/app/link_preview_resolver.cpp")
async_qt_network = read("src/transport/qt_network/async_http.cpp")
settings_qml = read("ui/components/LocaleSettingsPopup.qml")
shell_qml = read("ui/components/AppShell.qml")
gallery_layout = read("ui/components/GalleryLayout.qml")
cast_embeds = read("ui/components/CastEmbedsPanel.qml")
embed_preview = read("ui/components/EmbedPreview.qml")
link_preview = read("ui/components/LinkPreview.qml")
collectible = read("ui/components/CollectiblePreview.qml")
gallery_tile = read("ui/components/GalleryTile.qml")
ui_cmake = read("ui/CMakeLists.txt")
media_cmake = read("src/presentation/media/CMakeLists.txt")
tests_cmake = read("tests/unit/CMakeLists.txt")

for token in (
    "MediaCategory",
    "NetworkClass",
    "PolicySnapshot",
    "RuntimeState",
    "recordDownloadedBytes",
    "takeUsageDeltas",
):
    if token not in policy:
        errors.append(f"storage policy module missing {token}")

for category in (
    "photos", "video-previews", "nfts", "miniapps", "link-previews",
    "avatars", "animated-emoji", "other",
):
    if category not in policy:
        errors.append(f"storage policy missing stable category id {category}")

for token in (
    "QNetworkInformation",
    "QtConcurrent::run",
    "dataStorage/cacheLimitMiB",
    "dataStorage/retention/",
    "dataStorage/auto/",
    "dataStorage/usage/",
    "lifetimeToken",
    "scanAndMaintain",
    "legacyCacheRoot",
    "usageBreakdownForNetwork",
    "usageBytesForNetwork",
):
    if token not in controller:
        errors.append(f"DataStorageController missing {token}")

for forbidden in ("waitForFinished", "QEventLoop", "processEvents("):
    if forbidden in controller:
        errors.append(f"DataStorageController must remain non-blocking; found {forbidden}")

# Qt-facing conversion wrappers must not reuse the exported storage-policy function names.
# With MediaCategory/NetworkClass arguments, an unqualified same-name wrapper participates in
# argument-dependent lookup together with storage_policy::{mediaCategoryId,networkClassId},
# which Clang correctly diagnoses as ambiguous.
for exported_name, qt_wrapper in (
    ("mediaCategoryId", "qtMediaCategoryId"),
    ("networkClassId", "qtNetworkClassId"),
):
    if re.search(rf"\bQString\s+{exported_name}\s*\(", controller):
        errors.append(
            f"DataStorageController must not shadow storage_policy::{exported_name}; "
            f"use the Qt conversion wrapper {qt_wrapper}"
        )
    if qt_wrapper not in controller:
        errors.append(f"DataStorageController missing ADL-safe Qt conversion wrapper {qt_wrapper}")


# Named-module-owned RuntimeState must not leak through Qt textual headers.
# Clang gives module-attached declarations a distinct ownership identity; forward-declaring the
# same spelling in a textual header can therefore compile but produce a different linker symbol.
controller_header = read("ui/app/data_storage_controller.hpp")
provider_header = read("ui/app/remote_image_provider.hpp")
main_cpp = read("ui/app/main.cpp")
for rel, text in (
    ("ui/app/data_storage_controller.hpp", controller_header),
    ("ui/app/remote_image_provider.hpp", provider_header),
):
    if re.search(r"storage_policy\s*\{\s*class\s+RuntimeState", text):
        errors.append(f"{rel}: must not redeclare module-owned storage_policy::RuntimeState")
    if "storage_policy::RuntimeState" in text:
        errors.append(f"{rel}: module-owned RuntimeState must stay out of the Qt textual ABI")
if "std::shared_ptr<void> runtimeStateHandle() const" not in controller_header:
    errors.append("DataStorageController must expose the module state only as an opaque shared handle")
if "std::shared_ptr<void> storageStateHandle" not in provider_header:
    errors.append("RemoteImageProvider textual constructor must accept the opaque storage-state handle")
if "runtimeStateHandle()" not in main_cpp:
    errors.append("composition root must pass DataStorageController::runtimeStateHandle() to RemoteImageProvider")
if "std::static_pointer_cast<storage_policy::RuntimeState>" not in provider:
    errors.append("RemoteImageProvider implementation must recover RuntimeState only inside the module-aware .cpp")

for token in (
    "remote-media",
    "v22/",
    "manual",
    "recordDownloadedBytes",
    "networkAtStart",
    "policy.diskCacheEnabled",
    "retentionDays",
    "maximumNetworkImageBytes",
):
    if token not in provider:
        errors.append(f"RemoteImageProvider missing Data & Storage integration token {token}")
# The provider also enforces a per-origin 401/403 cooldown before
# consulting the automatic image-failure cache. Manual retries bypass only
# the negative-cache backoff; they must not bypass an origin-wide auth ban.
if (
    "originTemporarilyForbidden(urlText)" not in provider
    or "(!manual && requestIsBackedOff(urlText))" not in provider
):
    errors.append(
        "manual image loads must bypass automatic negative-cache backoff "
        "without bypassing origin-wide authorization cooldown"
    )
if "if (!policy.diskCacheEnabled)" not in provider or "legacyDiskCachePathForUrl" not in provider:
    errors.append("legacy cache migration must be gated by disk-cache policy")

for token in (
    "policyBlocked",
    "automaticPolicyBlocked",
    "manualLoadRequired",
    "manualLoadRequested",
    "dataStorage.autoDownloadAllowed",
    "dataStorage.remoteImageSource",
    "genycaster.storage.tap_to_load",
):
    if token not in image_surface:
        errors.append(f"SuperellipseImage missing policy-aware loading token {token}")

if "dynamicAspectDiscovery: true" in cast_embeds or "dynamicAspectDiscovery: true" in embed_preview:
    errors.append("feed/detail media must not discover aspect ratios after decode and resize late")
for prop in ("Behavior on x", "Behavior on y", "Behavior on width", "Behavior on height"):
    if prop in gallery_layout:
        errors.append(f"GalleryLayout must not animate geometry after image metadata changes: {prop}")
if "loadingHeight" in link_preview:
    errors.append("LinkPreview must reserve final poster geometry while artwork is loading")
if "visible: root.manualLoadRequired" not in image_surface:
    errors.append("Tap-to-load overlay must wait for an automatic cache miss before appearing")
if ('property bool forceAutomaticDownload: false' not in image_surface
        or 'root.manualLoadRequested || root.forceAutomaticDownload' not in image_surface):
    errors.append("SuperellipseImage must support explicit presentation-critical automatic downloads")
if ('mediaCategory: "miniapps"' not in link_preview
        or 'forceAutomaticDownload: true' not in link_preview
        or 'showManualLoadIndicator: false' not in link_preview):
    errors.append("Mini App discovery artwork must load automatically instead of exposing Tap to load")
if "if (!preview.ready)" not in gallery_tile:
    errors.append("GalleryTile must consume first tap for manual loading before gallery navigation")
if "artworkStillLoadingAt" not in link_preview:
    errors.append("LinkPreview must protect blocked artwork taps from launching destinations")
if "mediaRequested" not in collectible or "openArtwork" not in collectible:
    errors.append("NFT artwork must open in media viewer independently from marketplace navigation")

if "components/DataStoragePopup.qml" not in ui_cmake:
    errors.append("DataStoragePopup.qml is not registered in the QML manifest")
for component in ("components/GenySlider.qml", "components/GenyProgressBar.qml"):
    if component not in ui_cmake:
        errors.append(f"{component} is not registered in the QML manifest")
if not geny_slider or not geny_progress:
    errors.append("Data & Storage custom slider/progress controls are missing")
for native_control in ("Slider", "ProgressBar"):
    if re.search(rf"(?m)^\s*{native_control}\s*\{{", storage_qml):
        errors.append(f"DataStoragePopup must use Geny controls instead of raw {native_control}")
if "GenySlider" not in storage_qml or "GenyProgressBar" not in storage_qml:
    errors.append("DataStoragePopup must use the product-owned Geny slider/progress controls")
if "readonly property bool twoColumnLayout" not in storage_qml or "id: wideColumns" not in storage_qml:
    errors.append("DataStoragePopup must keep the adaptive independent-column desktop composition")
if "Layout.columnSpan: root.contentColumns" in storage_qml or "readonly property int contentColumns" in storage_qml:
    errors.append("DataStoragePopup must not use synchronized GridLayout rows for the primary two-column surface")
if storage_qml.count("roundCaps: false") < 2:
    errors.append("DataStoragePopup donut tracks must use flush segment caps to avoid overlap at the 12-o'clock seam")
if "GenySlider" not in settings_qml:
    errors.append("Settings read-range controls must use the shared Genycaster slider")
if "responseMayContainImage" not in provider or "looksLikeMarkupPayload" not in provider:
    errors.append("RemoteImageProvider must reject non-image HTTP payloads before decode")
if "imageIsPage" not in link_preview_resolver:
    errors.append("Mini App metadata must not route the app page itself into the image provider")
if "contextIterator->second->isCancelled || active.bodyLimitExceeded" not in async_qt_network:
    errors.append("Qt async cancellation must drain/discard cancelled media response bytes")
if "app/data_storage_controller.cpp" not in ui_cmake:
    errors.append("DataStorageController is not compiled into genycaster")
if "storage_policy.cppm" not in media_cmake or "storage_policy.cpp" not in media_cmake:
    errors.append("storage_policy module is not registered in presentation media target")
if "farcaster_storage_policy_tests" not in tests_cmake:
    errors.append("storage policy unit test is not registered")
if "onDataStorageRequested" not in shell_qml or "DataStoragePopup" not in shell_qml:
    errors.append("AppShell does not route Settings to the Data & Storage surface")
if "dataStorageRequested" not in settings_qml:
    errors.append("Settings does not expose the Data & Storage entry")

for token in (
    "Storage Usage",
    "cacheBreakdown",
    "clearEntireCache",
    "setRetentionDays",
    "Using Wi-Fi",
    "Using Cellular",
    "setAutoDownloadEnabled",
    "setAutoDownloadLimitMiB",
    "usageBreakdown",
    "usageBreakdownForNetwork",
    "usageBytesForNetwork",
    "DonutChart",
    "GenyTabStrip",
    "resetDataUsage",
):
    # Human-facing strings are translated IDs, so match either API/token or source catalog text.
    if token not in storage_qml and token not in read("ui/i18n/qml_en_US.ts"):
        errors.append(f"DataStoragePopup missing expected feature {token}")

referenced_storage_ids = set(re.findall(r'qsTrId\("(genycaster\.storage\.[^"]+)"', storage_qml + settings_qml + image_surface))
for path in sorted((root / "ui" / "i18n").glob("qml_*.ts")):
    catalog_root = ET.parse(path).getroot()
    ids = {message.get("id", "") for message in catalog_root.findall(".//message")}
    missing = sorted(referenced_storage_ids - ids)
    if missing:
        errors.append(f"{path.name}: missing Data & Storage ids: {', '.join(missing)}")

if errors:
    print("Data & Storage contract guard: FAILED")
    for error in errors:
        print(" -", error)
    raise SystemExit(1)

print(
    "Data & Storage contract guard: PASS "
    f"({len(referenced_storage_ids)} translation IDs, category-aware cache, adaptive UI, stable media geometry)"
)
