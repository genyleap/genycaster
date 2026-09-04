#include "emoji_controller.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

import farcaster.application.emoji.asset_repository;
import farcaster.application.emoji.catalog;
import farcaster.capability.animated_emoji;
import farcaster.domain.emoji;
import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.emoji.emoji_asset_validator;
import farcaster.provider.emoji.emoji_cache;
import farcaster.provider.emoji.noto_animated_emoji;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;
import farcaster.transport.qtnetwork.async;

namespace {

using namespace farcaster::application::emoji;
using namespace farcaster::provider::emoji;

constexpr auto RecentsKey = "emoji/recentsV1";
constexpr auto Revision = 1U;
constexpr std::size_t MaximumAssetBytes = 2U * 1024U * 1024U;

constexpr std::string_view EmojiResourcePrefix = ":/qt/qml/Farcaster/Client/assets/emoji/lottie/emoji_u";

[[nodiscard]] QString qrcPathForKey(const QString& key)
{
    return QString::fromLatin1(EmojiResourcePrefix) + key + QStringLiteral(".json");
}

[[nodiscard]] std::optional<std::filesystem::path> bundledPathFor(
    const std::string_view key)
{
    const QString qrc = qrcPathForKey(QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())));
    if (QFile::exists(qrc)) {
        return std::filesystem::path{qrc.toUtf8().toStdString()};
    }
    return std::nullopt;
}

[[nodiscard]] QString cacheDirectory()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (root.isEmpty()) {
        return {};
    }
    return QDir{root}.filePath(QStringLiteral("emoji"));
}

[[nodiscard]] QString categoryIcon(const catalog::EmojiCategory category)
{
    switch (category) {
    case catalog::EmojiCategory::Smileys: return QStringLiteral("social/smile");
    case catalog::EmojiCategory::People: return QStringLiteral("social/users");
    case catalog::EmojiCategory::Animals: return QStringLiteral("social/mood-excited");
    case catalog::EmojiCategory::Food: return QStringLiteral("reactions/art");
    case catalog::EmojiCategory::Activities: return QStringLiteral("system/bolt");
    case catalog::EmojiCategory::Travel: return QStringLiteral("system/pin");
    case catalog::EmojiCategory::Objects: return QStringLiteral("system/clock");
    case catalog::EmojiCategory::Symbols: return QStringLiteral("social/sparkles");
    case catalog::EmojiCategory::Flags: return QStringLiteral("social/verified");
    }
    return QStringLiteral("social/smile");
}

} // namespace

class EmojiController::Impl final {
public:
    Impl(QSettings& settings, EmojiController& owner)
        : settings(settings),
          owner(owner),
          provider(catalog, noto::NotoEmojiConfig{
                             .bundledPath = [](const std::string_view key) {
                                 return bundledPathFor(key);
                             },
                         }),
          diskCache([&] {
              cache::EmojiCacheConfig config{};
              config.rootDirectory = std::filesystem::path{cacheDirectory().toStdString()};
              config.maxCacheBytes = 96U * 1024U * 1024U;
              config.maxEntryBytes = MaximumAssetBytes + 64U * 1024U;
              return config;
          }()),
          repository(provider, diskCache, catalog)
    {
        {
            farcaster::transport::qtnetwork::async::NetworkLimits limits{};
            limits.maximumActiveRequests = 8U;
            limits.maximumActiveRequestsPerOrigin = 4U;
            limits.maximumBackgroundActiveRequests = 6U;
            limits.maximumBackgroundActiveRequestsPerOrigin = 3U;
            limits.maximumAcceptedRequests = 24U;
            limits.maximumAcceptedRequestBodyBytes = 1U * 1024U * 1024U;
            limits.maximumBufferedResponseBytes = MaximumAssetBytes + 64U * 1024U;
            limits.replyReadBufferBytes = 64U * 1024U;
            auto transportInstance = farcaster::transport::qtnetwork::async::makeAsyncTransport(
                farcaster::transport::qtnetwork::ProxyPolicy::System,
                farcaster::transport::qtnetwork::ProtocolPolicy::Automatic,
                limits);
            if (transportInstance) {
                transport = std::move(*transportInstance);
            }
        }
        restoreRecents();
    }

