#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QNetworkInformation>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QWheelEvent>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QTimeZone>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QtTranslation>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <expected>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "presentation_bridge.hpp"
#include "rendering_controller.hpp"
#include "emoji_controller.hpp"
#include "rich_inline_text_item.hpp"
#include "rich_composer_controller.hpp"
#include "appearance_controller.hpp"
#include "localization_controller.hpp"
#include "media_upload_controller.hpp"
#include "media_classifier_controller.hpp"
#include "media_playback_coordinator.hpp"
#include "geo_location_resolver.hpp"
#include "link_preview_resolver.hpp"
#include "miniapp_host_controller.hpp"
#include "media_web_host_controller.hpp"
#include "miniapp_catalog_controller.hpp"
#include "miniapp_task_manager_controller.hpp"
#include "live_insights_controller.hpp"
#include "node_selection_controller.hpp"
#include "update_controller.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "remote_image_provider.hpp"
#include "browser_openproof_auth.hpp"
#if defined(__EMSCRIPTEN__)
#include "browser_wheel_fallback.hpp"
#include "browser_signer_bridge.hpp"
#include "browser_cast_payload.hpp"
#endif
#include "data_storage_controller.hpp"

import farcaster.runtime.emoji;
import genycaster.ui.runtime;
#if GENYCASTER_HAS_MINIAPP_HOST
import farcaster.runtime.miniapp;
#endif
import farcaster.application.configuration;
import farcaster.presentation.media;
#if FARCASTER_HAS_MEDIA_UPLOAD
import farcaster.application.media.upload;
import farcaster.application.media.lifecycle;
import farcaster.capability.media;
import farcaster.transport.qtnetwork.media;
#endif
import farcaster.application.read.model;
import farcaster.application.read.async_gateway;
import farcaster.application.read.async_service;
import farcaster.capability.notification;
import farcaster.application.session;
import farcaster.application.signer;
import farcaster.application.write;
import farcaster.capability.catalog;
import farcaster.capability.feed;
import farcaster.capability.model;
import farcaster.capability.pagination;
import farcaster.capability.search;
import farcaster.capability.auth;
import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.capability.write;
import farcaster.capability.write_submit;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.session;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.execution.serial;
import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.read;
import farcaster.local.session;
import farcaster.local.signer;
import farcaster.local.write;
import farcaster.provider.model;
import farcaster.routing.health;
import farcaster.routing.policy;
import farcaster.routing.registry;
import farcaster.routing.router;
import farcaster.routing.read.async_gateway;
import farcaster.storage.read;
import farcaster.storage.session;
import farcaster.storage.signer;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;
import farcaster.transport.qtnetwork.async;

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
import farcaster.provider.hypersnap.async_gateway;
import farcaster.provider.hypersnap.config;
import farcaster.provider.hypersnap.registration;
import farcaster.storage.continuation;
#endif

#if FARCASTER_HAS_FARCASTER_AUTH
import farcaster.provider.compatibility.auth;
#endif

#if FARCASTER_HAS_PROTOCOL_SIGNING
import farcaster.platform.signer_vault;
import farcaster.protocol.write;
import farcaster.storage.write;
#endif

#if FARCASTER_HAS_SNAPCHAIN_SIGNER
import farcaster.provider.snapchain.signer;
#endif

#if FARCASTER_HAS_SNAPCHAIN_WRITE
import farcaster.provider.snapchain.write;
#endif

#if FARCASTER_HAS_BROWSER_WALLET_AUTHORIZER
import farcaster.provider.compatibility.browser_wallet.authorizer;
#endif

#if FARCASTER_HAS_MINIAPP_EVM_WALLET
import farcaster.capability.wallet;
import farcaster.provider.compatibility.browser_wallet.bridge;
#endif

#if FARCASTER_HAS_SIGNER_ATTESTATION
import farcaster.provider.compatibility.signer_attestation;
#endif

#if defined(__EMSCRIPTEN__)
namespace {
QPointer<QQuickWindow> browserWheelWindow{};
}

// Emscripten routes DOM wheel input directly into C++. Do not depend on
// Module exports, which are not guaranteed with Qt 6.12's JS loader.
namespace {
double normalizedCanvasCoordinate(int clientX, int clientY, int axis)
{
    return EM_ASM_DOUBLE({
        const x = $0;
        const y = $1;
        // The capture-phase DOM observer sees the real composed event path
        // even when Qt's screen canvas lives inside a shadow root or has
        // pointer-events retargeted to a wrapper. No Module export required.
        const hit = window.__genycasterWheelHit;
        if (hit && hit.clientX === x && hit.clientY === y
            && hit.x >= 0 && hit.y >= 0)
            return $2 === 0 ? hit.x : hit.y;
        let element = document.elementFromPoint(x, y);
        while (element && element.shadowRoot) {
            const child = element.shadowRoot.elementFromPoint(x, y);
            if (!child || child === element) break;
            element = child;
        }
        if (!(element instanceof HTMLCanvasElement)) return -1;
        const rect = element.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) return -1;
        return $2 === 0 ? (x - rect.left) / rect.width
            : (y - rect.top) / rect.height;
    }, clientX, clientY, axis);
}

EM_BOOL browserWheelCallback(int type, const EmscriptenWheelEvent* event, void*)
{
    QQuickWindow* window = browserWheelWindow.data();
    if (type != EMSCRIPTEN_EVENT_WHEEL || !event || !window ||
        !window->isVisible() || event->mouse.ctrlKey) return EM_FALSE;
    EM_ASM({
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.received += 1;
    });
    const double x = normalizedCanvasCoordinate(event->mouse.clientX, event->mouse.clientY, 0);
    const double y = normalizedCanvasCoordinate(event->mouse.clientX, event->mouse.clientY, 1);
    if (x < 0 || x > 1 || y < 0 || y > 1 ||
        !std::isfinite(event->deltaX) || !std::isfinite(event->deltaY)) return EM_FALSE;
    EM_ASM({
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.onCanvas += 1;
    });
    const double scale = event->deltaMode == DOM_DELTA_LINE ? 16.0
        : (event->deltaMode == DOM_DELTA_PAGE ? window->height() : 1.0);
    const auto bounded = [](double v) {
        return static_cast<int>(std::lround(std::clamp(v, -2000.0, 2000.0)));
    };
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    if (event->mouse.shiftKey) modifiers |= Qt::ShiftModifier;
    if (event->mouse.altKey) modifiers |= Qt::AltModifier;
    if (event->mouse.metaKey) modifiers |= Qt::MetaModifier;
    const QPointF local{x * window->width(), y * window->height()};
    const QPoint pixels{bounded(-event->deltaX * scale), bounded(-event->deltaY * scale)};
    const QPoint angles{bounded(-event->deltaX * scale * 1.2),
                        bounded(-event->deltaY * scale * 1.2)};
    // WebAssembly Qt's synthesized wheel events are not reliable for
    // Flickables: they can be discarded by the screen/pointer handler even
    // while dragging scrolls normally. Move the nearest interactive
    // Flickable's viewport directly, including ComboBox popup ListViews.
    if (genycaster::ui::browser_wheel::scroll(*window, local, pixels)) {
        EM_ASM({
            if (window.__genycasterWheelDiagnostics) {
                window.__genycasterWheelDiagnostics.forwarded += 1;
                window.__genycasterWheelDiagnostics.directScrolled += 1;
            }
        });
        return EM_TRUE;
    }
    // Retain Qt wheel delivery for non-Flickable controls, e.g. an editable
    // spin box; don't consume events that Qt itself declines.
    QWheelEvent wheel{local, window->mapToGlobal(local.toPoint()), pixels, angles,
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false};
    wheel.setAccepted(false);
    QCoreApplication::sendEvent(window, &wheel);
    // Do not swallow wheel events Qt Quick did not handle. In particular,
    // popup/list bounds and early screen initialization may leave events
    // unaccepted; allow Qt's own web event routing to handle those.
    if (!wheel.isAccepted()) return EM_FALSE;
    EM_ASM({
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.forwarded += 1;
    });
    return EM_TRUE;
}

void installBrowserWheelBridge(QQuickWindow* window)
{
    browserWheelWindow = window;
    EM_ASM({
        if (!window.__genycasterWheelPathInstalled) {
            window.__genycasterWheelPathInstalled = true;
            window.addEventListener('wheel', function(event) {
                const path = event.composedPath ? event.composedPath() : [event.target];
                const canvas = path.find(node => node instanceof HTMLCanvasElement);
                const hit = Object.create(null);
                hit.clientX = event.clientX;
                hit.clientY = event.clientY;
                hit.x = -1;
                hit.y = -1;
                if (canvas) {
                    const rect = canvas.getBoundingClientRect();
                    if (rect.width > 0 && rect.height > 0) {
                        hit.x = (event.clientX - rect.left) / rect.width;
                        hit.y = (event.clientY - rect.top) / rect.height;
                    }
                }
                window.__genycasterWheelHit = hit;
            }, {capture: true, passive: true});
        }
    });
    EM_ASM({
        if (!window.__genycasterWheelDiagnostics) {
            window.__genycasterWheelDiagnostics = Object.create(null);
            window.__genycasterWheelDiagnostics.received = 0;
            window.__genycasterWheelDiagnostics.onCanvas = 0;
            window.__genycasterWheelDiagnostics.forwarded = 0;
            window.__genycasterWheelDiagnostics.directScrolled = 0;
        }
    });
    const EMSCRIPTEN_RESULT result = emscripten_set_wheel_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, browserWheelCallback);
    if (result != EMSCRIPTEN_RESULT_SUCCESS)
        qWarning() << "WASM wheel callback registration failed:" << result;
}
}
#endif

namespace {

namespace application = farcaster::application;
namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace routing = farcaster::routing;

using genycaster::ui::runtime::Runtime;
using genycaster::ui::runtime::makeRuntime;

[[nodiscard]] QString statusText(const application::read::model::ReadMetadata& metadata)
{
    if (metadata.phase == application::read::model::ReadPhase::Refreshing) {
        return metadata.isStale ? qtTrId("genycaster.runtime.saved_data_refreshing")
                                : qtTrId("genycaster.runtime.loading_live_data");
    }
    if (metadata.phase == application::read::model::ReadPhase::Ready) {
        return qtTrId("genycaster.runtime.up_to_date");
    }
    if (!metadata.error) {
        return qtTrId("genycaster.runtime.refresh_did_not_complete");
    }

    using foundation::error::ErrorCode;
    const QString prefix = metadata.isStale ? qtTrId("genycaster.runtime.saved_data") : QString{};
    switch (metadata.error->code) {
    case ErrorCode::Timeout:
        return prefix + qtTrId("genycaster.runtime.live_read_timed_out");
    case ErrorCode::Unavailable:
        return prefix + qtTrId("genycaster.runtime.live_source_unavailable");
    case ErrorCode::TransportFailure:
        return prefix + qtTrId("genycaster.runtime.could_not_reach_live_source");
    case ErrorCode::ProtocolFailure:
        return prefix + qtTrId("genycaster.runtime.live_format_mismatch");
    default:
        return prefix + qtTrId("genycaster.runtime.refresh_failed");
    }
}

[[nodiscard]] QString errorText(const application::read::model::ReadMetadata& metadata)
{
    if (!metadata.error) { return {}; }
    using foundation::error::ErrorCode;
    const bool hasUsableSavedData = metadata.isStale;
    if (hasUsableSavedData &&
        (metadata.error->code == ErrorCode::Timeout ||
         metadata.error->code == ErrorCode::Unavailable ||
         metadata.error->code == ErrorCode::TransportFailure)) {
        return {};
    }
    switch (metadata.error->code) {
    case ErrorCode::InvalidArgument:
        return qtTrId("genycaster.runtime.that_read_request_is_not_valid");
    case ErrorCode::NotFound:
        return qtTrId("genycaster.runtime.nothing_was_found_for_that_identity_or_item");
    case ErrorCode::Unauthorized:
        return qtTrId("genycaster.runtime.the_selected_source_rejected_this_read");
    case ErrorCode::RateLimited:
        return qtTrId("genycaster.runtime.the_live_source_is_rate_limited_saved_data_remains_available");
    case ErrorCode::Timeout:
        return qtTrId("genycaster.runtime.the_live_source_did_not_answer_in_time_this_does_not_mean_yo");
    case ErrorCode::Unavailable:
        return qtTrId("genycaster.runtime.the_selected_live_source_is_temporarily_unavailable_your_con");
    case ErrorCode::TransportFailure:
        return qtTrId("genycaster.runtime.genycaster_could_not_reach_the_selected_live_source_check_th");
    case ErrorCode::Conflict:
        return qtTrId("genycaster.runtime.this_saved_page_cannot_continue_on_the_current_route_refresh");
    case ErrorCode::StorageFailure:
        return qtTrId("genycaster.runtime.the_local_cache_could_not_be_updated");
    case ErrorCode::ProtocolFailure:
        return qtTrId("genycaster.runtime.the_live_source_returned_data_this_build_cannot_decode_yet");
    case ErrorCode::Unsupported:
        return qtTrId("genycaster.runtime.this_read_is_not_available_in_the_current_build");
    case ErrorCode::InternalFailure:
        return qtTrId("genycaster.runtime.the_read_could_not_be_completed");
    }
    return qtTrId("genycaster.runtime.the_read_could_not_be_completed");
}

[[nodiscard]] bool isBusy(const application::read::model::ReadMetadata& metadata) noexcept
{
    return metadata.phase == application::read::model::ReadPhase::LoadingCache
        || metadata.phase == application::read::model::ReadPhase::Refreshing;
}

[[nodiscard]] QString diagnosticText(const application::read::model::ReadMetadata& metadata)
{
    if (!metadata.error) { return {}; }
    using foundation::error::ErrorCode;
    QString code{};
    switch (metadata.error->code) {
    case ErrorCode::InvalidArgument: code = QStringLiteral("InvalidArgument"); break;
    case ErrorCode::NotFound: code = QStringLiteral("NotFound"); break;
    case ErrorCode::Unavailable: code = QStringLiteral("Unavailable"); break;
    case ErrorCode::Unauthorized: code = QStringLiteral("Unauthorized"); break;
    case ErrorCode::RateLimited: code = QStringLiteral("RateLimited"); break;
    case ErrorCode::Timeout: code = QStringLiteral("Timeout"); break;
    case ErrorCode::Conflict: code = QStringLiteral("Conflict"); break;
    case ErrorCode::Unsupported: code = QStringLiteral("Unsupported"); break;
    case ErrorCode::StorageFailure: code = QStringLiteral("StorageFailure"); break;
    case ErrorCode::TransportFailure: code = QStringLiteral("TransportFailure"); break;
    case ErrorCode::ProtocolFailure: code = QStringLiteral("ProtocolFailure"); break;
    case ErrorCode::InternalFailure: code = QStringLiteral("InternalFailure"); break;
    }
    return code + QStringLiteral(" · ") + QString::fromStdString(metadata.error->message);
}

[[nodiscard]] QString firstUrlEmbed(const domain::cast::Cast& cast)
{
    for (const auto& embed : cast.embeds) {
        if (embed.kind == domain::cast::EmbedKind::Url) {
            return QString::fromStdString(embed.value);
        }
    }
    return {};
}

[[nodiscard]] QString remoteImageSource(const QString& rawUrl)
{
    QString normalized = rawUrl.trimmed();
    if (normalized.isEmpty() || normalized.startsWith(QStringLiteral("image://")) ||
        normalized.startsWith(QStringLiteral("qrc:/")) || normalized.startsWith(QStringLiteral("file:/"))) {
        return normalized;
    }

    QUrl parsed{normalized};
    if (parsed.scheme() == QStringLiteral("ipfs")) {
        const QString cid = parsed.host();
        if (cid.isEmpty()) {
            return {};
        }
        QString path = parsed.path();
        if (!path.startsWith(QLatin1Char('/'))) {
            path.prepend(QLatin1Char('/'));
        }
        normalized = QStringLiteral("https://ipfs.io/ipfs/%1%2").arg(cid, path);
        parsed = QUrl{normalized};
    }

    if (!parsed.isValid() || parsed.host().isEmpty() ||
        (parsed.scheme() != QStringLiteral("https") && parsed.scheme() != QStringLiteral("http"))) {
        return {};
    }
    const QByteArray encoded = normalized.toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("image://remote/") + QString::fromLatin1(encoded);
}

[[nodiscard]] QString username(const domain::profile::Profile* profile);
[[nodiscard]] QString displayName(const domain::profile::Profile* profile);
[[nodiscard]] QString avatarUrl(const domain::profile::Profile* profile);

[[nodiscard]] QString youtubeVideoId(const QString& rawUrl)
{
    const QUrl parsed{rawUrl};
    QString host = parsed.host().toLower();
    if (host.startsWith(QStringLiteral("www."))) host.remove(0, 4);
    if (host == QStringLiteral("youtu.be")) {
        const QString id = parsed.path().section(QLatin1Char('/'), 1, 1).trimmed();
        return id;
    }
    if (host == QStringLiteral("youtube.com") || host == QStringLiteral("m.youtube.com") ||
        host == QStringLiteral("music.youtube.com")) {
        const QUrlQuery query{parsed};
        const QString queryId = query.queryItemValue(QStringLiteral("v")).trimmed();
        if (!queryId.isEmpty()) return queryId;
        const QStringList segments = parsed.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (segments.size() >= 2 &&
            (segments[0] == QStringLiteral("shorts") || segments[0] == QStringLiteral("embed") ||
             segments[0] == QStringLiteral("live"))) {
            return segments[1];
        }
    }
    return {};
}

[[nodiscard]] QString youtubeThumbnailSource(const QString& rawUrl)
{
    const QString videoId = youtubeVideoId(rawUrl);
    if (videoId.isEmpty()) return {};
    return remoteImageSource(QStringLiteral("https://i.ytimg.com/vi/%1/hqdefault.jpg").arg(videoId));
}

[[nodiscard]] QString videoThumbnailSource(const QString& rawUrl)
{
    if (const QString youtube = youtubeThumbnailSource(rawUrl); !youtube.isEmpty()) {
        return youtube;
    }

    const QUrl parsed{rawUrl};
    const QString host = parsed.host().toLower();
    if (!(host == QStringLiteral("videodelivery.net") ||
          host.endsWith(QStringLiteral(".videodelivery.net")) ||
          host.endsWith(QStringLiteral(".cloudflarestream.com")))) {
        return {};
    }

    const QStringList segments = parsed.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.isEmpty()) return {};
    const QString videoId = segments.front();
    if (videoId.isEmpty()) return {};

    QUrl thumbnail = parsed;
    thumbnail.setPath(QStringLiteral("/%1/thumbnails/thumbnail.jpg").arg(videoId));
    QUrlQuery query{};
    query.addQueryItem(QStringLiteral("time"), QStringLiteral("1s"));
    query.addQueryItem(QStringLiteral("height"), QStringLiteral("720"));
    thumbnail.setQuery(query);
    return remoteImageSource(thumbnail.toString(QUrl::FullyEncoded));
}

[[nodiscard]] QString displayHost(const QString& rawUrl)
{
    QUrl parsed{rawUrl};
    QString host = parsed.host().toLower();
    if (host.startsWith(QStringLiteral("www."))) host.remove(0, 4);
    return host;
}

struct FeedPresentationIndex final {
    std::unordered_map<std::string, const domain::cast::Cast*> casts{};
    std::unordered_map<std::uint64_t, const domain::profile::Profile*> profiles{};
};

[[nodiscard]] FeedPresentationIndex makeFeedPresentationIndex(
    const application::read::model::FeedState& state)
{
    FeedPresentationIndex index{};
    index.casts.reserve(state.casts.size());
    index.profiles.reserve(state.profiles.size());
    for (const auto& cast : state.casts) {
        index.casts.emplace(cast.id.value, &cast);
    }
    for (const auto& profile : state.profiles) {
        index.profiles.emplace(profile.fid.value, &profile);
    }
    return index;
}

[[nodiscard]] const domain::cast::Cast* indexedCast(
    const FeedPresentationIndex& index,
    const std::string_view castId)
{
    const auto iterator = index.casts.find(std::string{castId});
    return iterator == index.casts.end() ? nullptr : iterator->second;
}

[[nodiscard]] const domain::profile::Profile* indexedProfile(
    const FeedPresentationIndex& index,
    const domain::identity::Fid fid)
{
    const auto iterator = index.profiles.find(fid.value);
    return iterator == index.profiles.end() ? nullptr : iterator->second;
}

[[nodiscard]] QVariantList embedItems(
    const domain::cast::Cast& cast,
    const FeedPresentationIndex* feedIndex = nullptr);

[[nodiscard]] QVariantMap referencedCastPresentation(
    const domain::cast::Embed& embed,
    const FeedPresentationIndex* const feedIndex)
{
    QVariantMap result{
        {QStringLiteral("kind"), QStringLiteral("cast-reference")},
        {QStringLiteral("castId"), QString::fromStdString(embed.value)},
        {QStringLiteral("authorFid"), embed.referencedAuthor
            ? QVariant::fromValue(static_cast<qulonglong>(embed.referencedAuthor->value))
            : QVariant::fromValue(static_cast<qulonglong>(0))},
    };
    if (feedIndex == nullptr) return result;

    const domain::cast::Cast* referenced = indexedCast(*feedIndex, embed.value);
    if (referenced == nullptr) return result;

    const domain::profile::Profile* authorProfile = indexedProfile(*feedIndex, referenced->author);
    result.insert(QStringLiteral("resolved"), true);
    result.insert(QStringLiteral("text"), QString::fromStdString(referenced->text));
    result.insert(QStringLiteral("displayName"), displayName(authorProfile));
    result.insert(QStringLiteral("author"), username(authorProfile));
    result.insert(QStringLiteral("avatarUrl"), avatarUrl(authorProfile));
    result.insert(QStringLiteral("createdAtMs"),
        static_cast<qlonglong>(referenced->createdAt.time_since_epoch().count()));
    // Keep quoted/referenced context self-contained without recursively hydrating
    // a chain of quoted casts. Media/link children are enough for the compact
    // context card; nested cast references remain lightweight IDs.
    result.insert(QStringLiteral("embeds"), embedItems(*referenced, nullptr));
    result.insert(QStringLiteral("replyCount"), static_cast<qulonglong>(referenced->engagement.replies));
    result.insert(QStringLiteral("recastCount"), static_cast<qulonglong>(referenced->engagement.recasts));
    result.insert(QStringLiteral("likeCount"), static_cast<qulonglong>(referenced->engagement.likes));
    return result;
}

[[nodiscard]] QVariantList embedItems(
    const domain::cast::Cast& cast,
    const FeedPresentationIndex* const feedIndex)
{
    QVariantList items{};
    for (const auto& embed : cast.embeds) {
        if (embed.kind == domain::cast::EmbedKind::CastReference) {
            items.push_back(referencedCastPresentation(embed, feedIndex));
            continue;
        }
        const QString url = QString::fromStdString(embed.value);
        // Presentation classification is centralized in farcaster.presentation.media;
        // QML and this read model must never drift into conflicting URL rules.
        const auto mediaClass = farcaster::presentation::media::classifyUrl(
            url.toStdString());
        const bool image = mediaClass.kind == farcaster::presentation::media::MediaKind::Image;
        const bool video = mediaClass.kind == farcaster::presentation::media::MediaKind::Video;
        const bool audio = mediaClass.kind == farcaster::presentation::media::MediaKind::Audio;
        const bool file = mediaClass.kind == farcaster::presentation::media::MediaKind::File;
        const QString thumbnail = video ? videoThumbnailSource(url) : QString{};
        const auto playbackSource = video
            ? farcaster::presentation::media::playbackSourceForUrl(embed.value)
            : farcaster::presentation::media::PlaybackSource{};
        const QUrl parsed{url};
        items.push_back(QVariantMap{
            {QStringLiteral("kind"), image ? QStringLiteral("image")
                : video ? QStringLiteral("video")
                : audio ? QStringLiteral("audio")
                : file ? QStringLiteral("file")
                : QStringLiteral("url")},
            {QStringLiteral("url"), url},
            {QStringLiteral("adaptiveUrl"),
             QString::fromStdString(playbackSource.adaptiveUrl)},
            {QStringLiteral("contentType"),
             QString::fromStdString(playbackSource.contentType)},
            {QStringLiteral("host"), displayHost(url)},
            {QStringLiteral("fileName"), parsed.fileName()},
            {QStringLiteral("imageSource"), image ? remoteImageSource(url) : thumbnail},
            {QStringLiteral("hasPreviewImage"), image || !thumbnail.isEmpty()},
        });
    }
    return items;
}

[[nodiscard]] QVariantMap threadReplyPresentation(const domain::cast::Cast& cast, const int depth = 0);

[[nodiscard]] QVariantList threadReplyItems(const domain::cast::Cast& cast, const int depth = 0)
{
    QVariantList replies{};
    if (depth >= 4) {
        return replies;
    }
    replies.reserve(static_cast<qsizetype>(cast.directReplies.size()));
    for (const auto& reply : cast.directReplies) {
        replies.push_back(threadReplyPresentation(reply, depth + 1));
    }
    return replies;
}

[[nodiscard]] QVariantMap threadReplyPresentation(const domain::cast::Cast& cast, const int depth)
{
    const qulonglong authorFid = static_cast<qulonglong>(cast.author.value);
    // Thread-aware reads embed the full author profile on every reply cast.
    // Fall back to the raw FID only when the payload did not carry one.
    const domain::profile::Profile* authorProfile =
        cast.authorProfile ? &*cast.authorProfile : nullptr;
    QString resolvedName = displayName(authorProfile);
    if (resolvedName.isEmpty()) {
        resolvedName = QStringLiteral("FID %1").arg(authorFid);
    }
    return QVariantMap{
        {QStringLiteral("castId"), QString::fromStdString(cast.id.value)},
        {QStringLiteral("authorFid"), authorFid},
        {QStringLiteral("author"), username(authorProfile)},
        {QStringLiteral("displayName"), resolvedName},
        {QStringLiteral("avatarUrl"), avatarUrl(authorProfile)},
        {QStringLiteral("text"), QString::fromStdString(cast.text)},
        {QStringLiteral("createdAtMs"), static_cast<qlonglong>(cast.createdAt.time_since_epoch().count())},
        {QStringLiteral("isReply"), true},
        {QStringLiteral("depth"), depth},
        {QStringLiteral("parentCastId"), cast.parent ? QString::fromStdString(cast.parent->value) : QString{}},
        {QStringLiteral("parentUrl"), cast.parentUrl ? QString::fromStdString(*cast.parentUrl) : QString{}},
        {QStringLiteral("embedUrl"), firstUrlEmbed(cast)},
        {QStringLiteral("embedCount"), static_cast<qulonglong>(cast.embeds.size())},
        {QStringLiteral("embeds"), embedItems(cast)},
        {QStringLiteral("replyCount"), static_cast<qulonglong>(cast.engagement.replies)},
        {QStringLiteral("recastCount"), static_cast<qulonglong>(cast.engagement.recasts)},
        {QStringLiteral("likeCount"), static_cast<qulonglong>(cast.engagement.likes)},
        {QStringLiteral("threadReplies"), threadReplyItems(cast, depth)},
    };
}

