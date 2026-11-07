#!/usr/bin/env python3
"""Guard the non-blocking and bounded resource contracts of read/media pipelines."""

from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
errors: list[str] = []


def read(rel: str) -> str:
    return (root / rel).read_text(encoding="utf-8")

async_http = read("src/transport/qt_network/async_http.cpp") + read("src/transport/qt_network/async_http.cppm")
async_contract = read("src/transport/async_http.cppm")
remote = read("ui/app/remote_image_provider.hpp") + read("ui/app/remote_image_provider.cpp")
rendering = read("src/presentation/qt/rendering_controller.hpp") + read("src/presentation/qt/rendering_controller.cpp")
media = read("src/transport/qt_network/media_upload.cpp")
feed = read("ui/components/FeedSurface.qml")
profile = read("ui/components/ProfileSurface.qml")
superellipse_image = read("ui/components/SuperellipseImage.qml")
runtime = read("ui/app/main.cpp") + read("ui/app/runtime_composition.cppm") + read("ui/app/runtime_composition.cpp")
storage = read("src/storage/read_store.cpp") + read("src/storage/sqlite_boundary.cpp")

# Blocking dispatch is only allowed toward a live, explicitly owned network
# thread. GUI dispatch must remain interruptible when the application stops.
blocking_dispatch_boundaries = {
    "src/transport/qt_network/async_http.cpp",
    "ui/app/link_preview_resolver.cpp",
}
global_pool_owners = {
    "ui/app/insights/live.cpp",  # Scope-bound HTTP, guarded delivery.
    "ui/app/miniapp_runtime.cpp",  # Scope-bound HTTP, CallbackGate delivery.
    "ui/app/runtime_ui_bindings.cpp",  # Owned recovery/runtime state.
    "ui/app/storage/controller.cpp",  # Owned storage snapshot, no Qt I/O loop.
}
for directory in (root / "src", root / "ui"):
    for path in directory.rglob("*.cpp"):
        rel = path.relative_to(root).as_posix()
        source = path.read_text(encoding="utf-8")
        if "Qt::BlockingQueuedConnection" in source and rel not in blocking_dispatch_boundaries:
            errors.append(f"{rel}: blocking dispatch requires an audited worker-thread lifetime; never wait unconditionally for the GUI.")
        if ("QtConcurrent::run" in source or "QThreadPool::globalInstance()->start" in source) and rel not in global_pool_owners:
            errors.append(f"{rel}: new global-pool owner requires a shutdown/lifetime audit and regression coverage.")
main = read("ui/app/main.cpp")
shutdown_scope = main.find("genycaster::ui::shutdown::ShutdownScope shutdownScope")
first_insights = main.find("std::make_unique<LiveInsightsController>")
if shutdown_scope < 0 or first_insights < shutdown_scope:
    errors.append("Native composition must install its shutdown scope before starting background controllers.")

for forbidden in ("QEventLoop", "waitForFinished", "std::this_thread::sleep_for"):
    if forbidden in async_http:
        errors.append(f"Async HTTP reactor must not contain {forbidden}.")
if "QQuickAsyncImageProvider" not in remote or "future.get()" in remote or "MediaNetworkThread" in remote:
    errors.append("Remote images must use QQuickAsyncImageProvider without blocking media-network workers.")
if "m_completed.exchange" not in remote or "Q_EMIT finished()" not in remote:
    errors.append("Cancelled async image responses must still reach one terminal finished() emission.")
if "control = std::move(control)" in remote:
    errors.append("Remote image async jobs must retain the response control for queue-rejection completion instead of moving it into the queued callable.")
if "if (!control)" not in remote:
    errors.append("Remote image completion helpers must reject an empty ResponseControl defensively.")
if ("maximumQueueRetries" not in remote
        or "maximumAdmissionRetries" not in remote
        or "self->startNetwork(control" not in remote
        or "responseHolder" not in remote):
    errors.append("Remote image decode backpressure must retry/continue rather than completing QML with a sticky queue-saturation sentinel.")
if ("requestMediaPresentation" not in rendering
        or "mediaPresentationGranted" not in rendering
        or "tryGrantMediaPresentation" not in rendering
        or "acknowledgeMediaPresentationFrame" not in rendering
        or "QQuickWindow::beforeSynchronizing" not in rendering
        or "m_synchronizedMediaPresentationGeneration" not in rendering
        or "requiresFramePacedPresentation" not in superellipse_image
        or "presentationAdmissionPending" not in superellipse_image):
    errors.append("Decoded large-image visibility must be admitted globally at no more than one surface per rendered frame.")