    QSettings& settings;
    EmojiController& owner;
    catalog::EmojiCatalog catalog{};
    noto::NotoAnimatedEmojiProvider provider;
    cache::EmojiAssetCache diskCache;
    repository::EmojiAssetRepository repository;
    std::unique_ptr<farcaster::transport::async_http::IAsyncHttpTransport> transport{};
    farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor executor{2U, 32U, 32U * 1024U * 1024U};

    catalog::RecentEmojiStore recentStore{};

    std::mutex pendingMutex{};
    std::unordered_set<std::string> pendingKeys{};

    // Diagnostic counters (bounded, non-identifying).
    std::atomic<std::uint64_t> counterAssetHit{};
    std::atomic<std::uint64_t> counterAssetMiss{};
    std::atomic<std::uint64_t> counterCacheHit{};
    std::atomic<std::uint64_t> counterCacheMiss{};
    std::atomic<std::uint64_t> counterCacheInvalid{};
    std::atomic<std::uint64_t> counterRendererFallback{};
    std::atomic<std::uint64_t> counterAnimationPausedOffscreen{};

    [[nodiscard]] std::optional<std::string> normalizedKeyOf(const QString& sequence) const
    {
        const auto utf8 = sequence.toUtf8();
        return repository.normalizedKey(std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())});
    }

    [[nodiscard]] QString sourceFor(const QString& sequence) const
    {
        const auto decision = repository.decide(
            std::string_view{sequence.toUtf8().constData(), static_cast<std::size_t>(sequence.toUtf8().size())},
            Revision);
        if (decision.asset && !decision.asset->localPath.empty()) {
            return QString::fromUtf8(decision.asset->localPath.string());
        }
        return {};
    }

    void requestAsset(const QString& sequence)
    {
        if (!transport || sequence.trimmed().isEmpty()) {
            return;
        }
        const auto key = normalizedKeyOf(sequence);
        if (!key || !provider.supports(*key)) {
            return;
        }
        const auto utf8SequenceView = std::string_view{
            sequence.toUtf8().constData(),
            static_cast<std::size_t>(sequence.toUtf8().size())};
        if (repository.isAvailable(utf8SequenceView, Revision)) {
            ++counterAssetHit;
            const QString source = sourceFor(sequence);
            if (!source.isEmpty()) {
                emit owner.assetReady(sequence, source);
            }
            return;
        }
        if (repository.recentlyFailed(utf8SequenceView)) {
            ++counterAssetMiss;
            emit owner.assetFailed(sequence);
            return;
        }

        {
            const std::scoped_lock lock{pendingMutex};
            if (pendingKeys.contains(*key)) {
                return; // deduplicated concurrent request
            }
            pendingKeys.insert(*key);
        }
        emit owner.fetchStateChanged();

        const std::string providerId{provider.providerId()};
        const std::string assetKey = *key;
        const std::string utf8Sequence = sequence.toUtf8().toStdString();

        // Disk-cache probe and any network fetch run off the UI thread.
        const auto self = this;
        const auto status = executor.post(
            0U,
            [self, providerId, assetKey, utf8Sequence] {
                self->resolveOffUiThread(providerId, assetKey, utf8Sequence);
            });
        if (!status) {
            const std::scoped_lock lock{pendingMutex};
            pendingKeys.erase(assetKey);
            ++counterAssetMiss;
            repository.markUnavailable(utf8Sequence);
            emit owner.fetchStateChanged();
        }
    }

    void resolveOffUiThread(
        const std::string& providerId,
        const std::string& assetKey,
        const std::string& utf8Sequence)
    {
        // 1. Disk cache hit (already validated at store time; cache re-checks
        //    the digest on read and evicts corrupt entries).
        auto cached = diskCache.read(providerId, assetKey, Revision);
        if (cached) {
            ++counterCacheHit;
            registerValidated(providerId, assetKey, Revision, cached->path, utf8Sequence);
            return;
        }
        if (cached.error().code == farcaster::foundation::error::ErrorCode::NotFound) {
            ++counterCacheMiss;
        } else {
            ++counterCacheInvalid;
        }

        // 2. Network fetch through the shared async transport.
        const QString urlText = QString::fromUtf8(provider.downloadUrlFor(assetKey));
        if (urlText.isEmpty()) {
            markFailed(utf8Sequence);
            return;
        }
        farcaster::transport::async_http::Request request{};
        request.method = farcaster::transport::async_http::Method::Get;
        request.url = urlText.toStdString();
        request.timeout = std::chrono::milliseconds{12'000};
        request.maxBodyBytes = MaximumAssetBytes;
        request.priority = farcaster::transport::async_http::Priority::Visible;
        request.retryPolicy = farcaster::transport::async_http::RetryPolicy::TransientRead;
        request.maximumAttempts = 2U;

        auto operation = transport->start(
            std::move(request),
            [self = this, providerId, assetKey, utf8Sequence](
                farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse> result) {
                if (!result || result->statusCode < 200U || result->statusCode >= 300U ||
                    result->body.empty() || result->body.size() > MaximumAssetBytes) {
                    self->markFailed(utf8Sequence);
                    return;
                }
                self->validateAndStore(providerId, assetKey, utf8Sequence, result->body);
            });
        if (!operation) {
            markFailed(utf8Sequence);
        }
    }

    void validateAndStore(
        const std::string& providerId,
        const std::string& assetKey,
        const std::string& utf8Sequence,
        const std::string& body)
    {
        const auto bytes = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(body.data()),
            body.size(),
        };
        auto validated = validator::validateEmojiAsset(providerId, assetKey, bytes);
        if (!validated) {
            markFailed(utf8Sequence);
            return;
        }
        const auto stored = diskCache.store(providerId, assetKey, Revision, bytes, validated->assetSha256);
        if (!stored) {
            markFailed(utf8Sequence);
            return;
        }
        registerValidated(providerId, assetKey, Revision, diskCache.pathFor(providerId, assetKey, Revision), utf8Sequence);
    }

    void registerValidated(
        const std::string& providerId,
        const std::string& assetKey,
        const std::uint32_t revision,
        const std::filesystem::path& path,
        const std::string& utf8Sequence)
    {
        farcaster::domain::emoji::AnimatedEmojiAsset asset{};
        asset.unicodeSequence = utf8Sequence;
        asset.provider = providerId;
        asset.assetKey = assetKey;
        asset.localPath = path;
        asset.revision = revision;
        asset.animated = true;
        asset.availableOffline = true;
        repository.registerValidatedAsset(asset);

        const QString sequence = QString::fromUtf8(utf8Sequence);
        const QString source = QString::fromUtf8(path.string());
        finishRequest(assetKey, [this, sequence, source] {
            ++counterAssetHit;
            emit owner.assetReady(sequence, source);
        });
    }

    void markFailed(const std::string& utf8Sequence)
    {
        const std::optional<std::string> key = normalizedKeyOf(QString::fromUtf8(utf8Sequence));
        repository.markUnavailable(utf8Sequence);
        ++counterAssetMiss;
        finishRequest(key.value_or(""), [this, sequence = QString::fromUtf8(utf8Sequence)] {
            emit owner.assetFailed(sequence);
        });
    }

    void finishRequest(const std::string& key, std::function<void()> onUiThread)
    {
        {
            const std::scoped_lock lock{pendingMutex};
            pendingKeys.erase(key);
        }
        // Hop to the Qt main thread before touching QObject state or signals.
        QMetaObject::invokeMethod(&owner, [this, callback = std::move(onUiThread)] {
            callback();
            emit owner.fetchStateChanged();
        }, Qt::QueuedConnection);
    }

    void recordUsage(const QString& sequence)
    {
        if (sequence.trimmed().isEmpty()) {
            return;
        }
        recentStore.recordUsage(
            sequence.toUtf8().toStdString(),
            QDateTime::currentMSecsSinceEpoch());
        persistRecents();
        emit owner.recentsChanged();
    }

    void persistRecents()
    {
        QJsonArray rows{};
        for (const auto& record : recentStore.snapshot()) {
            rows.append(QJsonObject{
                {QStringLiteral("unicode"), QString::fromUtf8(record.unicode)},
                {QStringLiteral("count"), static_cast<double>(record.useCount)},
                {QStringLiteral("lastUsedMs"), static_cast<double>(record.lastUsedEpochMs)},
            });
        }
        settings.setValue(QString::fromLatin1(RecentsKey), QJsonDocument{rows}.toJson(QJsonDocument::Compact));
        settings.sync();
    }

    void restoreRecents()
    {
        const auto stored = settings.value(QString::fromLatin1(RecentsKey)).toByteArray();
        if (stored.isEmpty()) {
            return;
        }
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(stored, &error);
        if (error.error != QJsonParseError::NoError || !document.isArray()) {
            settings.remove(QString::fromLatin1(RecentsKey));
            return;
        }
        std::vector<catalog::RecentEmojiRecord> records{};
        for (const auto& value : document.array()) {
            const auto row = value.toObject();
            const QString unicode = row.value(QStringLiteral("unicode")).toString().trimmed();
            if (unicode.isEmpty()) {
                continue;
            }
            records.push_back(catalog::RecentEmojiRecord{
                .unicode = unicode.toUtf8().toStdString(),
                .useCount = static_cast<std::uint64_t>(row.value(QStringLiteral("count")).toDouble()),
                .lastUsedEpochMs = static_cast<std::int64_t>(row.value(QStringLiteral("lastUsedMs")).toDouble()),
            });
        }
        recentStore.restore(records);
    }
};