[[nodiscard]] QVariantMap parentCastPresentation(
    const FeedPresentationIndex& feedIndex,
    const domain::cast::Cast& cast)
{
    if (!cast.parent) return {};
    QVariantMap parent{{QStringLiteral("castId"), QString::fromStdString(cast.parent->value)}};
    const domain::cast::Cast* parentCast = indexedCast(feedIndex, cast.parent->value);
    if (parentCast == nullptr) return parent;

    const domain::profile::Profile* profile = indexedProfile(feedIndex, parentCast->author);
    parent.insert(QStringLiteral("resolved"), true);
    parent.insert(QStringLiteral("authorFid"), static_cast<qulonglong>(parentCast->author.value));
    parent.insert(QStringLiteral("author"), username(profile));
    parent.insert(QStringLiteral("displayName"), displayName(profile));
    parent.insert(QStringLiteral("avatarUrl"), avatarUrl(profile));
    parent.insert(QStringLiteral("text"), QString::fromStdString(parentCast->text));
    parent.insert(QStringLiteral("createdAtMs"),
        static_cast<qlonglong>(parentCast->createdAt.time_since_epoch().count()));
    parent.insert(QStringLiteral("embeds"), embedItems(*parentCast, &feedIndex));
    parent.insert(QStringLiteral("replyCount"), static_cast<qulonglong>(parentCast->engagement.replies));
    parent.insert(QStringLiteral("recastCount"), static_cast<qulonglong>(parentCast->engagement.recasts));
    parent.insert(QStringLiteral("likeCount"), static_cast<qulonglong>(parentCast->engagement.likes));
    return parent;
}


[[nodiscard]] std::vector<domain::cast::CastId> missingReferencedCastIds(
    const application::read::model::FeedState& state, const std::size_t limit)
{
    std::vector<domain::cast::CastId> result{};
    result.reserve(limit);

    const auto contains = [&state](const domain::cast::CastId& id) {
        return std::ranges::any_of(state.casts, [&id](const domain::cast::Cast& candidate) {
            return candidate.id.value == id.value;
        });
    };
    const auto alreadyQueued = [&result](const domain::cast::CastId& id) {
        return std::ranges::any_of(result, [&id](const domain::cast::CastId& queued) {
            return queued.value == id.value;
        });
    };
    const auto append = [&](const domain::cast::CastId& id) {
        if (result.size() >= limit || id.value.empty() || contains(id) || alreadyQueued(id)) {
            return;
        }
        result.push_back(id);
    };

    // Parent casts are conversation-critical UI context. Hydrate them before
    // optional quote previews so a busy feed cannot consume the entire bounded
    // context budget with quoted casts while visible replies remain unresolved.
    for (const auto& cast : state.casts) {
        if (cast.parent) {
            append(*cast.parent);
        }
        if (result.size() >= limit) {
            return result;
        }
    }

    for (const auto& cast : state.casts) {
        for (const auto& embed : cast.embeds) {
            if (embed.kind == domain::cast::EmbedKind::CastReference) {
                append(domain::cast::CastId{embed.value});
            }
        }
        if (result.size() >= limit) {
            break;
        }
    }
    return result;
}

[[nodiscard]] QVariantMap castPreviewPresentation(
    const application::read::model::CastState& state)
{
    if (!state.cast) {
        return {};
    }
    QVariantMap preview{
        {QStringLiteral("castId"), QString::fromStdString(state.cast->id.value)},
        {QStringLiteral("resolved"), true},
        {QStringLiteral("authorFid"), static_cast<qulonglong>(state.cast->author.value)},
        {QStringLiteral("text"), QString::fromStdString(state.cast->text)},
        {QStringLiteral("createdAtMs"), static_cast<qlonglong>(state.cast->createdAt.time_since_epoch().count())},
        {QStringLiteral("embeds"), embedItems(*state.cast)},
        {QStringLiteral("replyCount"), static_cast<qulonglong>(state.cast->engagement.replies)},
        {QStringLiteral("recastCount"), static_cast<qulonglong>(state.cast->engagement.recasts)},
        {QStringLiteral("likeCount"), static_cast<qulonglong>(state.cast->engagement.likes)},
    };
    if (state.author) {
        preview.insert(QStringLiteral("author"), username(&*state.author));
        preview.insert(QStringLiteral("displayName"), displayName(&*state.author));
        preview.insert(QStringLiteral("avatarUrl"), avatarUrl(&*state.author));
    }
    return preview;
}

[[nodiscard]] QString username(const domain::profile::Profile* profile)
{
    return profile != nullptr && profile->username
        ? QString::fromStdString(profile->username->value) : QString{};
}

[[nodiscard]] QString displayName(const domain::profile::Profile* profile)
{
    return profile != nullptr && profile->displayName
        ? QString::fromStdString(profile->displayName->value) : username(profile);
}

[[nodiscard]] QString avatarUrl(const domain::profile::Profile* profile)
{
    return profile != nullptr && profile->avatarUrl
        ? remoteImageSource(QString::fromStdString(*profile->avatarUrl)) : QString{};
}

[[nodiscard]] QVariantList addressPresentation(const std::vector<std::string>& addresses)
{
    QVariantList result{};
    result.reserve(static_cast<qsizetype>(addresses.size()));
    for (const std::string& address : addresses) {
        result.push_back(QString::fromStdString(address));
    }
    return result;
}

[[nodiscard]] QVariantMap profilePresentation(const domain::profile::Profile* profile)
{
    if (profile == nullptr) {
        return {};
    }
    QVariantMap detail{
        {QStringLiteral("displayName"), displayName(profile)},
        {QStringLiteral("username"), username(profile)},
        {QStringLiteral("avatarUrl"), avatarUrl(profile)},
        {QStringLiteral("bannerUrl"), profile->bannerUrl
            ? remoteImageSource(QString::fromStdString(*profile->bannerUrl)) : QString{}},
        {QStringLiteral("location"), profile->location
            ? QString::fromStdString(*profile->location) : QString{}},
        {QStringLiteral("profileUrl"), profile->profileUrl
            ? QString::fromStdString(*profile->profileUrl) : QString{}},
        {QStringLiteral("bio"), QString::fromStdString(profile->bio)},
        {QStringLiteral("fid"), static_cast<qulonglong>(profile->fid.value)},
        {QStringLiteral("followerCount"), static_cast<qulonglong>(profile->followerCount)},
        {QStringLiteral("followingCount"), static_cast<qulonglong>(profile->followingCount)},
        {QStringLiteral("custodyAddress"), profile->custodyAddress
            ? QString::fromStdString(*profile->custodyAddress) : QString{}},
        {QStringLiteral("verifiedEthAddresses"), addressPresentation(profile->verifiedEthAddresses)},
        {QStringLiteral("verifiedSolAddresses"), addressPresentation(profile->verifiedSolAddresses)},
    };
    if (profile->qualityScore) {
        detail.insert(QStringLiteral("trustScore"), *profile->qualityScore);
        detail.insert(QStringLiteral("trustScoreSource"), QStringLiteral("neynar"));
    }
    return detail;
}

[[nodiscard]] QVariantList searchPresentation(const application::read::model::SearchState& state)
{
    QVariantList results{};
    results.reserve(static_cast<qsizetype>(state.page.profiles.size() + state.page.channels.size()));
    for (const auto& profile : state.page.profiles) {
        QVariantMap item = profilePresentation(&profile);
        item.insert(QStringLiteral("kind"), QStringLiteral("profile"));
        results.push_back(std::move(item));
    }
    for (const auto& channel : state.page.channels) {
        results.push_back(QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("channel")},
            {QStringLiteral("id"), QString::fromStdString(channel.id.value)},
            {QStringLiteral("name"), QString::fromStdString(channel.name)},
            {QStringLiteral("imageUrl"), channel.imageUrl
                ? remoteImageSource(QString::fromStdString(*channel.imageUrl)) : QString{}},
            {QStringLiteral("description"), channel.description
                ? QString::fromStdString(*channel.description) : QString{}},
            {QStringLiteral("followerCount"), static_cast<qulonglong>(channel.followerCount)},
        });
    }
    return results;
}

