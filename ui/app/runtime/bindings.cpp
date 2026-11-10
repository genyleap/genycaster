#include "genycaster_deployment.hpp"
#include <QCoreApplication>
#include <QDateTime>
#if !defined(Q_OS_WASM)
#include <QFutureWatcher>
#include <QtConcurrent>
#endif
#include <QDesktopServices>
#include <QDir>
#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <iterator>
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
#include "../identity/connections.hpp"
#include "rendering_controller.hpp"
#include "emoji_controller.hpp"
#include "rich_inline_text_item.hpp"
#include "rich_composer_controller.hpp"
#include "appearance_controller.hpp"
#include "localization_controller.hpp"
#include "media_upload_controller.hpp"
#include "media_classifier_controller.hpp"
#include "media_playback_coordinator.hpp"
#include "../network/geo.hpp"
#include "../network/link_preview.hpp"
#include "../social/cast_views.hpp"
#include "../miniapp/host.hpp"
#include "../media/web_host.hpp"
#include "../miniapp/catalog.hpp"
#include "../miniapp/tasks.hpp"
#include "../data/insights.hpp"
#include "../network/node_selection.hpp"
#include "../update/controller.hpp"

#if defined(Q_OS_WASM)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "../media/remote_image.hpp"
#include "../identity/browser_auth.hpp"
#if defined(Q_OS_WASM)
#include "../browser/signer_bridge.hpp"
#include "../browser/cast_payload.hpp"
#endif
#include "../data/storage.hpp"

// This adapter wires Qt signals to stable domain/application contracts.
// Concrete provider, storage and signer implementations belong only to
// genycaster.ui.runtime, not to the GUI-facing request observers.
import genycaster.ui.runtime;
import farcaster.application.read.model;
import farcaster.application.read.async_gateway;
import farcaster.application.read.async_service;
import farcaster.application.session;
import farcaster.application.signer;
import farcaster.application.write;
import farcaster.capability.auth;
import farcaster.capability.feed;
import farcaster.capability.notification;
import farcaster.capability.pagination;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.session;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
#if !defined(Q_OS_WASM) && FARCASTER_HAS_PROTOCOL_SIGNING && FARCASTER_HAS_FARCASTER_AUTH
// A C++ module import does not transitively expose declarations imported by
// other modules. Native recovery stores an ICustodyAuthorizer in RecoveryBundle,
// so this translation unit must import the defining capability module itself.
import farcaster.capability.signer;
import farcaster.platform.custody_recovery;
import farcaster.provider.compatibility.auth;
#endif
import farcaster.local.write;
import farcaster.presentation.media;
import farcaster.routing.policy;
import farcaster.provider.model;
import farcaster.routing.router;
import farcaster.capability.model;
import farcaster.transport.async_http;
import farcaster.transport.http;
#if FARCASTER_HAS_HYPERSNAP_PROVIDER
// The Qt adapter uses AsyncGateway directly for provider-specific block reads.
// Imports from genycaster.ui.runtime are not re-exported to this unit.
import farcaster.provider.hypersnap.async_gateway;
#endif
#if FARCASTER_HAS_MEDIA_UPLOAD
import farcaster.application.media.upload;
import farcaster.application.media.lifecycle;
import farcaster.capability.media;
#endif

#include "bindings.hpp"

namespace genycaster::ui::bindings {

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

[[nodiscard]] QString readableCastText(const domain::cast::Cast& cast);
void attachChannelPresentation(QVariantMap& row, const domain::cast::Cast& cast);

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
    result.insert(QStringLiteral("text"), readableCastText(*referenced));
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

[[nodiscard]] QVariantMap threadReplyPresentation(
    const domain::cast::Cast& cast, int depth = 0, bool nested = true);

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

[[nodiscard]] QVariantMap threadReplyPresentation(
    const domain::cast::Cast& cast, const int depth, const bool nested)
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
    QVariantMap result{
        {QStringLiteral("castId"), QString::fromStdString(cast.id.value)},
        {QStringLiteral("sourceClient"), cast.sourceClient
            ? QString::fromStdString(*cast.sourceClient) : QString{}},
        {QStringLiteral("signerPublicKeyHex"), cast.signerPublicKeyHex
            ? QString::fromStdString(*cast.signerPublicKeyHex) : QString{}},
        {QStringLiteral("sourceAppFid"), cast.sourceAppFid
            ? QString::number(cast.sourceAppFid->value) : QString{}},
        {QStringLiteral("authorFid"), authorFid},
        {QStringLiteral("author"), username(authorProfile)},
        {QStringLiteral("displayName"), resolvedName},
        {QStringLiteral("avatarUrl"), avatarUrl(authorProfile)},
        {QStringLiteral("text"), readableCastText(cast)},
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
        {QStringLiteral("threadReplies"), nested ? threadReplyItems(cast, depth) : QVariantList{}},
    };
    attachChannelPresentation(result, cast);
    return result;
}

// Present thread replies as a flat, ordered sequence prepared on the read
// worker. The Qt view virtualizes this list; no reply delegate owns a nested
// subtree or recursively constructs every avatar/embed/engagement bar.
[[nodiscard]] QVariantList flatThreadReplyItems(const domain::cast::Cast& root)
{
    struct Pending {
        const domain::cast::Cast* cast{};
        int depth{};
    };
    std::vector<Pending> pending{};
    pending.reserve(root.directReplies.size());
    for (auto it = root.directReplies.rbegin(); it != root.directReplies.rend(); ++it) {
        pending.push_back(Pending{&*it, 1});
    }
    QVariantList rows{};
    while (!pending.empty()) {
        const auto [reply, depth] = pending.back();
        pending.pop_back();
        rows.push_back(threadReplyPresentation(*reply, depth, false));
        // Preserve stable parent-before-child ordering while allowing a
        // virtualized QML delegate pool to render only relevant rows.
        for (auto it = reply->directReplies.rbegin(); it != reply->directReplies.rend(); ++it) {
            pending.push_back(Pending{&*it, depth + 1});
        }
    }
    return rows;
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
    parent.insert(QStringLiteral("text"), readableCastText(*parentCast));
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

    // Resolve parent and quoted context together, in the same order that
    // casts enter the viewport. Parent-first over the whole page exhausted
    // the hydration budget before reaching most quote cards, leaving permanent
    // "Quoted cast" placeholders despite successful feed pagination.
    for (const auto& cast : state.casts) {
        if (cast.parent) append(*cast.parent);
        for (const auto& embed : cast.embeds) {
            if (embed.kind == domain::cast::EmbedKind::CastReference)
                append(domain::cast::CastId{embed.value});
            if (result.size() >= limit) break;
        }
        if (result.size() >= limit) break;
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
        {QStringLiteral("text"), readableCastText(*state.cast)},
        {QStringLiteral("createdAtMs"), static_cast<qlonglong>(state.cast->createdAt.time_since_epoch().count())},
        {QStringLiteral("embeds"), embedItems(*state.cast)},
        {QStringLiteral("replyCount"), static_cast<qulonglong>(state.cast->engagement.replies)},
        {QStringLiteral("recastCount"), static_cast<qulonglong>(state.cast->engagement.recasts)},
        {QStringLiteral("likeCount"), static_cast<qulonglong>(state.cast->engagement.likes)},
    };
    attachChannelPresentation(preview, *state.cast);
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
        {QStringLiteral("socialCountsKnown"), profile->socialCountsKnown},
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
            {QStringLiteral("parentUrl"), channel.parentUrl
                ? QString::fromStdString(*channel.parentUrl) : QString{}},
            {QStringLiteral("imageUrl"), channel.imageUrl
                ? QString::fromStdString(*channel.imageUrl) : QString{}},
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
        QString::fromUtf8(genycaster::deployment::openProofIssuer.data()), {}, {});
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

// Preserve raw protocol text in the domain, use the verified hydrated handle
// projection only for human-facing feed, profile, reply and notification rows.
[[nodiscard]] QString readableCastText(const domain::cast::Cast& cast)
{
    return QString::fromStdString(cast.displayText.value_or(cast.text));
}

void attachChannelPresentation(QVariantMap& row, const domain::cast::Cast& cast)
{
    if (cast.channelId && !cast.channelId->empty())
        row.insert(QStringLiteral("channelId"), QString::fromStdString(*cast.channelId));
    if (cast.channelName && !cast.channelName->empty())
        row.insert(QStringLiteral("channelName"), QString::fromStdString(*cast.channelName));
    if (cast.channelImageUrl && !cast.channelImageUrl->empty())
        row.insert(QStringLiteral("channelImageUrl"),
                   remoteImageSource(QString::fromStdString(*cast.channelImageUrl)));
}