EmojiController::EmojiController(QSettings& settings, QObject* parent)
    : QObject(parent),
      m_impl(std::make_unique<Impl>(settings, *this))
{
}

EmojiController::~EmojiController() = default;

bool EmojiController::available() const noexcept
{
    return m_impl->transport != nullptr;
}

bool EmojiController::lottieAvailable() const noexcept
{
#ifdef FARCASTER_HAS_LOTTIE
    return true;
#else
    return false;
#endif
}

QString EmojiController::styleId() const
{
    return QStringLiteral("noto-animated");
}

QVariantList EmojiController::emojiGroups() const
{
    QVariantList groups{};

    const auto recentsList = recents();
    if (!recentsList.isEmpty()) {
        groups.push_back(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("recent")},
            {QStringLiteral("titleKey"), QStringLiteral("genycaster.emoji.category.recent")},
            {QStringLiteral("icon"), QStringLiteral("system/clock")},
            {QStringLiteral("values"), recentsList},
        });
    }

    for (std::size_t index = 0; index <= static_cast<std::size_t>(catalog::EmojiCategory::Flags); ++index) {
        const auto category = static_cast<catalog::EmojiCategory>(index);
        QVariantList values{};
        for (const auto& entry : m_impl->catalog.byCategory(category)) {
            values.push_back(QString::fromUtf8(entry.unicode.data(), static_cast<qsizetype>(entry.unicode.size())));
        }
        groups.push_back(QVariantMap{
            {QStringLiteral("id"), QString::fromUtf8(catalog::EmojiCatalog::categoryId(category))},
            {QStringLiteral("titleKey"),
             QString::fromUtf8(catalog::EmojiCatalog::categoryLabelId(category))},
            {QStringLiteral("icon"), categoryIcon(category)},
            {QStringLiteral("values"), values},
        });
    }
    return groups;
}