void applyProfileVerificationFixture(PresentationBridge& bridge)
{
    constexpr qulonglong fixtureFid = 6'841;
    const QString avatar = QStringLiteral(
        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/geny.svg");
    const QString banner = QStringLiteral(
        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/farcaster.svg");
    bridge.applyAuthenticationState(true, QStringLiteral("authenticated"), fixtureFid,
        QStringLiteral("protocolnotes"), QStringLiteral("Protocol Field Notes"), avatar,
        QStringLiteral("https://openproof.genyleap.com"), {}, {});
    bridge.applyDetail(QStringLiteral("profile"), QVariantMap{
        {QStringLiteral("fid"), fixtureFid},
        {QStringLiteral("displayName"), QStringLiteral("Protocol Field Notes")},
        {QStringLiteral("username"), QStringLiteral("protocolnotes")},
        {QStringLiteral("avatarUrl"), avatar},
        {QStringLiteral("bannerUrl"), banner},
        {QStringLiteral("bio"), QStringLiteral("A local-first Farcaster field journal for native clients.")},
        {QStringLiteral("location"), QStringLiteral("Tehran")},
        {QStringLiteral("profileUrl"), QStringLiteral("https://example.com/protocol-notes")},
        {QStringLiteral("followerCount"), 12'840},
        {QStringLiteral("followingCount"), 318},
        {QStringLiteral("trustScore"), 0.91},
        {QStringLiteral("trustScoreSource"), QStringLiteral("neynar")},
        {QStringLiteral("verified"), true},
    }, {}, {}, {}, false, false, false);
}

[[nodiscard]] QVariantList feedPresentationRows(
    PresentationBridge& bridge,
    const application::read::model::FeedState& state,
    const QString& artChannelId = {})
{
    const FeedPresentationIndex feedIndex = makeFeedPresentationIndex(state);
    QVariantList rows{};
    rows.reserve(static_cast<qsizetype>(state.entries.size()));
    for (const auto& entry : state.entries) {
        const auto* cast = indexedCast(feedIndex, entry.castId.value);
        if (cast == nullptr) { continue; }
        const auto* profile = indexedProfile(feedIndex, cast->author);
        QVariantList embeds = embedItems(*cast, &feedIndex);
        QVariantMap parentPreview = parentCastPresentation(feedIndex, *cast);
        QStringList revisionParts{
            QString::fromStdString(cast->id.value),
            QString::fromStdString(cast->text),
            profile && profile->username ? QString::fromStdString(profile->username->value) : QString{},
            profile && profile->displayName ? QString::fromStdString(profile->displayName->value) : QString{},
            avatarUrl(profile),
            cast->parent ? QString::fromStdString(cast->parent->value) : QString{},
            cast->parentUrl ? QString::fromStdString(*cast->parentUrl) : QString{},
            parentPreview.value(QStringLiteral("text")).toString(),
            parentPreview.value(QStringLiteral("displayName")).toString(),
            parentPreview.value(QStringLiteral("avatarUrl")).toString(),
            parentPreview.value(QStringLiteral("resolved")).toBool() ? QStringLiteral("1") : QStringLiteral("0"),
            QString::number(parentPreview.value(QStringLiteral("replyCount")).toULongLong()),
            QString::number(parentPreview.value(QStringLiteral("recastCount")).toULongLong()),
            QString::number(parentPreview.value(QStringLiteral("likeCount")).toULongLong()),
            QString::number(parentPreview.value(QStringLiteral("embeds")).toList().size()),
        };
        for (const auto& item : embeds) {
            const QVariantMap embed = item.toMap();
            revisionParts.push_back(embed.value(QStringLiteral("kind")).toString());
            revisionParts.push_back(embed.value(QStringLiteral("url")).toString());
            revisionParts.push_back(embed.value(QStringLiteral("castId")).toString());
            revisionParts.push_back(embed.value(QStringLiteral("text")).toString());
            revisionParts.push_back(embed.value(QStringLiteral("displayName")).toString());
            revisionParts.push_back(embed.value(QStringLiteral("avatarUrl")).toString());
            revisionParts.push_back(embed.value(QStringLiteral("resolved")).toBool() ? QStringLiteral("1") : QStringLiteral("0"));
            revisionParts.push_back(QString::number(embed.value(QStringLiteral("replyCount")).toULongLong()));
            revisionParts.push_back(QString::number(embed.value(QStringLiteral("recastCount")).toULongLong()));
            revisionParts.push_back(QString::number(embed.value(QStringLiteral("likeCount")).toULongLong()));
            revisionParts.push_back(QString::number(embed.value(QStringLiteral("embeds")).toList().size()));
        }
        const QString structuralRevision = revisionParts.join(QChar{0x1f});
        QVariantMap row{
            {QStringLiteral("_revision"), structuralRevision},
            {QStringLiteral("castId"), QString::fromStdString(cast->id.value)},
            {QStringLiteral("authorFid"), static_cast<qulonglong>(cast->author.value)},
            {QStringLiteral("author"), username(profile)},
            {QStringLiteral("displayName"), displayName(profile)},
            {QStringLiteral("avatarUrl"), avatarUrl(profile)},
            {QStringLiteral("text"), QString::fromStdString(cast->text)},
            {QStringLiteral("createdAtMs"), static_cast<qlonglong>(cast->createdAt.time_since_epoch().count())},
            {QStringLiteral("isReply"), cast->parent.has_value()},
            {QStringLiteral("parentCastId"), cast->parent ? QString::fromStdString(cast->parent->value) : QString{}},
            {QStringLiteral("parentUrl"), cast->parentUrl ? QString::fromStdString(*cast->parentUrl) : QString{}},
            {QStringLiteral("embedUrl"), firstUrlEmbed(*cast)},
            {QStringLiteral("embedCount"), static_cast<qulonglong>(cast->embeds.size())},
            {QStringLiteral("embeds"), std::move(embeds)},
            {QStringLiteral("parentPreview"), std::move(parentPreview)},
            {QStringLiteral("replyCount"), static_cast<qulonglong>(cast->engagement.replies)},
            {QStringLiteral("recastCount"), static_cast<qulonglong>(cast->engagement.recasts)},
            {QStringLiteral("likeCount"), static_cast<qulonglong>(cast->engagement.likes)},
        };
        if (state.query.kind == application::read::model::FeedQuery::Kind::UserLikes &&
            state.query.fid && bridge.isAuthenticated() &&
            bridge.accountFid() == static_cast<qulonglong>(state.query.fid->value)) {
            row.insert(QStringLiteral("likedByMe"), true);
        }
        rows.push_back(std::move(row));
    }
    if (!artChannelId.isEmpty()) {
        for (QVariant& value : rows) {
            QVariantMap row = value.toMap();
            row.insert(QStringLiteral("channelId"), artChannelId);
            row.insert(QStringLiteral("_artChannelSource"), true);
            value = std::move(row);
        }
    }
    return rows;
}

void applyFeedState(PresentationBridge& bridge, application::read::model::FeedState state)
{
    bridge.setCommunityFallback(state.metadata.communityFallback);
    QVariantList rows = feedPresentationRows(bridge, state);
    QString selector{};
    QString view{QStringLiteral("feed")};
    switch (state.query.kind) {
    case application::read::model::FeedQuery::Kind::Following:
        selector = QStringLiteral("following");
        break;
    case application::read::model::FeedQuery::Kind::Trending:
        selector = QStringLiteral("trending");
        break;
    case application::read::model::FeedQuery::Kind::Channel:
        selector = QStringLiteral("channel");
        break;
    case application::read::model::FeedQuery::Kind::UserPosts:
        view = QStringLiteral("profile");
        selector = QStringLiteral("user-posts");
        break;
    case application::read::model::FeedQuery::Kind::UserQuotes:
        view = QStringLiteral("profile");
        selector = QStringLiteral("user-quotes");
        break;
    case application::read::model::FeedQuery::Kind::UserReplies:
        view = QStringLiteral("profile");
        selector = QStringLiteral("user-replies");
        break;
    case application::read::model::FeedQuery::Kind::UserLikes:
        view = QStringLiteral("profile");
        selector = QStringLiteral("user-likes");
        break;
    case application::read::model::FeedQuery::Kind::UserPopular:
        view = QStringLiteral("profile");
        selector = QStringLiteral("user-popular");
        break;
    }
    bridge.applyList(
        std::move(view), std::move(selector), std::move(rows),
        state.next ? QString::fromStdString(state.next->value) : QString{},
        statusText(state.metadata), errorText(state.metadata), diagnosticText(state.metadata), isBusy(state.metadata),
        state.metadata.isStale, state.metadata.isSourceUnavailable, state.query.continuation.has_value());
}

void applyNotificationState(
    PresentationBridge& bridge,
    application::read::model::NotificationState state,
    const bool append)
{
    QVariantList rows{};
    rows.reserve(static_cast<qsizetype>(state.items.size()));
    for (const auto& item : state.items) {
        QString kind{QStringLiteral("unknown")};
        switch (item.kind) {
        case capability::notification::NotificationKind::Mention:
            kind = QStringLiteral("mention");
            break;
        case capability::notification::NotificationKind::Reply:
            kind = QStringLiteral("reply");
            break;
        case capability::notification::NotificationKind::Reaction:
            kind = QStringLiteral("reaction");
            break;
        case capability::notification::NotificationKind::Recast:
            kind = QStringLiteral("recast");
            break;
        case capability::notification::NotificationKind::Follow:
            kind = QStringLiteral("follow");
            break;
        case capability::notification::NotificationKind::Unknown:
            break;
        }

        QVariantMap row{
            {QStringLiteral("id"), QString::fromStdString(item.stableId)},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("observedAtMs"), static_cast<qlonglong>(item.observedAt.time_since_epoch().count())},
            {QStringLiteral("aggregateCount"), static_cast<qulonglong>(item.aggregateCount)},
        };
        QVariantList actorRows{};
        actorRows.reserve(static_cast<qsizetype>(std::min<std::size_t>(item.actors.size(), 3U)));
        for (std::size_t actorIndex = 0;
             actorIndex < std::min<std::size_t>(item.actors.size(), 3U);
             ++actorIndex) {
            const auto& actor = item.actors[actorIndex];
            actorRows.push_back(QVariantMap{
                {QStringLiteral("fid"), static_cast<qulonglong>(actor.fid.value)},
                {QStringLiteral("username"), username(&actor)},
                {QStringLiteral("displayName"), displayName(&actor)},
                {QStringLiteral("avatarUrl"), avatarUrl(&actor)},
            });
        }
        row.insert(QStringLiteral("actors"), actorRows);
        if (!item.actors.empty()) {
            const auto& actor = item.actors.front();
            row.insert(QStringLiteral("actorFid"), static_cast<qulonglong>(actor.fid.value));
            row.insert(QStringLiteral("actor"), username(&actor));
            row.insert(QStringLiteral("actorDisplayName"), displayName(&actor));
            row.insert(QStringLiteral("actorAvatarUrl"), avatarUrl(&actor));
        }
        if (item.cast) {
            row.insert(QStringLiteral("castId"), QString::fromStdString(item.cast->id.value));
            row.insert(QStringLiteral("castAuthorFid"), static_cast<qulonglong>(item.cast->author.value));
            row.insert(QStringLiteral("castParentId"), item.cast->parent ? QString::fromStdString(item.cast->parent->value) : QString{});
            row.insert(QStringLiteral("castText"), QString::fromStdString(item.cast->text));
            row.insert(QStringLiteral("castReplyCount"), static_cast<qulonglong>(item.cast->engagement.replies));
            row.insert(QStringLiteral("castRecastCount"), static_cast<qulonglong>(item.cast->engagement.recasts));
            row.insert(QStringLiteral("castLikeCount"), static_cast<qulonglong>(item.cast->engagement.likes));
            row.insert(QStringLiteral("castEmbedCount"), static_cast<qulonglong>(item.cast->embeds.size()));
        }
        if (item.parentCast) {
            row.insert(QStringLiteral("parentCastId"), QString::fromStdString(item.parentCast->id.value));
            row.insert(QStringLiteral("parentText"), QString::fromStdString(item.parentCast->text));
            row.insert(QStringLiteral("parentReplyCount"), static_cast<qulonglong>(item.parentCast->engagement.replies));
        }
        if (item.parentAuthor) {
            row.insert(QStringLiteral("parentAuthorFid"), static_cast<qulonglong>(item.parentAuthor->fid.value));
            row.insert(QStringLiteral("parentAuthor"), username(&*item.parentAuthor));
            row.insert(QStringLiteral("parentAuthorDisplayName"), displayName(&*item.parentAuthor));
            row.insert(QStringLiteral("parentAvatarUrl"), avatarUrl(&*item.parentAuthor));
        }
        rows.push_back(std::move(row));
    }

    const bool authoritativeFirstPage = !append && !isBusy(state.metadata)
        && !state.metadata.error.has_value();
    bridge.applyNotifications(
        std::move(rows),
        state.next ? QString::fromStdString(state.next->value) : QString{},
        errorText(state.metadata),
        isBusy(state.metadata),
        append);
    if (authoritativeFirstPage) {
        // Clear the badge only after the first visible page arrived. The bridge
        // advances its seen watermark to the newest row actually presented.
        bridge.markNotificationsRead();
    }
}

void applyNotificationBadgeState(
    PresentationBridge& bridge, application::read::model::NotificationState state)
{
    QVariantList observedAtMs{};
    observedAtMs.reserve(static_cast<qsizetype>(state.items.size()));
    for (const auto& item : state.items) {
        observedAtMs.push_back(
            static_cast<qlonglong>(item.observedAt.time_since_epoch().count()));
    }
    const bool busy = isBusy(state.metadata);
    const bool authoritative = !busy && !state.metadata.error.has_value();
    bridge.applyNotificationBadge(std::move(observedAtMs), busy, authoritative);
}

void applySocialState(PresentationBridge& bridge, application::read::model::SocialState state)
{
    QVariantList rows{};
    for (const auto& profile : state.profiles) {
        rows.push_back(QVariantMap{
            {QStringLiteral("fid"), static_cast<qulonglong>(profile.fid.value)},
            {QStringLiteral("author"), username(&profile)},
            {QStringLiteral("displayName"), displayName(&profile)},
            {QStringLiteral("avatarUrl"), avatarUrl(&profile)},
        });
    }
    const QString view = state.query.kind == application::read::model::SocialQuery::Kind::Followers
        ? QStringLiteral("followers") : QStringLiteral("following");
    bridge.applyList(view, view, std::move(rows),
        state.next ? QString::fromStdString(state.next->value) : QString{},
        statusText(state.metadata), errorText(state.metadata), diagnosticText(state.metadata), isBusy(state.metadata),
        state.metadata.isStale, state.metadata.isSourceUnavailable, state.query.continuation.has_value());
}

[[nodiscard]] QString sessionPhaseText(const application::session::SessionPhase phase)
{
    switch (phase) {
    case application::session::SessionPhase::Guest:
        return QStringLiteral("signed-out");
    case application::session::SessionPhase::Starting:
        return QStringLiteral("starting");
    case application::session::SessionPhase::AwaitingApproval:
        return QStringLiteral("awaiting-approval");
    case application::session::SessionPhase::LinkingFarcaster:
        return QStringLiteral("linking-farcaster");
    case application::session::SessionPhase::Authenticated:
        return QStringLiteral("authenticated");
    case application::session::SessionPhase::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("failed");
}

[[nodiscard]] QString authenticationErrorText(
    const application::session::SessionState& state, const QString& route)
{
    if (!state.error) {
        return {};
    }
    using foundation::error::ErrorCode;
    QString summary{};
    switch (state.error->code) {
    case ErrorCode::Unauthorized:
        summary = route == QStringLiteral("farcaster")
            ? qtTrId("genycaster.runtime.farcaster_could_not_verify_this_sign_in_start_a_new_request_")
            : qtTrId("genycaster.setup.auth_rejected_recover");
        break;
    case ErrorCode::RateLimited:
        summary = qtTrId("genycaster.runtime.sign_in_could_not_be_completed");
        break;
    case ErrorCode::Timeout:
    case ErrorCode::Unavailable:
    case ErrorCode::TransportFailure:
        summary = route == QStringLiteral("farcaster")
            ? qtTrId("genycaster.setup.farcaster_relay_unreachable")
            : qtTrId("genycaster.runtime.sign_in_could_not_be_completed");
        break;
    case ErrorCode::Unsupported:
        summary = qtTrId("genycaster.runtime.sign_in_could_not_be_completed");
        break;
    case ErrorCode::InvalidArgument:
        summary = qtTrId("genycaster.runtime.this_build_is_missing_valid_sign_in_with_farcaster_app_confi");
        break;
    case ErrorCode::StorageFailure:
        summary = qtTrId("genycaster.runtime.the_local_account_session_could_not_be_saved");
        break;
    case ErrorCode::ProtocolFailure:
        summary = qtTrId("genycaster.setup.auth_provider_response_invalid");
        break;
    case ErrorCode::Conflict:
        summary = qtTrId("genycaster.setup.auth_conflict");
        break;
    case ErrorCode::NotFound:
        summary = qtTrId("genycaster.setup.auth_missing_link");
        break;
    case ErrorCode::InternalFailure:
        summary = qtTrId("genycaster.runtime.sign_in_could_not_be_completed");
        break;
    }
    return summary.isEmpty() ? qtTrId("genycaster.runtime.sign_in_could_not_be_completed") : summary;
}

[[nodiscard]] QString signerPhaseText(const application::signer::SignerPhase phase)
{
    using application::signer::SignerPhase;
    switch (phase) {
    case SignerPhase::SignedOut: return QStringLiteral("signed-out");
    case SignerPhase::Checking: return QStringLiteral("checking");
    case SignerPhase::Missing: return QStringLiteral("missing");
    case SignerPhase::AuthorizationRequired: return QStringLiteral("authorization-required");
    case SignerPhase::Provisioning: return QStringLiteral("provisioning");
    case SignerPhase::Ready: return QStringLiteral("ready");
    case SignerPhase::Revoked: return QStringLiteral("revoked");
    case SignerPhase::Expired: return QStringLiteral("expired");
    case SignerPhase::Unavailable: return QStringLiteral("unavailable");
    case SignerPhase::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("failed");
}

[[nodiscard]] QString signerStatusText(const application::signer::SignerState& state)
{
    using application::signer::SignerPhase;
    switch (state.phase) {
    case SignerPhase::SignedOut:
        return qtTrId("genycaster.runtime.sign_in_before_enabling_interactions");
    case SignerPhase::Checking:
        return qtTrId("genycaster.runtime.checking_the_protocol_signer_against_the_network");
    case SignerPhase::Missing:
        return qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet");
    case SignerPhase::AuthorizationRequired:
        return qtTrId("genycaster.runtime.custody_wallet_approval_is_required_to_enable_interactions");
    case SignerPhase::Provisioning:
        return qtTrId("genycaster.runtime.waiting_for_custody_approval_and_network_confirmation");
    case SignerPhase::Ready:
        return qtTrId("genycaster.runtime.interaction_signing_is_ready");
    case SignerPhase::Revoked:
        return qtTrId("genycaster.runtime.the_protocol_signer_has_been_revoked");
    case SignerPhase::Expired:
        return qtTrId("genycaster.runtime.the_protocol_signer_expired_enable_interactions_again_to_con");
    case SignerPhase::Unavailable:
        return qtTrId("genycaster.runtime.interaction_signing_is_temporarily_unavailable_reading_is_un");
    case SignerPhase::Failed:
        return qtTrId("genycaster.runtime.interaction_signing_could_not_be_verified_safely_reading_is_");
    }
    return qtTrId("genycaster.runtime.interaction_signing_is_unavailable");
}

[[nodiscard]] bool isExpectedSignerConfigurationError(const foundation::error::Error& error)
{
    const std::string_view message{error.message};
    const bool endpointResolutionFailure =
        message.find("Host ") != std::string_view::npos &&
        message.find(" not found") != std::string_view::npos;
    if (endpointResolutionFailure) {
        return true;
    }

    if (error.code != foundation::error::ErrorCode::InvalidArgument &&
        error.code != foundation::error::ErrorCode::Unsupported) {
        return false;
    }
    return message.find("client attribution is not configured") != std::string_view::npos ||
        message.find("App-FID attribution requires") != std::string_view::npos ||
        message.find("attestation provider") != std::string_view::npos;
}

[[nodiscard]] QString signerErrorText(const application::signer::SignerState& state)
{
    if (!state.error) return {};
    const std::string_view rawMessage{state.error->message};
    if (rawMessage.find("client attribution is not configured") != std::string_view::npos) {
        return qtTrId("genycaster.runtime.client_attribution_not_configured");
    }
    if (rawMessage.find("App-FID attribution requires") != std::string_view::npos ||
        rawMessage.find("attestation provider") != std::string_view::npos ||
        rawMessage.find("Signer-attestation endpoint returned HTTP 404") != std::string_view::npos) {
        return qtTrId("genycaster.runtime.signer_attestation_not_configured");
    }

    using foundation::error::ErrorCode;
    QString text{};
    switch (state.error->code) {
    case ErrorCode::Unauthorized:
        text = qtTrId("genycaster.runtime.the_custody_wallet_did_not_authorize_this_signer_request");
        break;
    case ErrorCode::Timeout:
        text = qtTrId("genycaster.runtime.the_custody_approval_or_network_confirmation_timed_out");
        break;
    case ErrorCode::Unavailable:
    case ErrorCode::TransportFailure:
        text = qtTrId("genycaster.runtime.the_secure_signer_wallet_or_snapchain_endpoint_is_currently_");
        break;
    case ErrorCode::Unsupported:
        text = qtTrId("genycaster.runtime.this_build_is_missing_a_required_signer_capability_or_endpoi");
        break;
    case ErrorCode::StorageFailure:
        text = qtTrId("genycaster.runtime.the_platform_secure_store_or_signer_metadata_could_not_be_ac");
        break;
    case ErrorCode::ProtocolFailure:
    case ErrorCode::Conflict:
        text = qtTrId("genycaster.runtime.snapchain_rejected_or_contradicted_the_signer_authorization_");
        break;
    case ErrorCode::InvalidArgument:
        text = qtTrId("genycaster.runtime.the_signer_authorization_configuration_is_invalid");
        break;
    case ErrorCode::NotFound:
    case ErrorCode::RateLimited:
    case ErrorCode::InternalFailure:
        text = qtTrId("genycaster.runtime.the_signer_operation_could_not_be_completed_safely");
        break;
    }
    return text;
}

void applySignerState(PresentationBridge& bridge, application::signer::SignerState state)
{
#ifndef NDEBUG
    if (state.error && !isExpectedSignerConfigurationError(*state.error)) {
        qDebug().noquote() << "Genycaster signer operation:"
                           << QString::fromStdString(state.error->message);
    }
#endif
    const bool busy = state.phase == application::signer::SignerPhase::Checking ||
        state.phase == application::signer::SignerPhase::Provisioning;
    const bool ready = state.phase == application::signer::SignerPhase::Ready;
    bridge.applySignerState(
        signerPhaseText(state.phase),
        busy,
        ready,
        signerStatusText(state),
        signerErrorText(state));
}

[[nodiscard]] QString writePhaseText(const application::write::WritePhase phase)
{
    switch (phase) {
    case application::write::WritePhase::Queued: return QStringLiteral("queued");
    case application::write::WritePhase::Submitting: return QStringLiteral("submitting");
    case application::write::WritePhase::RetryScheduled: return QStringLiteral("retry-scheduled");
    case application::write::WritePhase::Confirmed: return QStringLiteral("confirmed");
    case application::write::WritePhase::Failed: return QStringLiteral("failed");
    case application::write::WritePhase::Blocked: return QStringLiteral("blocked");
    }
    return QStringLiteral("failed");
}

[[nodiscard]] QString writeRowState(const application::write::WritePhase phase)
{
    switch (phase) {
    case application::write::WritePhase::Queued:
    case application::write::WritePhase::Submitting:
        return QStringLiteral("pending");
    case application::write::WritePhase::RetryScheduled:
        return QStringLiteral("retry");
    case application::write::WritePhase::Confirmed:
        return QStringLiteral("confirmed");
    case application::write::WritePhase::Failed:
        return QStringLiteral("failed");
    case application::write::WritePhase::Blocked:
        return QStringLiteral("blocked");
    }
    return QStringLiteral("failed");
}

[[nodiscard]] QString operationPresentationKind(const application::write::OperationKind kind)
{
    using application::write::OperationKind;
    switch (kind) {
    case OperationKind::PublishCast: return QStringLiteral("publish-cast");
    case OperationKind::RemoveCast: return QStringLiteral("remove-cast");
    case OperationKind::Like: return QStringLiteral("like");
    case OperationKind::Unlike: return QStringLiteral("unlike");
    case OperationKind::Recast: return QStringLiteral("recast");
    case OperationKind::UndoRecast: return QStringLiteral("undo-recast");
    case OperationKind::Follow: return QStringLiteral("follow");
    case OperationKind::Unfollow: return QStringLiteral("unfollow");
    }
    return QStringLiteral("protocol-write");
}

[[nodiscard]] QString operationLabel(const application::write::WriteState& state)
{
    if (!state.record) return qtTrId("genycaster.runtime.protocol_write");
    using application::write::OperationKind;
    switch (state.record->kind) {
    case OperationKind::PublishCast:
        return state.record->optimisticCast && state.record->optimisticCast->parent
            ? qtTrId("genycaster.runtime.reply") : qtTrId("genycaster.runtime.cast");
    case OperationKind::RemoveCast: return qtTrId("genycaster.runtime.cast_deletion");
    case OperationKind::Like: return qtTrId("genycaster.runtime.like");
    case OperationKind::Unlike: return qtTrId("genycaster.runtime.unlike");
    case OperationKind::Recast: return qtTrId("genycaster.runtime.recast");
    case OperationKind::UndoRecast: return qtTrId("genycaster.runtime.recast_removal");
    case OperationKind::Follow: return qtTrId("genycaster.runtime.follow");
    case OperationKind::Unfollow: return qtTrId("genycaster.runtime.unfollow");
    }
    return qtTrId("genycaster.runtime.protocol_write");
}

[[nodiscard]] QString writeStatusText(const application::write::WriteState& state)
{
    const QString label = operationLabel(state);
    switch (state.phase) {
    case application::write::WritePhase::Queued:
        if (state.error && state.error->code == foundation::error::ErrorCode::Unsupported) {
            return label + qtTrId("genycaster.runtime.saved_locally_network_submission_is_not_configured_in_this_r");
        }
        return label + qtTrId("genycaster.runtime.saved_locally_before_network_submission");
    case application::write::WritePhase::Submitting:
        return qtTrId("genycaster.runtime.submitting_value_to_farcaster").arg(label.toLower());
    case application::write::WritePhase::RetryScheduled:
        return label + qtTrId("genycaster.runtime.is_saved_locally_a_bounded_network_retry_is_scheduled");
    case application::write::WritePhase::Confirmed:
        return label + qtTrId("genycaster.runtime.confirmed_by_authoritative_farcaster_state");
    case application::write::WritePhase::Failed:
        return label + qtTrId("genycaster.runtime.failed_the_signed_operation_remains_recorded_locally");
    case application::write::WritePhase::Blocked:
        return label + qtTrId("genycaster.runtime.is_paused_until_the_verified_signer_is_available_again");
    }
    return {};
}

[[nodiscard]] QString writeErrorText(const application::write::WriteState& state)
{
    if (!state.error) return {};
    using foundation::error::ErrorCode;
    switch (state.error->code) {
    case ErrorCode::Unauthorized:
        return qtTrId("genycaster.runtime.this_interaction_needs_the_current_verified_farcaster_signer");
    case ErrorCode::RateLimited:
        return qtTrId("genycaster.runtime.farcaster_is_rate_limiting_this_write_it_is_saved_locally_fo");
    case ErrorCode::Timeout:
    case ErrorCode::Unavailable:
    case ErrorCode::TransportFailure:
        return qtTrId("genycaster.runtime.the_network_is_temporarily_unavailable_the_write_is_saved_lo");
    case ErrorCode::Conflict:
        return qtTrId("genycaster.runtime.the_network_reported_authoritative_state_that_conflicts_with");
    case ErrorCode::Unsupported:
        return qtTrId("genycaster.runtime.this_runtime_has_no_compatible_snapchain_write_endpoint_the_");
    case ErrorCode::InvalidArgument:
        return qtTrId("genycaster.runtime.this_interaction_does_not_satisfy_the_current_farcaster_prot");
    case ErrorCode::ProtocolFailure:
        return qtTrId("genycaster.runtime.farcaster_rejected_the_canonical_protocol_message");
    case ErrorCode::StorageFailure:
        return qtTrId("genycaster.runtime.the_write_could_not_be_saved_safely_to_the_durable_outbox");
    case ErrorCode::NotFound:
    case ErrorCode::InternalFailure:
        return qtTrId("genycaster.runtime.the_protocol_write_could_not_be_completed_safely");
    }
    return qtTrId("genycaster.runtime.the_protocol_write_could_not_be_completed_safely");
}

[[nodiscard]] QVariantMap writePresentation(
    const application::write::WriteState& state,
    const std::optional<domain::session::ClientSession>& session)
{
    if (!state.record) return {};
    const auto& record = *state.record;
    QVariantMap presentation{
        {QStringLiteral("operationKind"), operationPresentationKind(record.kind)},
        {QStringLiteral("writeState"), writeRowState(state.phase)},
    };
    if (record.optimisticCast) {
        const auto& cast = *record.optimisticCast;
        QString author = qtTrId("genycaster.runtime.you");
        QString display = qtTrId("genycaster.runtime.you_2");
        QString avatar{};
        if (session) {
            if (session->username) author = QString::fromStdString(*session->username);
            if (session->displayName) display = QString::fromStdString(*session->displayName);
            else if (session->username) display = QString::fromStdString(*session->username);
            if (session->avatarUrl) avatar = remoteImageSource(QString::fromStdString(*session->avatarUrl));
        }
        presentation.insert(QStringLiteral("castId"), QString::fromStdString(cast.id.value));
        presentation.insert(QStringLiteral("authorFid"), static_cast<qulonglong>(cast.author.value));
        presentation.insert(QStringLiteral("author"), author);
        presentation.insert(QStringLiteral("displayName"), display);
        presentation.insert(QStringLiteral("avatarUrl"), avatar);
        presentation.insert(QStringLiteral("text"), QString::fromStdString(cast.text));
        presentation.insert(QStringLiteral("createdAtMs"), static_cast<qlonglong>(cast.createdAt.time_since_epoch().count()));
        presentation.insert(QStringLiteral("isReply"), cast.parent.has_value());
        presentation.insert(QStringLiteral("parentCastId"), cast.parent ? QString::fromStdString(cast.parent->value) : QString{});
        presentation.insert(QStringLiteral("parentUrl"), cast.parentUrl ? QString::fromStdString(*cast.parentUrl) : QString{});
        presentation.insert(QStringLiteral("embedUrl"), firstUrlEmbed(cast));
        presentation.insert(QStringLiteral("embedCount"), static_cast<qulonglong>(cast.embeds.size()));
        presentation.insert(QStringLiteral("embeds"), embedItems(cast));
        presentation.insert(QStringLiteral("replyCount"), static_cast<qulonglong>(cast.engagement.replies));
        presentation.insert(QStringLiteral("recastCount"), static_cast<qulonglong>(cast.engagement.recasts));
        presentation.insert(QStringLiteral("likeCount"), static_cast<qulonglong>(cast.engagement.likes));
    }
    if (record.target.cast) {
        presentation.insert(QStringLiteral("targetCastId"), QString::fromStdString(record.target.cast->id.value));
        presentation.insert(QStringLiteral("targetAuthorFid"), static_cast<qulonglong>(record.target.cast->author.value));
    }
    if (record.target.fid) {
        presentation.insert(QStringLiteral("targetFid"), static_cast<qulonglong>(record.target.fid->value));
    }
    return presentation;
}

[[nodiscard]] QVariantList reactionProjectionRows(
    const std::vector<farcaster::local::write::ReactionProjection>& projections)
{
    QVariantList rows{};
    rows.reserve(static_cast<qsizetype>(projections.size()));
    for (const auto& projection : projections) {
        rows.push_back(QVariantMap{
            {QStringLiteral("castId"), QString::fromStdString(projection.target.id.value)},
            {QStringLiteral("kind"), projection.reaction == domain::write::ReactionKind::Like
                ? QStringLiteral("like") : QStringLiteral("recast")},
            {QStringLiteral("active"), projection.isActive},
        });
    }
    return rows;
}

[[nodiscard]] QVariantList followingProjectionRows(
    const std::vector<farcaster::local::write::FollowingProjection>& projections)
{
    QVariantList rows{};
    rows.reserve(static_cast<qsizetype>(projections.size()));
    for (const auto& projection : projections) {
        rows.push_back(QVariantMap{
            {QStringLiteral("targetFid"), static_cast<qulonglong>(projection.target.value)},
            {QStringLiteral("following"), projection.isFollowing},
        });
    }
    return rows;
}

void hydrateWriteProjections(
    PresentationBridge& bridge,
    const std::shared_ptr<Runtime>& runtime,
    const std::optional<domain::session::ClientSession>& session)
{
    const QPointer<PresentationBridge> bridgeGuard{&bridge};
    if (!session || !runtime->signerExecutor || !runtime->writeOutbox) {
        QMetaObject::invokeMethod(
            &bridge,
            [bridgeGuard] {
                if (bridgeGuard) {
                    bridgeGuard->applyReactionProjections({});
                    bridgeGuard->applyFollowingProjections({});
                    bridgeGuard->applyCastProjections(0, {});
                }
            },
            Qt::QueuedConnection);
        return;
    }

    const domain::identity::Fid accountFid = session->fid;
    const auto accountSession = *session;
    auto scheduled = runtime->signerExecutor->post([bridgeGuard, runtime, accountFid, accountSession] {
        auto reactions = runtime->writeOutbox->listReactionProjections(accountFid);
        auto following = runtime->writeOutbox->listFollowingProjections(accountFid);
        auto localCasts = runtime->writeOutbox->listCastProjections(accountFid, 200U);
        if (!reactions || !following || !localCasts) {
#ifndef NDEBUG
            if (!reactions) {
                qInfo().noquote() << "Genycaster local reaction projection failure:"
                                  << QString::fromStdString(reactions.error().message);
            }
            if (!following) {
                qInfo().noquote() << "Genycaster local following projection failure:"
                                  << QString::fromStdString(following.error().message);
            }
            if (!localCasts) {
                qInfo().noquote() << "Genycaster local cast projection failure:"
                                  << QString::fromStdString(localCasts.error().message);
            }
#endif
            return;
        }
        auto reactionRows = reactionProjectionRows(*reactions);
        auto followingRows = followingProjectionRows(*following);
        // Hydration is not publication. Never promote failed or blocked
        // local writes back into a public feed after restarting. Preserve
        // the durable outbox for explicit recovery/diagnostics, not for
        // impersonating a network-accepted cast in the timeline.
        QVariantList castRows{};
        for (auto it = localCasts->begin(); it != localCasts->end(); ++it) {
            if (!it->operation.optimisticCast
                || it->status == domain::write::OutboxStatus::Failed) continue;
#if defined(__EMSCRIPTEN__)
            // Browser builds without protocol write authority must not
            // rehydrate pending drafts as if they were published casts.
            if (it->status != domain::write::OutboxStatus::Confirmed) continue;
#endif
            application::write::WritePhase phase =
                it->status == domain::write::OutboxStatus::Failed
                    ? application::write::WritePhase::Failed
                    : it->status == domain::write::OutboxStatus::Confirmed
                        ? application::write::WritePhase::Confirmed
                        : it->nextAttemptAt
                            ? application::write::WritePhase::RetryScheduled
                            : application::write::WritePhase::Queued;
            application::write::WriteState snapshot{
                .phase = phase,
                .record = application::write::WriteRecord{
                    .idempotencyKey = it->operation.idempotencyKey.value,
                    .kind = application::write::OperationKind::PublishCast,
                    .author = it->operation.author,
                    .optimisticCast = it->operation.optimisticCast,
                    .target = application::write::OperationTarget{
                        .cast = it->operation.target.cast,
                        .fid = it->operation.target.fid,
                    },
                    .status = it->status == domain::write::OutboxStatus::Failed
                        ? application::write::DurableWriteStatus::Failed
                        : it->status == domain::write::OutboxStatus::Confirmed
                            ? application::write::DurableWriteStatus::Confirmed
                            : application::write::DurableWriteStatus::Pending,
                    .attemptCount = it->attemptCount,
                    .nextAttemptAt = it->nextAttemptAt,
                },
            };
            castRows.push_back(writePresentation(snapshot, accountSession));
        }
        PresentationBridge* const target = bridgeGuard.data();
        if (target == nullptr) {
            return;
        }
        QMetaObject::invokeMethod(
            target,
            [bridgeGuard, accountFid, reactionRows = std::move(reactionRows),
             followingRows = std::move(followingRows), castRows = std::move(castRows)]() mutable {
                if (bridgeGuard) {
                    bridgeGuard->applyReactionProjections(std::move(reactionRows));
                    bridgeGuard->applyFollowingProjections(std::move(followingRows));
                    bridgeGuard->applyCastProjections(
                        static_cast<qulonglong>(accountFid.value), std::move(castRows));
                }
            },
            Qt::QueuedConnection);
    });
#ifndef NDEBUG
    if (!scheduled) {
        qInfo().noquote() << "Genycaster local write projection scheduling failure:"
                          << QString::fromStdString(scheduled.error().message);
    }
#endif
}

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
void refreshProfileFollowsMe(
    PresentationBridge& bridge,
    const std::shared_ptr<Runtime>& runtime,
    const domain::identity::Fid accountFid,
    const domain::identity::Fid profileFid,
    const bool outgoing = false)
{
    if (accountFid.value == 0 || profileFid.value == 0 || accountFid.value == profileFid.value
        || !runtime->asyncGateway) {
        return;
    }

    const std::string relationshipKey =
        std::to_string(accountFid.value) + ":" + std::to_string(profileFid.value)
        + (outgoing ? ":outgoing" : ":incoming");
    {
        std::scoped_lock lock{runtime->profileRelationshipMutex};
        const auto cached = runtime->followsMeCache.find(relationshipKey);
        const auto observed = runtime->followsMeObservedAt.find(relationshipKey);
        if (!outgoing && cached != runtime->followsMeCache.end()
            && observed != runtime->followsMeObservedAt.end()
            && std::chrono::steady_clock::now() - observed->second < std::chrono::seconds{60}) {
            const bool followsMe = cached->second;
            const QPointer<PresentationBridge> bridgeGuard{&bridge};
            QMetaObject::invokeMethod(
                &bridge,
                [bridgeGuard, accountFid, profileFid, followsMe] {
                    if (bridgeGuard && bridgeGuard->isAuthenticated()
                        && bridgeGuard->accountFid() == static_cast<qulonglong>(accountFid.value)) {
                        bridgeGuard->applyProfileFollowsMe(
                            static_cast<qulonglong>(profileFid.value), followsMe);
                    }
                },
                Qt::QueuedConnection);
            return;
        }
        if (runtime->followsMeInFlight.size() >= 12
            || !runtime->followsMeInFlight.insert(relationshipKey).second) {
            return;
        }
    }

    const QPointer<PresentationBridge> bridgeGuard{&bridge};
    auto request = runtime->asyncGateway->isFollowing(
        outgoing ? accountFid : profileFid,
        outgoing ? profileFid : accountFid,
        [bridgeGuard, runtime, accountFid, profileFid, relationshipKey, outgoing](
            foundation::result::Result<bool> relationship) mutable {
            {
                std::scoped_lock lock{runtime->profileRelationshipMutex};
                runtime->followsMeInFlight.erase(relationshipKey);
                runtime->followsMeRequests.erase(relationshipKey);
                if (relationship && !outgoing) {
                    if (runtime->followsMeCache.size() >= 256) {
                        runtime->followsMeCache.clear();
                        runtime->followsMeObservedAt.clear();
                    }
                    runtime->followsMeCache.insert_or_assign(relationshipKey, *relationship);
                    runtime->followsMeObservedAt.insert_or_assign(relationshipKey, std::chrono::steady_clock::now());
                }
            }
            if (!relationship) {
                return;
            }
            PresentationBridge* const target = bridgeGuard.data();
            if (target == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                target,
                [bridgeGuard, accountFid, profileFid, outgoing, followsMe = *relationship] {
                    if (bridgeGuard && bridgeGuard->isAuthenticated()
                        && bridgeGuard->accountFid() == static_cast<qulonglong>(accountFid.value)) {
                        if (outgoing) {
                            bridgeGuard->applySearchFollowing(
                                static_cast<qulonglong>(profileFid.value), followsMe);
                        } else {
                            bridgeGuard->applyProfileFollowsMe(
                                static_cast<qulonglong>(profileFid.value), followsMe);
                        }
                    }
                },
                Qt::QueuedConnection);
        });
    if (!request) {
        std::scoped_lock lock{runtime->profileRelationshipMutex};
        runtime->followsMeInFlight.erase(relationshipKey);
        return;
    }
    std::scoped_lock lock{runtime->profileRelationshipMutex};
    runtime->followsMeRequests.insert_or_assign(relationshipKey, std::move(*request));
}

void refreshViewerReactionSnapshot(
    PresentationBridge&,
    const std::shared_ptr<Runtime>& runtime,
    const domain::identity::Fid accountFid,
    const bool force)
{
    if (accountFid.value == 0 || !runtime->asyncGateway) {
        return;
    }
    if (force) {
        runtime->viewerStateLastRefreshMs.store(0, std::memory_order_release);
    }
    // Exact interaction hydration is performed against the visible feed/cast
    // targets after the read completes. Avoid downloading a large account-wide
    // reaction snapshot solely to render a small viewport.
}

void refreshVisibleViewerReactionState(
    PresentationBridge& bridge,
    const std::shared_ptr<Runtime>& runtime,
    const domain::identity::Fid accountFid,
    std::vector<domain::cast::CastReference> targets)
{
    if (accountFid.value == 0 || targets.empty() || !runtime->asyncGateway) {
        return;
    }
    if (targets.size() > 24U) {
        targets.resize(24U);
    }

    auto rowTargets = targets;
    const QPointer<PresentationBridge> bridgeGuard{&bridge};
    auto request = runtime->asyncGateway->viewerReactionsFor(
        accountFid,
        std::move(targets),
        [bridgeGuard, runtime, accountFid, rowTargets = std::move(rowTargets)](
            foundation::result::Result<domain::feed::ViewerReactionSnapshot> snapshot) mutable {
            {
                std::scoped_lock lock{runtime->viewerOperationMutex};
                runtime->activeViewerReactionRequest.reset();
            }
            if (!snapshot) {
                return;
            }
            QVariantList rows{};
            rows.reserve(static_cast<qsizetype>(rowTargets.size()));
            for (const auto& target : rowTargets) {
                const auto isPresent = [&target](const std::vector<domain::cast::CastId>& ids) {
                    return std::ranges::any_of(ids, [&target](const domain::cast::CastId& id) {
                        return id.value == target.id.value;
                    });
                };
                rows.push_back(QVariantMap{
                    {QStringLiteral("castId"), QString::fromStdString(target.id.value)},
                    {QStringLiteral("liked"), isPresent(snapshot->liked)},
                    {QStringLiteral("recasted"), isPresent(snapshot->recasted)},
                });
            }
            runtime->viewerStateLastRefreshMs.store(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count(),
                std::memory_order_release);
            PresentationBridge* const targetBridge = bridgeGuard.data();
            if (targetBridge == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                targetBridge,
                [bridgeGuard, accountFid, rows = std::move(rows)]() mutable {
                    if (bridgeGuard && bridgeGuard->isAuthenticated()
                        && bridgeGuard->accountFid() == static_cast<qulonglong>(accountFid.value)) {
                        bridgeGuard->applyViewerReactionDelta(std::move(rows));
                    }
                },
                Qt::QueuedConnection);
        });
    if (!request) {
        return;
    }
    std::scoped_lock lock{runtime->viewerOperationMutex};
    if (runtime->activeViewerReactionRequest) {
        runtime->activeViewerReactionRequest->cancel();
    }
    runtime->activeViewerReactionRequest = std::move(*request);
}
#else
void refreshViewerReactionSnapshot(
    PresentationBridge&,
    const std::shared_ptr<Runtime>&,
    domain::identity::Fid,
    bool)
{
}

void refreshVisibleViewerReactionState(
    PresentationBridge&,
    const std::shared_ptr<Runtime>&,
    domain::identity::Fid,
    std::vector<domain::cast::CastReference>)
{
}
#endif

void applyWriteState(
    PresentationBridge& bridge,
    application::write::WriteState state,
    const std::optional<domain::session::ClientSession>& session)
{
    if (state.record) {
        const auto authorFid = state.record->author.value;
        if (!bridge.isAuthenticated() || bridge.accountFid() != authorFid) {
            return;
        }
    }
#ifndef NDEBUG
    if (state.error) {
        qInfo().noquote() << "Genycaster write failure:"
                          << QString::fromStdString(state.error->message);
    }
#endif
    const bool busy = state.phase == application::write::WritePhase::Submitting;
    bridge.applyWriteState(
        writePhaseText(state.phase),
        busy,
        writeStatusText(state),
        writeErrorText(state),
        writePresentation(state, session));
}

void applySessionState(
    PresentationBridge& bridge,
    application::session::SessionState state,
    const bool authenticationAvailable)
{
#ifndef NDEBUG
    if (state.error) {
        qInfo().noquote() << "Genycaster authentication failure:"
                          << QString::fromStdString(state.error->message);
    }
#endif
    qulonglong fid = 0;
    QString username{};
    QString displayName{};
    QString avatarUrl{};
    QString identityIssuer{};
    if (state.session) {
        fid = static_cast<qulonglong>(state.session->fid.value);
        if (state.session->username) {
            username = QString::fromStdString(*state.session->username);
        }
        if (state.session->displayName) {
            displayName = QString::fromStdString(*state.session->displayName);
        }
        if (state.session->avatarUrl) {
            avatarUrl = remoteImageSource(QString::fromStdString(*state.session->avatarUrl));
        }
        if (state.session->identityIssuer) {
            identityIssuer = QString::fromStdString(*state.session->identityIssuer);
        }
    }
    bridge.applyAuthenticationState(
        authenticationAvailable,
        sessionPhaseText(state.phase),
        fid,
        std::move(username),
        std::move(displayName),
        std::move(avatarUrl),
        std::move(identityIssuer),
        QString::fromStdString(state.approvalUrl),
        authenticationErrorText(state, bridge.authenticationRoute()));
}

void hydrateAuthenticatedAccountProfile(
    PresentationBridge& bridge, const std::shared_ptr<Runtime>& runtime)
{
    runtime->cancelAccountProfileRequest();
    if (!bridge.isAuthenticated() || bridge.accountFid() == 0 || !runtime->service) {
        return;
    }

    const qulonglong accountFid = bridge.accountFid();
    auto request = runtime->service->openProfile(
        domain::identity::Fid{static_cast<std::uint64_t>(accountFid)},
        [&bridge, accountFid](application::read::model::ProfileState state) mutable {
            if (!state.profile || state.profile->fid.value != static_cast<std::uint64_t>(accountFid)) {
                return;
            }
            const auto* profile = &*state.profile;
            QString profileDisplayName = displayName(profile);
            QString profileUsername = username(profile);
            QString profileAvatarUrl = avatarUrl(profile);
            QVariantMap profileDetail = profilePresentation(profile);
            QMetaObject::invokeMethod(
                &bridge,
                [&bridge, accountFid, profileDisplayName = std::move(profileDisplayName),
                    profileUsername = std::move(profileUsername),
                    profileAvatarUrl = std::move(profileAvatarUrl),
                    profileDetail = std::move(profileDetail)]() mutable {
                    if (!bridge.isAuthenticated() || bridge.accountFid() != accountFid) {
                        return;
                    }
                    bridge.applyAccountProfile(
                        accountFid, std::move(profileDisplayName), std::move(profileUsername),
                        std::move(profileAvatarUrl), std::move(profileDetail));
                },
                Qt::QueuedConnection);
        });
    if (request) {
        runtime->keepAccountProfileRequest(std::move(*request));
    }
}

#if FARCASTER_HAS_MEDIA_UPLOAD
void scheduleManagedMediaCleanup(
    RichComposerController& richComposer,
    const std::shared_ptr<Runtime>& runtime)
{
    if (!runtime->mediaLifecycleService) {
        return;
    }

    const QPointer<RichComposerController> composerGuard{&richComposer};
    for (const auto& token : richComposer.pendingManagedAssetCleanup()) {
        const QByteArray tokenUtf8 = token.toUtf8();
        const std::string managementToken{
            tokenUtf8.constData(), static_cast<std::size_t>(tokenUtf8.size())};
        {
            std::scoped_lock lock{runtime->mediaCleanupMutex};
            if (!runtime->mediaCleanupInFlight.insert(managementToken).second) {
                continue;
            }
        }

        auto scheduled = runtime->mediaLifecycleService->remove(
            runtime->currentSessionSnapshot(),
            managementToken,
            [runtime, composerGuard](application::media::lifecycle::CleanupState state) mutable {
                using application::media::lifecycle::CleanupPhase;
                if (state.phase != CleanupPhase::Completed && state.phase != CleanupPhase::Failed) {
                    return;
                }

                {
                    std::scoped_lock lock{runtime->mediaCleanupMutex};
                    runtime->mediaCleanupInFlight.erase(state.managementToken);
                }
#ifndef NDEBUG
                if (state.phase == CleanupPhase::Failed && state.error) {
                    qInfo().noquote() << "Genycaster managed media cleanup failure:"
                                      << QString::fromStdString(state.error->message);
                }
#endif
                if (state.phase != CleanupPhase::Completed || !composerGuard) {
                    return;
                }

                const QString completedToken = QString::fromUtf8(
                    state.managementToken.data(),
                    static_cast<qsizetype>(state.managementToken.size()));
                QMetaObject::invokeMethod(
                    composerGuard.data(),
                    [composerGuard, completedToken] {
                        if (composerGuard) {
                            composerGuard->markManagedAssetCleanupCompleted(completedToken);
                        }
                    },
                    Qt::QueuedConnection);
            });
        if (!scheduled) {
            std::scoped_lock lock{runtime->mediaCleanupMutex};
            runtime->mediaCleanupInFlight.erase(managementToken);
#ifndef NDEBUG
            qInfo().noquote() << "Genycaster managed media cleanup could not start:"
                              << QString::fromStdString(scheduled.error().message);
#endif
        }
    }
}
#endif

void connectRuntime(
    PresentationBridge& bridge,
    RichComposerController& richComposer,
    const std::shared_ptr<Runtime>& runtime,
    const bool authenticationAvailable,
    std::string siwfDomain,
    std::string siwfUri,
    const QString& browserSnapchainOrigin)
{
#if defined(__EMSCRIPTEN__)
    auto* const browserSigner = new genycaster::ui::browser_signing::Bridge(
        bridge, browserSnapchainOrigin);
#else
    Q_UNUSED(browserSnapchainOrigin);
#endif
    const auto applyNetworkAffinity = [runtime](const QString& network) {
        if (runtime->router) {
            runtime->router->setPreferredNetwork(
                network == QStringLiteral("farcaster")
                    ? routing::policy::PreferredNetwork::Farcaster
                    : routing::policy::PreferredNetwork::Hypersnap);
        }
    };
    applyNetworkAffinity(bridge.preferredNetwork());
    QObject::connect(&bridge, &PresentationBridge::preferredNetworkChanged, &bridge,
        [&bridge, applyNetworkAffinity](const QString& network) {
            applyNetworkAffinity(network);
            if (bridge.runtimeReady() && !bridge.isBusy()) {
                bridge.retryAutomatically();
            }
        });
#if FARCASTER_HAS_MEDIA_UPLOAD
    QObject::connect(
        &richComposer,
        &RichComposerController::managedAssetCleanupRequested,
        &bridge,
        [&richComposer, runtime] { scheduleManagedMediaCleanup(richComposer, runtime); },
        Qt::QueuedConnection);
#endif

    const auto writeObserver = [&bridge, &richComposer, runtime](application::write::WriteState state) {
        const auto session = runtime->currentSessionSnapshot();
        QString removedCastId{};
        if (state.phase == application::write::WritePhase::Confirmed && state.record &&
            state.record->kind == application::write::OperationKind::RemoveCast &&
            state.record->target.cast) {
            removedCastId = QString::fromStdString(state.record->target.cast->id.value);
        }
        QMetaObject::invokeMethod(
            &bridge,
            [&bridge, &richComposer, runtime, state = std::move(state), session,
             removedCastId = std::move(removedCastId)]() mutable {
                applyWriteState(bridge, std::move(state), session);
#if FARCASTER_HAS_MEDIA_UPLOAD
                if (!removedCastId.isEmpty()) {
                    (void)richComposer.releaseManagedAssetsForCast(removedCastId);
                    scheduleManagedMediaCleanup(richComposer, runtime);
                }
#endif
            },
            Qt::QueuedConnection);
    };

    const auto signerObserver = [&bridge, &richComposer, runtime, writeObserver](application::signer::SignerState state) {
        const bool ready = state.phase == application::signer::SignerPhase::Ready;
        const bool authorityTerminated = state.phase == application::signer::SignerPhase::Revoked ||
            state.phase == application::signer::SignerPhase::Expired;
        const auto observedSession = runtime->currentSessionSnapshot();
        QMetaObject::invokeMethod(
            &bridge,
            [&bridge, state = std::move(state), observedSession]() mutable {
                // A queued result from the prior account must not mark the
                // new account write-ready or launch another custody request.
                if (state.phase == application::signer::SignerPhase::SignedOut) {
                    if (bridge.isAuthenticated()) return;
                } else if (!observedSession || !bridge.isAuthenticated()
                           || bridge.accountFid() != observedSession->fid.value
                           || (state.record
                               && state.record->local.fid.value != bridge.accountFid())) {
                    return;
                }
                applySignerState(bridge, std::move(state));
            },
            Qt::QueuedConnection);
        if (authorityTerminated) {
            if (runtime->writeService) {
                (void)runtime->writeService->invalidatePendingAuthority(
                    runtime->currentSessionSnapshot(), writeObserver);
            }
        } else if (ready) {
            if (runtime->writeService) {
                (void)runtime->writeService->resume(runtime->currentSessionSnapshot(), writeObserver);
            }
#if FARCASTER_HAS_MEDIA_UPLOAD
            QMetaObject::invokeMethod(
                &richComposer,
                [&richComposer, runtime] { scheduleManagedMediaCleanup(richComposer, runtime); },
                Qt::QueuedConnection);
#endif
        }
    };

    // Backoff deadlines are persisted in SQLite, not in a Qt timer. The
    // network can stay "online" while a retry becomes due, so resume must
    // also be revisited after startup without requiring a reachability edge.
    auto* const outboxRetryTimer = new QTimer(&bridge);
    outboxRetryTimer->setInterval(30'000);
    QObject::connect(outboxRetryTimer, &QTimer::timeout, &bridge,
        [&bridge, runtime, writeObserver] {
            if (!bridge.isAuthenticated() || !bridge.writeReady() || bridge.writeBusy() ||
                !runtime->writeService) return;
            (void)runtime->writeService->resume(runtime->currentSessionSnapshot(), writeObserver);
        });
    outboxRetryTimer->start();

    QObject::connect(&bridge, &PresentationBridge::networkReachabilityChanged, &bridge,
        [&bridge, &richComposer, runtime, writeObserver](const bool known, const bool online) {
            if (!known || !online) {
                return;
            }
#if FARCASTER_HAS_MEDIA_UPLOAD
            scheduleManagedMediaCleanup(richComposer, runtime);
#endif
            if (!runtime->writeService || !bridge.writeReady() || bridge.writeBusy()) {
                return;
            }
            (void)runtime->writeService->resume(runtime->currentSessionSnapshot(), writeObserver);
        });

    const auto sessionObserver = [&bridge, runtime, authenticationAvailable, signerObserver
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ] (application::session::SessionState state) {
            const auto signerSession = state.session;
            runtime->setCurrentSession(signerSession);
            QMetaObject::invokeMethod(
                &bridge,
                [&bridge, runtime, state = std::move(state), authenticationAvailable
#if defined(__EMSCRIPTEN__)
                 , browserSigner
#endif
                ]() mutable {
#if defined(__EMSCRIPTEN__)
                    const auto completedPhase = state.phase;
                    const auto verifiedFid = state.session ? state.session->fid.value : 0U;
#endif
                    applySessionState(bridge, std::move(state), authenticationAvailable);
                    runtime->authenticationPollInFlight.store(false, std::memory_order_release);
                    hydrateAuthenticatedAccountProfile(bridge, runtime);
#if defined(__EMSCRIPTEN__)
                    // Restore only after PresentationBridge has committed
                    // the actual signed-in FID; never race queued session UI.
                    if (completedPhase == application::session::SessionPhase::Authenticated
                        && verifiedFid != 0U) {
                        // A sign-in only restores an already-authorized signer.
                        // Never open the recovery dialog without explicit user action.
                        browserSigner->start(QStringLiteral("restore"),
                            QString::number(verifiedFid));
                    } else if (completedPhase == application::session::SessionPhase::Guest) {
                        browserSigner->start(QStringLiteral("signed-out"), {});
                    }
#endif
                },
                Qt::QueuedConnection);
            hydrateWriteProjections(bridge, runtime, signerSession);
            if (signerSession) {
                refreshViewerReactionSnapshot(bridge, runtime, signerSession->fid, true);
            } else {
                QMetaObject::invokeMethod(&bridge, [&bridge] {
                    bridge.applyViewerReactionSnapshot({});
                }, Qt::QueuedConnection);
            }

            if (runtime->signerService) {
                (void)runtime->signerService->restore(signerSession, signerObserver);
            } else if (signerSession) {
                application::signer::SignerState signerState{
                    .phase = application::signer::SignerPhase::Unavailable,
                    .error = runtime->signerSetupError.value_or(foundation::error::Error::make(
                        foundation::error::ErrorCode::Unsupported,
                        "Protocol signer composition is not available in this build.")),
                };
                signerObserver(std::move(signerState));
            } else {
                signerObserver(application::signer::SignerState{
                    .phase = application::signer::SignerPhase::SignedOut,
                });
            }
        };

#if defined(__EMSCRIPTEN__)
    // A browser-safe OpenProof device grant is independent from the blocking
    // native authentication adapter and from protocol signer authorization.
    auto* const browserAuth = new BrowserOpenProofAuth(&bridge);
    const std::function<void(application::session::SessionState)> applyBrowserSession = sessionObserver;
    const auto browserObserver = [applyBrowserSession](BrowserOpenProofState event) {
        application::session::SessionState state{};
        using Phase = BrowserOpenProofState::Phase;
        switch (event.phase) {
        case Phase::Starting:
            state.phase = application::session::SessionPhase::Starting;
            break;
        case Phase::AwaitingApproval:
            state.phase = application::session::SessionPhase::AwaitingApproval;
            state.approvalUrl = event.approvalUrl.toStdString();
            break;
        case Phase::Authenticated: {
            const auto authenticatedAt = foundation::time::now();
            state.phase = application::session::SessionPhase::Authenticated;
            state.session = domain::session::ClientSession{
                .identityIssuer = std::string{"https://openproof.genyleap.com"},
                .identitySubject = event.subject.toStdString(),
                .fid = domain::identity::Fid{event.fid},
                .username = event.username.isEmpty()
                    ? std::nullopt : std::optional<std::string>{event.username.toStdString()},
                .displayName = event.displayName.isEmpty()
                    ? std::nullopt : std::optional<std::string>{event.displayName.toStdString()},
                .avatarUrl = event.avatarUrl.isEmpty()
                    ? std::nullopt : std::optional<std::string>{event.avatarUrl.toStdString()},
                .method = domain::session::AuthenticationMethod::Custody,
                .authenticatedAt = authenticatedAt,
                .expiresAt = authenticatedAt + std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::seconds{event.tokenExpiresSeconds}),
            };
            break;
        }
        case Phase::Failed:
            state.phase = application::session::SessionPhase::Failed;
            state.error = foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized, event.error.toStdString());
            break;
        case Phase::SignedOut:
            state.phase = application::session::SessionPhase::Guest;
            break;
        }
        applyBrowserSession(std::move(state));
    };
    browserSigner->setRecoveryObserver(
        [sessionObserver](QString fid, QString error, const bool verified) {
            application::session::SessionState state{};
            if (!verified) {
                if (error.isEmpty()) {
                    state.phase = application::session::SessionPhase::Guest;
                } else {
                    state.phase = application::session::SessionPhase::Failed;
                    state.error = foundation::error::Error::make(
                        foundation::error::ErrorCode::Unauthorized, error.toStdString());
                }
            } else {
                bool valid = false;
                const qulonglong number = fid.toULongLong(&valid);
                if (!valid || number == 0U) return;
                const auto authenticatedAt = foundation::time::now();
                state.phase = application::session::SessionPhase::Authenticated;
                state.session = domain::session::ClientSession{
                    .identityIssuer = std::string{"farcaster:optimism-idregistry"},
                    .identitySubject = std::string{"fid:"} + fid.toStdString(),
                    .fid = domain::identity::Fid{static_cast<std::uint64_t>(number)},
                    .method = domain::session::AuthenticationMethod::Custody,
                    .authenticatedAt = authenticatedAt,
                    .expiresAt = authenticatedAt
                        + std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::hours{1}),
                };
            }
            sessionObserver(std::move(state));
        });
    QObject::connect(&bridge, &PresentationBridge::signInRequested, browserAuth,
        [browserAuth, browserObserver, browserSigner, &bridge, sessionObserver](const QString& route) {
            if (route == QStringLiteral("openproof")) {
                browserSigner->cancelRecovery();
                browserAuth->begin(browserObserver);
            } else if (route == QStringLiteral("farcaster")) {
                browserSigner->cancelRecovery();
                browserAuth->beginFarcaster(browserObserver);
            } else if (route == QStringLiteral("recovery")) {
                sessionObserver(application::session::SessionState{
                    .phase = application::session::SessionPhase::Starting,
                });
                browserSigner->startRecovery();
            }
        });
    QObject::connect(&bridge, &PresentationBridge::signInCancelRequested,
                     browserAuth, [browserAuth, browserSigner] {
                         browserSigner->cancelRecovery();
                         browserAuth->cancel();
                     });
    QObject::connect(&bridge, &PresentationBridge::signOutRequested,
                     browserAuth, [browserAuth, browserSigner] {
                         browserSigner->cancelRecovery();
                         browserAuth->signOut();
                     });
#else
    QObject::connect(&bridge, &PresentationBridge::signInRequested, &bridge,
        [&bridge, runtime, sessionObserver, siwfDomain, siwfUri](const QString& route) {
            const auto signInRoute = route == QStringLiteral("farcaster")
                ? capability::auth::SignInRoute::Farcaster
                : capability::auth::SignInRoute::OpenProof;
            auto status = runtime->sessionService->beginSignIn(
                capability::auth::SignInRequest{
                    .route = signInRoute,
                    .domain = siwfDomain,
                    .siweUri = siwfUri,
                },
                sessionObserver);
            if (!status) {
                bridge.applyAuthenticationError(
                    qtTrId("genycaster.runtime.the_authentication_worker_is_unavailable"));
            }
        });

    QObject::connect(&bridge, &PresentationBridge::signInCancelRequested, &bridge,
        [runtime, sessionObserver] {
            (void)runtime->sessionService->cancelSignIn(sessionObserver);
        });

    QObject::connect(&bridge, &PresentationBridge::signOutRequested, &bridge,
        [runtime, sessionObserver] {
            (void)runtime->sessionService->signOut(sessionObserver);
        });
#endif

    QObject::connect(&bridge, &PresentationBridge::signerProvisionRequested, &bridge,
        [&bridge, runtime, signerObserver
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ] {
#if defined(__EMSCRIPTEN__)
            if (bridge.isAuthenticated() && bridge.accountFid() != 0)
                browserSigner->start(QStringLiteral("provision"),
                    QString::number(bridge.accountFid()));
            return;
#endif
            if (!runtime->signerService) {
                application::signer::SignerState state{
                    .phase = application::signer::SignerPhase::Unavailable,
                    .error = runtime->signerSetupError.value_or(foundation::error::Error::make(
                        foundation::error::ErrorCode::Unsupported,
                        "Protocol signer composition is not available in this build.")),
                };
                applySignerState(bridge, std::move(state));
                return;
            }
            auto status = runtime->signerService->provision(
                runtime->currentSessionSnapshot(), signerObserver);
            if (!status) {
                application::signer::SignerState state{
                    .phase = application::signer::SignerPhase::Unavailable,
                    .error = std::move(status.error()),
                };
                applySignerState(bridge, std::move(state));
            }
        });

    QObject::connect(&bridge, &PresentationBridge::signerRevokeRequested, &bridge,
        [&bridge, runtime, signerObserver
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ] {
#if defined(__EMSCRIPTEN__)
            if (bridge.isAuthenticated() && bridge.accountFid() != 0)
                browserSigner->start(QStringLiteral("revoke"),
                    QString::number(bridge.accountFid()));
            return;
#endif
            if (!runtime->signerService) { return; }
            auto status = runtime->signerService->revoke(
                runtime->currentSessionSnapshot(), signerObserver);
            if (!status) {
                application::signer::SignerState state{
                    .phase = application::signer::SignerPhase::Unavailable,
                    .error = std::move(status.error()),
                };
                applySignerState(bridge, std::move(state));
            }
        });

    const auto mentionFidsFromVariants = [](const QVariantList& values) {
        std::vector<domain::identity::Fid> result{};
        result.reserve(static_cast<std::size_t>(values.size()));
        for (const auto& value : values) {
            bool ok = false;
            const auto fid = value.toULongLong(&ok);
            if (!ok || fid == 0U) { return std::vector<domain::identity::Fid>{}; }
            result.push_back(domain::identity::Fid{static_cast<std::uint64_t>(fid)});
        }
        return result;
    };
    const auto mentionPositionsFromVariants = [](const QVariantList& values) {
        std::vector<std::uint32_t> result{};
        result.reserve(static_cast<std::size_t>(values.size()));
        for (const auto& value : values) {
            bool ok = false;
            const auto position = value.toUInt(&ok);
            if (!ok) { return std::vector<std::uint32_t>{}; }
            result.push_back(position);
        }
        return result;
    };
    const auto embedsFromVariants = [](const QVariantList& values) -> std::optional<std::vector<domain::cast::Embed>> {
        if (values.size() > 2) return std::nullopt;
        std::vector<domain::cast::Embed> result{};
        result.reserve(static_cast<std::size_t>(values.size()));
        for (const auto& value : values) {
            const auto map = value.toMap();
            const auto kind = map.value(QStringLiteral("kind")).toString();
            const auto payload = map.value(QStringLiteral("value")).toString().trimmed();
            if (payload.isEmpty()) return std::nullopt;
            if (kind == QStringLiteral("url")) {
                result.push_back(domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::Url,
                    .value = payload.toUtf8().toStdString(),
                });
                continue;
            }
            if (kind == QStringLiteral("cast-reference")) {
                bool ok = false;
                const auto authorFid = map.value(QStringLiteral("authorFid")).toULongLong(&ok);
                if (!ok || authorFid == 0U) return std::nullopt;
                result.push_back(domain::cast::Embed{
                    .kind = domain::cast::EmbedKind::CastReference,
                    .value = payload.toUtf8().toStdString(),
                    .referencedAuthor = domain::identity::Fid{static_cast<std::uint64_t>(authorFid)},
                });
                continue;
            }
            return std::nullopt;
        }
        return result;
    };

    QObject::connect(&bridge, &PresentationBridge::publishCastRequested, &bridge,
        [&bridge, runtime, writeObserver, mentionFidsFromVariants, mentionPositionsFromVariants, embedsFromVariants
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ](
            const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions,
            const QVariantList& embeds) {
#if defined(__EMSCRIPTEN__)

            if (!bridge.isAuthenticated() || !bridge.writeReady()) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            const auto payload = genycaster::ui::browser_signing::makeCastPayload(
                text, mentionFids, mentionPositions, embeds);
            if (!payload) {
                bridge.applyWriteState(QStringLiteral("failed"), false, {},
                    qtTrId("genycaster.bridge.invalid_cast_embeds"));
                return;
            }
            browserSigner->start(QStringLiteral("cast"),
                QString::number(bridge.accountFid()), *payload);
            return;
#endif
            if (!runtime->writeService) {
                bridge.applyWriteState(
                    QStringLiteral("blocked"), false,
                    qtTrId("genycaster.runtime.publishing_is_unavailable_in_this_runtime"),
                    runtime->writeSetupError
                        ? QString::fromStdString(runtime->writeSetupError->message)
                        : qtTrId("genycaster.runtime.durable_publishcast_composition_is_not_available"));
                return;
            }
            const auto fids = mentionFidsFromVariants(mentionFids);
            const auto positions = mentionPositionsFromVariants(mentionPositions);
            const auto protocolEmbeds = embedsFromVariants(embeds);
            if (fids.size() != static_cast<std::size_t>(mentionFids.size()) ||
                positions.size() != static_cast<std::size_t>(mentionPositions.size())) {
                bridge.applyWriteState(QStringLiteral("failed"), false, {}, qtTrId("genycaster.bridge.invalid_semantic_mentions"));
                return;
            }
            if (!protocolEmbeds) {
                bridge.applyWriteState(QStringLiteral("failed"), false, {}, qtTrId("genycaster.bridge.invalid_cast_embeds"));
                return;
            }
            auto status = runtime->writeService->publish(
                runtime->currentSessionSnapshot(),
                application::write::PublishCastRequest{
                    .text = text.toUtf8().toStdString(),
                    .embeds = *protocolEmbeds,
                    .mentions = fids,
                    .mentionPositions = positions,
                },
                writeObserver);
            if (!status) {
                bridge.applyWriteState(
                    QStringLiteral("failed"), false,
                    qtTrId("genycaster.runtime.publish_could_not_be_scheduled"),
                    QString::fromStdString(status.error().message));
            }
        });

    QObject::connect(&bridge, &PresentationBridge::publishReplyRequested, &bridge,
        [&bridge, runtime, writeObserver, mentionFidsFromVariants, mentionPositionsFromVariants, embedsFromVariants
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ](
            const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions,
            const QVariantList& embeds, const qulonglong parentAuthorFid, const QString& parentCastId) {
#if defined(__EMSCRIPTEN__)

            const auto parent = genycaster::ui::browser_signing::makeCastReference(
                parentAuthorFid, parentCastId);
            const auto payload = parent
                ? genycaster::ui::browser_signing::makeCastPayload(
                    text, mentionFids, mentionPositions, embeds, parent) : std::nullopt;
            if (!bridge.isAuthenticated() || !bridge.writeReady() || !payload) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            browserSigner->start(QStringLiteral("cast"),
                QString::number(bridge.accountFid()), *payload);
            return;
#endif
            if (!runtime->writeService) {
                bridge.applyWriteState(
                    QStringLiteral("blocked"), false,
                    qtTrId("genycaster.runtime.replying_is_unavailable_in_this_runtime"),
                    runtime->writeSetupError
                        ? QString::fromStdString(runtime->writeSetupError->message)
                        : qtTrId("genycaster.runtime.durable_cast_add_composition_is_not_available"));
                return;
            }
            const auto fids = mentionFidsFromVariants(mentionFids);
            const auto positions = mentionPositionsFromVariants(mentionPositions);
            const auto protocolEmbeds = embedsFromVariants(embeds);
            if (fids.size() != static_cast<std::size_t>(mentionFids.size()) ||
                positions.size() != static_cast<std::size_t>(mentionPositions.size())) {
                bridge.applyWriteState(QStringLiteral("failed"), false, {}, qtTrId("genycaster.bridge.invalid_semantic_mentions"));
                return;
            }
            if (!protocolEmbeds) {
                bridge.applyWriteState(QStringLiteral("failed"), false, {}, qtTrId("genycaster.bridge.invalid_cast_embeds"));
                return;
            }
            auto status = runtime->writeService->publish(
                runtime->currentSessionSnapshot(),
                application::write::PublishCastRequest{
                    .text = text.toUtf8().toStdString(),
                    .embeds = *protocolEmbeds,
                    .mentions = fids,
                    .mentionPositions = positions,
                    .parent = domain::cast::CastReference{
                        .author = domain::identity::Fid{static_cast<std::uint64_t>(parentAuthorFid)},
                        .id = domain::cast::CastId{parentCastId.toUtf8().toStdString()},
                    },
                },
                writeObserver);
            if (!status) {
                bridge.applyWriteState(
                    QStringLiteral("failed"), false,
                    qtTrId("genycaster.runtime.reply_could_not_be_scheduled"),
                    QString::fromStdString(status.error().message));
            }
        });


    QObject::connect(&bridge, &PresentationBridge::publishThreadRequested, &bridge,
        [&bridge, runtime, writeObserver, mentionFidsFromVariants, mentionPositionsFromVariants, embedsFromVariants
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ](
            const QVariantList& casts, const qulonglong parentAuthorFid, const QString& parentCastId) {
#if defined(__EMSCRIPTEN__)

            if (!bridge.isAuthenticated() || !bridge.writeReady()
                || casts.isEmpty() || casts.size() > 32) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            QJsonArray serialized{};
            std::optional<QJsonObject> parent{};
            if (parentAuthorFid > 0 || !parentCastId.isEmpty()) {
                parent = genycaster::ui::browser_signing::makeCastReference(
                    parentAuthorFid, parentCastId);
                if (!parent) {
                    bridge.applyWriteState(QStringLiteral("failed"), false, {},
                        qtTrId("genycaster.bridge.invalid_cast_embeds"));
                    return;
                }
            }
            for (const auto& castValue : casts) {
                const auto cast = castValue.toMap();
                const auto payload = genycaster::ui::browser_signing::makeCastPayload(
                    cast.value(QStringLiteral("text")).toString(),
                    cast.value(QStringLiteral("mentionFids")).toList(),
                    cast.value(QStringLiteral("mentionPositions")).toList(),
                    cast.value(QStringLiteral("embeds")).toList());
                if (!payload) {
                    bridge.applyWriteState(QStringLiteral("failed"), false, {},
                        qtTrId("genycaster.bridge.invalid_cast_embeds"));
                    return;
                }
                serialized.append(*payload);
            }
            QJsonObject data{{QStringLiteral("casts"), serialized}};
            if (parent) data.insert(QStringLiteral("parent"), *parent);
            browserSigner->start(QStringLiteral("thread"),
                QString::number(bridge.accountFid()), data);
            return;
#endif
            if (!runtime->writeService) {
                bridge.applyWriteState(
                    QStringLiteral("blocked"), false,
                    qtTrId("genycaster.runtime.publishing_is_unavailable_in_this_runtime"),
                    runtime->writeSetupError
                        ? QString::fromStdString(runtime->writeSetupError->message)
                        : qtTrId("genycaster.runtime.durable_publishcast_composition_is_not_available"));
                return;
            }

            std::vector<application::write::PublishCastRequest> requests{};
            requests.reserve(static_cast<std::size_t>(casts.size()));
            for (qsizetype index = 0; index < casts.size(); ++index) {
                const QVariantMap cast = casts[index].toMap();
                const QVariantList mentionFids = cast.value(QStringLiteral("mentionFids")).toList();
                const QVariantList mentionPositions = cast.value(QStringLiteral("mentionPositions")).toList();
                const QVariantList embeds = cast.value(QStringLiteral("embeds")).toList();
                const auto fids = mentionFidsFromVariants(mentionFids);
                const auto positions = mentionPositionsFromVariants(mentionPositions);
                const auto protocolEmbeds = embedsFromVariants(embeds);
                if (fids.size() != static_cast<std::size_t>(mentionFids.size()) ||
                    positions.size() != static_cast<std::size_t>(mentionPositions.size())) {
                    bridge.applyWriteState(QStringLiteral("failed"), false, {},
                        qtTrId("genycaster.bridge.invalid_semantic_mentions"));
                    return;
                }
                if (!protocolEmbeds) {
                    bridge.applyWriteState(QStringLiteral("failed"), false, {},
                        qtTrId("genycaster.bridge.invalid_cast_embeds"));
                    return;
                }
                application::write::PublishCastRequest request{
                    .text = cast.value(QStringLiteral("text")).toString().toUtf8().toStdString(),
                    .embeds = *protocolEmbeds,
                    .mentions = fids,
                    .mentionPositions = positions,
                };
                if (index == 0 && parentAuthorFid > 0 && !parentCastId.isEmpty()) {
                    request.parent = domain::cast::CastReference{
                        .author = domain::identity::Fid{static_cast<std::uint64_t>(parentAuthorFid)},
                        .id = domain::cast::CastId{parentCastId.toUtf8().toStdString()},
                    };
                }
                requests.push_back(std::move(request));
            }

            auto status = runtime->writeService->publishThread(
                runtime->currentSessionSnapshot(), std::move(requests), writeObserver);
            if (!status) {
                bridge.applyWriteState(
                    QStringLiteral("failed"), false,
                    qtTrId("genycaster.runtime.thread_could_not_be_scheduled"),
                    QString::fromStdString(status.error().message));
            }
        });

    QObject::connect(&bridge, &PresentationBridge::reactionRequested, &bridge,
        [&bridge, runtime, writeObserver
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ](const QString& kind, const qulonglong targetAuthorFid,
            const QString& targetCastId, const bool active) {
#if defined(__EMSCRIPTEN__)

            const auto target = genycaster::ui::browser_signing::makeCastReference(
                targetAuthorFid, targetCastId);
            if (!bridge.isAuthenticated() || !bridge.writeReady() || !target
                || (kind != QStringLiteral("like") && kind != QStringLiteral("recast"))) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            browserSigner->start(QStringLiteral("reaction"),
                QString::number(bridge.accountFid()),
                QJsonObject{{QStringLiteral("target"), *target},
                            {QStringLiteral("reaction"), kind},
                            {QStringLiteral("active"), active}});
            return;
#endif
            if (!runtime->writeService) {
                bridge.applyWriteState(
                    QStringLiteral("blocked"),
                    false,
                    qtTrId("genycaster.runtime.reactions_are_unavailable_in_this_runtime"),
                    runtime->writeSetupError
                        ? QString::fromStdString(runtime->writeSetupError->message)
                        : qtTrId("genycaster.runtime.durable_reaction_composition_is_not_available"));
                return;
            }
            const auto reaction = kind == QStringLiteral("like")
                ? application::write::ReactionKind::Like
                : application::write::ReactionKind::Recast;
            auto status = runtime->writeService->setReaction(
                runtime->currentSessionSnapshot(),
                domain::cast::CastReference{
                    .author = domain::identity::Fid{static_cast<std::uint64_t>(targetAuthorFid)},
                    .id = domain::cast::CastId{targetCastId.toStdString()},
                },
                reaction,
                active,
                writeObserver);
            if (!status) {
                bridge.applyWriteState(
                    QStringLiteral("failed"),
                    false,
                    qtTrId("genycaster.runtime.reaction_could_not_be_scheduled"),
                    QString::fromStdString(status.error().message));
            }
        });

    QObject::connect(&bridge, &PresentationBridge::followingMutationRequested, &bridge,
        [&bridge, runtime, writeObserver
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ](const qulonglong targetFid, const bool following) {
#if defined(__EMSCRIPTEN__)

            if (!bridge.isAuthenticated() || !bridge.writeReady()
                || targetFid == 0 || targetFid == bridge.accountFid()) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            browserSigner->start(QStringLiteral("follow"),
                QString::number(bridge.accountFid()),
                QJsonObject{{QStringLiteral("targetFid"), QString::number(targetFid)},
                            {QStringLiteral("active"), following}});
            return;
#endif
            if (!runtime->writeService) {
                bridge.applyWriteState(
                    QStringLiteral("blocked"),
                    false,
                    qtTrId("genycaster.runtime.follow_changes_are_unavailable_in_this_runtime"),
                    runtime->writeSetupError
                        ? QString::fromStdString(runtime->writeSetupError->message)
                        : qtTrId("genycaster.runtime.durable_follow_composition_is_not_available"));
                return;
            }
            auto status = runtime->writeService->setFollowing(
                runtime->currentSessionSnapshot(),
                domain::identity::Fid{static_cast<std::uint64_t>(targetFid)},
                following,
                writeObserver);
            if (!status) {
                bridge.applyWriteState(
                    QStringLiteral("failed"),
                    false,
                    qtTrId("genycaster.runtime.follow_change_could_not_be_scheduled"),
                    QString::fromStdString(status.error().message));
            }
        });

    QObject::connect(&bridge, &PresentationBridge::removeCastRequested, &bridge,
        [&bridge, runtime, writeObserver
#if defined(__EMSCRIPTEN__)
        , browserSigner
#endif
        ](const qulonglong authorFid, const QString& castId) {
#if defined(__EMSCRIPTEN__)

            const auto target = genycaster::ui::browser_signing::makeCastReference(
                authorFid, castId);
            if (!bridge.isAuthenticated() || !bridge.writeReady() || !target
                || authorFid != bridge.accountFid()) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            browserSigner->start(QStringLiteral("remove"),
                QString::number(bridge.accountFid()),
                QJsonObject{{QStringLiteral("target"), *target}});
            return;
#endif
            if (!runtime->writeService) {
                bridge.applyWriteState(
                    QStringLiteral("blocked"),
                    false,
                    qtTrId("genycaster.runtime.cast_deletion_is_unavailable_in_this_runtime"),
                    runtime->writeSetupError
                        ? QString::fromStdString(runtime->writeSetupError->message)
                        : qtTrId("genycaster.runtime.durable_cast_remove_composition_is_not_available"));
                return;
            }
            auto status = runtime->writeService->removeCast(
                runtime->currentSessionSnapshot(),
                domain::cast::CastReference{
                    .author = domain::identity::Fid{static_cast<std::uint64_t>(authorFid)},
                    .id = domain::cast::CastId{castId.toStdString()},
                },
                writeObserver);
            if (!status) {
                bridge.applyWriteState(
                    QStringLiteral("failed"),
                    false,
                    qtTrId("genycaster.runtime.cast_deletion_could_not_be_scheduled"),
                    QString::fromStdString(status.error().message));
            }
        });

    // Network reachability and signer-ready transitions are the primary recovery
    // wakeups. This low-frequency timer also retries durable managed-media cleanup.
    auto* const runtimeRecoveryTimer = new QTimer(&bridge);
    runtimeRecoveryTimer->setInterval(60'000);
    QObject::connect(runtimeRecoveryTimer, &QTimer::timeout, &bridge,
        [&bridge, &richComposer, runtime, writeObserver] {
            if (bridge.isOffline()) {
                return;
            }
#if FARCASTER_HAS_MEDIA_UPLOAD
            scheduleManagedMediaCleanup(richComposer, runtime);
#endif
            if (!runtime->writeService || !bridge.writeReady() || bridge.writeBusy()) {
                return;
            }
            (void)runtime->writeService->resume(runtime->currentSessionSnapshot(), writeObserver);
        });
    runtimeRecoveryTimer->start();

#if !defined(__EMSCRIPTEN__)
    // Native auth uses SessionService polling and persistent local sessions.
    // Browser OpenProof uses its own async QNetworkAccessManager device grant;
    // native polling would cancel its challenge as a spurious Guest response.
    auto* const authenticationPollTimer = new QTimer(&bridge);
    authenticationPollTimer->setInterval(1'000);
    QObject::connect(authenticationPollTimer, &QTimer::timeout, &bridge,
        [&bridge, runtime, sessionObserver] {
            if (bridge.authenticationPhase() != QStringLiteral("awaiting-approval") &&
                bridge.authenticationPhase() != QStringLiteral("linking-farcaster")) {
                return;
            }
            bool expected = false;
            if (!runtime->authenticationPollInFlight.compare_exchange_strong(
                    expected, true, std::memory_order_acq_rel)) {
                return;
            }
            auto status = runtime->sessionService->pollSignIn(sessionObserver);
            if (!status) {
                runtime->authenticationPollInFlight.store(false, std::memory_order_release);
            }
        });
    authenticationPollTimer->start();

    (void)runtime->sessionService->restore(sessionObserver);
#else
    // Browser identity is intentionally ephemeral and never restored from
    // native persisted sessions (which might already be expired).
    sessionObserver(application::session::SessionState{
        .phase = application::session::SessionPhase::Guest,
    });
#endif

    QObject::connect(&bridge, &PresentationBridge::searchSuggestionsCancelled, &bridge,
        [runtime] { runtime->cancelSearchRequest(); });
    QObject::connect(&bridge, &PresentationBridge::searchSuggestionsRequested, &bridge,
        [&bridge, runtime](const QString& queryText) {
            runtime->cancelSearchRequest();
            auto request = runtime->service->openSearch(
                application::read::model::SearchQuery{
                    .text = queryText.toStdString(),
                    .limit = 6,
                },
                [&bridge, runtime](application::read::model::SearchState state) {
                    QMetaObject::invokeMethod(&bridge, [&bridge, runtime, state = std::move(state)]() mutable {
                        bridge.applySearchSuggestions(
                            QString::fromStdString(state.query.text),
                            searchPresentation(state),
                            errorText(state.metadata),
                            isBusy(state.metadata));
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
                        // Bounded enrichment of only the still-visible query;
                        // failures leave relationship fields unknown, not false.
                        if (state.metadata.phase == application::read::model::ReadPhase::Ready
                            && bridge.isAuthenticated()) {
                            const domain::identity::Fid viewer{static_cast<std::uint64_t>(bridge.accountFid())};
                            for (const auto& value : bridge.searchResults()) {
                                const QVariantMap item = value.toMap();
                                if (item.value(QStringLiteral("kind")).toString() != QStringLiteral("profile")) continue;
                                const domain::identity::Fid target{static_cast<std::uint64_t>(
                                    item.value(QStringLiteral("fid")).toULongLong())};
                                if (!item.value(QStringLiteral("followingKnown")).toBool())
                                    refreshProfileFollowsMe(bridge, runtime, viewer, target, true);
                                refreshProfileFollowsMe(bridge, runtime, viewer, target);
                            }
                        }
#endif
                    }, Qt::QueuedConnection);
                });
            if (request) {
                runtime->keepSearchRequest(std::move(*request));
            } else {
                bridge.applySearchSuggestions(
                    queryText,
                    {},
                    qtTrId("genycaster.runtime.the_read_worker_is_unavailable"),
                    false);
            }
        });

    QObject::connect(&bridge, &PresentationBridge::identityRequested, &bridge,
        [&bridge, runtime](const QString& requestedUsername, const bool allowRecoveryProbe) {
            runtime->cancelAllRequests();
            const std::uint64_t requestGeneration = runtime->beginPrimaryRequestGeneration();
            auto request = runtime->service->openIdentity(requestedUsername.toStdString(),
                [&bridge, runtime, requestGeneration](application::read::model::IdentityState state) {
                    if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                        return;
                    }
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, runtime, requestGeneration, state = std::move(state)]() mutable {
                            if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                                return;
                            }
                            const auto* profile = state.profile ? &*state.profile : nullptr;
                            bridge.applyIdentity(
                                state.fid ? static_cast<qulonglong>(state.fid->value) : 0,
                                displayName(profile), username(profile), profilePresentation(profile), statusText(state.metadata),
                                errorText(state.metadata), diagnosticText(state.metadata), isBusy(state.metadata),
                                state.metadata.isStale, state.metadata.isSourceUnavailable);
                        }, Qt::QueuedConnection);
                }, application::read::model::ReadOptions{
                    .allowRecoveryProbe = allowRecoveryProbe,
                });
            if (request) { runtime->keepPrimaryRequest(std::move(*request)); }
            else { bridge.applyRuntimeState(false, qtTrId("genycaster.runtime.the_read_worker_is_unavailable")); }
        });

    QObject::connect(&bridge, &PresentationBridge::profileUsernameRequested, &bridge,
        [&bridge, runtime](const QString& requestedUsername, const bool allowRecoveryProbe) {
            runtime->cancelAllRequests();
            const std::uint64_t requestGeneration = runtime->beginPrimaryRequestGeneration();
            auto request = runtime->service->openIdentity(requestedUsername.toStdString(),
                [&bridge, runtime, requestGeneration](application::read::model::IdentityState state) {
                    if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                        return;
                    }
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, runtime, requestGeneration, state = std::move(state)]() mutable {
                            if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                                return;
                            }
                            bridge.applyLookupState(
                                statusText(state.metadata), errorText(state.metadata), diagnosticText(state.metadata),
                                isBusy(state.metadata), state.metadata.isStale, state.metadata.isSourceUnavailable);
                            if (state.fid) {
                                bridge.openProfileFid(static_cast<qulonglong>(state.fid->value));
                            }
                        }, Qt::QueuedConnection);
                }, application::read::model::ReadOptions{
                    .allowRecoveryProbe = allowRecoveryProbe,
                });
            if (request) { runtime->keepPrimaryRequest(std::move(*request)); }
            else { bridge.applyLookupState({}, qtTrId("genycaster.runtime.the_read_worker_is_unavailable"), {}, false, false, true); }
        });

    QObject::connect(&bridge, &PresentationBridge::explorerArtRequested, &bridge,
        [&bridge, runtime] {
            constexpr std::array<std::string_view, 10> artChannels{
                "art", "photography", "basepaint", "design", "superrare",
                "nft", "cryptoart", "digital-art", "artists", "illustration"};
            constexpr std::size_t artPageLimit{12U};
            constexpr std::size_t artChannelCount{artChannels.size()};
            const std::uint64_t requestGeneration = runtime->beginExplorerArtRequestGeneration();
            auto completedMutex = std::make_shared<std::mutex>();
            auto completedChannels = std::make_shared<std::unordered_set<std::string>>();

            for (const std::string_view channelName : artChannels) {
                application::read::model::FeedQuery query{};
                query.kind = application::read::model::FeedQuery::Kind::Channel;
                query.channel = domain::channel::ChannelId{std::string{channelName}};
                query.limit = artPageLimit;
                const QString channelId = QString::fromUtf8(channelName.data(), static_cast<qsizetype>(channelName.size()));
                auto request = runtime->service->openFeed(
                    std::move(query),
                    [&bridge, runtime, requestGeneration, channelId,
                     completedMutex, completedChannels](application::read::model::FeedState state) {
                        if (!runtime->isCurrentExplorerArtRequestGeneration(requestGeneration)) {
                            return;
                        }
                        if (state.metadata.phase != application::read::model::ReadPhase::Ready) {
                            return;
                        }

                        QVariantList rows = feedPresentationRows(bridge, state, channelId);
                        bool allChannelsReady = false;
                        {
                            std::scoped_lock lock{*completedMutex};
                            completedChannels->insert(channelId.toStdString());
                            allChannelsReady = completedChannels->size() >= artChannelCount;
                        }
                        QMetaObject::invokeMethod(
                            &bridge,
                            [&bridge, runtime, requestGeneration, rows = std::move(rows), allChannelsReady]() mutable {
                                if (runtime->isCurrentExplorerArtRequestGeneration(requestGeneration)) {
                                    bridge.applyExplorerArtRows(std::move(rows), !allChannelsReady);
                                }
                            },
                            Qt::QueuedConnection);
                    },
                    application::read::model::ReadOptions{
                        .allowRecoveryProbe = false,
                        .intent = application::read::model::ReadIntent::Prefetch,
                    });
                if (request) {
                    runtime->keepExplorerArtRequest(std::move(*request));
                }
            }

            // A failed or unknown channel must not leave the supplemental loading
            // state stuck forever. Late successful reads may still merge afterward.
            QTimer::singleShot(10'000, &bridge, [&bridge, runtime, requestGeneration] {
                if (runtime->isCurrentExplorerArtRequestGeneration(requestGeneration)) {
                    bridge.applyExplorerArtRows({}, false);
                }
            });
        });

    QObject::connect(&bridge, &PresentationBridge::feedRequested, &bridge,
        [&bridge, runtime](const QString& feed, const qulonglong fid,
            const QString& channel, const QString& continuation,
            const bool allowRecoveryProbe) {
            const bool profileFeedRequest = feed.startsWith(QStringLiteral("user-"));
            if (profileFeedRequest) {
                runtime->cancelFeedRequest();
            } else {
                runtime->cancelAllRequests();
            }
            const std::uint64_t requestGeneration = runtime->beginFeedRequestGeneration();
            const bool continuationRequest = !continuation.isEmpty();
            if (!continuationRequest && bridge.isAuthenticated() && bridge.accountFid() > 0) {
                refreshViewerReactionSnapshot(
                    bridge, runtime,
                    domain::identity::Fid{static_cast<std::uint64_t>(bridge.accountFid())},
                    false);
            }
            application::read::model::FeedQuery query{};
            if (feed == QStringLiteral("following")) {
                query.kind = application::read::model::FeedQuery::Kind::Following;
            } else if (feed == QStringLiteral("channel")) {
                query.kind = application::read::model::FeedQuery::Kind::Channel;
            } else if (feed == QStringLiteral("user-posts")) {
                query.kind = application::read::model::FeedQuery::Kind::UserPosts;
            } else if (feed == QStringLiteral("user-quotes")) {
                query.kind = application::read::model::FeedQuery::Kind::UserQuotes;
            } else if (feed == QStringLiteral("user-replies")) {
                query.kind = application::read::model::FeedQuery::Kind::UserReplies;
            } else if (feed == QStringLiteral("user-likes")) {
                query.kind = application::read::model::FeedQuery::Kind::UserLikes;
            } else if (feed == QStringLiteral("user-popular")) {
                query.kind = application::read::model::FeedQuery::Kind::UserPopular;
            } else {
                query.kind = application::read::model::FeedQuery::Kind::Trending;
            }

            const bool needsFid = query.kind == application::read::model::FeedQuery::Kind::Following ||
                query.kind == application::read::model::FeedQuery::Kind::UserPosts ||
                query.kind == application::read::model::FeedQuery::Kind::UserQuotes ||
                query.kind == application::read::model::FeedQuery::Kind::UserReplies ||
                query.kind == application::read::model::FeedQuery::Kind::UserLikes ||
                query.kind == application::read::model::FeedQuery::Kind::UserPopular;
            if (needsFid) {
                query.fid = domain::identity::Fid{static_cast<std::uint64_t>(fid)};
            }
            if (query.kind == application::read::model::FeedQuery::Kind::Channel) {
                query.channel = domain::channel::ChannelId{channel.toStdString()};
            }
            // User-tunable read budget. The canonical provider contract validates
            // the same 1..100 range before any request leaves the presentation boundary.
            query.limit = static_cast<std::size_t>(bridge.feedPageSize());
            if (continuationRequest) {
                query.continuation = capability::pagination::ContinuationToken{continuation.toStdString()};
            }
            auto request = runtime->service->openFeed(std::move(query),
                [&bridge, runtime, requestGeneration](application::read::model::FeedState state) {
                    if (!runtime->isCurrentFeedRequestGeneration(requestGeneration)) {
                        return;
                    }
                    constexpr std::size_t maximumContextHydrationsPerFeedState{24U};
                    std::vector<domain::cast::CastId> missingReferences{};
                    if (state.metadata.phase == application::read::model::ReadPhase::Ready) {
                        missingReferences = missingReferencedCastIds(
                            state, std::min(
                                static_cast<std::size_t>(bridge.contextHydrationLimit()),
                                maximumContextHydrationsPerFeedState));
                    }
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
                    if (const auto session = runtime->currentSessionSnapshot();
                        session && session->fid.value != 0 && !state.casts.empty()
                        && state.metadata.phase == application::read::model::ReadPhase::Ready) {
                        std::vector<domain::cast::CastReference> visibleTargets{};
                        // Hydrate viewer reaction state for the complete current page
                        // whenever the provider batch contract allows it. Limiting this to the
                        // first 12 casts made liked/recast state disappear on later rows and on
                        // sparse Explorer projections (Devs/NFTs often select rows well past 12).
                        constexpr std::size_t maximumVisibleReactionHydrations{64U};
                        const std::size_t visibleReactionLimit = std::min<std::size_t>(
                            {state.casts.size(), static_cast<std::size_t>(bridge.feedPageSize()),
                             maximumVisibleReactionHydrations});
                        visibleTargets.reserve(visibleReactionLimit);
                        for (const auto& cast : state.casts) {
                            if (cast.id.value.empty()) {
                                continue;
                            }
                            visibleTargets.push_back(domain::cast::CastReference{
                                .author = cast.author,
                                .id = cast.id,
                            });
                            if (visibleTargets.size() >= visibleReactionLimit) {
                                break;
                            }
                        }
                        refreshVisibleViewerReactionState(
                            bridge, runtime, session->fid, std::move(visibleTargets));
                    }
#endif

                    QMetaObject::invokeMethod(
                        &bridge,
                        [&bridge, runtime, requestGeneration, state = std::move(state)]() mutable {
                            if (runtime->isCurrentFeedRequestGeneration(requestGeneration)) {
                                applyFeedState(bridge, std::move(state));
                            }
                        },
                        Qt::QueuedConnection);

                    for (const auto& referencedId : missingReferences) {
                        if (!runtime->beginReferencedPreviewRequest(referencedId)) {
                            continue;
                        }
                        auto previewRequest = runtime->service->openCast(
                            referencedId,
                            [&bridge, runtime, referencedId, requestGeneration](
                                application::read::model::CastState referencedState) {
                                const auto phase = referencedState.metadata.phase;
                                const QString castId = referencedState.cast
                                    ? QString::fromStdString(referencedState.cast->id.value) : QString{};
                                const QVariantMap preview = castPreviewPresentation(referencedState);
                                const bool resolved = !castId.isEmpty() && !preview.isEmpty();
                                if (!resolved) {
                                    // Refreshing/LoadingCache are intermediate
                                    // local-first snapshots, not failed reads.
                                    // Keeping the request alive is what allows
                                    // an uncached parent cast to arrive from the
                                    // provider and replace the loading affordance.
                                    if (phase == application::read::model::ReadPhase::Ready
                                        || phase == application::read::model::ReadPhase::Failed) {
                                        runtime->finishReferencedPreviewRequest(referencedId, false);
                                    }
                                    return;
                                }
                                runtime->finishReferencedPreviewRequest(referencedId, true);
                                if (!runtime->isCurrentFeedRequestGeneration(requestGeneration)) {
                                    return;
                                }
                                QMetaObject::invokeMethod(
                                    &bridge,
                                    [&bridge, runtime, requestGeneration, castId, preview] {
                                        if (runtime->isCurrentFeedRequestGeneration(requestGeneration)) {
                                            bridge.applyReferencedCastPreview(castId, preview);
                                        }
                                    },
                                    Qt::QueuedConnection);
                            },
                            application::read::model::ReadOptions{
                                .allowRecoveryProbe = false,
                                .intent = application::read::model::ReadIntent::Prefetch,
                            });
                        if (!previewRequest) {
                            runtime->finishReferencedPreviewRequest(referencedId, false);
                        } else {
                            runtime->keepReferencedPreviewRequest(
                                referencedId, std::move(*previewRequest));
                        }
                    }
                }, application::read::model::ReadOptions{
                    .allowRecoveryProbe = allowRecoveryProbe && !continuationRequest,
                    .intent = continuationRequest
                        ? application::read::model::ReadIntent::Prefetch
                        : application::read::model::ReadIntent::UserVisible,
                });
            if (request) { runtime->keepFeedRequest(std::move(*request)); }
            else { bridge.applyRuntimeState(false, qtTrId("genycaster.runtime.the_read_worker_is_unavailable")); }
        });

    QObject::connect(&bridge, &PresentationBridge::notificationsRequested, &bridge,
        [&bridge, runtime](const QString& continuation, const bool allowRecoveryProbe) {
            if (!bridge.isAuthenticated() || bridge.accountFid() == 0) {
                bridge.applyNotifications({}, {}, qtTrId("genycaster.notifications.sign_in_required"), false, false);
                return;
            }
            runtime->cancelPrimaryRequest();
            application::read::model::NotificationQuery query{
                .viewer = domain::identity::Fid{static_cast<std::uint64_t>(bridge.accountFid())},
                .limit = 30,
            };
            const bool append = !continuation.isEmpty();
            if (append) {
                query.continuation = capability::pagination::ContinuationToken{continuation.toStdString()};
            }
            auto request = runtime->service->openNotifications(
                std::move(query),
                [&bridge, append](application::read::model::NotificationState state) {
                    QMetaObject::invokeMethod(&bridge, [&bridge, append, state = std::move(state)]() mutable {
                        applyNotificationState(bridge, std::move(state), append);
                    }, Qt::QueuedConnection);
                },
                application::read::model::ReadOptions{.allowRecoveryProbe = allowRecoveryProbe});
            if (request) {
                runtime->keepPrimaryRequest(std::move(*request));
            } else {
                bridge.applyNotifications(
                    {}, {}, qtTrId("genycaster.runtime.the_read_worker_is_unavailable"), false, append);
            }
        });

    QObject::connect(&bridge, &PresentationBridge::notificationBadgeRequested, &bridge,
        [&bridge, runtime](const bool allowRecoveryProbe) {
            if (!bridge.isAuthenticated() || bridge.accountFid() == 0) {
                bridge.applyNotificationBadge({}, false, false);
                return;
            }
            application::read::model::NotificationQuery query{
                .viewer = domain::identity::Fid{static_cast<std::uint64_t>(bridge.accountFid())},
                .limit = 100,
            };
            auto request = runtime->service->openNotifications(
                std::move(query),
                [&bridge](application::read::model::NotificationState state) {
                    QMetaObject::invokeMethod(&bridge, [&bridge, state = std::move(state)]() mutable {
                        applyNotificationBadgeState(bridge, std::move(state));
                    }, Qt::QueuedConnection);
                },
                application::read::model::ReadOptions{.allowRecoveryProbe = allowRecoveryProbe});
            if (request) {
                runtime->keepNotificationBadgeRequest(std::move(*request));
            } else {
                bridge.applyNotificationBadge({}, false, false);
            }
        });

    QObject::connect(&bridge, &PresentationBridge::castRequested, &bridge,
        [&bridge, runtime](const QString& castId, const bool allowRecoveryProbe) {
            runtime->cancelAllRequests();
            const std::uint64_t requestGeneration = runtime->beginPrimaryRequestGeneration();
            if (const auto session = runtime->currentSessionSnapshot()) {
                refreshViewerReactionSnapshot(bridge, runtime, session->fid, false);
            }
            auto request = runtime->service->openCast(domain::cast::CastId{castId.toStdString()},
                [&bridge, runtime, requestGeneration](application::read::model::CastState state) {
                    if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                        return;
                    }
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
                    if (state.cast) {
                        if (const auto session = runtime->currentSessionSnapshot();
                            session && session->fid.value != 0) {
                            refreshVisibleViewerReactionState(
                                bridge,
                                runtime,
                                session->fid,
                                {domain::cast::CastReference{
                                    .author = state.cast->author,
                                    .id = state.cast->id,
                                }});
                        }
                    }
#endif
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, runtime, requestGeneration, state = std::move(state)]() mutable {
                        if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                            return;
                        }
                        QVariantMap detail{{QStringLiteral("castId"), QString::fromStdString(state.castId.value)}};
                        if (state.cast) {
                            detail.insert(QStringLiteral("authorFid"), static_cast<qulonglong>(state.cast->author.value));
                            detail.insert(QStringLiteral("text"), QString::fromStdString(state.cast->text));
                            detail.insert(QStringLiteral("createdAtMs"),
                                static_cast<qlonglong>(state.cast->createdAt.time_since_epoch().count()));
                            detail.insert(QStringLiteral("embedUrl"), firstUrlEmbed(*state.cast));
                            detail.insert(QStringLiteral("embeds"), embedItems(*state.cast));
                            detail.insert(QStringLiteral("replyCount"), static_cast<qulonglong>(state.cast->engagement.replies));
                            detail.insert(QStringLiteral("recastCount"), static_cast<qulonglong>(state.cast->engagement.recasts));
                            detail.insert(QStringLiteral("likeCount"), static_cast<qulonglong>(state.cast->engagement.likes));
                            detail.insert(QStringLiteral("parentCastId"), state.cast->parent
                                ? QString::fromStdString(state.cast->parent->value) : QString{});
                            detail.insert(QStringLiteral("parentUrl"), state.cast->parentUrl
                                ? QString::fromStdString(*state.cast->parentUrl) : QString{});
                            detail.insert(QStringLiteral("threadReplies"), threadReplyItems(*state.cast));
                        }
                        if (state.author) {
                            detail.insert(QStringLiteral("author"), displayName(&*state.author));
                            detail.insert(QStringLiteral("username"), username(&*state.author));
                            detail.insert(QStringLiteral("avatarUrl"), avatarUrl(&*state.author));
                        }
                        bridge.applyDetail(QStringLiteral("cast"), std::move(detail),
                            statusText(state.metadata), errorText(state.metadata), diagnosticText(state.metadata), isBusy(state.metadata),
                            state.metadata.isStale, state.metadata.isSourceUnavailable);
                    }, Qt::QueuedConnection);
                }, application::read::model::ReadOptions{
                    .allowRecoveryProbe = allowRecoveryProbe,
                });
            if (request) { runtime->keepPrimaryRequest(std::move(*request)); }
            else { bridge.applyRuntimeState(false, qtTrId("genycaster.runtime.the_read_worker_is_unavailable")); }
        });

    QObject::connect(&bridge, &PresentationBridge::profileRequested, &bridge,
        [&bridge, runtime](const qulonglong fid, const bool allowRecoveryProbe) {
            runtime->cancelPrimaryRequest();
            const std::uint64_t requestGeneration = runtime->beginPrimaryRequestGeneration();
            auto request = runtime->service->openProfile(
                domain::identity::Fid{static_cast<std::uint64_t>(fid)},
                [&bridge, runtime, requestGeneration](application::read::model::ProfileState state) {
                    if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                        return;
                    }
                    std::optional<bool> followingByMe{};
                    std::optional<domain::identity::Fid> accountFid{};
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
                    if (state.profile && state.metadata.phase == application::read::model::ReadPhase::Ready) {
                        if (const auto session = runtime->currentSessionSnapshot();
                            session && session->fid.value != 0 && session->fid.value != state.profile->fid.value) {
                            accountFid = session->fid;
                        }
                    }
                    if (state.profile && accountFid) {
                        refreshProfileFollowsMe(bridge, runtime, *accountFid, state.profile->fid);
                    }
#endif
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, runtime, requestGeneration, state = std::move(state), followingByMe]() mutable {
                        if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                            return;
                        }
                        QVariantMap detail = profilePresentation(state.profile ? &*state.profile : nullptr);
                        if (state.profile && followingByMe) {
                            detail.insert(QStringLiteral("followingByMe"), *followingByMe);
                            detail.insert(QStringLiteral("followingKnown"), true);
                        }
                        bridge.applyDetail(QStringLiteral("profile"), std::move(detail),
                            statusText(state.metadata), errorText(state.metadata), diagnosticText(state.metadata), isBusy(state.metadata),
                            state.metadata.isStale, state.metadata.isSourceUnavailable);
                    }, Qt::QueuedConnection);
                }, application::read::model::ReadOptions{
                    .allowRecoveryProbe = allowRecoveryProbe,
                });
            if (request) { runtime->keepPrimaryRequest(std::move(*request)); }
            else { bridge.applyRuntimeState(false, qtTrId("genycaster.runtime.the_read_worker_is_unavailable")); }
        });

    QObject::connect(&bridge, &PresentationBridge::socialRequested, &bridge,
        [&bridge, runtime](const QString& kind, const qulonglong fid,
            const QString& continuation, const bool allowRecoveryProbe) {
            runtime->cancelAllRequests();
            application::read::model::SocialQuery query{
                .kind = kind == QStringLiteral("followers")
                    ? application::read::model::SocialQuery::Kind::Followers
                    : application::read::model::SocialQuery::Kind::Following,
                .fid = domain::identity::Fid{static_cast<std::uint64_t>(fid)},
            };
            if (!continuation.isEmpty()) {
                query.continuation = capability::pagination::ContinuationToken{continuation.toStdString()};
            }
            auto request = runtime->service->openSocial(std::move(query),
                [&bridge](application::read::model::SocialState state) {
                    QMetaObject::invokeMethod(&bridge, [&bridge, state = std::move(state)]() mutable {
                        applySocialState(bridge, std::move(state));
                    }, Qt::QueuedConnection);
                }, application::read::model::ReadOptions{
                    .allowRecoveryProbe = allowRecoveryProbe,
                });
            if (request) { runtime->keepPrimaryRequest(std::move(*request)); }
            else { bridge.applyRuntimeState(false, qtTrId("genycaster.runtime.the_read_worker_is_unavailable")); }
        });
}