// Immutable feed-state values are prepared on the bounded read executor;
// never inspect a GUI-owned QObject in this function.
[[nodiscard]] QVariantList feedPresentationRows(
    const application::read::model::FeedState& state,
    const qulonglong authenticatedViewerFid,
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
            readableCastText(*cast),
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
        revisionParts.push_back(cast->sourceClient
            ? QString::fromStdString(*cast->sourceClient) : QString{});
        revisionParts.push_back(cast->signerPublicKeyHex
            ? QString::fromStdString(*cast->signerPublicKeyHex) : QString{});
        revisionParts.push_back(cast->sourceAppFid
            ? QString::number(cast->sourceAppFid->value) : QString{});
        const QString structuralRevision = revisionParts.join(QChar{0x1f});
        QVariantMap row{
            {QStringLiteral("_revision"), structuralRevision},
            {QStringLiteral("castId"), QString::fromStdString(cast->id.value)},
            {QStringLiteral("sourceClient"), cast->sourceClient
                ? QString::fromStdString(*cast->sourceClient) : QString{}},
            {QStringLiteral("signerPublicKeyHex"), cast->signerPublicKeyHex
                ? QString::fromStdString(*cast->signerPublicKeyHex) : QString{}},
            {QStringLiteral("sourceAppFid"), cast->sourceAppFid
                ? QString::number(cast->sourceAppFid->value) : QString{}},
            {QStringLiteral("authorFid"), static_cast<qulonglong>(cast->author.value)},
            {QStringLiteral("author"), username(profile)},
            {QStringLiteral("displayName"), displayName(profile)},
            {QStringLiteral("avatarUrl"), avatarUrl(profile)},
            {QStringLiteral("text"), readableCastText(*cast)},
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
        attachChannelPresentation(row, *cast);
        if (state.query.kind == application::read::model::FeedQuery::Kind::UserLikes &&
            state.query.fid && authenticatedViewerFid != 0
            && authenticatedViewerFid == static_cast<qulonglong>(state.query.fid->value)) {
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

[[nodiscard]] application::read::model::FeedQuery feedQueryForSurface(
    const QString& feed, const qulonglong fid, const QString& channel,
    const std::size_t pageSize)
{
    application::read::model::FeedQuery query{};
    using Kind = application::read::model::FeedQuery::Kind;
    if (feed == QStringLiteral("following")) query.kind = Kind::Following;
    else if (feed == QStringLiteral("nfts")) query.kind = Kind::Nfts;
    else if (feed == QStringLiteral("developers")) query.kind = Kind::Developers;
    else if (feed == QStringLiteral("recent")) query.kind = Kind::Recent;
    else if (feed == QStringLiteral("conversations")) query.kind = Kind::Conversations;
    else if (feed == QStringLiteral("channel")) query.kind = Kind::Channel;
    else if (feed == QStringLiteral("user-posts")) query.kind = Kind::UserPosts;
    else if (feed == QStringLiteral("user-quotes")) query.kind = Kind::UserQuotes;
    else if (feed == QStringLiteral("user-replies")) query.kind = Kind::UserReplies;
    else if (feed == QStringLiteral("user-likes")) query.kind = Kind::UserLikes;
    else if (feed == QStringLiteral("user-popular")) query.kind = Kind::UserPopular;
    else query.kind = Kind::Trending;

    if (query.kind == Kind::Following || query.kind == Kind::UserPosts ||
        query.kind == Kind::UserQuotes || query.kind == Kind::UserReplies ||
        query.kind == Kind::UserLikes || query.kind == Kind::UserPopular) {
        query.fid = domain::identity::Fid{static_cast<std::uint64_t>(fid)};
    }
    if (query.kind == Kind::Channel) {
        query.channel = domain::channel::ChannelId{channel.toStdString()};
    }
    query.limit = pageSize;
    return query;
}

void applyFeedState(PresentationBridge& bridge, application::read::model::FeedState state,
                    QVariantList preparedRows)
{
    if (state.localWindowAppend && state.metadata.phase == application::read::model::ReadPhase::Failed) {
        // The requested local slice failed. Existing viewport rows and its
        // local continuation remain valid; nothing is appended or erased.
        bridge.applyLocalFeedWindowFailure(errorText(state.metadata));
        return;
    }
    bridge.setCommunityFallback(state.metadata.communityFallback);
    bridge.setLocalDerivedRecent(state.localDerivedRecent);
    bridge.setLocalDerivedFollowing(state.localDerivedFollowing);
    QVariantList rows = std::move(preparedRows);
    QString selector{};
    QString view{QStringLiteral("feed")};
    switch (state.query.kind) {
    case application::read::model::FeedQuery::Kind::Following:
        selector = QStringLiteral("following");
        break;
    case application::read::model::FeedQuery::Kind::Trending:
        selector = QStringLiteral("trending");
        break;
    case application::read::model::FeedQuery::Kind::Recent:
        selector = QStringLiteral("recent");
        break;
    case application::read::model::FeedQuery::Kind::Conversations:
        selector = QStringLiteral("conversations");
        break;
    case application::read::model::FeedQuery::Kind::Nfts:
        selector = QStringLiteral("nfts");
        break;
    case application::read::model::FeedQuery::Kind::Developers:
        selector = QStringLiteral("developers");
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
        state.metadata.isStale, state.metadata.isSourceUnavailable,
        state.query.continuation.has_value() || state.localWindowAppend,
        !state.query.continuation && !state.localWindowAppend && state.localHasMore);
    if (!state.query.continuation || state.localWindowAppend) {
        bridge.applyLocalFeedWindowProgress(
            state.localHasMore, static_cast<qulonglong>(state.localNextOffset));
    }
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
        case capability::notification::NotificationKind::Quote:
            kind = QStringLiteral("quote");
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
            attachChannelPresentation(row, *item.cast);
            row.insert(QStringLiteral("castId"), QString::fromStdString(item.cast->id.value));
            row.insert(QStringLiteral("castAuthorFid"), static_cast<qulonglong>(item.cast->author.value));
            row.insert(QStringLiteral("castParentId"), item.cast->parent ? QString::fromStdString(item.cast->parent->value) : QString{});
            row.insert(QStringLiteral("castText"), readableCastText(*item.cast));
            if (item.cast->parentUrl) {
                const QString channelUrl = QString::fromStdString(*item.cast->parentUrl);
                if (channelUrl.contains(QStringLiteral("/channel/"))) {
                    row.insert(QStringLiteral("castChannel"), channelUrl.section(QLatin1Char('/'), -1));
                }
            }
            row.insert(QStringLiteral("castReplyCount"), static_cast<qulonglong>(item.cast->engagement.replies));
            row.insert(QStringLiteral("castRecastCount"), static_cast<qulonglong>(item.cast->engagement.recasts));
            row.insert(QStringLiteral("castLikeCount"), static_cast<qulonglong>(item.cast->engagement.likes));
            row.insert(QStringLiteral("castEmbedCount"), static_cast<qulonglong>(item.cast->embeds.size()));
            for (const auto& embed : item.cast->embeds) {
                if (embed.kind == domain::cast::EmbedKind::CastReference && !embed.value.empty()
                    && item.kind == capability::notification::NotificationKind::Quote) {
                    row.insert(QStringLiteral("quotedCastId"), QString::fromStdString(embed.value));
                }
                if (embed.kind == domain::cast::EmbedKind::Url && !embed.value.empty()
                    && !row.contains(QStringLiteral("castEmbedUrl"))) {
                    row.insert(QStringLiteral("castEmbedUrl"), QString::fromStdString(embed.value));
                }
            }
        }
        if (item.parentCast) {
            row.insert(QStringLiteral("parentCastId"), QString::fromStdString(item.parentCast->id.value));
            row.insert(QStringLiteral("parentText"), readableCastText(*item.parentCast));
            if (!row.contains(QStringLiteral("castChannel")) && item.parentCast->parentUrl) {
                const QString channelUrl = QString::fromStdString(*item.parentCast->parentUrl);
                if (channelUrl.contains(QStringLiteral("/channel/"))) {
                    row.insert(QStringLiteral("castChannel"), channelUrl.section(QLatin1Char('/'), -1));
                }
            }
            row.insert(QStringLiteral("parentReplyCount"), static_cast<qulonglong>(item.parentCast->engagement.replies));
            for (const auto& embed : item.parentCast->embeds) {
                if (embed.kind == domain::cast::EmbedKind::Url && !embed.value.empty()) {
                    row.insert(QStringLiteral("parentEmbedUrl"), QString::fromStdString(embed.value));
                    break;
                }
            }
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
    // Recovery is a local Farcaster custody ceremony, not Genyleap/OpenProof
    // OAuth. Never label recovery failures as provider sign-in rejection.
    // Invalid individual recovery words and checksum errors are handled in
    // the editable browser dialog before this session callback is reached.
    if (route == QStringLiteral("recovery")) {
        return qtTrId("genycaster.setup.recovery_failed_retry");
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
    case OperationKind::Block: return QStringLiteral("block");
    case OperationKind::Unblock: return QStringLiteral("unblock");
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
    case OperationKind::Block: return QStringLiteral("block");
    case OperationKind::Unblock: return QStringLiteral("unblock");
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
        attachChannelPresentation(presentation, cast);
        presentation.insert(QStringLiteral("castId"), QString::fromStdString(cast.id.value));
        presentation.insert(QStringLiteral("sourceClient"), QStringLiteral("Genycaster"));
        presentation.insert(QStringLiteral("authorFid"), static_cast<qulonglong>(cast.author.value));
        presentation.insert(QStringLiteral("author"), author);
        presentation.insert(QStringLiteral("displayName"), display);
        presentation.insert(QStringLiteral("avatarUrl"), avatar);
        presentation.insert(QStringLiteral("text"), readableCastText(cast));
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
#if defined(Q_OS_WASM)
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

// A selected node is authoritative for Block state. Never silently fall
// back to a different network just to decorate the profile.
void refreshProfileBlocked(
    PresentationBridge& bridge,
    const std::shared_ptr<Runtime>& runtime,
    const domain::identity::Fid viewer,
    const domain::identity::Fid profile,
    const bool outgoing)
{
    if (viewer.value == 0 || profile.value == 0 || viewer.value == profile.value)
        return;
    using Gateway = farcaster::provider::hypersnap::async_gateway::AsyncGateway;
    Gateway* gateway = runtime->asyncGateway.get();
    const auto& selected = runtime->requestedReadProviderId;
    if (selected == "farcaster-snapchain") {
        gateway = runtime->farcasterReadGateway.get();
    } else if (!selected.empty() && selected != "hypersnap-v2") {
        gateway = nullptr;
        for (std::size_t i = 0; i < runtime->additionalReadProviderIds.size(); ++i) {
            if (runtime->additionalReadProviderIds[i] == selected) {
                gateway = runtime->additionalReadGateways[i].get();
                break;
            }
        }
    }
    if (!gateway) return; // The selected provider has no compatible link reader.

    const auto source = outgoing ? viewer : profile;
    const auto target = outgoing ? profile : viewer;
    const std::string key = std::to_string(viewer.value) + ":"
        + std::to_string(profile.value) + (outgoing ? ":block-out" : ":block-in");
    {
        std::scoped_lock lock{runtime->profileRelationshipMutex};
        if (!runtime->followsMeInFlight.insert(key).second) return;
    }
    const QPointer<PresentationBridge> bridgeGuard{&bridge};
    auto request = gateway->isBlocked(
        source, target,
        [runtime, bridgeGuard, viewer, profile, key, outgoing](
            foundation::result::Result<bool> relationship) mutable {
            {
                std::scoped_lock lock{runtime->profileRelationshipMutex};
                runtime->followsMeInFlight.erase(key);
                runtime->followsMeRequests.erase(key);
            }
            if (!relationship || !bridgeGuard) return;
            QMetaObject::invokeMethod(bridgeGuard.data(),
                [bridgeGuard, viewer, profile, outgoing, blocked = *relationship] {
                    if (bridgeGuard && bridgeGuard->isAuthenticated()
                        && bridgeGuard->accountFid() == viewer.value)
                        bridgeGuard->applyProfileBlockRelation(profile.value, outgoing, blocked);
                }, Qt::QueuedConnection);
        });
    if (!request) {
        std::scoped_lock lock{runtime->profileRelationshipMutex};
        runtime->followsMeInFlight.erase(key);
        return;
    }
    std::scoped_lock lock{runtime->profileRelationshipMutex};
    runtime->followsMeRequests.insert_or_assign(key, std::move(*request));
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
    // Provider batches are at most 64 targets, but a feed page may be
    // larger. Defer overflow instead of silently discarding casts 65..100.
    // Identity-based admission applies to the casts being hydrated, not the
    // position of the page within an accumulated feed snapshot. The cache only
    // stores complete target-specific evidence, never partial absence.
    std::vector<domain::cast::CastReference> pendingTargets{};
    std::uint64_t generation{};
    std::uint64_t surfaceGeneration{};
    std::optional<application::read::async_gateway::Request> obsoleteRequest{};
    {
        std::scoped_lock lock{runtime->viewerOperationMutex};
        if (runtime->activeViewerFid != accountFid.value) {
            ++runtime->viewerOperationGeneration;
            ++runtime->viewerSurfaceGeneration;
            runtime->viewerReactionDeliveredGeneration.clear();
            // Never cancel while holding the owner lock: completion can be inline.
            obsoleteRequest = std::move(runtime->activeViewerReactionRequest);
            runtime->activeViewerReactionRequest.reset();
            runtime->viewerReactionRegistering = false;
            runtime->activeViewerTargetIds.clear();
            runtime->deferredViewerTargets.clear();
            // A new account has a different identity namespace even if its
            // visible rows reuse exactly the same cast IDs.
            runtime->viewerReactionFreshUntil.clear();
            runtime->activeViewerFid = accountFid.value;
        }
    }
    if (obsoleteRequest) {
        obsoleteRequest->cancel();
    }
    {
        std::scoped_lock lock{runtime->viewerOperationMutex};
        const auto now = std::chrono::steady_clock::now();
        std::unordered_set<std::string> unique{};
        for (const auto& target : targets) {
            if (target.id.value.empty() || !unique.insert(target.id.value).second
                || !runtime->viewerReactionConsumerCounts.contains(target.id.value)) {
                // A recycled row must not make a deferred retry acquire a cast
                // without an active, account-scoped consumer.
                continue;
            }
            const auto like = runtime->viewerReactionFreshUntil.find("like:" + target.id.value);
            const auto recast = runtime->viewerReactionFreshUntil.find("recast:" + target.id.value);
            if (like != runtime->viewerReactionFreshUntil.end() && like->second > now
                && recast != runtime->viewerReactionFreshUntil.end() && recast->second > now) {
                continue;
            }
            pendingTargets.push_back(target);
        }
        if (pendingTargets.empty()) {
            return;
        }
        constexpr std::size_t maximumBatch{64U};
        if (pendingTargets.size() > maximumBatch) {
            for (std::size_t i = maximumBatch; i < pendingTargets.size(); ++i) {
                const auto& target = pendingTargets[i];
                const bool queued = std::ranges::any_of(runtime->deferredViewerTargets,
                    [&](const auto& other) { return other.id.value == target.id.value; });
                if (!queued && runtime->deferredViewerTargets.size() < 512U) {
                    runtime->deferredViewerTargets.push_back(target);
                }
            }
            pendingTargets.resize(maximumBatch);
        }
        // The provider operation may serve other live consumers. Queue new
        // targets and drain them on completion instead of canceling its work.
        if (runtime->activeViewerReactionRequest || runtime->viewerReactionRegistering) {
            for (const auto& target : pendingTargets) {
                const bool active = std::ranges::find(runtime->activeViewerTargetIds,
                    target.id.value) != runtime->activeViewerTargetIds.end();
                const bool deferred = std::ranges::any_of(runtime->deferredViewerTargets,
                    [&](const auto& queued) { return queued.id.value == target.id.value; });
                if (!active && !deferred && runtime->deferredViewerTargets.size() < 512U) {
                    runtime->deferredViewerTargets.push_back(target);
                }
            }
            return;
        }
        generation = ++runtime->viewerOperationGeneration;
        surfaceGeneration = runtime->viewerSurfaceGeneration;
        runtime->viewerReactionRegistering = true;
        runtime->activeViewerReactionRequest.reset();
        runtime->activeViewerTargetIds.clear();
        for (const auto& target : pendingTargets) {
            runtime->activeViewerTargetIds.push_back(target.id.value);
        }
    }
    auto rowTargets = pendingTargets;
    // Keep the rejected batch's demand; a synchronous failure must not cause
    // this active viewport to be permanently forgotten.
    auto rejectedTargets = pendingTargets;
    auto terminal = std::make_shared<std::atomic_bool>(false);
    const QPointer<PresentationBridge> bridgeGuard{&bridge};
    auto request = runtime->asyncGateway->viewerReactionsFor(
        accountFid,
        std::move(pendingTargets),
        [bridgeGuard, runtime, accountFid, generation, surfaceGeneration, terminal,
         rowTargets = std::move(rowTargets)](
            foundation::result::Result<domain::feed::ViewerReactionSnapshot> snapshot) mutable {
            terminal->store(true, std::memory_order_release);
            std::vector<domain::cast::CastReference> deferredTargets{};
            {
                std::scoped_lock lock{runtime->viewerOperationMutex};
                if (generation != runtime->viewerOperationGeneration) {
                    return;
                }
                runtime->activeViewerReactionRequest.reset();
                runtime->viewerReactionRegistering = false;
                runtime->activeViewerTargetIds.clear();
                deferredTargets = std::move(runtime->deferredViewerTargets);
                runtime->deferredViewerTargets.clear();
                // Worker completion owns operation cleanup only. Freshness cannot
                // be published until an accepted GUI delivery has applied evidence.
                // Otherwise a later batch may suppress this result while its targets
                // are marked fresh and cannot be retried.
            }
            if (!snapshot) {
                // Failure cannot erase active demand. Retry this page and any
                // targets admitted during the request after a bounded cooldown.
                rowTargets.insert(rowTargets.end(),
                    std::make_move_iterator(deferredTargets.begin()),
                    std::make_move_iterator(deferredTargets.end()));
                if (bridgeGuard) {
                    QMetaObject::invokeMethod(bridgeGuard.data(),
                        [bridgeGuard, runtime, surfaceGeneration, accountFid,
                         retryTargets = std::move(rowTargets)]() mutable {
                            if (!bridgeGuard) {
                                return;
                            }
                            QTimer::singleShot(15'000, bridgeGuard.data(),
                                [bridgeGuard, runtime, surfaceGeneration, accountFid,
                                 retryTargets = std::move(retryTargets)]() mutable {
                                    if (!bridgeGuard || !bridgeGuard->isAuthenticated()
                                        || bridgeGuard->accountFid() != accountFid.value) {
                                        return;
                                    }
                                    {
                                        std::scoped_lock lock{runtime->viewerOperationMutex};
                                        if (surfaceGeneration != runtime->viewerSurfaceGeneration
                                            || runtime->activeViewerFid != accountFid.value) {
                                            return;
                                        }
                                    }
                                    refreshVisibleViewerReactionState(*bridgeGuard, runtime,
                                        accountFid, std::move(retryTargets));
                                });
                        }, Qt::QueuedConnection);
                }
                return;
            }
            QVariantList rows{};
            rows.reserve(static_cast<qsizetype>(rowTargets.size()));
            for (const auto& target : rowTargets) {
                // A missing target from an incomplete enrichment is unknown.
                // Never let it clear optimistic or previously confirmed state.
                const auto isPresent = [&target](const std::vector<domain::cast::CastId>& ids) {
                    return std::ranges::any_of(ids, [&target](const domain::cast::CastId& id) {
                        return id.value == target.id.value;
                    });
                };
                const bool knowsLike = isPresent(snapshot->likeExamined);
                const bool knowsRecast = isPresent(snapshot->recastExamined);
                if (!knowsLike && !knowsRecast) {
                    continue;
                }
                QVariantMap reaction{{QStringLiteral("castId"),
                                      QString::fromStdString(target.id.value)}};
                if (knowsLike) {
                    reaction.insert(QStringLiteral("liked"), isPresent(snapshot->liked));
                }
                if (knowsRecast) {
                    reaction.insert(QStringLiteral("recasted"), isPresent(snapshot->recasted));
                }
                rows.push_back(std::move(reaction));
            }
            PresentationBridge* const targetBridge = bridgeGuard.data();
            if (targetBridge == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                targetBridge,
                [bridgeGuard, runtime, generation, surfaceGeneration, accountFid,
                 rows = std::move(rows), rowTargets = std::move(rowTargets),
                 deferredTargets = std::move(deferredTargets)]() mutable {
                    if (!bridgeGuard || !bridgeGuard->isAuthenticated()
                        || bridgeGuard->accountFid() != accountFid.value) {
                        return;
                    }
                    QVariantList accepted{};
                    accepted.reserve(rows.size());
                    {
                        std::scoped_lock lock{runtime->viewerOperationMutex};
                        // Batch B may start before batch A reaches this queued GUI
                        // delivery. Only a navigation/account epoch invalidates A.
                        if (surfaceGeneration != runtime->viewerSurfaceGeneration
                            || runtime->activeViewerFid != accountFid.value) {
                            return;
                        }
                        const auto now = std::chrono::steady_clock::now();
                        for (auto& row : rows) {
                            auto item = row.toMap();
                            const auto castId = item.value(QStringLiteral("castId")).toString().toStdString();
                            for (const auto& [role, prefix] : {
                                     std::pair{QStringLiteral("liked"), std::string_view{"like:"}},
                                     std::pair{QStringLiteral("recasted"), std::string_view{"recast:"}}}) {
                                if (!item.contains(role)) {
                                    continue;
                                }
                                const std::string key = std::string{prefix} + castId;
                                auto& lastDelivered = runtime->viewerReactionDeliveredGeneration[key];
                                if (lastDelivered > generation) {
                                    item.remove(role);
                                    continue;
                                }
                                lastDelivered = generation;
                                runtime->viewerReactionFreshUntil.insert_or_assign(
                                    key, now + std::chrono::seconds{90});
                            }
                            if (item.size() > 1) {
                                accepted.push_back(std::move(item));
                            }
                        }
                        // Partial/unknown results get only a short cooldown, which
                        // is renewed on a later probe once the previous entry expires.
                        for (const auto& target : rowTargets) {
                            for (const std::string_view prefix : {"like:", "recast:"}) {
                                const std::string key = std::string{prefix} + target.id.value;
                                const auto existing = runtime->viewerReactionFreshUntil.find(key);
                                if (existing == runtime->viewerReactionFreshUntil.end()
                                    || existing->second <= now) {
                                    runtime->viewerReactionFreshUntil.insert_or_assign(
                                        key, now + std::chrono::seconds{15});
                                }
                            }
                        }
                        constexpr std::size_t maximumEntries{256};
                        if (runtime->viewerReactionFreshUntil.size() > maximumEntries) {
                            std::erase_if(runtime->viewerReactionFreshUntil,
                                [now](const auto& entry) { return entry.second <= now; });
                            if (runtime->viewerReactionFreshUntil.size() > maximumEntries) {
                                runtime->viewerReactionFreshUntil.clear();
                            }
                        }
                        if (runtime->viewerReactionDeliveredGeneration.size() > maximumEntries) {
                            // Order is relevant only within the bounded evidence lifetime.
                            // Versions grow monotonically and never cross an account change.
                            runtime->viewerReactionDeliveredGeneration.clear();
                        }
                    }
                    runtime->viewerStateLastRefreshMs.store(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count(),
                        std::memory_order_release);
                    if (!accepted.isEmpty()) {
                        bridgeGuard->applyViewerReactionDelta(std::move(accepted));
                    }
                    if (!deferredTargets.empty()) {
                        refreshVisibleViewerReactionState(*bridgeGuard, runtime,
                            accountFid, std::move(deferredTargets));
                    }
                },
                Qt::QueuedConnection);
        });
    if (!request) {
        {
            std::scoped_lock lock{runtime->viewerOperationMutex};
            if (generation != runtime->viewerOperationGeneration
                || terminal->load(std::memory_order_acquire)) {
                // A provider can complete inline before returning its handle.
                // Such a completion is already terminal: no second retry.
                return;
            }
            runtime->activeViewerTargetIds.clear();
            runtime->viewerReactionRegistering = false;
            rejectedTargets.insert(rejectedTargets.end(),
                std::make_move_iterator(runtime->deferredViewerTargets.begin()),
                std::make_move_iterator(runtime->deferredViewerTargets.end()));
            runtime->deferredViewerTargets.clear();
        }
        // Provider-admission failure is not a terminal rejection of active
        // demand. Retry at a bounded cadence while this viewer remains active.
        QTimer::singleShot(15'000, &bridge,
            [bridgeGuard, runtime, accountFid, surfaceGeneration,
             targets = std::move(rejectedTargets)]() mutable {
                if (!bridgeGuard || !bridgeGuard->isAuthenticated()
                    || bridgeGuard->accountFid() != accountFid.value) {
                    return;
                }
                {
                    std::scoped_lock lock{runtime->viewerOperationMutex};
                    if (surfaceGeneration != runtime->viewerSurfaceGeneration
                        || runtime->activeViewerFid != accountFid.value) {
                        return;
                    }
                }
                refreshVisibleViewerReactionState(*bridgeGuard, runtime,
                    accountFid, std::move(targets));
            });
        return;
    }
    std::scoped_lock lock{runtime->viewerOperationMutex};
    if (generation != runtime->viewerOperationGeneration
        || terminal->load(std::memory_order_acquire)) {
        request->cancel();
        return;
    }
    runtime->activeViewerReactionRequest = std::move(*request);
    runtime->viewerReactionRegistering = false;
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
    OpenProofConnectionsController& connections,
    const std::shared_ptr<Runtime>& runtime,
    const bool authenticationAvailable,
    std::string siwfDomain,
    std::string siwfUri,
    const QString& browserSnapchainOrigin)
{
#if defined(Q_OS_WASM)
    auto* const browserSigner = new genycaster::ui::browser_signing::Bridge(
        bridge, richComposer, browserSnapchainOrigin,
        connections.recoveryGatewayOrigin());
    browserSigner->setAccountSessionObserver(
        [&connections](const qulonglong fid, QByteArray csrf) {
            connections.acceptBrowserAccountSession(fid,std::move(csrf));
        });
#else
    Q_UNUSED(browserSnapchainOrigin);
#endif
    const auto applyNetworkAffinity = [runtime, &bridge](const QString& network) {
        if (!runtime->router) return;
        const bool hosted = network == bridge.hostedNetworkKey();
        // Existing two-way setting remains compatible, but route selection is
        // now by registered provider instance id, not hardcoded vendor logic.
        runtime->router->setPreferredNetwork(
            hosted ? routing::policy::PreferredNetwork::Hosted
                   : routing::policy::PreferredNetwork::Independent);
        const std::string& providerId = hosted
            ? runtime->hostedReadProviderId : runtime->independentReadProviderId;
        if (providerId.empty()) {
            runtime->router->clearPreferredProviderForAccess(
                farcaster::capability::model::CapabilityAccess::Read);
        } else {
            runtime->router->setPreferredProviderForAccess(
                farcaster::capability::model::CapabilityAccess::Read,
                routing::router::ProviderPreference{
                    .provider = farcaster::provider::model::ProviderId{providerId},
                    .require = false,
                });
        }
    };
    applyNetworkAffinity(bridge.preferredNetwork());
    // An explicitly selected registered provider overrides the legacy two-way
    // selection on startup. The next user selection supersedes this pin.
    if (runtime->router && !runtime->requestedReadProviderId.empty()) {
        runtime->router->setPreferredProviderForAccess(
            farcaster::capability::model::CapabilityAccess::Read,
            routing::router::ProviderPreference{
                .provider = farcaster::provider::model::ProviderId{
                    runtime->requestedReadProviderId},
                .require = !runtime->allowCompatibleReadFallback,
            });
    }
    QObject::connect(&bridge, &PresentationBridge::preferredNetworkChanged, &bridge,
        [&bridge, runtime, applyNetworkAffinity](const QString& network) {
            runtime->requestedReadProviderId.clear();
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
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ] (application::session::SessionState state) {
            const auto signerSession = state.session;
            runtime->setCurrentSession(signerSession);
            QMetaObject::invokeMethod(
                &bridge,
                [&bridge, runtime, state = std::move(state), authenticationAvailable
#if defined(Q_OS_WASM)
                 , browserSigner
#endif
                ]() mutable {
#if defined(Q_OS_WASM)
                    const auto completedPhase = state.phase;
                    const auto verifiedFid = state.session ? state.session->fid.value : 0U;
                    const bool deviceLocalSession = state.session
                        && state.session->identityIssuer == "genycaster:device-local";
#endif
                    applySessionState(bridge, std::move(state), authenticationAvailable);
                    runtime->authenticationPollInFlight.store(false, std::memory_order_release);
                    hydrateAuthenticatedAccountProfile(bridge, runtime);
#if defined(Q_OS_WASM)
                    // Restore only after PresentationBridge has committed
                    // the actual signed-in FID; never race queued session UI.
                    if (completedPhase == application::session::SessionPhase::Authenticated
                        && verifiedFid != 0U) {
                        // A sign-in only restores an already-authorized signer.
                        // Never open the recovery dialog without explicit user action.
                        browserSigner->start(QStringLiteral("restore"),
                            QString::number(verifiedFid),
                            deviceLocalSession ? QJsonObject{}
                                : QJsonObject{{QStringLiteral("rememberLocal"), true}});
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

#if defined(Q_OS_WASM)
    // A browser-safe OpenProof device grant is independent from the blocking
    // native authentication adapter and from protocol signer authorization.
    auto* const browserAuth = new BrowserOpenProofAuth(&bridge);
    const std::function<void(application::session::SessionState)> applyBrowserSession = sessionObserver;
    const auto browserObserver = [applyBrowserSession, &bridge](BrowserOpenProofState event) {
        // A cancelled, asynchronous Genyleap/OpenProof result must never
        // overwrite the independent Farcaster recovery ceremony on mobile.
        // Recovery uses a separate identity callback, but explicit sign-out
        // must always clear the current session even when its last sign-in
        // route was recovery. Otherwise both Sign Out and local signer reset
        // appear ineffective on a recovered Farcaster account.
        if (bridge.authenticationRoute() == QStringLiteral("recovery")
            && event.phase != BrowserOpenProofState::Phase::SignedOut) return;
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
                .identityIssuer = std::string{genycaster::deployment::openProofIssuer},
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
        [sessionObserver, &connections, &bridge](QString fid, QString error,
                          const bool verified, const bool restoredDevice,
                          QString openProofSubject, QString openProofIssuer,
                          QByteArray accountTicket, const int accountExpiresSeconds) {
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
                const bool canonicalOpenProof = !restoredDevice
                    && !openProofSubject.isEmpty() && openProofSubject.size() <= 512
                    && openProofIssuer == connections.identityIssuer();
                state.session = domain::session::ClientSession{
                    .identityIssuer = canonicalOpenProof
                        ? openProofIssuer.toStdString()
                        : restoredDevice
                        ? std::string{"genycaster:device-local"}
                        : std::string{"farcaster:optimism-idregistry"},
                    .identitySubject = canonicalOpenProof
                        ? openProofSubject.toStdString()
                        : std::string{"fid:"} + fid.toStdString(),
                    .fid = domain::identity::Fid{static_cast<std::uint64_t>(number)},
                    .method = domain::session::AuthenticationMethod::Custody,
                    .authenticatedAt = authenticatedAt,
                    .expiresAt = authenticatedAt
                        + std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::hours{1}),
                };
            }
            sessionObserver(std::move(state));
            // The session commit was queued onto the bridge. Deliver the
            // ephemeral OpenProof account capability after that same queue
            // establishes the verified issuer and owning Farcaster FID.
            if (verified && !restoredDevice && !openProofSubject.isEmpty()
                && openProofIssuer == connections.identityIssuer()
                && !accountTicket.isEmpty()) {
                bool validFid = false;
                const auto ownerFid = fid.toULongLong(&validFid);
                if (validFid) {
                    QMetaObject::invokeMethod(
                        &bridge,
                        [&bridge, &connections, ticket = std::move(accountTicket),
                         subject = std::move(openProofSubject), ownerFid,
                         accountExpiresSeconds]() mutable {
                            if (bridge.isAuthenticated()
                                && bridge.accountFid() == ownerFid
                                && bridge.accountIdentityIssuer() == connections.identityIssuer()) {
                                connections.acceptRecoveryGrant(
                                    std::move(ticket), std::move(subject),
                                    ownerFid, accountExpiresSeconds);
                            } else {
                                ticket.fill('\0');
                            }
                        }, Qt::QueuedConnection);
                }
            }
        });
    QObject::connect(&bridge, &PresentationBridge::signInRequested, browserAuth,
        [browserAuth, browserObserver, browserSigner, &bridge, &connections, sessionObserver](const QString& route) {
            if (route == QStringLiteral("openproof")) {
                browserSigner->cancelRecovery();
                browserAuth->begin(browserObserver,
                    [&bridge, &connections](QByteArray bearer, QString subject,
                                   const std::uint64_t fid, const int expiresSeconds) {
                        // BrowserOpenProofAuth posts its verified session state
                        // before calling this grant callback. Queue both GUI
                        // operations to the same bridge receiver and priority:
                        // unlike a zero-timer, this makes their order strict.
                        QMetaObject::invokeMethod(&bridge,
                            [&bridge, &connections, bearer = std::move(bearer),
                             subject = std::move(subject), fid, expiresSeconds]() mutable {
                                if (!bridge.isAuthenticated()
                                    || bridge.accountFid() != fid
                                    || bridge.authenticationRoute() != QStringLiteral("openproof")
                                    || bridge.accountIdentityIssuer() != connections.identityIssuer()) {
                                    bearer.fill('\0');
                                    return;
                                }
                                connections.acceptAuthenticatedGrant(
                                    std::move(bearer), std::move(subject),
                                    fid, expiresSeconds);
                            }, Qt::QueuedConnection);
                    });
            } else if (route == QStringLiteral("farcaster")) {
                browserSigner->cancelRecovery();
                browserAuth->beginFarcaster(browserObserver,
                    [&bridge, &connections](QByteArray ticket, QString subject,
                                           const std::uint64_t fid, const int expiresSeconds) {
                        // Farcaster channel proof was verified by OpenProof,
                        // including its sole linked FID. Queue the temporary
                        // account capability after the canonical UI session.
                        QMetaObject::invokeMethod(&bridge,
                            [&bridge, &connections, ticket = std::move(ticket),
                             subject = std::move(subject), fid, expiresSeconds]() mutable {
                                if (!bridge.isAuthenticated() || bridge.accountFid() != fid
                                    || bridge.authenticationRoute() != QStringLiteral("farcaster")
                                    || bridge.accountIdentityIssuer() != connections.identityIssuer()) {
                                    ticket.fill('\0');
                                    return;
                                }
                                connections.acceptRecoveryGrant(
                                    std::move(ticket), std::move(subject), fid, expiresSeconds);
                            }, Qt::QueuedConnection);
                    });
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
                     browserAuth, [browserAuth, browserSigner, sessionObserver] {
                         // Recovery sign-in does not install an OpenProof observer.
                         // Always send the authoritative local Guest transition,
                         // rather than relying on OpenProof::cancel() to do so.
                         // This also makes Sign Out idempotent across all routes.
                         browserSigner->forgetAccount();
                         browserAuth->signOut();
                         sessionObserver(application::session::SessionState{
                             .phase = application::session::SessionPhase::Guest,
                         });
                     });
#else
#if FARCASTER_HAS_PROTOCOL_SIGNING && FARCASTER_HAS_FARCASTER_AUTH
    // Native recovery remains off the GUI thread. This composition layer is
    // the only place that may briefly hold the phrase, and the provider sees
    // public custody identity plus a one-time signature only.
    struct RecoveryBundle final {
        domain::identity::Fid fid{};
        std::string issuer{};
        std::string subject{};
        std::string custodyAddress{};
        std::string accountTicket{};
        int expiresSeconds{};
        std::shared_ptr<capability::signer::ICustodyAuthorizer> oneShotSigner{};
    };
    using RecoveryOutcome = foundation::result::Result<RecoveryBundle>;
    auto nativeRecoveryGeneration = std::make_shared<std::atomic_uint64_t>(0U);
    QObject::connect(&bridge, &PresentationBridge::signInCancelRequested, &bridge,
        [nativeRecoveryGeneration] {
            nativeRecoveryGeneration->fetch_add(1U, std::memory_order_acq_rel);
        });
    QObject::connect(&bridge, &PresentationBridge::signOutRequested, &bridge,
        [nativeRecoveryGeneration] {
            nativeRecoveryGeneration->fetch_add(1U, std::memory_order_acq_rel);
        });
    QObject::connect(&bridge, &PresentationBridge::nativeRecoveryRequested, &bridge,
        [&bridge, &connections, runtime, sessionObserver, signerObserver,
         nativeRecoveryGeneration](QString words) {
            const std::uint64_t generation =
                nativeRecoveryGeneration->fetch_add(1U, std::memory_order_acq_rel) + 1U;
            const QString configuredGateway = connections.recoveryGatewayOrigin();
            if (configuredGateway.isEmpty()) {
                words.fill(QChar{char16_t{0}});
                bridge.applyAuthenticationError(
                    qtTrId("genycaster.setup.recovery_failed_retry"));
                return;
            }
            QByteArray utf8 = words.toUtf8();
            words.fill(QChar{char16_t{0}});
            sessionObserver(application::session::SessionState{
                .phase = application::session::SessionPhase::Starting,
            });

            auto* watcher = new QFutureWatcher<RecoveryOutcome>(&bridge);
            QObject::connect(watcher, &QFutureWatcher<RecoveryOutcome>::finished,
                &bridge,
                [&bridge, &connections, runtime, sessionObserver, signerObserver,
                 nativeRecoveryGeneration, generation, watcher] {
                    RecoveryOutcome verified = watcher->result();
                    watcher->deleteLater();
                    if (nativeRecoveryGeneration->load(std::memory_order_acquire)
                        != generation
                        || bridge.authenticationRoute() != QStringLiteral("recovery")
                        || bridge.isAuthenticated()) {
                        if (verified) {
                            std::fill(verified->accountTicket.begin(),
                                      verified->accountTicket.end(), char{0});
                        }
                        return;
                    }
                    if (!verified) {
                        // Never present raw gateway diagnostics that could
                        // contain a challenge or an authorization signature.
                        bridge.applyAuthenticationError(
                            qtTrId("genycaster.setup.recovery_failed_retry"));
                        return;
                    }
                    const std::uint64_t fid = verified->fid.value;
                    const auto authenticatedAt = foundation::time::now();
                    domain::session::ClientSession session{
                        .identityIssuer = std::move(verified->issuer),
                        .identitySubject = verified->subject,
                        .fid = domain::identity::Fid{fid},
                        .custodyAddress = verified->custodyAddress,
                        .method = domain::session::AuthenticationMethod::Custody,
                        .authenticatedAt = authenticatedAt,
                        .expiresAt = authenticatedAt
                            + std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::seconds{verified->expiresSeconds}),
                    };
                    // Only SessionService may commit the verified login. It
                    // persists this public snapshot before publishing the
                    // account to Runtime, signer gates and presentation.
                    auto ticket = std::make_shared<QByteArray>(
                        QByteArray::fromStdString(verified->accountTicket));
                    std::fill(verified->accountTicket.begin(),
                              verified->accountTicket.end(), char{0});
                    const QString subject = QString::fromStdString(verified->subject);
                    const int expires = verified->expiresSeconds;
                    const auto persistedObserver =
                        [&bridge, &connections, runtime, sessionObserver, signerObserver,
                         oneShotSigner = verified->oneShotSigner,
                         nativeRecoveryGeneration, generation, ticket,
                         subject, fid, expires]
                        (application::session::SessionState state) mutable {
                        const bool approved =
                            state.phase == application::session::SessionPhase::Authenticated
                            && state.session && state.session->fid.value == fid
                            && state.session->identityIssuer
                            && *state.session->identityIssuer
                                == genycaster::deployment::openProofIssuer;
                        if (nativeRecoveryGeneration->load(std::memory_order_acquire)
                            != generation) {
                            ticket->fill(char{0});
                            return;
                        }
                        sessionObserver(std::move(state));
                        if (!approved) {
                            ticket->fill(char{0});
                            return;
                        }
                        // Both transitions are queued to one GUI thread, so
                        // the grant cannot arrive before the persisted session.
                        QMetaObject::invokeMethod(&bridge,
                            [&bridge, &connections, runtime, signerObserver,
                             oneShotSigner, ticket, subject, fid,
                             expires, nativeRecoveryGeneration, generation]() mutable {
                                if (nativeRecoveryGeneration->load(std::memory_order_acquire)
                                        == generation
                                    && bridge.isAuthenticated()
                                    && bridge.accountFid() == fid
                                    && bridge.authenticationRoute() == QStringLiteral("recovery")
                                    && bridge.accountIdentityIssuer() == connections.identityIssuer()) {
                                    connections.acceptRecoveryGrant(
                                        std::move(*ticket), subject, fid, expires);
                                    // Login, signer metadata restoration and the
                                    // account grant all refer to the same FID.
                                    // Use the ephemeral recovery signer only for
                                    // one network-confirmed KEY_ADD registration.
                                    if (runtime->signerService && oneShotSigner) {
                                        auto active = runtime->currentSessionSnapshot();
                                        if (active && active->fid.value == fid
                                            && active->custodyAddress) {
                                            auto started = runtime->signerService->provision(
                                                std::move(active), signerObserver,
                                                std::move(oneShotSigner));
                                            if (!started) {
                                                applySignerState(bridge,
                                                    application::signer::SignerState{
                                                        .phase = application::signer::SignerPhase::Unavailable,
                                                        .error = std::move(started.error())});
                                            }
                                        }
                                    }
                                } else {
                                    ticket->fill(char{0});
                                }
                            }, Qt::QueuedConnection);
                    };
                    auto committed = runtime->sessionService->acceptVerifiedRecovery(
                        std::move(session), persistedObserver);
                    if (!committed) {
                        ticket->fill(char{0});
                        bridge.applyAuthenticationError(
                            qtTrId("genycaster.setup.recovery_failed_retry"));
                    }
                });
            watcher->setFuture(QtConcurrent::run(
                [runtime, gateway = configuredGateway.toStdString(),
                 words = std::move(utf8)]() mutable -> RecoveryOutcome {
                    std::string mnemonic{words.constData(),
                                         static_cast<std::size_t>(words.size())};
                    words.fill(char{0});
                    struct EraseOnExit final {
                        std::string& value;
                        ~EraseOnExit()
                        {
                            volatile char* const data = value.data();
                            for (std::size_t i{}; i < value.size(); ++i)
                                data[i] = 0;
                        }
                    } erase{mnemonic};
                    const auto failure = [] {
                        return foundation::error::Error::make(
                            foundation::error::ErrorCode::Unauthorized,
                            "Native recovery verification failed.");
                    };
                    auto address = farcaster::platform::custody_recovery::
                        derivePublicCustodyAddress(mnemonic);
                    if (!address) return std::unexpected(failure());
                    auto fid = farcaster::provider::compatibility::auth::
                        resolveRecoveryCustodyFid(*runtime->transport, *address);
                    if (!fid) return std::unexpected(failure());
                    auto challenge = farcaster::provider::compatibility::auth::
                        startRecoveryChallenge(*runtime->transport, gateway, *fid, *address);
                    if (!challenge) return std::unexpected(failure());
                    auto proof = farcaster::platform::custody_recovery::
                        signOneTimeIdentityProof(mnemonic, fid->value, challenge->message);
                    if (!proof || proof->address != *address)
                        return std::unexpected(failure());
                    auto verified = farcaster::provider::compatibility::auth::
                        completeRecoveryChallenge(
                            *runtime->transport, gateway, *challenge,
                            std::move(proof->signature));
                    if (!verified || verified->fid.value != fid->value)
                        return std::unexpected(failure());
                    verified->custodyAddress = std::move(*address);
                    // Derive a single-use EIP-712 signing capability only
                    // AFTER server-side FID/custody proof succeeds. No phrase
                    // or custody key material leaves this secure C++ boundary.
                    auto signer = farcaster::platform::custody_recovery::
                        makeOneShotRecoveryAuthorizer(mnemonic,
                                                      verified->custodyAddress,
                                                      verified->fid.value);
                    if (!signer) return std::unexpected(failure());
                    return RecoveryBundle{
                        .fid = verified->fid,
                        .issuer = std::move(verified->issuer),
                        .subject = std::move(verified->subject),
                        .custodyAddress = std::move(verified->custodyAddress),
                        .accountTicket = std::move(verified->accountTicket),
                        .expiresSeconds = verified->expiresSeconds,
                        .oneShotSigner = std::move(*signer),
                    };
                }));
        });
#else
    QObject::connect(&bridge, &PresentationBridge::nativeRecoveryRequested, &bridge,
        [&bridge](QString words) {
            words.fill(QChar{char16_t{0}});
            bridge.applyAuthenticationError(qtTrId("genycaster.setup.recovery_failed_retry"));
        });
#endif
    QObject::connect(&bridge, &PresentationBridge::signInRequested, &bridge,
        [&bridge, runtime, sessionObserver, siwfDomain, siwfUri](const QString& route) {
            if (route == QStringLiteral("recovery")) {
                // Native has no browser DOM recovery importer. Never mistake
                // this action for a Genyleap/OpenProof sign-in request.
                bridge.applyAuthenticationError(
                    qtTrId("genycaster.setup.recovery_failed_retry"));
                return;
            }
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
        [&bridge, runtime, sessionObserver] {
            if (bridge.authenticationRoute() == QStringLiteral("recovery")) {
                // Even if a one-shot proof completed concurrently, a
                // cancellation must invalidate its persisted session.
                (void)runtime->sessionService->signOut(sessionObserver);
                return;
            }
            (void)runtime->sessionService->cancelSignIn(sessionObserver);
        });

    QObject::connect(&bridge, &PresentationBridge::signOutRequested, &bridge,
        [runtime, sessionObserver] {
            // All native routes now share the authoritative persisted
            // session store and exactly the same sign-out behavior.
            (void)runtime->sessionService->signOut(sessionObserver);
        });
#endif

    // A restored account may lose registry reachability without losing its
    // keychain signer. Re-verification is read-only: it never opens the
    // recovery flow, signs KEY_ADD, or mutates the secure vault.
    QObject::connect(&bridge, &PresentationBridge::signerRefreshRequested, &bridge,
        [&bridge, runtime, signerObserver
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ] {
            if (!bridge.isAuthenticated() || bridge.signerBusy() || bridge.writeReady())
                return;
#if defined(Q_OS_WASM)
            if (bridge.accountFid() != 0) {
                const auto session = runtime->currentSessionSnapshot();
                const bool deviceLocalSession = session
                    && session->identityIssuer == "genycaster:device-local";
                browserSigner->start(QStringLiteral("restore"),
                    QString::number(bridge.accountFid()),
                    deviceLocalSession ? QJsonObject{}
                        : QJsonObject{{QStringLiteral("rememberLocal"), true}});
            }
#else
            if (!runtime->signerService) return;
            const auto session = runtime->currentSessionSnapshot();
            if (!session || session->fid.value != bridge.accountFid()) return;
            (void)runtime->signerService->restore(session, signerObserver);
#endif
        });

    QObject::connect(&bridge, &PresentationBridge::networkReachabilityChanged, &bridge,
        [&bridge](bool known, bool online) {
            if (known && online && bridge.isAuthenticated()
                && !bridge.signerBusy() && !bridge.writeReady()
                && (bridge.signerPhase() == QStringLiteral("unavailable")
                    || bridge.signerPhase() == QStringLiteral("failed"))) {
                bridge.refreshSigner();
            }
        });

    QObject::connect(&bridge, &PresentationBridge::signerProvisionRequested, &bridge,
        [&bridge, runtime, signerObserver
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ] {
#if defined(Q_OS_WASM)
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
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ] {
#if defined(Q_OS_WASM)
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
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](
            const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions,
            const QVariantList& embeds, const QString& parentUrl) {
#if defined(Q_OS_WASM)

            if (!bridge.isAuthenticated() || !bridge.writeReady()) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            const auto payload = genycaster::ui::browser_signing::makeCastPayload(
                text, mentionFids, mentionPositions, embeds, std::nullopt, parentUrl);
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
                    .parentUrl = parentUrl.isEmpty()
                        ? std::nullopt
                        : std::optional<std::string>{parentUrl.toUtf8().toStdString()},
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
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](
            const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions,
            const QVariantList& embeds, const qulonglong parentAuthorFid, const QString& parentCastId) {
#if defined(Q_OS_WASM)

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
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](
            const QVariantList& casts, const qulonglong parentAuthorFid, const QString& parentCastId) {
#if defined(Q_OS_WASM)

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
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](const QString& kind, const qulonglong targetAuthorFid,
            const QString& targetCastId, const bool active) {
#if defined(Q_OS_WASM)

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

    QObject::connect(&bridge, &PresentationBridge::protocolProfileUpdateRequested, &bridge,
        [&bridge, runtime
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](const QVariantMap& changes) {
#if defined(Q_OS_WASM)
            if (!bridge.isAuthenticated() || !bridge.writeReady()) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.profile.protocol_signer_required"));
                return;
            }
            QJsonObject payload{};
            for (auto it = changes.cbegin(); it != changes.cend(); ++it)
                payload.insert(it.key(), it.value().toString());
            browserSigner->start(QStringLiteral("profile"),
                QString::number(bridge.accountFid()), payload);
#else
            // Native profile mutations require the same durable outbox path
            // as other writes. Do not silently pretend that local settings
            // are a successful Farcaster UserDataAdd.
            Q_UNUSED(runtime);
            Q_UNUSED(changes);
            bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                qtTrId("genycaster.profile.protocol_native_unavailable"));
            emit bridge.protocolProfileWriteCompleted(false,
                qtTrId("genycaster.profile.protocol_native_unavailable"));
#endif
        });

    QObject::connect(&bridge, &PresentationBridge::followingMutationRequested, &bridge,
        [&bridge, runtime, writeObserver
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](const qulonglong targetFid, const bool following) {
#if defined(Q_OS_WASM)

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

    QObject::connect(&bridge, &PresentationBridge::blockingMutationRequested, &bridge,
        [&bridge, runtime, writeObserver
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](const qulonglong targetFid, const bool blocked) {
#if defined(Q_OS_WASM)

            if (!bridge.isAuthenticated() || !bridge.writeReady()
                || targetFid == 0 || targetFid == bridge.accountFid()) {
                bridge.applyWriteState(QStringLiteral("blocked"), false, {},
                    qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet"));
                return;
            }
            browserSigner->start(QStringLiteral("block"),
                QString::number(bridge.accountFid()),
                QJsonObject{{QStringLiteral("targetFid"), QString::number(targetFid)},
                            {QStringLiteral("active"), blocked}});
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
            auto status = runtime->writeService->setBlocked(
                runtime->currentSessionSnapshot(),
                domain::identity::Fid{static_cast<std::uint64_t>(targetFid)},
                blocked,
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
#if defined(Q_OS_WASM)
        , browserSigner
#endif
        ](const qulonglong authorFid, const QString& castId) {
#if defined(Q_OS_WASM)

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

#if !defined(Q_OS_WASM)
    // Native auth uses SessionService polling and persistent local sessions.
    // Browser OpenProof uses its own async QNetworkAccessManager device grant;
    // native polling would cancel its challenge as a spurious Guest response.
    auto* const authenticationPollTimer = new QTimer(&bridge);
    authenticationPollTimer->setInterval(1'000);
    QObject::connect(authenticationPollTimer, &QTimer::timeout, &bridge,
        [&bridge, &connections, runtime, sessionObserver] {
            if (bridge.authenticationPhase() != QStringLiteral("awaiting-approval") &&
                bridge.authenticationPhase() != QStringLiteral("linking-farcaster")) {
                return;
            }
            bool expected = false;
            if (!runtime->authenticationPollInFlight.compare_exchange_strong(
                    expected, true, std::memory_order_acq_rel)) {
                return;
            }
            // The native OpenProof provider has already verified the matching
            // Farcaster FID. Forward its short-lived bearer only through trusted
            // C++ composition; never persist it or expose it to QML. The
            // session observer queues the canonical UI identity first.
            const auto accountGrantObserver =
                [&bridge, &connections](capability::auth::VerifiedAccountGrant grant) {
                const auto fid = grant.fid;
                const auto subject = QString::fromStdString(grant.subject);
                QByteArray bearer = QByteArray::fromStdString(grant.bearer);
                std::fill(grant.bearer.begin(), grant.bearer.end(), '\0');
                const int expiresSeconds = grant.expiresSeconds;
                QMetaObject::invokeMethod(&bridge,
                    [&bridge, &connections, fid, subject,
                     bearer = std::move(bearer), expiresSeconds]() mutable {
                        if (bridge.isAuthenticated() && bridge.accountFid() == fid
                            && bridge.accountIdentityIssuer() == connections.identityIssuer()) {
                            connections.acceptAuthenticatedGrant(
                                std::move(bearer), subject, fid, expiresSeconds);
                        } else {
                            bearer.fill('\0');
                        }
                    }, Qt::QueuedConnection);
            };
            auto status = runtime->sessionService->pollSignIn(
                sessionObserver, accountGrantObserver);
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
    // Restore a locally remembered device identity only after its
    // non-extractable WebCrypto key completes a challenge-response proof.
    // This is local continuity, not a newly refreshed OpenProof token.
    // Guest state was queued first. Restore only afterwards, otherwise the
    // queued Guest sign-out would invalidate the in-flight local proof.
    QMetaObject::invokeMethod(&bridge, [browserSigner] {
        browserSigner->restoreRememberedAccount();
    }, Qt::QueuedConnection);
    // Storage can be unavailable in private browsing. Never keep the splash
    // indefinitely if the browser drops an IndexedDB/WebCrypto callback.
    // The fallback only reveals the actual signed-out Setup; it grants no auth.
    QTimer::singleShot(25000, &bridge, [&bridge] {
        bridge.finishBrowserAccountRestore();
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

                        QVariantList rows = feedPresentationRows(state, 0U, channelId);
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

#if FARCASTER_HAS_HYPERSNAP_PROVIDER
    // The QML viewport reports lifetime-scoped interest, never a provider
    // request. Qt composition owns admission and the account-scoped request.
    QObject::connect(&bridge, &PresentationBridge::viewerReactionInterestRetained,
        &bridge, [&bridge, runtime](const QString& id, const qulonglong authorFid,
                                    const qulonglong viewerFid) {
            if (!bridge.isAuthenticated() || bridge.accountFid() != viewerFid
                || authorFid == 0 || viewerFid == 0) {
                return;
            }
            const QString normalized = id.trimmed().toLower();
            if (normalized.size() != 42 || !normalized.startsWith(QStringLiteral("0x"))) {
                return;
            }
            for (qsizetype i = 2; i < normalized.size(); ++i) {
                const QChar ch = normalized.at(i);
                if (!((ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                    || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')))) {
                    return;
                }
            }
            const auto canonical = normalized.toStdString();
            bool firstConsumer = false;
            {
                std::scoped_lock lock{runtime->viewerOperationMutex};
                if (runtime->activeViewerFid != viewerFid) {
                    runtime->viewerReactionConsumerCounts.clear();
                }
                constexpr std::size_t maximumDistinctVisibleInterests{512};
                auto it = runtime->viewerReactionConsumerCounts.find(canonical);
                if (it == runtime->viewerReactionConsumerCounts.end()) {
                    if (runtime->viewerReactionConsumerCounts.size()
                        >= maximumDistinctVisibleInterests) {
                        return;
                    }
                    runtime->viewerReactionConsumerCounts.emplace(canonical, 1U);
                    firstConsumer = true;
                } else {
                    ++it->second;
                }
            }
            if (firstConsumer) {
                refreshVisibleViewerReactionState(bridge, runtime,
                    domain::identity::Fid{viewerFid},
                    {domain::cast::CastReference{
                        .author = domain::identity::Fid{authorFid},
                        .id = domain::cast::CastId{canonical},
                    }});
            }
        });
    QObject::connect(&bridge, &PresentationBridge::viewerReactionInterestReleased,
        &bridge, [runtime](const QString& id, const qulonglong viewerFid) {
            const auto canonical = id.trimmed().toLower().toStdString();
            std::scoped_lock lock{runtime->viewerOperationMutex};
            if (runtime->activeViewerFid != viewerFid) {
                return;
            }
            auto it = runtime->viewerReactionConsumerCounts.find(canonical);
            if (it == runtime->viewerReactionConsumerCounts.end()) {
                return;
            }
            if (--it->second != 0U) {
                return; // Another visible consumer still needs the same cast.
            }
            runtime->viewerReactionConsumerCounts.erase(it);
            // Queued work never served by the provider has no failure/backoff.
            std::erase_if(runtime->deferredViewerTargets,
                [&canonical](const auto& target) { return target.id.value == canonical; });
        });
#endif

    QObject::connect(&bridge, &PresentationBridge::feedRequested, &bridge,
        [&bridge, runtime](const QString& feed, const qulonglong fid,
            const QString& channel, const QString& continuation,
            const bool allowRecoveryProbe) {
            const bool continuationRequest = !continuation.isEmpty();
            // Snapshot GUI-owned read settings before asynchronous callbacks.
            const std::size_t requestedPageSize = static_cast<std::size_t>(bridge.feedPageSize());
            const std::size_t contextHydrationLimit = static_cast<std::size_t>(bridge.contextHydrationLimit());
            const qulonglong viewerFid = bridge.isAuthenticated() ? bridge.accountFid() : 0U;
            // Cache materialization is bounded for both cold and warm reads.
            // The presentation layer reconciles a warm prefix without evicting
            // retained viewport identities farther down the cached timeline.
            // Feed identity, not the presence of a cursor, owns previews and
            // viewer enrichment. A new page or same-surface refresh must not
            // discard work shared by the currently visible cast delegates.
            const std::string surfaceIdentity = feed.toStdString() + "\x1f"
                + std::to_string(fid) + "\x1f" + channel.toStdString();
            // Profile metadata and its feed are siblings, not replacements.
            // The profileRequested signal has already started openProfile();
            // selecting the initial or another profile tab must not retire it.
            const bool profileFeed = feed.startsWith(QStringLiteral("user-"));
            if (!runtime->selectFeedSurfaceIdentity(surfaceIdentity, profileFeed)) {
                runtime->cancelFeedRequest();
            }
            const std::uint64_t surfaceGeneration =
                runtime->feedSurfaceGeneration.load(std::memory_order_acquire);
            const std::uint64_t requestGeneration = runtime->beginFeedRequestGeneration();
            if (!continuationRequest && bridge.isAuthenticated() && bridge.accountFid() > 0) {
                refreshViewerReactionSnapshot(
                    bridge, runtime,
                    domain::identity::Fid{static_cast<std::uint64_t>(bridge.accountFid())},
                    false);
            }
            // A local cache offset and a remote provider continuation must not
            // share the same selector or pagination vocabulary.
            auto query = feedQueryForSurface(feed, fid, channel, requestedPageSize);
            if (continuationRequest) {
                query.continuation = capability::pagination::ContinuationToken{continuation.toStdString()};
            }
            auto request = runtime->service->openFeed(std::move(query),
                [&bridge, runtime, requestGeneration, surfaceGeneration,
                 viewerFid, contextHydrationLimit](application::read::model::FeedState state) {
                    if (!runtime->isCurrentFeedRequestGeneration(requestGeneration)) {
                        return;
                    }
                    constexpr std::size_t maximumContextHydrationsPerFeedState{24U};
                    std::vector<domain::cast::CastId> missingReferences{};
                    if (state.metadata.phase == application::read::model::ReadPhase::Ready) {
                        missingReferences = missingReferencedCastIds(
                            state, std::min(
                                contextHydrationLimit,
                                maximumContextHydrationsPerFeedState));
                    }
                    // Reaction demand is now owned by visible EngagementBar
                    // consumers instead of fetched pages. The same page may
                    // contain offscreen casts which must not consume requests.


                    // Called on the bounded cache executor, not the GUI.
                    QVariantList prepared = feedPresentationRows(state, viewerFid);
                    if (!runtime->isCurrentFeedRequestGeneration(requestGeneration)) {
                        return;
                    }
                    QMetaObject::invokeMethod(
                        &bridge,
                        [&bridge, runtime, requestGeneration, state = std::move(state),
                         prepared = std::move(prepared)]() mutable {
                            if (runtime->isCurrentFeedRequestGeneration(requestGeneration)) {
                                applyFeedState(bridge, std::move(state), std::move(prepared));
                            }
                        },
                        Qt::QueuedConnection);

                    for (const auto& referencedId : missingReferences) {
                        if (!runtime->beginReferencedPreviewRequest(referencedId)) {
                            continue;
                        }
                        auto previewRequest = runtime->service->openCast(
                            referencedId,
                            [&bridge, runtime, referencedId, surfaceGeneration](
                                application::read::model::CastState referencedState) {
                                if (!runtime->isCurrentFeedSurfaceGeneration(surfaceGeneration)) {
                                    return;
                                }
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
                                // A cached preview may be followed by a fresher
                                // provider snapshot. Only the terminal state
                                // releases the in-flight request's ownership.
                                if (phase == application::read::model::ReadPhase::Ready
                                    || phase == application::read::model::ReadPhase::Failed) {
                                    runtime->finishReferencedPreviewRequest(referencedId, true);
                                }
                                if (!runtime->isCurrentFeedSurfaceGeneration(surfaceGeneration)) {
                                    return;
                                }
                                QMetaObject::invokeMethod(
                                    &bridge,
                                    [&bridge, runtime, surfaceGeneration, castId, preview] {
                                        if (runtime->isCurrentFeedSurfaceGeneration(surfaceGeneration)) {
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
                    // PresentationBridge retains existing rows and appends by
                    // stable Cast ID; never rehydrate the entire SQLite feed
                    // only to render one new page.
                    .pageOnlyContinuation = continuationRequest,
                    // Bound the first cache paint and fresh local hydration.
                    // All remaining local pages are requested on explicit
                    // viewport demand using a separate offset contract.
                    .localFeedWindowLimit = continuationRequest ? 0U : 64U,
                });
            if (request) { runtime->keepFeedRequest(std::move(*request)); }
            else { bridge.applyRuntimeState(false, qtTrId("genycaster.runtime.the_read_worker_is_unavailable")); }
        });

    QObject::connect(&bridge, &PresentationBridge::localFeedWindowRequested, &bridge,
        [&bridge, runtime](const QString& feed, const qulonglong fid,
                           const QString& channel, const qulonglong offset,
                           const qulonglong limit) {
            // The active feed owns this demand. Do not start another remote
            // request or release referenced previews to fetch cached rows.
            const std::string identity = feed.toStdString() + "\x1f"
                + std::to_string(fid) + "\x1f" + channel.toStdString();
            bool belongsToActiveSurface{};
            {
                std::scoped_lock lock{runtime->feedSurfaceMutex};
                belongsToActiveSurface = runtime->feedSurfaceIdentity == identity;
            }
            if (!belongsToActiveSurface) {
                bridge.applyLocalFeedWindowFailure(
                    qtTrId("genycaster.runtime.the_read_worker_is_unavailable"));
                return;
            }
            const auto surfaceGeneration =
                runtime->feedSurfaceGeneration.load(std::memory_order_acquire);
            const auto requestGeneration = runtime->beginFeedRequestGeneration();
            const auto pageSize = static_cast<std::size_t>(bridge.feedPageSize());
            const qulonglong viewerFid = bridge.isAuthenticated() ? bridge.accountFid() : 0U;
            auto query = feedQueryForSurface(feed, fid, channel, pageSize);
            auto request = runtime->service->openLocalFeedWindow(
                std::move(query), static_cast<std::size_t>(offset),
                static_cast<std::size_t>(limit),
                [&bridge, runtime, requestGeneration, surfaceGeneration, viewerFid](
                    application::read::model::FeedState state) {
                    if (!runtime->isCurrentFeedRequestGeneration(requestGeneration)
                        || !runtime->isCurrentFeedSurfaceGeneration(surfaceGeneration))
                        return;
                    QVariantList prepared = feedPresentationRows(state, viewerFid);
                    if (!runtime->isCurrentFeedRequestGeneration(requestGeneration)
                        || !runtime->isCurrentFeedSurfaceGeneration(surfaceGeneration))
                        return;
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, runtime, requestGeneration, surfaceGeneration,
                         state = std::move(state), prepared = std::move(prepared)]() mutable {
                            if (runtime->isCurrentFeedRequestGeneration(requestGeneration)
                                && runtime->isCurrentFeedSurfaceGeneration(surfaceGeneration))
                                applyFeedState(bridge, std::move(state), std::move(prepared));
                        }, Qt::QueuedConnection);
                });
            if (request) {
                runtime->keepFeedRequest(std::move(*request));
            } else {
                bridge.applyLocalFeedWindowFailure(
                    qtTrId("genycaster.runtime.the_read_worker_is_unavailable"));
            }
        });

    QObject::connect(&bridge, &PresentationBridge::notificationsRequested, &bridge,
        [&bridge, runtime](const QString& continuation, const bool allowRecoveryProbe) {
            if (!bridge.isAuthenticated() || bridge.accountFid() == 0) {
                bridge.applyNotifications({}, {}, qtTrId("genycaster.notifications.sign_in_required"), false, false);
                return;
            }
            if (continuation.isEmpty()) {
                runtime->cancelAllRequests();
            } else {
                runtime->cancelPrimaryRequest();
            }
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

    // Topic discovery hydrates only visible canonical hashes through the
    // provider-neutral read service. Never turn a topic view into bookmarks.
    QObject::connect(&bridge, &PresentationBridge::topicHydrationRequested,
        &bridge, [&bridge, runtime](const QStringList& hashes) {
            const qulonglong ownerFid = bridge.accountFid();
            if (!ownerFid || !bridge.isAuthenticated()) return;
            for (const QString& hash : hashes.mid(0, 8)) {
                const domain::cast::CastId castId{hash.toStdString()};
                if (!runtime->beginReferencedPreviewRequest(castId)) continue;
                auto request = runtime->service->openCast(castId,
                    [&bridge, runtime, castId, hash, ownerFid](
                        application::read::model::CastState state) {
                        const auto phase = state.metadata.phase;
                        if (state.cast) {
                            QVariantMap preview = castPreviewPresentation(state);
                            QMetaObject::invokeMethod(&bridge,
                                [&bridge, ownerFid, hash, preview = std::move(preview)]() mutable {
                                    if (bridge.isAuthenticated() && bridge.accountFid() == ownerFid)
                                        bridge.applyHydratedTopicCast(hash, std::move(preview));
                                }, Qt::QueuedConnection);
                        }
                        if (phase == application::read::model::ReadPhase::Ready
                            || phase == application::read::model::ReadPhase::Failed)
                            runtime->finishReferencedPreviewRequest(castId, bool(state.cast));
                    },
                    application::read::model::ReadOptions{
                        .allowRecoveryProbe = false,
                        .intent = application::read::model::ReadIntent::Prefetch,
                    });
                if (request)
                    runtime->keepReferencedPreviewRequest(castId, std::move(*request));
                else
                    runtime->finishReferencedPreviewRequest(castId, false);
            }
        });

    // Hydrate only visible saved casts. This uses the same provider-routed,
    // cancelable C++ read service as cast detail, without navigating, mutating
    // bookmarks or launching unbounded requests for the full reading list.
    QObject::connect(&bridge, &PresentationBridge::bookmarkHydrationRequested,
        &bridge, [&bridge, runtime](const QStringList& hashes) {
            const qulonglong ownerFid = bridge.accountFid();
            if (!ownerFid || !bridge.isAuthenticated()) return;
            for (const QString& hash : hashes.mid(0, 12)) {
                const domain::cast::CastId castId{hash.toStdString()};
                if (!runtime->beginReferencedPreviewRequest(castId)) continue;
                auto request = runtime->service->openCast(castId,
                    [&bridge, runtime, castId, hash, ownerFid](
                        application::read::model::CastState state) {
                        const auto phase = state.metadata.phase;
                        if (state.cast) {
                            QVariantMap cast = castPreviewPresentation(state);
                            QMetaObject::invokeMethod(&bridge,
                                [&bridge, ownerFid, hash, cast = std::move(cast)]() mutable {
                                    if (bridge.isAuthenticated() &&
                                        bridge.accountFid() == ownerFid)
                                        bridge.applyHydratedBookmark(hash, std::move(cast));
                                }, Qt::QueuedConnection);
                        }
                        if (phase == application::read::model::ReadPhase::Ready
                            || phase == application::read::model::ReadPhase::Failed)
                            runtime->finishReferencedPreviewRequest(castId, bool(state.cast));
                    },
                    application::read::model::ReadOptions{.allowRecoveryProbe = false});
                if (request)
                    runtime->keepReferencedPreviewRequest(castId, std::move(*request));
                else
                    runtime->finishReferencedPreviewRequest(castId, false);
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
                    // Detail and all thread replies retain their own canonical
                    // interest from their visible EngagementBar lifetimes.

                    // Quote and parent context must also hydrate on direct
                    // detail navigation, not just during a feed page read.
                    std::vector<domain::cast::CastId> detailReferences{};
                    if (state.cast && state.metadata.phase == application::read::model::ReadPhase::Ready) {
                        const auto appendReference = [&](const domain::cast::CastId& id) {
                            if (id.value.empty() || id.value == state.cast->id.value
                                || detailReferences.size() >= 6U
                                || std::ranges::any_of(detailReferences, [&id](const auto& prior) {
                                    return prior.value == id.value;
                                })) return;
                            detailReferences.push_back(id);
                        };
                        if (state.cast->parent) appendReference(*state.cast->parent);
                        for (const auto& embed : state.cast->embeds) {
                            if (embed.kind == domain::cast::EmbedKind::CastReference)
                                appendReference(domain::cast::CastId{embed.value});
                        }
                    }
                    QVariantList flattenedReplies{};
                    if (state.cast) {
                        // QVariant construction and tree flattening happen on
                        // the application callback worker, never the GUI thread.
                        flattenedReplies = flatThreadReplyItems(*state.cast);
                    }
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, runtime, requestGeneration, state = std::move(state),
                         flattenedReplies = std::move(flattenedReplies)]() mutable {
                        if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                            return;
                        }
                        QVariantMap detail{{QStringLiteral("castId"), QString::fromStdString(state.castId.value)}};
                        if (state.cast) {
                            attachChannelPresentation(detail, *state.cast);
                            detail.insert(QStringLiteral("authorFid"), static_cast<qulonglong>(state.cast->author.value));
                            detail.insert(QStringLiteral("text"), readableCastText(*state.cast));
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
                            detail.insert(QStringLiteral("threadReplies"), std::move(flattenedReplies));
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

                    for (const auto& referencedId : detailReferences) {
                        if (!runtime->beginReferencedPreviewRequest(referencedId))
                            continue;
                        auto previewRequest = runtime->service->openCast(
                            referencedId,
                            [&bridge, runtime, referencedId, requestGeneration](
                                application::read::model::CastState referencedState) {
                                if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration)) {
                                    return;
                                }
                                const auto phase = referencedState.metadata.phase;
                                if (!referencedState.cast) {
                                    if (phase == application::read::model::ReadPhase::Ready
                                        || phase == application::read::model::ReadPhase::Failed)
                                        runtime->finishReferencedPreviewRequest(referencedId, false);
                                    return;
                                }
                                const QVariantMap preview = castPreviewPresentation(referencedState);
                                if (phase == application::read::model::ReadPhase::Ready
                                    || phase == application::read::model::ReadPhase::Failed) {
                                    runtime->finishReferencedPreviewRequest(referencedId, true);
                                }
                                if (!runtime->isCurrentPrimaryRequestGeneration(requestGeneration))
                                    return;
                                QMetaObject::invokeMethod(
                                    &bridge,
                                    [&bridge, runtime, requestGeneration, referencedId, preview] {
                                        if (runtime->isCurrentPrimaryRequestGeneration(requestGeneration))
                                            bridge.applyReferencedCastPreview(
                                                QString::fromStdString(referencedId.value), preview);
                                    },
                                    Qt::QueuedConnection);
                            },
                            application::read::model::ReadOptions{
                                .allowRecoveryProbe = false,
                                .intent = application::read::model::ReadIntent::Prefetch,
                            });
                        if (!previewRequest)
                            runtime->finishReferencedPreviewRequest(referencedId, false);
                        else
                            runtime->keepReferencedPreviewRequest(referencedId, std::move(*previewRequest));
                    }
                }, application::read::model::ReadOptions{
                    .allowRecoveryProbe = allowRecoveryProbe,
                });
            if (request) { runtime->keepPrimaryRequest(std::move(*request)); }
            else { bridge.applyRuntimeState(false, qtTrId("genycaster.runtime.the_read_worker_is_unavailable")); }
        });

    QObject::connect(&bridge, &PresentationBridge::profileRequested, &bridge,
        [&bridge, runtime](const qulonglong fid, const bool allowRecoveryProbe) {
            // Profile navigation retires the previous feed/cast's shared
            // quote and reaction interests, not only its primary request.
            runtime->cancelAllRequests();
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
                        // Both directions are independent: "follows you" does
                        // not establish whether you follow the visited profile.
                        // The latter drives the Follow/Unfollow control.
                        refreshProfileFollowsMe(bridge, runtime, *accountFid, state.profile->fid);
                        refreshProfileFollowsMe(bridge, runtime, *accountFid, state.profile->fid,
                                                true);
                        refreshProfileBlocked(bridge, runtime, *accountFid, state.profile->fid, true);
                        refreshProfileBlocked(bridge, runtime, *accountFid, state.profile->fid, false);
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

    QObject::connect(&bridge, &PresentationBridge::followingStoriesRequested, &bridge,
        [&bridge, runtime](const qulonglong viewerFid) {
            if (viewerFid == 0U || viewerFid != bridge.accountFid()
                || !bridge.isAuthenticated()) {
                return;
            }
            // This request owns its lifetime independently of the active feed,
            // profile, and art reads. Navigating Explorer must not cancel it.
            application::read::model::SocialQuery query{
                .kind = application::read::model::SocialQuery::Kind::Following,
                .fid = domain::identity::Fid{static_cast<std::uint64_t>(viewerFid)},
                .limit = 60,
            };
            auto request = runtime->service->openSocial(
                std::move(query),
                [&bridge, viewerFid](application::read::model::SocialState state) {
                    const bool finished = !isBusy(state.metadata);
                    const bool successful = finished && !state.metadata.error.has_value();
                    QVariantList following{};
                    if (!state.metadata.error) {
                        following.reserve(static_cast<qsizetype>(state.profiles.size()));
                        for (const auto& profile : state.profiles) {
                            following.push_back(QVariantMap{
                                {QStringLiteral("fid"), static_cast<qulonglong>(profile.fid.value)},
                                {QStringLiteral("username"), username(&profile)},
                                {QStringLiteral("displayName"), displayName(&profile)},
                                {QStringLiteral("avatarUrl"), avatarUrl(&profile)},
                                {QStringLiteral("activityKind"), QStringLiteral("cast")},
                            });
                        }
                    }
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, viewerFid, following = std::move(following),
                         finished, successful]() mutable {
                            bridge.applyFollowingStoryProfiles(
                                viewerFid, std::move(following), finished, successful);
                        }, Qt::QueuedConnection);
                },
                application::read::model::ReadOptions{
                    .intent = application::read::model::ReadIntent::Prefetch,
                });
            if (request) {
                runtime->keepFollowingStoriesRequest(std::move(*request));
            } else {
                bridge.applyFollowingStoryProfiles(viewerFid, {}, true, false);
            }
        });

    QObject::connect(&bridge, &PresentationBridge::followingStoryActivitiesRequested, &bridge,
        [&bridge, runtime](const qulonglong viewerFid) {
            if (viewerFid == 0U || viewerFid != bridge.accountFid()
                || !bridge.isAuthenticated()) {
                return;
            }
            application::read::model::FeedQuery query{
                .kind = application::read::model::FeedQuery::Kind::Following,
                .fid = domain::identity::Fid{static_cast<std::uint64_t>(viewerFid)},
                .limit = 80U,
            };
            auto request = runtime->service->openFeed(
                std::move(query),
                [&bridge, viewerFid](application::read::model::FeedState state) {
                    const bool finished = !isBusy(state.metadata);
                    const bool successful = !state.metadata.error.has_value();
                    QHash<qulonglong, QVariantMap> latest{};
                    if (successful) {
                        for (const auto& cast : state.casts) {
                            const qulonglong fid = cast.author.value;
                            const qint64 when = static_cast<qint64>(
                                cast.createdAt.time_since_epoch().count());
                            if (fid == 0U || cast.id.value.empty()) { continue; }
                            const auto prior = latest.constFind(fid);
                            if (prior != latest.cend()
                                && prior->value(QStringLiteral("createdAtMs")).toLongLong() >= when) {
                                continue;
                            }
                            latest.insert(fid, QVariantMap{
                                {QStringLiteral("fid"), fid},
                                {QStringLiteral("latestCastId"), QString::fromStdString(cast.id.value)},
                                {QStringLiteral("createdAtMs"), when},
                                {QStringLiteral("activityKind"), cast.parent
                                    ? QStringLiteral("reply") : QStringLiteral("cast")},
                            });
                        }
                    }
                    QVariantList activities{};
                    activities.reserve(latest.size());
                    for (auto it = latest.cbegin(); it != latest.cend(); ++it) {
                        activities.push_back(it.value());
                    }
                    QMetaObject::invokeMethod(&bridge,
                        [&bridge, viewerFid, activities = std::move(activities),
                         finished, successful]() mutable {
                            bridge.applyFollowingStoryActivities(
                                viewerFid, std::move(activities), finished, successful);
                        }, Qt::QueuedConnection);
                },
                application::read::model::ReadOptions{
                    .intent = application::read::model::ReadIntent::Prefetch,
                });
            if (request) {
                runtime->keepFollowingActivityRequest(std::move(*request));
            } else {
                bridge.applyFollowingStoryActivities(viewerFid, {}, true, false);
            }
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

#if defined(Q_OS_WASM)
// The browser owns selected Files and non-extractable signer keys. Keep
// media bytes out of the Qt heap and transfer only bounded state events.
void connectBrowserMediaUpload(
    MediaUploadController& controller, PresentationBridge& bridge,
    RichComposerController& richComposer)
{
    const auto refreshAvailability = [&controller, &bridge] {
        const bool allowed = bridge.isAuthenticated() && bridge.writeReady()
            && !bridge.signerBusy();
        if (!allowed && controller.busy()) controller.cancel();
        controller.setAvailable(allowed);
    };
    QObject::connect(&bridge, &PresentationBridge::authenticationStateChanged,
                     &controller, refreshAvailability);
    QObject::connect(&bridge, &PresentationBridge::stateChanged,
                     &controller, refreshAvailability);
    QObject::connect(&controller, &MediaUploadController::browserImagePickRequested,
                     &controller, [&bridge](const int afterIndex) {
        if (!bridge.isAuthenticated() || !bridge.writeReady()) return;
        const QByteArray fid = QByteArray::number(bridge.accountFid());
        EM_ASM({
            window.__genycasterBrowserMedia?.pick(
                UTF8ToString($0,$1),$2);
        },fid.constData(),fid.size(),afterIndex);
    });
    QObject::connect(&controller, &MediaUploadController::cancelRequested,
                     &controller, [&controller] {
        EM_ASM({window.__genycasterBrowserMedia?.cancel();});
        controller.applyCancelled();
    });

    auto* const polling = new QTimer(&controller);
    polling->setInterval(250);
    // A failed network cleanup stays durable and retries at most once per
    // minute, never from an unauthenticated or unverified signer session.
    auto cleanupNextAttempt = std::make_shared<QHash<QString, qint64>>();
    QObject::connect(polling, &QTimer::timeout, &controller,
                     [&controller, &bridge, &richComposer, cleanupNextAttempt] {
        for(int index=0;index<5;++index) {
            std::array<char,8192> buffer{};
            const int received=EM_ASM_INT({
                const media=window.__genycasterBrowserMedia;
                if(!media?.events?.length)return 0;
                const value=JSON.stringify(media.events.shift());
                if(lengthBytesUTF8(value)+1>$1)return -1;
                stringToUTF8(value,$0,$1);
                return 1;
            },buffer.data(),buffer.size());
            // An empty event queue must not skip the durable cleanup tick.
            if(received == 0) break;
            if(received<0)continue;
            const QJsonDocument json=QJsonDocument::fromJson(buffer.data());
            if(!json.isObject())continue;
            const QJsonObject event=json.object();
            if(!bridge.isAuthenticated() || !bridge.writeReady()
               || event.value(QStringLiteral("fid")).toString()
                    !=QString::number(bridge.accountFid()))continue;
            const QString kind=event.value(QStringLiteral("kind")).toString();
            if(kind==QStringLiteral("cleanup-completed")) {
                const QString token=event.value(QStringLiteral("managementToken")).toString();
                if(cleanupNextAttempt->contains(token)) {
                    richComposer.markManagedAssetCleanupCompleted(token);
                    cleanupNextAttempt->remove(token);
                }
            }else if(kind==QStringLiteral("cleanup-failed")) {
                // Token remains durable; the next scheduled attempt is
                // rate-limited. No management token enters the UI/log.
                continue;
            }else if(kind==QStringLiteral("selected")) {
                controller.applyBrowserSelection(
                    event.value(QStringLiteral("fileName")).toString(),
                    event.value(QStringLiteral("contentType")).toString(),
                    static_cast<qulonglong>(event.value(QStringLiteral("sizeBytes")).toDouble()),
                    event.value(QStringLiteral("afterIndex")).toInt(-1));
            }else if(kind==QStringLiteral("preview")) {
                controller.applyBrowserPreview(
                    event.value(QStringLiteral("previewUrl")).toString());
            }else if(kind==QStringLiteral("progress")) {
                controller.applyProgress(
                    event.value(QStringLiteral("phase")).toString(),
                    static_cast<qulonglong>(event.value(QStringLiteral("transferred")).toDouble()),
                    static_cast<qulonglong>(event.value(QStringLiteral("total")).toDouble()));
            }else if(kind==QStringLiteral("completed")) {
                const QString url=event.value(QStringLiteral("assetUrl")).toString();
                if(!url.startsWith((QString::fromUtf8(genycaster::deployment::mediaPublicOrigin.data()) + QStringLiteral("/m/v1/")))) {
                    controller.applyFailed(QStringLiteral("Untrusted media asset response"));
                    continue;
                }
                controller.applyCompleted(
                    url,
                    event.value(QStringLiteral("contentType")).toString(),
                    event.value(QStringLiteral("fileName")).toString(),
                    event.value(QStringLiteral("managementToken")).toString(),
                    static_cast<qulonglong>(event.value(QStringLiteral("sizeBytes")).toDouble()));
            }else if(kind==QStringLiteral("failed")) {
                controller.applyFailed(event.value(QStringLiteral("error")).toString());
            }
        }

        if (!bridge.isAuthenticated() || !bridge.writeReady()
            || bridge.signerBusy()) return;
        const qint64 now=QDateTime::currentMSecsSinceEpoch();
        const auto pending=richComposer.pendingManagedAssetCleanup();
        for (const QString& token : pending) {
            if (token.isEmpty() || token.size()>4096
                || cleanupNextAttempt->value(token, 0)>now) continue;
            (*cleanupNextAttempt)[token]=now+60'000;
            const QByteArray tokenUtf8=token.toUtf8();
            const QByteArray fidUtf8=QByteArray::number(bridge.accountFid());
            EM_ASM({
                window.__genycasterBrowserMedia?.remove(
                    UTF8ToString($0,$1),UTF8ToString($2,$3));
            },fidUtf8.constData(),fidUtf8.size(),
              tokenUtf8.constData(),tokenUtf8.size());
            // One signed cleanup per tick keeps the QML thread responsive.
            break;
        }
    });
    polling->start();
    refreshAvailability();
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
            // Farcaster's HLS manifests redirect to signed Cloudflare Stream
            // URLs. HEAD is not uniformly supported through proxy paths and
            // must never block the actual Qt/FFmpeg media pipeline. The media
            // backend itself reports validity and decoder failures.
            if (QUrl{source}.path().endsWith(QLatin1StringView{".m3u8"}, Qt::CaseInsensitive)) {
                dispatch(true);
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


void attachRuntime(RuntimeAttachment attachment,
                   std::shared_ptr<Runtime> readyRuntime)
{
    // QObject connections and presentation state always publish on the GUI
    // thread; the provider factory itself may run in a background worker.
    Q_ASSERT(QThread::currentThread() == attachment.presentationBridge.thread());
    if (!readyRuntime) {
        attachment.presentationBridge.applyRuntimeState(
            false, QStringLiteral("Runtime construction returned no instance."));
        return;
    }

    attachment.runtime = std::move(readyRuntime);
#if !defined(Q_OS_WASM) && FARCASTER_HAS_PROTOCOL_SIGNING && FARCASTER_HAS_FARCASTER_AUTH
    // A signed-in native client can silently prove its existing registered
    // Ed25519 signer after the in-memory Account API cookie expires. The
    // signer remains in the OS-protected vault; only the domain-separated,
    // one-use proof leaves this process. This avoids launching Farcaster
    // solely to query a Pro membership.
    attachment.openProofConnections.setAccountProofSigner(
        [runtimeWeak = std::weak_ptr<Runtime>{attachment.runtime}](
            const qulonglong fid, const QString& nonce)
            -> std::optional<OpenProofConnectionsController::AccountSignerProof> {
        auto current = runtimeWeak.lock();
        if (!current || !current->signerVault || fid == 0) return std::nullopt;
        auto signer = current->signerVault->load(domain::identity::Fid{
            static_cast<std::uint64_t>(fid)});
        if (!signer || !*signer) return std::nullopt;
        auto publicKey = (*signer)->publicKey();
        if (!publicKey || publicKey->hex.size() != 64) return std::nullopt;
        const std::string canonical =
            std::string{"genycaster-account-session-v1\n"}
            + std::to_string(fid) + "\n" + nonce.toStdString();
        auto signature = (*signer)->sign(canonical);
        if (!signature || signature->bytes.size() != 64) return std::nullopt;
        const QByteArray raw{signature->bytes.data(),
            static_cast<qsizetype>(signature->bytes.size())};
        return OpenProofConnectionsController::AccountSignerProof{
            QString::fromStdString(publicKey->hex),
            QString::fromLatin1(raw.toHex())};
    });
#endif
    connectRuntime(
        attachment.presentationBridge,
        attachment.richComposerController,
        attachment.openProofConnections,
        attachment.runtime,
        attachment.authenticationAvailable,
        std::move(attachment.siwfDomain),
        std::move(attachment.siwfUri),
        attachment.browserSnapchainOrigin);
    connectMediaPlayback(attachment.mediaPlayback, attachment.runtime);
#if defined(Q_OS_WASM)
    connectBrowserMediaUpload(
        attachment.mediaUploadController, attachment.presentationBridge,
        attachment.richComposerController);
#elif FARCASTER_HAS_MEDIA_UPLOAD
    connectMediaUpload(attachment.mediaUploadController, attachment.runtime);
#else
    attachment.mediaUploadController.setAvailable(false);
#endif

    // Publish readiness only after every GUI-facing capability is wired.
    attachment.presentationBridge.applyRuntimeState(true);
    if (attachment.startupIdentity.isEmpty()) {
        if (attachment.openExploreForTest)
            attachment.presentationBridge.selectFeed(QStringLiteral("recent"));
        return;
    }
    // PresentationBridge owns restored/manual identity transition. This
    // function must never infer a signer or selected network from the FID.
    attachment.presentationBridge.openIdentity(attachment.startupIdentity);
}

} // namespace genycaster::ui::bindings