QVariantList EmojiController::recents() const
{
    QVariantList result{};
    for (const auto& record : m_impl->recentStore.top(32U)) {
        result.push_back(QString::fromUtf8(record.unicode));
    }
    return result;
}

QVariantMap EmojiController::diagnostics() const
{
    return QVariantMap{
        {QStringLiteral("assetHit"), static_cast<qulonglong>(m_impl->counterAssetHit.load())},
        {QStringLiteral("assetMiss"), static_cast<qulonglong>(m_impl->counterAssetMiss.load())},
        {QStringLiteral("cacheHit"), static_cast<qulonglong>(m_impl->counterCacheHit.load())},
        {QStringLiteral("cacheMiss"), static_cast<qulonglong>(m_impl->counterCacheMiss.load())},
        {QStringLiteral("cacheInvalid"), static_cast<qulonglong>(m_impl->counterCacheInvalid.load())},
        {QStringLiteral("rendererFallback"), static_cast<qulonglong>(m_impl->counterRendererFallback.load())},
        {QStringLiteral("animationPausedOffscreen"), static_cast<qulonglong>(m_impl->counterAnimationPausedOffscreen.load())},
    };
}

int EmojiController::pendingFetches() const noexcept
{
    const std::scoped_lock lock{m_impl->pendingMutex};
    return static_cast<int>(m_impl->pendingKeys.size());
}