#if FARCASTER_HAS_MEDIA_UPLOAD
[[nodiscard]] std::filesystem::path nativeMediaPath(const QString& localPath)
{
#if defined(Q_OS_WIN)
    return std::filesystem::path{localPath.toStdWString()};
#else
    const QByteArray encoded = QFile::encodeName(localPath);
    return std::filesystem::path{std::string{encoded.constData(), static_cast<std::size_t>(encoded.size())}};
#endif
}

[[nodiscard]] QString mediaPhaseToken(const application::media::upload::UploadPhase phase)
{
    using application::media::upload::UploadPhase;
    switch (phase) {
    case UploadPhase::Queued: return QStringLiteral("queued");
    case UploadPhase::Preparing: return QStringLiteral("preparing");
    case UploadPhase::Authorizing: return QStringLiteral("authorizing");
    case UploadPhase::Uploading: return QStringLiteral("uploading");
    case UploadPhase::Verifying: return QStringLiteral("verifying");
    case UploadPhase::Completed: return QStringLiteral("completed");
    case UploadPhase::Cancelled: return QStringLiteral("cancelled");
    case UploadPhase::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("preparing");
}

void connectMediaUpload(
    MediaUploadController& controller,
    const std::shared_ptr<Runtime>& runtime)
{
    if (!runtime->mediaUploadService) {
        controller.setAvailable(false);
        return;
    }
    controller.setAvailable(true);
    const QPointer<MediaUploadController> controllerGuard{&controller};

    QObject::connect(&controller, &MediaUploadController::uploadRequested, &controller,
        [runtime, controllerGuard](const QString& localPath, const int) {
            if (!controllerGuard || !runtime->mediaUploadService) {
                return;
            }
            auto started = runtime->mediaUploadService->start(
                runtime->currentSessionSnapshot(),
                nativeMediaPath(localPath),
                [runtime, controllerGuard](application::media::upload::UploadState state) mutable {
                    if (state.phase == application::media::upload::UploadPhase::Failed) {
#ifndef NDEBUG
                        if (state.error) {
                            qInfo().noquote() << "Genycaster media upload failure:"
                                              << QString::fromStdString(state.error->message);
                        }
#endif
                        if (controllerGuard) {
                            QMetaObject::invokeMethod(
                                controllerGuard.data(),
                                [controllerGuard] {
                                    if (controllerGuard) {
                                        controllerGuard->applyFailed(
                                            qtTrId("genycaster.media.upload_failed"));
                                    }
                                },
                                Qt::QueuedConnection);
                        }
                        return;
                    }
                    if (state.phase == application::media::upload::UploadPhase::Cancelled) {
                        if (controllerGuard) {
                            QMetaObject::invokeMethod(
                                controllerGuard.data(),
                                [controllerGuard] {
                                    if (controllerGuard) { controllerGuard->applyCancelled(); }
                                },
                                Qt::QueuedConnection);
                        }
                        return;
                    }
                    if (state.phase == application::media::upload::UploadPhase::Completed && state.asset) {
                        const auto asset = std::move(*state.asset);
                        const QString assetUrl = QString::fromStdString(asset.url);
                        const QString contentType = QString::fromStdString(asset.contentType);
                        const QString fileName = QString::fromStdString(asset.fileName);
                        const QString managementToken = QString::fromUtf8(
                            state.managementToken.data(),
                            static_cast<qsizetype>(state.managementToken.size()));
                        const qulonglong sizeBytes = static_cast<qulonglong>(asset.sizeBytes);
                        if (controllerGuard) {
                            QMetaObject::invokeMethod(
                                controllerGuard.data(),
                                [controllerGuard, assetUrl, contentType, fileName, managementToken, sizeBytes] {
                                    if (controllerGuard) {
                                        controllerGuard->applyCompleted(
                                            assetUrl, contentType, fileName, managementToken, sizeBytes);
                                    }
                                },
                                Qt::QueuedConnection);
                        }
                        return;
                    }
                    const QString phase = mediaPhaseToken(state.phase);
                    const qulonglong transferred = static_cast<qulonglong>(state.transferred);
                    const qulonglong total = static_cast<qulonglong>(state.total);
                    if (controllerGuard) {
                        QMetaObject::invokeMethod(
                            controllerGuard.data(),
                            [controllerGuard, phase, transferred, total] {
                                if (controllerGuard) {
                                    controllerGuard->applyProgress(phase, transferred, total);
                                }
                            },
                            Qt::QueuedConnection);
                    }
                });
            if (!started) {
#ifndef NDEBUG
                qInfo().noquote() << "Genycaster media upload could not start:"
                                  << QString::fromStdString(started.error().message);
#endif
                controllerGuard->applyFailed(qtTrId("genycaster.media.upload_failed"));
                return;
            }
            std::scoped_lock lock{runtime->mediaUploadMutex};
            runtime->activeMediaUpload = std::move(*started);
        });

    QObject::connect(&controller, &MediaUploadController::cancelRequested, &controller,
        [runtime] {
            std::scoped_lock lock{runtime->mediaUploadMutex};
            if (runtime->activeMediaUpload) {
                runtime->activeMediaUpload->cancel();
            }
        });
}
#endif

void connectMediaPlayback(
    MediaPlaybackCoordinator& coordinator,
    const std::shared_ptr<Runtime>& runtime)
{
    const QPointer<MediaPlaybackCoordinator> coordinatorGuard{&coordinator};
    const std::weak_ptr<Runtime> runtimeGuard{runtime};
    coordinator.setSourcePreparationBackend({
        .start = [coordinatorGuard, runtimeGuard](
            const qulonglong requestId,
            const QString& source) {
            const auto dispatch = [coordinatorGuard, requestId, source](const bool available) {
                MediaPlaybackCoordinator* const target = coordinatorGuard.data();
                if (target == nullptr) {
                    return;
                }
                const QString errorText = available
                    ? QString{} : qtTrId("genycaster.player.video_unavailable");
                QMetaObject::invokeMethod(
                    target,
                    [coordinatorGuard, requestId, source, available, errorText] {
                        if (coordinatorGuard) {
                            coordinatorGuard->completeSourcePreparation(
                                requestId, source, available, errorText);
                        }
                    },
                    Qt::QueuedConnection);
            };

            const auto activeRuntime = runtimeGuard.lock();
            if (!activeRuntime || !activeRuntime->asyncTransport) {
                dispatch(false);
                return;
            }
            farcaster::transport::async_http::Request request{
                .method = farcaster::transport::async_http::Method::Head,
                .url = source.toUtf8().toStdString(),
                .timeout = std::chrono::milliseconds{8'000},
                .maxBodyBytes = 1'024U,
                .priority = farcaster::transport::async_http::Priority::Interactive,
                .retryPolicy = farcaster::transport::async_http::RetryPolicy::None,
                .maximumAttempts = 1U,
            };
            auto started = activeRuntime->asyncTransport->start(
                std::move(request),
                [dispatch](foundation::result::Result<
                    farcaster::transport::http::HttpResponse> response) {
                    // Any HTTP response proves that DNS/proxy/TLS/redirect
                    // setup completed. QMediaPlayer remains responsible for
                    // interpreting the resource status and media payload.
                    dispatch(response.has_value());
                });
            if (!started) {
                dispatch(false);
            }
        },
    });
}

[[nodiscard]] QString optionValue(const QStringList& arguments, const QString& prefix)
{
    for (const QString& argument : arguments) {
        if (argument.startsWith(prefix)) {
            return argument.sliced(prefix.size()).trimmed();
        }
    }
    return {};
}


[[nodiscard]] QString startupRenderingProfile(const int argc, char* argv[])
{
    QString explicitProfile{};
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        constexpr auto prefix = "--render-profile=";
        if (argument.startsWith(QString::fromLatin1(prefix))) {
            explicitProfile = argument.mid(static_cast<qsizetype>(std::char_traits<char>::length(prefix)));
            break;
        }
    }
    if (explicitProfile.isEmpty()) {
        explicitProfile = qEnvironmentVariable("GENYCASTER_RENDER_PROFILE").trimmed();
    }
    if (!explicitProfile.isEmpty()) {
        const QString canonical = RenderingController::canonicalProfile(explicitProfile);
        return canonical.isEmpty() ? QStringLiteral("adaptive") : canonical;
    }

    QSettings startupSettings{
        QSettings::NativeFormat,
        QSettings::UserScope,
        QStringLiteral("Genycaster"),
        QStringLiteral("Genycaster")};
    return RenderingController::canonicalProfile(
        startupSettings.value(QStringLiteral("rendering/profile"), QStringLiteral("adaptive")).toString());
}

[[nodiscard]] QString normalizeConfigurationToken(QString value)
{
    value = value.trimmed();
    for (int pass = 0; pass < 2 && value.size() >= 2; ++pass) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('\"') && last == QLatin1Char('\"')) ||
            (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            value = value.sliced(1, value.size() - 2).trimmed();
            continue;
        }
        if (value.startsWith(QStringLiteral("\\\"")) &&
            value.endsWith(QStringLiteral("\\\"")) && value.size() >= 4) {
            value = value.sliced(2, value.size() - 4).trimmed();
            continue;
        }
        break;
    }
    return value;
}