if ("deferNewLoadsWhileScrolling" not in superellipse_image
        or "root.activeSource" not in superellipse_image
        or "cache: root.remoteSource ? false : root.cache" not in superellipse_image):
    errors.append("Remote image QML surfaces must defer new feed loads during flings and avoid sticky duplicate QML caching above RemoteImageProvider.")
media_scale_block = rendering.split("double RenderingController::mediaDecodeScale() const noexcept", 1)[1].split("double RenderingController::geometryQualityScale() const noexcept", 1)[0]
if "m_effectiveQuality" in media_scale_block:
    errors.append("Adaptive frame-quality transitions must not change mediaDecodeScale because changing Image.sourceSize reloads images.")
if 'm_devicePixelRatio >= 1.5 ? 0.88 : 1.0' not in media_scale_block:
    errors.append("Adaptive media decode must keep native 1x detail while using the fixed HiDPI upload budget.")
if "QEventLoop" in media or "QNetworkAccessManager" in media:
    errors.append("Media upload must reuse the shared async HTTP transport rather than a private nested Qt event loop.")
for token in (
    "maximumActiveRequests{96U}",
    "maximumActiveRequestsPerOrigin{32U}",
    "maximumAcceptedRequests{320U}",
    "maximumAcceptedRequestBodyBytes{64U * 1024U * 1024U}",
    "maximumBufferedResponseBytes{96U * 1024U * 1024U}",
    "replyReadBufferBytes{256U * 1024U}",
):
    if token not in async_http:
        errors.append(f"Async HTTP hard resource limit missing: {token}")
if "UploadProgress uploadProgress" not in async_contract or "&QNetworkReply::uploadProgress" not in async_http:
    errors.append("Async transport must preserve upload progress without a media-side event loop.")
if "WeightedThreadPoolExecutor" not in runtime or "64U * 1024U * 1024U" not in runtime:
    errors.append("Runtime must keep JSON decode work on a weighted bounded CPU pool.")
if "RoutedReadGateway" not in runtime or "*runtime->routedAsyncGateway" not in runtime:
    errors.append("Async application reads must retain capability routing and circuit-breaker observation.")
if "viewerReactions(accountFid, 10'000)" in runtime or "viewerStateExecutor" in runtime or "readExecutor" in runtime:
    errors.append("Runtime must not restore the legacy blocking/high-volume viewer-state read lanes.")
if "JSON.stringify" in feed:
    errors.append("Feed reconciliation must not serialize nested QVariant payloads in JavaScript.")
if "scrollWorkThrottle" not in feed:
    errors.append("Feed scroll work must remain coalesced rather than running for every contentY change.")
# Compact and desktop feeds now use separate bounded cache budgets to avoid
# retaining heavyweight rich-post delegates offscreen on smaller devices.
if ("cacheBuffer: root.compact ? Math.min(Math.max(height * 0.33, 240), 440)" not in feed
        or ": Math.min(Math.max(height * 0.65, 480), 820)" not in feed
        or "pixelAligned: false" not in feed):
    errors.append("Feed must retain bounded compact/desktop delegate runways and preserve subpixel scrolling.")
if ("cacheBuffer: Math.min(Math.max(height * 0.75, 560), 960)" not in profile
        or "if (!profileList.moving && !root.restoringViewport && !scrollWorkThrottle.running)" not in profile):
    errors.append("Profile feed scroll work must stay idle during kinetic motion and use the bounded pre-incubation runway shared with the main feed.")
if "INSERT OR IGNORE INTO timeline_entries" not in storage or "idx_timeline_entries_identity" not in storage:
    errors.append("Feed pagination must append unseen rows incrementally with a stable identity index.")
if "INSERT OR IGNORE INTO social_list_entries" not in storage or "idx_social_list_identity" not in storage:
    errors.append("Social pagination must append unseen rows incrementally with a stable identity index.")

if errors:
    print("Async resource contract guard: FAILED")
    for error in errors:
        print(" -", error)
    raise SystemExit(1)
print("Async resource contract guard: PASS")