QString EmojiController::assetSourceFor(const QString& sequence) const
{
    const auto decision = m_impl->repository.decide(
        std::string_view{sequence.toUtf8().constData(), static_cast<std::size_t>(sequence.toUtf8().size())},
        Revision);
    if (decision.asset && !decision.asset->localPath.empty()) {
        return QString::fromUtf8(decision.asset->localPath.string());
    }
    return {};
}

bool EmojiController::animatedAvailableFor(const QString& sequence) const
{
    const auto utf8 = sequence.toUtf8();
    return m_impl->repository.isAvailable(
        std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())},
        Revision);
}

void EmojiController::requestAsset(const QString& sequence)
{
    m_impl->requestAsset(sequence);
}

void EmojiController::recordUsage(const QString& sequence)
{
    m_impl->recordUsage(sequence);
}

QVariantList EmojiController::search(const QString& query, const int limit) const
{
    QVariantList result{};
    const auto utf8 = query.toUtf8();
    for (const auto unicode :
         m_impl->catalog.search(std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())},
                                static_cast<std::size_t>(std::max(0, limit)))) {
        result.push_back(QString::fromUtf8(unicode.data(), static_cast<qsizetype>(unicode.size())));
    }
    return result;
}

QVariantMap EmojiController::entryFor(const QString& sequence) const
{
    const auto utf8 = sequence.toUtf8();
    const auto* entry = m_impl->catalog.find(
        std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())});
    if (entry == nullptr) {
        return {};
    }
    return QVariantMap{
        {QStringLiteral("unicode"), QString::fromUtf8(entry->unicode.data(), static_cast<qsizetype>(entry->unicode.size()))},
        {QStringLiteral("name"), QString::fromUtf8(entry->name.data(), static_cast<qsizetype>(entry->name.size()))},
        {QStringLiteral("category"), QString::fromUtf8(catalog::EmojiCatalog::categoryId(entry->category))},
        {QStringLiteral("key"), QString::fromUtf8(entry->key.data(), static_cast<qsizetype>(entry->key.size()))},
    };
}

QVariantMap EmojiController::sourcesForSequences(const QVariantList& sequences) const
{
    QVariantMap result{};
    for (const auto& value : sequences) {
        const QString sequence = value.toString();
        const QString source = assetSourceFor(sequence);
        if (!source.isEmpty()) {
            result.insert(sequence, source);
        }
    }
    return result;
}

void EmojiController::clearRecents()
{
    m_impl->recentStore.clear();
    m_impl->persistRecents();
    emit recentsChanged();
}