[[nodiscard]] bool hasUnsafeConfigurationCharacters(const QStringView value)
{
    for (const QChar ch : value) {
        if (ch.isSpace() || ch == QLatin1Char('\\') || ch == QLatin1Char('\"') ||
            ch == QLatin1Char('\'')) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isValidHttpServiceUrl(
    const QString& value,
    const bool allowQuery = false)
{
    if (value.isEmpty() || hasUnsafeConfigurationCharacters(value)) {
        return false;
    }

    const QUrl url{value, QUrl::StrictMode};
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() ||
        (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) ||
        url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment()) {
        return false;
    }
    return allowQuery || !url.hasQuery();
}

[[nodiscard]] bool isValidSiwfUri(const QString& value)
{
    return isValidHttpServiceUrl(value, true);
}

[[nodiscard]] bool isValidSiwfDomain(const QString& value)
{
    if (value.isEmpty() || hasUnsafeConfigurationCharacters(value) ||
        value.contains(QLatin1Char('/')) || value.contains(QLatin1Char('?')) ||
        value.contains(QLatin1Char('#'))) {
        return false;
    }
    const QUrl authority{QStringLiteral("http://") + value, QUrl::StrictMode};
    return authority.isValid() && !authority.host().isEmpty() && authority.userInfo().isEmpty();
}

[[nodiscard]] bool siwfDomainMatchesUri(const QString& domain, const QString& uri)
{
    const QUrl authority{QStringLiteral("http://") + domain, QUrl::StrictMode};
    const QUrl relyingParty{uri, QUrl::StrictMode};
    if (!authority.isValid() || !relyingParty.isValid()) {
        return false;
    }
    if (authority.host().compare(relyingParty.host(), Qt::CaseInsensitive) != 0) {
        return false;
    }
    return authority.port(-1) == relyingParty.port(-1);
}

[[nodiscard]] std::function<foundation::result::Status(const std::string&)>
makeWalletApprovalLauncher(QString launchTemplate, const bool preferCustodyWalletBrowser = false)
{
    return [launchTemplate = std::move(launchTemplate), preferCustodyWalletBrowser](const std::string& approvalUrl)
        -> foundation::result::Status {
        const QString rawUrl = QString::fromStdString(approvalUrl);
        QString target = rawUrl;
        if (!launchTemplate.isEmpty()) {
            if (!launchTemplate.contains(QStringLiteral("{url}")) &&
                !launchTemplate.contains(QStringLiteral("{rawUrl}"))) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::InvalidArgument,
                    "Wallet launch URL template must contain {url} or {rawUrl}."));
            }
            target = launchTemplate;
            target.replace(
                QStringLiteral("{url}"),
                QString::fromLatin1(QUrl::toPercentEncoding(rawUrl)));
            target.replace(QStringLiteral("{rawUrl}"), rawUrl);
        }

        const QUrl parsed{target};
        const QString scheme = parsed.scheme().toLower();
        if (!parsed.isValid() || scheme.isEmpty() ||
            scheme == QStringLiteral("javascript") ||
            scheme == QStringLiteral("data") ||
            scheme == QStringLiteral("file")) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Wallet approval launcher produced an invalid or unsafe URL."));
        }

        bool opened{false};
        const bool preferInstalledWalletBrowser = preferCustodyWalletBrowser && launchTemplate.isEmpty()
            && parsed.scheme() == QStringLiteral("http")
            && parsed.host() == QStringLiteral("127.0.0.1");
        const auto open = [&opened, parsed, preferInstalledWalletBrowser] {
#if defined(Q_OS_MACOS)
            if (preferInstalledWalletBrowser) {
                // Safari has no injected EIP-1193 wallet by default. An
                // installed wallet-capable browser avoids forcing users to
                // hand-copy a time-limited loopback URL on every attempt.
                for (const QString& appName : {
                         QStringLiteral("Brave Browser"),
                         QStringLiteral("Google Chrome")}) {
                    const QString bundle = appName + QStringLiteral(".app");
                    if (QFileInfo::exists(QStringLiteral("/Applications/") + bundle)
                        || QFileInfo::exists(QDir::homePath()
                                            + QStringLiteral("/Applications/") + bundle)) {
                        opened = QProcess::startDetached(
                            QStringLiteral("/usr/bin/open"),
                            {QStringLiteral("-a"), appName, parsed.toString()});
                        if (opened) break;
                    }
                }
            }
#else
            Q_UNUSED(preferInstalledWalletBrowser);
#endif
            if (!opened) opened = QDesktopServices::openUrl(parsed);
        };
        if (auto* app = QCoreApplication::instance(); app != nullptr &&
            QThread::currentThread() != app->thread()) {
            if (!QMetaObject::invokeMethod(app, open, Qt::BlockingQueuedConnection)) {
                return std::unexpected(foundation::error::Error::make(
                    foundation::error::ErrorCode::Unavailable,
                    "Could not dispatch wallet approval URL opening to the Qt application thread."));
            }
        } else {
            open();
        }
        if (!opened) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "The platform could not open a browser or wallet for Mini App approval."));
        }
        return {};
    };
}

#if FARCASTER_HAS_MINIAPP_EVM_WALLET
[[nodiscard]] std::vector<std::string> miniAppEthereumChains()
{
    // Mirrors the EVM CAIP-2 chain set accepted by the current Farcaster Mini App manifest schema.
    const std::array<std::string_view, 16> allowed{
        "eip155:1", "eip155:8453", "eip155:42161", "eip155:421614",
        "eip155:84532", "eip155:666666666", "eip155:100", "eip155:10",
        "eip155:11155420", "eip155:137", "eip155:11155111", "eip155:7777777",
        "eip155:130", "eip155:10143", "eip155:42220", "eip155:999",
    };
    const QString configured = qEnvironmentVariable("GENYCASTER_MINIAPP_EVM_CHAINS").trimmed();
    if (configured.isEmpty()) {
        return {allowed.begin(), allowed.end()};
    }
    std::vector<std::string> selected{};
    for (const QString& token : configured.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const std::string chain = token.trimmed().toStdString();
        if (std::ranges::find(allowed, std::string_view{chain}) == allowed.end()) {
            qWarning().noquote() << "Ignoring unsupported GENYCASTER_MINIAPP_EVM_CHAINS entry:"
                                 << QString::fromStdString(chain);
            continue;
        }
        if (std::ranges::find(selected, chain) == selected.end()) selected.push_back(chain);
    }
    return selected;
}
#endif

}

int main(int argc, char* argv[])
{
    // Adaptive is the product default. Qt Quick's standard scene graph keeps
    // its default vertex-AA path, while Genycaster's curved design primitives
    // use Shape.CurveRenderer for native high-quality curve AA without MSAA.
    // Fidelity remains an explicit 4x choice because window-wide multisampling
    // has hardware-dependent GPU and memory cost.
    const QString startupRenderProfile = startupRenderingProfile(argc, argv);
    QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setSamples(RenderingController::sampleCountForProfile(startupRenderProfile));
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Genycaster"));
    QCoreApplication::setApplicationName(QStringLiteral("Genycaster"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(GENYCASTER_APP_VERSION));
    QQuickStyle::setStyle(QStringLiteral(FARCASTER_QT_QUICK_CONTROLS_STYLE));
    QString bundledPersianFontFamily{};
    QString bundledDevanagariFontFamily{};
    QString bundledJapaneseFontFamily{};
    QString bundledKoreanFontFamily{};
    QString bundledChineseFontFamily{};
    QString bundledIconFontFamily{};
    QString bundledIconRegularFontFamily{};

    // The minimal QPA backend used by headless structure tests intentionally has
    // no application-font backend. Shipping platforms load every bundled family;
    // a missing resource is a packaging defect rather than a system-font fallback.
    if (QGuiApplication::platformName() != QStringLiteral("minimal")) {
        const auto loadApplicationFont = [](const QString& resource) -> QStringList {
            const int fontId = QFontDatabase::addApplicationFont(resource);
            if (fontId < 0) {
                qCritical().noquote() << "Unable to load embedded Genycaster font:" << resource;
                return {};
            }
            return QFontDatabase::applicationFontFamilies(fontId);
        };

        constexpr std::array bundledPersianFontResources{
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Regular.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Medium.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-SemiBold.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Bold.ttf",
            ":/qt/qml/Farcaster/Client/assets/fonts/Vazirmatn-Black.ttf",
        };
        for (const char* const resource : bundledPersianFontResources) {
            const QStringList families = loadApplicationFont(QString::fromLatin1(resource));
            if (families.isEmpty()) {
                return 2;
            }
            if (bundledPersianFontFamily.isEmpty()) {
                bundledPersianFontFamily = families.front();
            }
        }

#if defined(__EMSCRIPTEN__)
        // Qt/WASM does not inherit the browser's native emoji font fallback.
        // Load our verified Unicode fallback before creating any QML text.
        if (loadApplicationFont(QStringLiteral(
                ":/qt/qml/Farcaster/Client/assets/fonts/NotoColorEmoji-wasm.ttf")).isEmpty()) {
            return 2;
        }
#endif

        // Devanagari/CJK resources are optional in source/developer builds.
        // Production packaging can require them through
        // GENYCASTER_REQUIRE_SCRIPT_FONTS; when absent, the platform fallback
        // families selected below provide shaping without making configure depend
        // on network access.
        const QString devanagariResource =
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/fonts/NotoSansDevanagari.ttf");
        if (QFile::exists(devanagariResource)) {
            const QStringList devanagariFamilies = loadApplicationFont(devanagariResource);
            if (!devanagariFamilies.isEmpty()) {
                bundledDevanagariFontFamily = devanagariFamilies.front();
            }
        }

        const QString cjkResource =
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/fonts/NotoSansCJK-Regular.ttc");
        if (QFile::exists(cjkResource)) {
            const QStringList cjkFamilies = loadApplicationFont(cjkResource);
            if (!cjkFamilies.isEmpty()) {
                const auto chooseCjkFamily = [&cjkFamilies](const QString& suffix) {
                    const auto exact = std::ranges::find_if(cjkFamilies, [&](const QString& family) {
                        return family.contains(suffix, Qt::CaseInsensitive);
                    });
                    return exact != cjkFamilies.end() ? *exact : cjkFamilies.front();
                };
                bundledJapaneseFontFamily = chooseCjkFamily(QStringLiteral("JP"));
                bundledKoreanFontFamily = chooseCjkFamily(QStringLiteral("KR"));
                bundledChineseFontFamily = chooseCjkFamily(QStringLiteral("SC"));
            }
        }

        const QStringList iconRegularFamilies = loadApplicationFont(
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/fa-regular-400.ttf"));
        const QStringList iconSolidFamilies = loadApplicationFont(
            QStringLiteral(":/qt/qml/Farcaster/Client/assets/icons/fa-solid-900.ttf"));
        if (iconRegularFamilies.isEmpty() || iconSolidFamilies.isEmpty()) {
            return 2;
        }
        bundledIconRegularFontFamily = iconRegularFamilies.front();
        // Keep the legacy property populated for compatibility with older QML;
        // new Icon.qml selects the regular or solid family explicitly.
        bundledIconFontFamily = iconSolidFamilies.front();
    }

    const auto installedFamilies = QFontDatabase::families();
    const auto preferredFamily = [&installedFamilies](const QStringList& candidates, const QString& fallback) {
        for (const auto& candidate : candidates) {
            if (installedFamilies.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
        return fallback;
    };
    const QString systemFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#if defined(Q_OS_MACOS)
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("SF Pro Text"), QStringLiteral("Helvetica Neue")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Geeza Pro"), QStringLiteral("Arial Unicode MS"), QStringLiteral("Arial")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Kohinoor Devanagari"), QStringLiteral("Devanagari Sangam MN")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Hiragino Sans"), QStringLiteral("YuGothic")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Apple SD Gothic Neo"), QStringLiteral("Arial Unicode MS")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("PingFang SC"), QStringLiteral("Hiragino Sans GB")}, systemFamily);
#elif defined(Q_OS_WIN)
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Segoe UI"), QStringLiteral("Tahoma"), QStringLiteral("Arial")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Nirmala UI"), QStringLiteral("Mangal")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Yu Gothic UI"), QStringLiteral("Meiryo UI")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Malgun Gothic")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft JhengHei UI")}, systemFamily);
#else
    const QString platformTextFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")}, systemFamily);
    const QString platformRtlFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans Arabic"), QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")}, systemFamily);
    const QString platformDevanagariFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformJapaneseFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK JP"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformKoreanFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK KR"), QStringLiteral("Noto Sans")}, systemFamily);
    const QString platformChineseFontFamily = preferredFamily(
        {QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Noto Sans")}, systemFamily);
#endif

    application.setFont(QFont{platformTextFontFamily});

    const QStringList arguments = QCoreApplication::arguments();
    const bool isSmokeTest = arguments.contains(QStringLiteral("--smoke-test"));
    const bool isVisualTest = arguments.contains(QStringLiteral("--visual-test"));
    const bool isRuntimeSmokeTest = arguments.contains(QStringLiteral("--runtime-smoke-test"));
    const bool openExploreForTest = arguments.contains(QStringLiteral("--open-explore"));
    const qsizetype snapshotIndex = arguments.indexOf(QStringLiteral("--snapshot"));
    const QString snapshotPath = snapshotIndex >= 0 && snapshotIndex + 1 < arguments.size()
        ? arguments[snapshotIndex + 1] : QString{};

    const QString settingsPath = optionValue(arguments, QStringLiteral("--settings-path="));
#if defined(__EMSCRIPTEN__)
    // QSettings NativeFormat writes under ~/.config, which is not mounted
    // by IDBFS. Keep only non-secret preferences/consent in the durable
    // virtual user-data directory. The preRun hook restores it before main().
    auto settingsStorage = std::make_unique<QSettings>(
        settingsPath.isEmpty()
            ? QStringLiteral("/home/web_user/.local/share/genycaster-settings.ini")
            : settingsPath,
        QSettings::IniFormat);
#else
    auto settingsStorage = settingsPath.isEmpty()
        ? std::make_unique<QSettings>()
        : std::make_unique<QSettings>(settingsPath, QSettings::IniFormat);
#endif
    QSettings& settings = *settingsStorage;

    const auto deploymentEndpoints =
        application::configuration::configuredDeploymentEndpoints();
    const auto deploymentIdentity =
        application::configuration::configuredDeploymentIdentity();

    QString networkProxyText = optionValue(arguments, QStringLiteral("--network-proxy="));
    if (networkProxyText.isEmpty()) {
        networkProxyText = qEnvironmentVariable("GENYCASTER_NETWORK_PROXY").trimmed();
    }
    if (networkProxyText.isEmpty()) {
        networkProxyText = settings.value(
            QStringLiteral("network/proxyMode"), QStringLiteral("system")).toString();
    }
    networkProxyText = networkProxyText.toLower();
    farcaster::transport::qtnetwork::ProxyPolicy networkProxyPolicy{};
    if (networkProxyText == QStringLiteral("direct")) {
        networkProxyPolicy = farcaster::transport::qtnetwork::ProxyPolicy::Direct;
    } else if (networkProxyText == QStringLiteral("system")) {
        networkProxyPolicy = farcaster::transport::qtnetwork::ProxyPolicy::System;
    } else {
        qCritical().noquote()
            << "GENYCASTER_NETWORK_PROXY/--network-proxy must be 'system' or 'direct'.";
        return 2;
    }

    QString providerBaseUrlText = optionValue(
        arguments, QStringLiteral("--hypersnap-base-url="));
    if (providerBaseUrlText.isEmpty()) {
        providerBaseUrlText = qEnvironmentVariable("GENYCASTER_HYPERSNAP_BASE_URL").trimmed();
    }
    if (providerBaseUrlText.isEmpty()) {
        providerBaseUrlText = settings.value(
            QStringLiteral("network/hypersnapBaseUrl")).toString().trimmed();
    }
    providerBaseUrlText = normalizeConfigurationToken(std::move(providerBaseUrlText));
    if (!providerBaseUrlText.isEmpty() && !isValidHttpServiceUrl(providerBaseUrlText)) {
        qWarning().noquote()
            << "Ignoring invalid Hypersnap base URL override; using the built-in public read source.";
        providerBaseUrlText.clear();
    }
    const std::optional<std::string> providerBaseUrl = providerBaseUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{providerBaseUrlText.toStdString()};

    // An independent hosted Farcaster v2-compatible indexed API is optional.
    // Never hardcode/distribute a Neynar key; a deployment supplies it in the
    // process environment or proxies requests from its own authenticated host.
    QString farcasterReadBaseUrlText = optionValue(
        arguments, QStringLiteral("--farcaster-read-base-url="));
    if (farcasterReadBaseUrlText.isEmpty()) {
        farcasterReadBaseUrlText = qEnvironmentVariable(
            "GENYCASTER_FARCASTER_READ_BASE_URL").trimmed();
    }
    QString farcasterReadApiKeyText = qEnvironmentVariable(
        "GENYCASTER_FARCASTER_READ_API_KEY").trimmed();
    if (!farcasterReadApiKeyText.isEmpty() && farcasterReadBaseUrlText.isEmpty()) {
        farcasterReadBaseUrlText = QStringLiteral("https://api.neynar.com");
    }
    farcasterReadBaseUrlText = normalizeConfigurationToken(std::move(farcasterReadBaseUrlText));
    if (!farcasterReadBaseUrlText.isEmpty() && !isValidHttpServiceUrl(farcasterReadBaseUrlText)) {
        qWarning().noquote() << "Farcaster read endpoint is invalid; hosted provider will not be registered.";
        farcasterReadBaseUrlText.clear();
    }
    if (farcasterReadApiKeyText.size() > 256
        || farcasterReadApiKeyText.contains(QLatin1Char('\r'))
        || farcasterReadApiKeyText.contains(QLatin1Char('\n'))) {
        qCritical().noquote() << "Invalid Farcaster read credential format.";
        return 2;
    }
    // Never disclose a Neynar API key to a custom host or redirect target.
    if (!farcasterReadApiKeyText.isEmpty()
        && QUrl(farcasterReadBaseUrlText).host().compare(
            QStringLiteral("api.neynar.com"), Qt::CaseInsensitive) != 0) {
        qCritical().noquote()
            << "Neynar API key may only be sent to the canonical api.neynar.com host.";
        return 2;
    }
    const std::optional<std::string> farcasterReadBaseUrl = farcasterReadBaseUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{farcasterReadBaseUrlText.toStdString()};
    const std::optional<std::string> farcasterReadApiKey = farcasterReadApiKeyText.isEmpty()
        ? std::nullopt : std::optional<std::string>{farcasterReadApiKeyText.toStdString()};

    QStringList nodeCandidateEndpoints{};
    const QString nodeCandidatesText =
        qEnvironmentVariable("GENYCASTER_HYPERSNAP_NODE_CANDIDATES").trimmed();
    if (!nodeCandidatesText.isEmpty()) {
        nodeCandidateEndpoints = nodeCandidatesText.split(
            QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString& endpoint : nodeCandidateEndpoints) {
            endpoint = endpoint.trimmed();
        }
    }
    if (!providerBaseUrlText.isEmpty()) {
        nodeCandidateEndpoints.prepend(providerBaseUrlText);
    }
    const QString appliedNodeEndpoint = providerBaseUrlText.isEmpty()
        ? QStringLiteral("https://haatz.quilibrium.com")
        : providerBaseUrlText;

    QString snapchainBaseUrlText = optionValue(arguments, QStringLiteral("--snapchain-base-url="));
    if (snapchainBaseUrlText.isEmpty()) {
        snapchainBaseUrlText = qEnvironmentVariable("GENYCASTER_SNAPCHAIN_BASE_URL").trimmed();
    }
    if (snapchainBaseUrlText.isEmpty()) {
        snapchainBaseUrlText = QString::fromStdString(
            std::string{deploymentEndpoints.snapchainBaseUrl});
    }
    const std::optional<std::string> snapchainBaseUrl{snapchainBaseUrlText.toStdString()};

    QString mediaGatewayBaseUrlText = optionValue(
        arguments, QStringLiteral("--media-gateway-base-url="));
    if (mediaGatewayBaseUrlText.isEmpty()) {
        mediaGatewayBaseUrlText =
            qEnvironmentVariable("GENYCASTER_MEDIA_GATEWAY_BASE_URL").trimmed();
    }
    if (mediaGatewayBaseUrlText.isEmpty()) {
        mediaGatewayBaseUrlText = QString::fromStdString(
            std::string{deploymentEndpoints.mediaGatewayBaseUrl});
    }
    mediaGatewayBaseUrlText = normalizeConfigurationToken(std::move(mediaGatewayBaseUrlText));
    if (!isValidHttpServiceUrl(mediaGatewayBaseUrlText)) {
        qCritical().noquote()
            << "GENYCASTER_MEDIA_GATEWAY_BASE_URL/--media-gateway-base-url must be a credential-free HTTP(S) origin.";
        return 2;
    }
    const std::optional<std::string> mediaGatewayBaseUrl{mediaGatewayBaseUrlText.toStdString()};

    QString snapchainAuthorizationHeaderText = optionValue(
        arguments, QStringLiteral("--snapchain-auth-header="));
    if (snapchainAuthorizationHeaderText.isEmpty()) {
        snapchainAuthorizationHeaderText =
            qEnvironmentVariable("GENYCASTER_SNAPCHAIN_AUTH_HEADER").trimmed();
    }
    QString snapchainAuthorizationValueText = optionValue(
        arguments, QStringLiteral("--snapchain-auth-value="));
    if (snapchainAuthorizationValueText.isEmpty()) {
        snapchainAuthorizationValueText =
            qEnvironmentVariable("GENYCASTER_SNAPCHAIN_AUTH_VALUE").trimmed();
    }
    const std::optional<std::string> snapchainAuthorizationHeader =
        snapchainAuthorizationHeaderText.isEmpty()
            ? std::nullopt
            : std::optional<std::string>{snapchainAuthorizationHeaderText.toStdString()};
    const std::optional<std::string> snapchainAuthorizationValue =
        snapchainAuthorizationValueText.isEmpty()
            ? std::nullopt
            : std::optional<std::string>{snapchainAuthorizationValueText.toStdString()};
    if (snapchainAuthorizationHeader.has_value() != snapchainAuthorizationValue.has_value()) {
        qCritical().noquote()
            << "Snapchain advanced authorization requires both --snapchain-auth-header/"
               "GENYCASTER_SNAPCHAIN_AUTH_HEADER and --snapchain-auth-value/"
               "GENYCASTER_SNAPCHAIN_AUTH_VALUE.";
        return 2;
    }

    QString appFidText = optionValue(arguments, QStringLiteral("--app-fid="));
    if (appFidText.isEmpty()) {
        appFidText = qEnvironmentVariable("GENYCASTER_APP_FID").trimmed();
    }
    std::optional<std::uint64_t> appFid{};
    if (!appFidText.isEmpty()) {
        bool ok{false};
        const qulonglong parsed = appFidText.toULongLong(&ok);
        if (!ok || parsed == 0U) {
            qCritical().noquote() << "GENYCASTER_APP_FID/--app-fid must be a non-zero unsigned integer.";
            return 2;
        }
        appFid = static_cast<std::uint64_t>(parsed);
    }
    if (!appFid && deploymentIdentity.appFid != 0U) {
        appFid = deploymentIdentity.appFid;
    }

    QString signerAttestationUrlText = optionValue(
        arguments, QStringLiteral("--signer-attestation-url="));
    if (signerAttestationUrlText.isEmpty()) {
        signerAttestationUrlText =
            qEnvironmentVariable("GENYCASTER_SIGNER_ATTESTATION_URL").trimmed();
    }
    // Snapchain gateways implement signed-message submission, NOT App-FID
    // SignedKeyRequest attestation. Never infer a signing endpoint from the
    // public read/write gateway: it returns 404 route_not_available and cannot
    // produce a valid App-FID signature. Only an explicitly configured trusted
    // signer-attestation service may authorize a new protocol signer.
    const std::optional<std::string> signerAttestationUrl = signerAttestationUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{signerAttestationUrlText.toStdString()};

    QString walletLaunchTemplate = optionValue(
        arguments, QStringLiteral("--wallet-launch-url-template="));
    if (walletLaunchTemplate.isEmpty()) {
        walletLaunchTemplate =
            qEnvironmentVariable("GENYCASTER_WALLET_LAUNCH_URL_TEMPLATE").trimmed();
    }

    const QString compiledSiwfDomain = normalizeConfigurationToken(
        QString::fromStdString(std::string{deploymentEndpoints.siwfDomain}));
    const QString compiledSiwfUri = normalizeConfigurationToken(
        QString::fromStdString(std::string{deploymentEndpoints.siwfUri}));

    QString siwfDomainOverride = optionValue(arguments, QStringLiteral("--siwf-domain="));
    const bool explicitSiwfDomainOverride = !siwfDomainOverride.isEmpty();
    if (siwfDomainOverride.isEmpty()) {
        siwfDomainOverride = qEnvironmentVariable("GENYCASTER_SIWF_DOMAIN").trimmed();
    }
    QString siwfUriOverride = optionValue(arguments, QStringLiteral("--siwf-uri="));
    const bool explicitSiwfUriOverride = !siwfUriOverride.isEmpty();
    if (siwfUriOverride.isEmpty()) {
        siwfUriOverride = qEnvironmentVariable("GENYCASTER_SIWF_URI").trimmed();
    }
    const bool hasRuntimeSiwfDomainOverride = !siwfDomainOverride.isEmpty();
    const bool hasRuntimeSiwfUriOverride = !siwfUriOverride.isEmpty();
    siwfDomainOverride = normalizeConfigurationToken(std::move(siwfDomainOverride));
    siwfUriOverride = normalizeConfigurationToken(std::move(siwfUriOverride));

    QString siwfDomainText = compiledSiwfDomain;
    QString siwfUriText = compiledSiwfUri;
    if (hasRuntimeSiwfDomainOverride && hasRuntimeSiwfUriOverride) {
        if (isValidSiwfDomain(siwfDomainOverride) && isValidSiwfUri(siwfUriOverride) &&
            siwfDomainMatchesUri(siwfDomainOverride, siwfUriOverride)) {
            siwfDomainText = std::move(siwfDomainOverride);
            siwfUriText = std::move(siwfUriOverride);
        } else if (explicitSiwfDomainOverride || explicitSiwfUriOverride) {
            qWarning().noquote()
                << "Invalid explicit SIWF override; using the compiled deployment identity.";
        }
    }

    if (!isValidSiwfDomain(siwfDomainText) || !isValidSiwfUri(siwfUriText) ||
        !siwfDomainMatchesUri(siwfDomainText, siwfUriText)) {
        qWarning().noquote()
            << "Sign in with Farcaster is disabled because the compiled SIWF deployment configuration is invalid.";
        siwfDomainText.clear();
        siwfUriText.clear();
    }
    QString authRelayUrlText = optionValue(arguments, QStringLiteral("--farcaster-auth-relay="));
    const bool explicitAuthRelayOverride = !authRelayUrlText.isEmpty();
    if (authRelayUrlText.isEmpty()) {
        authRelayUrlText = qEnvironmentVariable("GENYCASTER_FARCASTER_AUTH_RELAY").trimmed();
    }
    authRelayUrlText = normalizeConfigurationToken(std::move(authRelayUrlText));
    if (!authRelayUrlText.isEmpty() && !isValidHttpServiceUrl(authRelayUrlText)) {
        if (explicitAuthRelayOverride) {
            qWarning().noquote()
                << "Invalid explicit Farcaster authentication relay override; using the official relay default.";
        }
        authRelayUrlText.clear();
    }

    QString optimismRpcUrlText = optionValue(arguments, QStringLiteral("--optimism-rpc-url="));
    const bool explicitOptimismRpcOverride = !optimismRpcUrlText.isEmpty();
    if (optimismRpcUrlText.isEmpty()) {
        optimismRpcUrlText = qEnvironmentVariable("GENYCASTER_OPTIMISM_RPC_URL").trimmed();
    }
    optimismRpcUrlText = normalizeConfigurationToken(std::move(optimismRpcUrlText));
    if (!optimismRpcUrlText.isEmpty() && !isValidHttpServiceUrl(optimismRpcUrlText, true)) {
        if (explicitOptimismRpcOverride) {
            qWarning().noquote()
                << "Invalid explicit Optimism RPC override; using the built-in public RPC default.";
        }
        optimismRpcUrlText.clear();
    }

    const std::optional<std::string> authRelayUrl = authRelayUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{authRelayUrlText.toStdString()};
    const std::optional<std::string> optimismRpcUrl = optimismRpcUrlText.isEmpty()
        ? std::nullopt : std::optional<std::string>{optimismRpcUrlText.toStdString()};
    const std::string siwfDomain = siwfDomainText.toStdString();
    const std::string siwfUri = siwfUriText.toStdString();
#if defined(__EMSCRIPTEN__)
    const bool authenticationAvailable = true; // OpenProof browser device OAuth, read-only session.
#elif GENYCASTER_HAS_OPENPROOF_AUTH
    const bool authenticationAvailable = true;
#elif FARCASTER_HAS_FARCASTER_AUTH
    const bool authenticationAvailable = !siwfDomain.empty() && !siwfUri.empty();
#else
    const bool authenticationAvailable = false;
#endif

    // Optional development/demo identity. Normal guest startup never restores a
    // previously browsed identity as if it were the current account.
    QString startupIdentity = optionValue(arguments, QStringLiteral("--auto-open-identity="));
    if (startupIdentity.isEmpty()) {
        startupIdentity = qEnvironmentVariable("GENYCASTER_DEMO_IDENTITY").trimmed();
    }

    LocalizationController::StartupOverrides localizationOverrides{
        .languageTag = optionValue(arguments, QStringLiteral("--language=")),
        .regionalLocaleTag = optionValue(arguments, QStringLiteral("--regional-locale=")),
        .calendarId = optionValue(arguments, QStringLiteral("--calendar=")),
        .timestampDisplayMode = optionValue(arguments, QStringLiteral("--timestamp-display=")),
        .timeZoneId = optionValue(arguments, QStringLiteral("--time-zone=")),
        .currencyCode = optionValue(arguments, QStringLiteral("--currency=")),
        .additionalCurrencyCodes = {},
    };
    if (localizationOverrides.languageTag.isEmpty()) {
        localizationOverrides.languageTag = qEnvironmentVariable("GENYCASTER_LANGUAGE").trimmed();
    }
    if (localizationOverrides.regionalLocaleTag.isEmpty()) {
        localizationOverrides.regionalLocaleTag = qEnvironmentVariable("GENYCASTER_REGIONAL_LOCALE").trimmed();
    }
    if (localizationOverrides.calendarId.isEmpty()) {
        localizationOverrides.calendarId = qEnvironmentVariable("GENYCASTER_CALENDAR").trimmed();
    }
    if (localizationOverrides.timestampDisplayMode.isEmpty()) {
        localizationOverrides.timestampDisplayMode = qEnvironmentVariable("GENYCASTER_TIMESTAMP_DISPLAY").trimmed();
    }
    if (localizationOverrides.timeZoneId.isEmpty()) {
        localizationOverrides.timeZoneId = qEnvironmentVariable("GENYCASTER_TIME_ZONE").trimmed();
    }
    if (localizationOverrides.currencyCode.isEmpty()) {
        localizationOverrides.currencyCode = qEnvironmentVariable("GENYCASTER_CURRENCY").trimmed();
    }
    QString additionalCurrencies = optionValue(arguments, QStringLiteral("--additional-currencies="));
    if (additionalCurrencies.isEmpty()) {
        additionalCurrencies = qEnvironmentVariable("GENYCASTER_ADDITIONAL_CURRENCIES").trimmed();
    }
    if (!additionalCurrencies.isEmpty()) {
        localizationOverrides.additionalCurrencyCodes = additionalCurrencies.split(
            QLatin1Char(','), Qt::SkipEmptyParts);
    }

    AppearanceController::StartupOverrides appearanceOverrides{
        .themeMode = optionValue(arguments, QStringLiteral("--theme-mode=")),
        .colorThemeId = optionValue(arguments, QStringLiteral("--color-theme=")),
        .reduceMotion = optionValue(arguments, QStringLiteral("--reduce-motion=")),
    };
    if (appearanceOverrides.themeMode.isEmpty()) {
        appearanceOverrides.themeMode = qEnvironmentVariable("GENYCASTER_THEME_MODE").trimmed();
    }
    if (appearanceOverrides.colorThemeId.isEmpty()) {
        appearanceOverrides.colorThemeId = qEnvironmentVariable("GENYCASTER_COLOR_THEME").trimmed();
    }
    if (appearanceOverrides.reduceMotion.isEmpty()) {
        appearanceOverrides.reduceMotion = qEnvironmentVariable("GENYCASTER_REDUCE_MOTION").trimmed();
    }

    // The context objects must outlive QQmlApplicationEngine. During orderly
    // teardown the engine reevaluates/destroys QML bindings; destroying these
    // objects first turns every context-property access into a spurious null
    // TypeError. Declare the owners before the engine so reverse destruction
    // order tears the QML engine down first.
    std::unique_ptr<AppearanceController> appearanceControllerOwner{};
    std::unique_ptr<RenderingController> renderingControllerOwner{};
    std::unique_ptr<LocalizationController> localizationControllerOwner{};
    std::unique_ptr<PresentationBridge> presentationBridgeOwner{};
    std::unique_ptr<DataStorageController> dataStorageControllerOwner{};
    std::unique_ptr<RichComposerController> richComposerControllerOwner{};
    std::unique_ptr<EmojiController> emojiControllerOwner{};
    std::unique_ptr<farcaster::runtime::emoji::EmojiRuntime> emojiRuntimeOwner{};
    std::unique_ptr<MediaUploadController> mediaUploadControllerOwner{};
    std::unique_ptr<MediaClassifierController> mediaClassifierControllerOwner{};
    std::unique_ptr<MediaPlaybackCoordinator> mediaPlaybackCoordinatorOwner{};
    std::unique_ptr<LiveInsightsController> liveInsightsControllerOwner{};
    std::unique_ptr<NodeSelectionController> nodeSelectionControllerOwner{};
    std::unique_ptr<UpdateController> updateControllerOwner{};
    std::unique_ptr<MiniAppHostController> miniAppHostControllerOwner{};
    std::unique_ptr<MediaWebHostController> mediaWebHostControllerOwner{};
    std::unique_ptr<MiniAppCatalogController> miniAppCatalogControllerOwner{};
    std::unique_ptr<MiniAppTaskManagerController> miniAppTaskManagerControllerOwner{};
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
    std::unique_ptr<capability::wallet::IEthereumWalletBridge> miniAppWalletBridgeOwner{};
#endif
#if GENYCASTER_HAS_MINIAPP_HOST
    std::unique_ptr<farcaster::runtime::miniapp::MiniAppRuntime> miniAppRuntimeOwner{};
#endif

    // Qt's generated static module registration for the executable (render
    // C++ types such as SceneGraphSuperellipseItem) must be primed before the
    // engine imports Farcaster.Client. The QQmlModuleRegistration static
    // initializer alone is not enough when the module also ships as a resource
    // qmldir.
    extern void qml_register_types_Farcaster_Client();
    qml_register_types_Farcaster_Client();
    qmlRegisterType<RichInlineTextItem>(
        "Farcaster.Client", 1, 0, "RichInlineTextItem");

    QQmlApplicationEngine engine{};

    appearanceControllerOwner = std::make_unique<AppearanceController>(
        settings, std::move(appearanceOverrides));
    AppearanceController& appearanceController = *appearanceControllerOwner;
    renderingControllerOwner = std::make_unique<RenderingController>(settings);
    RenderingController& renderingController = *renderingControllerOwner;

    try {
        localizationControllerOwner = std::make_unique<LocalizationController>(
            engine, settings, std::move(localizationOverrides));
    } catch (const std::exception& error) {
        qCritical().noquote() << "Unable to initialize Genycaster localization:" << error.what();
        return 2;
    }
    LocalizationController& localizationController = *localizationControllerOwner;

    nodeSelectionControllerOwner = std::make_unique<NodeSelectionController>(
        settings,
        appliedNodeEndpoint,
        nodeCandidateEndpoints,
        networkProxyText,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest);
    NodeSelectionController& nodeSelectionController = *nodeSelectionControllerOwner;
    updateControllerOwner = std::make_unique<UpdateController>(
        settings, !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest, &application);
    UpdateController& updateController = *updateControllerOwner;

    const auto applicationFontFamilyForLanguage = [&](QString languageTag) -> QString {
        languageTag = languageTag.trimmed().toLower();
        if (languageTag == QStringLiteral("fa") || languageTag == QStringLiteral("ar")) {
            return !bundledPersianFontFamily.isEmpty() ? bundledPersianFontFamily : platformRtlFontFamily;
        }
        if (languageTag == QStringLiteral("hi")) {
            return !bundledDevanagariFontFamily.isEmpty() ? bundledDevanagariFontFamily : platformDevanagariFontFamily;
        }
        if (languageTag == QStringLiteral("ja")) {
            return !bundledJapaneseFontFamily.isEmpty() ? bundledJapaneseFontFamily : platformJapaneseFontFamily;
        }
        if (languageTag == QStringLiteral("ko")) {
            return !bundledKoreanFontFamily.isEmpty() ? bundledKoreanFontFamily : platformKoreanFontFamily;
        }
        if (languageTag == QStringLiteral("zh")) {
            return !bundledChineseFontFamily.isEmpty() ? bundledChineseFontFamily : platformChineseFontFamily;
        }
        return platformTextFontFamily;
    };
    const auto applyApplicationLocaleFont = [&] {
        QFont localeFont = application.font();
        localeFont.setFamily(applicationFontFamilyForLanguage(localizationController.effectiveLanguageTag()));
        application.setFont(localeFont);
    };
    applyApplicationLocaleFont();
    QObject::connect(
        &localizationController, &LocalizationController::localeChanged,
        &application, applyApplicationLocaleFont);
#if defined(__EMSCRIPTEN__)
    // Keep the phrase-import DOM sheet in the same UI language as Qt/QML,
    // including live changes in the Genycaster language selector. This passes
    // only a public locale tag; recovery words never cross this bridge.
    const auto updateBrowserRecoveryLocale = [&localizationController] {
        const QByteArray language = localizationController.effectiveLanguageTag().toUtf8();
        EM_ASM({
            window.__genycasterPreferredLanguage = UTF8ToString($0, $1);
        }, language.constData(), language.size());
    };
    updateBrowserRecoveryLocale();
    QObject::connect(
        &localizationController, &LocalizationController::localeChanged,
        &application, updateBrowserRecoveryLocale);
#endif

    GeoLocationResolver geoLocationResolver{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct};
    LinkPreviewResolver linkPreviewResolver{
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct};
    liveInsightsControllerOwner = std::make_unique<LiveInsightsController>(
        settings,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest);
    LiveInsightsController& liveInsightsController = *liveInsightsControllerOwner;

    // Construct the bridge only after the translation catalog is active because
    // its initial user-visible status strings are ID-based translations.
    presentationBridgeOwner = std::make_unique<PresentationBridge>(nullptr, &settings);
    PresentationBridge& presentationBridge = *presentationBridgeOwner;
    presentationBridge.setFarcasterReadConfigured(farcasterReadBaseUrl.has_value());
    presentationBridge.setFarcasterReadEndpoint(farcasterReadBaseUrlText);

    dataStorageControllerOwner = std::make_unique<DataStorageController>(settings);
    DataStorageController& dataStorageController = *dataStorageControllerOwner;

#if defined(__EMSCRIPTEN__)
    constexpr bool miniAppLaunchAvailable = true; // External tab, without privileged SDK.
#else
    constexpr bool miniAppLaunchAvailable = GENYCASTER_HAS_MINIAPP_HOST != 0;
#endif
    miniAppHostControllerOwner = std::make_unique<MiniAppHostController>(
        miniAppLaunchAvailable, settings);
    MiniAppHostController& miniAppHost = *miniAppHostControllerOwner;
    mediaWebHostControllerOwner = std::make_unique<MediaWebHostController>(
        GENYCASTER_HAS_MINIAPP_HOST != 0, QUrl{siwfUriText});
    MediaWebHostController& mediaWebHost = *mediaWebHostControllerOwner;

    const QByteArray configuredMiniAppDiscoveryApiKey =
        qEnvironmentVariable("GENYCASTER_NEYNAR_API_KEY").trimmed().toUtf8();
    const bool usingNeynarDocsAccess = configuredMiniAppDiscoveryApiKey.isEmpty();
    const QByteArray miniAppDiscoveryApiKey = usingNeynarDocsAccess
        ? QByteArrayLiteral("NEYNAR_API_DOCS")
        : configuredMiniAppDiscoveryApiKey;
    MiniAppCatalogEndpoint primaryMiniAppCatalogue{
        .catalogueUrl = QStringLiteral(
            "https://api.neynar.com/v2/farcaster/frame/catalog/?limit=100&time_window=7d"),
        .searchUrl = QStringLiteral("https://api.neynar.com/v2/farcaster/frame/search/"),
        .relevantUrl = QStringLiteral("https://api.neynar.com/v2/farcaster/frame/relevant/?time_window=7d"),
        .authorizationHeaderName = QByteArrayLiteral("x-api-key"),
        .authorizationHeaderValue = miniAppDiscoveryApiKey,
        .providerLabel = usingNeynarDocsAccess
            ? QStringLiteral("Neynar Mini Apps · docs access")
            : QStringLiteral("Neynar Mini Apps"),
        .format = MiniAppCatalogFormat::Json,
    };
    MiniAppCatalogEndpoint fallbackMiniAppCatalogue{
        .catalogueUrl = QStringLiteral("https://api.farcaster.xyz/v1/developer-rewards-winner-history?limit=100"),
        .providerLabel = QStringLiteral("Farcaster public developer index"),
        .format = MiniAppCatalogFormat::Json,
    };
#if defined(__EMSCRIPTEN__)
    // Keep upstream API credentials out of browser binaries and avoid CORS.
    // The first-party gateway only exposes bounded public catalogue reads.
    primaryMiniAppCatalogue = MiniAppCatalogEndpoint{
        .catalogueUrl = QStringLiteral("https://storage-farcaster.genyleap.com/v1/browser/miniapps?kind=catalog"),
        .searchUrl = QStringLiteral("https://storage-farcaster.genyleap.com/v1/browser/miniapps?kind=search"),
        .relevantUrl = QStringLiteral("https://storage-farcaster.genyleap.com/v1/browser/miniapps?kind=relevant"),
        .providerLabel = QStringLiteral("Genycaster public Mini Apps catalogue"),
        .format = MiniAppCatalogFormat::Json,
    };
    fallbackMiniAppCatalogue = MiniAppCatalogEndpoint{};
#endif
    miniAppCatalogControllerOwner = std::make_unique<MiniAppCatalogController>(
        settings,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        std::move(primaryMiniAppCatalogue),
        std::move(fallbackMiniAppCatalogue),
        !isSmokeTest && !isVisualTest && !isRuntimeSmokeTest);
    MiniAppCatalogController& miniAppCatalog = *miniAppCatalogControllerOwner;
    miniAppTaskManagerControllerOwner = std::make_unique<MiniAppTaskManagerController>();
    MiniAppTaskManagerController& miniAppTaskManager = *miniAppTaskManagerControllerOwner;

    miniAppHost.setReduceMotion(appearanceController.reduceMotion());
    QObject::connect(&appearanceController, &AppearanceController::appearanceChanged, &miniAppHost,
        [&appearanceController, &miniAppHost] {
            miniAppHost.setReduceMotion(appearanceController.reduceMotion());
        });
    mediaWebHost.setReduceMotion(appearanceController.reduceMotion());
    QObject::connect(&appearanceController, &AppearanceController::appearanceChanged, &mediaWebHost,
        [&appearanceController, &mediaWebHost] {
            mediaWebHost.setReduceMotion(appearanceController.reduceMotion());
        });
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
    {
        auto walletBridge = farcaster::provider::compatibility::browser_wallet::bridge::makeBridge({
            .launch = makeWalletApprovalLauncher(walletLaunchTemplate),
            .supportedChains = miniAppEthereumChains(),
        });
        if (walletBridge) {
            miniAppWalletBridgeOwner = std::move(*walletBridge);
        } else {
            qWarning().noquote() << "Mini App EIP-1193 wallet bridge unavailable:"
                                 << QString::fromStdString(walletBridge.error().message);
        }
    }
#endif
#if GENYCASTER_HAS_MINIAPP_HOST
    miniAppRuntimeOwner = std::make_unique<farcaster::runtime::miniapp::MiniAppRuntime>(
        miniAppHost, miniAppCatalog, miniAppTaskManager,
        networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
        static_cast<qulonglong>(appFid.value_or(0))
#if FARCASTER_HAS_MINIAPP_EVM_WALLET
        , miniAppWalletBridgeOwner.get()
#else
        , nullptr
#endif
    );
#endif
    const auto updateMiniAppUser = [&presentationBridge, &miniAppHost] {
        QVariantMap user;
        if (presentationBridge.isAuthenticated() && presentationBridge.accountFid() > 0
            && presentationBridge.accountFid() <= 9007199254740991ULL) {
            user = {{QStringLiteral("fid"), presentationBridge.accountFid()},
                    {QStringLiteral("username"), presentationBridge.accountUsername()},
                    {QStringLiteral("displayName"), presentationBridge.accountName()},
                    {QStringLiteral("pfpUrl"), presentationBridge.accountAvatarUrl()}};
        }
        miniAppHost.setUser(user);
    };
    QObject::connect(&presentationBridge, &PresentationBridge::stateChanged,
        &miniAppHost, updateMiniAppUser);
    updateMiniAppUser();
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &miniAppHost, &MiniAppHostController::closeAll);
    QObject::connect(&presentationBridge, &PresentationBridge::embeddedMediaRequested,
        &mediaWebHost, [&presentationBridge, &mediaWebHost](const QString& url, const QString& title) {
            if (!mediaWebHost.open(url, title)) {
                presentationBridge.openExternalUrl(url);
            }
        });
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
        &mediaWebHost, &MediaWebHostController::close);

    qulonglong notificationSettingsFid = 0;
    bool notificationBadgeBootstrapped = false;
    QObject::connect(&presentationBridge, &PresentationBridge::notificationSeenAtChanged,
        &presentationBridge, [&settings](const qulonglong fid, const qlonglong observedAtMs) {
            if (fid == 0) {
                return;
            }
            settings.setValue(
                QStringLiteral("notifications/seen_at/%1").arg(fid),
                observedAtMs);
            settings.sync();
        });
    QObject::connect(&application, &QGuiApplication::applicationStateChanged,
        &presentationBridge, [&presentationBridge](const Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive || !presentationBridge.isAuthenticated()) {
                return;
            }
            // Refresh after focus settles so returning from sleep/background does not
            // leave a stale navigation badge for another polling interval.
            QTimer::singleShot(250, &presentationBridge, [&presentationBridge] {
                presentationBridge.refreshNotificationBadge();
            });
        });
    QObject::connect(&presentationBridge, &PresentationBridge::stateChanged,
        &presentationBridge,
        [&presentationBridge, &settings, &notificationSettingsFid, &notificationBadgeBootstrapped] {
            const qulonglong fid = presentationBridge.isAuthenticated()
                ? presentationBridge.accountFid() : 0;
            if (fid != notificationSettingsFid) {
                notificationSettingsFid = fid;
                notificationBadgeBootstrapped = false;
                if (fid != 0) {
                    presentationBridge.restoreNotificationSeenAt(
                        fid,
                        settings.value(
                            QStringLiteral("notifications/seen_at/%1").arg(fid),
                            0).toLongLong());
                }
            }
            if (fid != 0 && presentationBridge.runtimeReady() && !notificationBadgeBootstrapped) {
                notificationBadgeBootstrapped = true;
                QTimer::singleShot(750, &presentationBridge, [&presentationBridge] {
                    presentationBridge.refreshNotificationBadge();
                });
            }
        });


    richComposerControllerOwner = std::make_unique<RichComposerController>(settings);
    RichComposerController& richComposerController = *richComposerControllerOwner;
    emojiControllerOwner = std::make_unique<EmojiController>(settings);
    EmojiController& emojiController = *emojiControllerOwner;
    emojiRuntimeOwner = std::make_unique<farcaster::runtime::emoji::EmojiRuntime>(emojiController);
    mediaUploadControllerOwner = std::make_unique<MediaUploadController>();
    MediaUploadController& mediaUploadController = *mediaUploadControllerOwner;
    mediaClassifierControllerOwner = std::make_unique<MediaClassifierController>();
    MediaClassifierController& mediaClassifier = *mediaClassifierControllerOwner;
    mediaPlaybackCoordinatorOwner = std::make_unique<MediaPlaybackCoordinator>(settings);
    MediaPlaybackCoordinator& mediaPlayback = *mediaPlaybackCoordinatorOwner;
    // Connections in GenyVideoPlayer retain this context object as a signal
    // target. Keep its lifetime unequivocally C++-owned across QML teardown.
    QQmlEngine::setObjectOwnership(&mediaPlayback, QQmlEngine::CppOwnership);
    QObject::connect(&localizationController, &LocalizationController::localeChanged,
        &presentationBridge, [&presentationBridge] {
            presentationBridge.retranslateTransientState();
        });
    if (!isSmokeTest && !isVisualTest) {
        QNetworkInformation::loadDefaultBackend();
        if (auto* networkInformation = QNetworkInformation::instance()) {
            const auto initialReachability = networkInformation->reachability();
            const auto applyReachability = [&presentationBridge](const QNetworkInformation::Reachability reachability) {
                const bool known = reachability != QNetworkInformation::Reachability::Unknown;
                const bool online = reachability != QNetworkInformation::Reachability::Disconnected;
                presentationBridge.applyNetworkReachability(known, online);
            };
            applyReachability(initialReachability);
            QObject::connect(networkInformation, &QNetworkInformation::reachabilityChanged,
                &presentationBridge,
                [&presentationBridge, previousReachability = initialReachability, applyReachability]
                (const QNetworkInformation::Reachability reachability) mutable {
                    const bool reconnected = previousReachability == QNetworkInformation::Reachability::Disconnected
                        && reachability != QNetworkInformation::Reachability::Disconnected
                        && reachability != QNetworkInformation::Reachability::Unknown;
                    previousReachability = reachability;
                    applyReachability(reachability);
                    if (reconnected && presentationBridge.runtimeReady() && !presentationBridge.isBusy()) {
                        QTimer::singleShot(350, &presentationBridge, [&presentationBridge] {
                            if (!presentationBridge.isBusy() && !presentationBridge.isOffline()) {
                                presentationBridge.retryAutomatically();
                            }
                        });
                    }
                });
        } else {
            presentationBridge.applyNetworkReachability(false, true);
        }

        auto* const sourceRecoveryTimer = new QTimer(&presentationBridge);
        sourceRecoveryTimer->setInterval(35'000);
        QObject::connect(sourceRecoveryTimer, &QTimer::timeout, &presentationBridge,
            [&presentationBridge] {
                if (!presentationBridge.runtimeReady() || presentationBridge.isBusy() ||
                    presentationBridge.isOffline() || !presentationBridge.sourceUnavailable()) {
                    return;
                }
                presentationBridge.retryAutomatically();
            });
        sourceRecoveryTimer->start();
    }
    if (isSmokeTest || isVisualTest) {
        const bool showInitialSetup =
            isVisualTest && arguments.contains(QStringLiteral("--open-initial-setup"));
        if (showInitialSetup) {
            presentationBridge.applyRuntimeState(true);
            presentationBridge.applyAuthenticationState(
                true,
                QStringLiteral("signed-out"),
                0,
                {},
                {},
                {},
                {},
                {},
                {});
        } else {
            presentationBridge.applyVerificationFixture();
        }
        if (isVisualTest && arguments.contains(QStringLiteral("--open-profile"))) {
            applyProfileVerificationFixture(presentationBridge);
        }
    }
    engine.addImageProvider(
        QStringLiteral("remote"),
        new RemoteImageProvider{
            networkProxyPolicy == farcaster::transport::qtnetwork::ProxyPolicy::Direct,
            dataStorageController.runtimeStateHandle()});
    engine.rootContext()->setContextProperty(QStringLiteral("platformTextFontFamily"), platformTextFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformRtlFontFamily"), platformRtlFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformDevanagariFontFamily"), platformDevanagariFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformJapaneseFontFamily"), platformJapaneseFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformKoreanFontFamily"), platformKoreanFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("platformChineseFontFamily"), platformChineseFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledPersianFontFamily"), bundledPersianFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledDevanagariFontFamily"), bundledDevanagariFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledJapaneseFontFamily"), bundledJapaneseFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledKoreanFontFamily"), bundledKoreanFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledChineseFontFamily"), bundledChineseFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledIconFontFamily"), bundledIconFontFamily);
    engine.rootContext()->setContextProperty(
        QStringLiteral("bundledIconRegularFontFamily"), bundledIconRegularFontFamily);
    engine.rootContext()->setContextProperty(QStringLiteral("presentationBridge"), &presentationBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("dataStorage"), &dataStorageController);
    engine.rootContext()->setContextProperty(QStringLiteral("richComposer"), &richComposerController);
    engine.rootContext()->setContextProperty(QStringLiteral("emojiController"), &emojiController);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaUpload"), &mediaUploadController);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaClassifier"), &mediaClassifier);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaPlayback"), &mediaPlayback);
    engine.rootContext()->setContextProperty(QStringLiteral("localizationController"), &localizationController);
    engine.rootContext()->setContextProperty(QStringLiteral("geoLocationResolver"), &geoLocationResolver);
    engine.rootContext()->setContextProperty(QStringLiteral("linkPreviewResolver"), &linkPreviewResolver);
    engine.rootContext()->setContextProperty(QStringLiteral("miniAppHost"), &miniAppHost);
    engine.rootContext()->setContextProperty(QStringLiteral("miniAppCatalog"), &miniAppCatalog);
    engine.rootContext()->setContextProperty(QStringLiteral("miniAppTaskManager"), &miniAppTaskManager);
    engine.rootContext()->setContextProperty(QStringLiteral("liveInsights"), &liveInsightsController);
    engine.rootContext()->setContextProperty(QStringLiteral("nodeSelectionController"), &nodeSelectionController);
    engine.rootContext()->setContextProperty(QStringLiteral("updateController"), &updateController);
#if defined(__EMSCRIPTEN__)
    engine.rootContext()->setContextProperty(QStringLiteral("wasmReadOnly"), true);
#else
    engine.rootContext()->setContextProperty(QStringLiteral("wasmReadOnly"), false);
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("appearanceController"), &appearanceController);
    engine.rootContext()->setContextProperty(QStringLiteral("renderingController"), &renderingController);
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderLabel"), QStringLiteral("Hypersnap v2"));
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderIcon"), QStringLiteral("quilibrium/hypersnap"));
#else
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderLabel"), QStringLiteral("Self-hosted"));
    engine.rootContext()->setContextProperty(QStringLiteral("nodeProviderIcon"), QStringLiteral("quilibrium/quilibrium"));
#endif
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Farcaster.Client"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) { return 1; }
    renderingController.attachWindow(qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()));
#if defined(__EMSCRIPTEN__)
    installBrowserWheelBridge(qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()));
#endif

    std::shared_ptr<Runtime> runtime{};
    std::jthread bootstrap{};
    if (!isSmokeTest && !isVisualTest) {
        const QString databasePathText = optionValue(
            arguments, QStringLiteral("--database-path="));
        const std::filesystem::path databasePath = databasePathText.isEmpty()
            ? std::filesystem::path{
                  QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()}
                  / "genycaster.sqlite3"
            : std::filesystem::path{databasePathText.toStdString()};
        auto bootstrapRuntime = [&presentationBridge, &richComposerController, &mediaUploadController,
                                  &mediaPlayback, &runtime,
                                  databasePath, networkProxyPolicy, providerBaseUrl,
                                  farcasterReadBaseUrl, farcasterReadApiKey, authRelayUrl,
                                  optimismRpcUrl, snapchainBaseUrl,
                                  mediaGatewayBaseUrl, snapchainAuthorizationHeader,
                                  snapchainAuthorizationValue, appFid,
                                  signerAttestationUrl, walletLaunchTemplate,
                                  siwfDomain, siwfUri, startupIdentity,
                                  openExploreForTest] {
            auto made = makeRuntime(
                databasePath,
                networkProxyPolicy,
                providerBaseUrl,
                farcasterReadBaseUrl,
                farcasterReadApiKey,
                authRelayUrl,
                optimismRpcUrl,
                snapchainBaseUrl,
                mediaGatewayBaseUrl,
                snapchainAuthorizationHeader,
                snapchainAuthorizationValue,
                appFid,
                signerAttestationUrl,
                makeWalletApprovalLauncher(walletLaunchTemplate, true));
            if (!made) {
                const QString message = QString::fromStdString(made.error().message);
                QMetaObject::invokeMethod(&presentationBridge,
                    [&presentationBridge, message] { presentationBridge.applyRuntimeState(false, message); },
                    Qt::QueuedConnection);
                return;
            }
            auto readyRuntime = std::shared_ptr<Runtime>{std::move(*made)};
            QMetaObject::invokeMethod(&presentationBridge,
                [&presentationBridge, &richComposerController, &mediaUploadController,
                 &mediaPlayback, &runtime, readyRuntime = std::move(readyRuntime),
                 siwfDomain, siwfUri, startupIdentity,
                 snapchainBaseUrl, openExploreForTest]() mutable {
                    runtime = std::move(readyRuntime);
                    connectRuntime(
                        presentationBridge, richComposerController, runtime, authenticationAvailable,
                        siwfDomain, siwfUri,
                        snapchainBaseUrl ? QString::fromStdString(*snapchainBaseUrl)
                                         : QStringLiteral("https://snapchain.genyleap.com"));
                    connectMediaPlayback(mediaPlayback, runtime);
#if FARCASTER_HAS_MEDIA_UPLOAD
                    connectMediaUpload(mediaUploadController, runtime);
#else
                    mediaUploadController.setAvailable(false);
#endif
                    presentationBridge.applyRuntimeState(true);
                    if (startupIdentity.isEmpty()) {
                        if (openExploreForTest) {
                            presentationBridge.selectFeed(QStringLiteral("trending"));
                        }
                        return;
                    }
                    // PresentationBridge owns the identity-to-Home transition for both
                    // restored and manually opened identities. This keeps Trending an
                    // explicit Explore capability and avoids startup-only orchestration.
                    presentationBridge.openIdentity(startupIdentity);
                }, Qt::QueuedConnection);
        };
#if defined(__EMSCRIPTEN__)
        // std::jthread cannot start in a single-threaded WASM browser runtime.
        // Defer initialization until Qt's main event loop is active.
        QTimer::singleShot(0, &application, std::move(bootstrapRuntime));
#else
        bootstrap = std::jthread(std::move(bootstrapRuntime));
#endif
    }

    if (isSmokeTest) {
        QObject* const rootObject = engine.rootObjects().constFirst();
        bool ready = rootObject != nullptr
            && rootObject->objectName() == QStringLiteral("rootWindow")
            && rootObject->property("isReady").toBool()
            && rootObject->findChild<QObject*>(QStringLiteral("appShell")) != nullptr
            && rootObject->findChild<QObject*>(QStringLiteral("readList")) != nullptr;
        const QStringList sidebarActions{
            QStringLiteral("trending"),
            QStringLiteral("following"),
            QStringLiteral("channels"),
            QStringLiteral("notifications"),
            QStringLiteral("store"),
            QStringLiteral("business"),
            QStringLiteral("messages"),
            QStringLiteral("bookmarks"),
            QStringLiteral("profile"),
            QStringLiteral("settings"),
        };
        for (const QString& action : sidebarActions) {
            QObject* const button = rootObject == nullptr ? nullptr
                : rootObject->findChild<QObject*>(QStringLiteral("sidebarAction-") + action);
            ready = ready && button != nullptr && button->property("enabled").toBool()
                && QMetaObject::invokeMethod(button, "clicked", Qt::DirectConnection);
        }
        QObject* const emojiPicker = rootObject == nullptr ? nullptr
            : rootObject->findChild<QObject*>(QStringLiteral("emojiPicker"));
        ready = ready && emojiPicker != nullptr;
        if (!ready) { return 2; }
        QMetaObject::invokeMethod(&application, [] { QCoreApplication::exit(0); }, Qt::QueuedConnection);
    }
    if (isRuntimeSmokeTest) {
        auto* const poller = new QTimer(&application);
        poller->setInterval(25);
        QObject::connect(poller, &QTimer::timeout, &application,
            [&presentationBridge] {
                if (!presentationBridge.runtimeReady()) { return; }
                if (!presentationBridge.errorText().isEmpty()) {
                    qWarning().noquote() << "Read diagnostic:" << presentationBridge.diagnosticText();
                    QCoreApplication::exit(5);
                } else if (!presentationBridge.rows().isEmpty() && !presentationBridge.isBusy()) {
                    QCoreApplication::exit(0);
                }
            });
        poller->start();
        QTimer::singleShot(10'000, &application, [] { QCoreApplication::exit(6); });
    }
    if (isVisualTest && !snapshotPath.isEmpty()) {
        auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        if (window == nullptr) { return 3; }
        for (const QString& argument : arguments) {
            if (argument.startsWith(QStringLiteral("--width="))) {
                window->setWidth(argument.sliced(8).toInt());
            } else if (argument.startsWith(QStringLiteral("--height="))) {
                window->setHeight(argument.sliced(9).toInt());
            }
        }
        if (arguments.contains(QStringLiteral("--open-settings"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("settingsPopup"))) {
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (openExploreForTest) {
            presentationBridge.selectFeed(QStringLiteral("trending"));
        }
        if (arguments.contains(QStringLiteral("--open-composer"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("composerDrawer"))) {
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--open-reply-composer"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("composerDrawer"))) {
                popup->setProperty("parentCast", QVariantMap{
                    {QStringLiteral("castId"), QStringLiteral("visual-reply-target")},
                    {QStringLiteral("author"), QStringLiteral("kmacb.eth")},
                    {QStringLiteral("displayName"), QStringLiteral("KMac")},
                    {QStringLiteral("avatarUrl"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/farcaster.svg")},
                    {QStringLiteral("text"), QStringLiteral(
                        "Fair question — here is the original cast with enough context to reply confidently.")},
                });
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--open-create"))) {
            if (auto* const popup = window->findChild<QObject*>(QStringLiteral("mobileCreateMenu"))) {
                QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--open-gallery"))) {
            if (auto* const gallery = window->findChild<QObject*>(QStringLiteral("mediaGallery"))) {
                gallery->setProperty("items", QVariantList{
                    QVariantMap{{QStringLiteral("imageSource"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/genyleap.svg")}},
                    QVariantMap{{QStringLiteral("imageSource"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/farcaster.svg")}},
                    QVariantMap{{QStringLiteral("imageSource"), QStringLiteral(
                        "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/quilibrium/quilibrium.svg")}},
                });
                gallery->setProperty("requestedIndex", 0);
                QMetaObject::invokeMethod(gallery, "open", Qt::DirectConnection);
            }
        }
        if (arguments.contains(QStringLiteral("--exercise-code-block-lifetime"))) {
            QTimer::singleShot(180, &application, [&richComposerController] {
                richComposerController.insertBlock(QStringLiteral("code"));
            });
            QTimer::singleShot(320, &application, [&richComposerController] {
                richComposerController.setCodeLanguage(1, QStringLiteral("cpp"));
            });
            QTimer::singleShot(460, &application, [&richComposerController] {
                richComposerController.removeBlock(1);
            });
        }
        bool snapshotDelayValid = false;
        const int requestedSnapshotDelay = optionValue(
            arguments, QStringLiteral("--snapshot-delay-ms=")).toInt(&snapshotDelayValid);
        const int snapshotDelay = snapshotDelayValid
            ? std::clamp(requestedSnapshotDelay, 100, 30'000) : 600;
        QTimer::singleShot(snapshotDelay, &application, [window, snapshotPath] {
            const bool saved = window->grabWindow().save(snapshotPath);
            QCoreApplication::exit(saved ? 0 : 4);
        });
    }
    const QString snapshotLivePath = optionValue(arguments, QStringLiteral("--snapshot-live="));
    if (!snapshotLivePath.isEmpty()) {
        // Grab a screenshot of the real running app (live data, not the fixture)
        // once the demo feed has rendered. Used to visually verify real content.
        if (auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst())) {
            window->resize(1280, 900);
        }
        bool liveSnapshotDelayValid = false;
        const int requestedLiveSnapshotDelay = optionValue(
            arguments, QStringLiteral("--snapshot-delay-ms=")).toInt(&liveSnapshotDelayValid);
        const int liveSnapshotDelay = liveSnapshotDelayValid
            ? std::clamp(requestedLiveSnapshotDelay, 100, 30'000) : 5'000;
        QTimer::singleShot(liveSnapshotDelay, &application, [&engine, snapshotLivePath] {
            auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
            if (window == nullptr) {
                QCoreApplication::exit(3);
                return;
            }
            const bool saved = window->grabWindow().save(snapshotLivePath);
            QCoreApplication::exit(saved ? 0 : 4);
        });
    }
#if defined(__EMSCRIPTEN__)
    // IDBFS is async: periodically flush SQLite's virtual file changes to
    // IndexedDB. The preRun hook restores the directory before main().
    QTimer wasmPersistenceFlush{&application};
    wasmPersistenceFlush.setInterval(8'000);
    QObject::connect(&wasmPersistenceFlush, &QTimer::timeout, &application, [] {
        EM_ASM({
            FS.syncfs(false, function(err) {
                if (err) console.error('[Genycaster] IndexedDB sync failed', err);
            });
        });
    });
    wasmPersistenceFlush.start();
    // Persist onboarding/consent immediately instead of waiting for the
    // periodic SQLite flush. Never synchronize a private mnemonic here.
    QObject::connect(&bridge, &PresentationBridge::setupStateChanged,
                     &application, [&settings] {
        settings.sync();
        EM_ASM({
            if (typeof FS !== 'undefined' && typeof FS.syncfs === 'function')
                FS.syncfs(false, function(err) {
                    if (err) console.error('[Genycaster] Settings save failed', err);
                });
        });
    });
#endif
    return application.exec();
}
