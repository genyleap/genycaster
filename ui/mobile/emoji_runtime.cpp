module;

#include "emoji_controller.hpp"

#include <QByteArray>
#include <QBuffer>
#include <QCache>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.runtime.emoji;

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
namespace catalog = farcaster::application::emoji::catalog;
namespace repository = farcaster::application::emoji::repository;
namespace noto = farcaster::provider::emoji::noto;
namespace cache = farcaster::provider::emoji::cache;
namespace validator = farcaster::provider::emoji::validator;

constexpr auto Revision = 1U;
constexpr std::size_t MaximumAssetBytes = 2U * 1024U * 1024U;
constexpr std::size_t MaximumRasterBytes = 8U * 1024U * 1024U;
constexpr auto SpriteProvider = "noto-sprite-96-v1";
constexpr int SpriteFrameSize = 96;
constexpr int SpriteColumns = 12;
constexpr int MaximumSpriteFrames = 120;
constexpr int MaximumInputFrames = 600;
constexpr int MaximumDurationMs = 20'000;

// Cancellation is monotonic. Re-entry waits for a cancelled job to leave the
// bounded queue instead of reviving a decoder that already observed cancellation.
struct SpriteTicket final {
    std::atomic<bool> wanted{true};
    std::mutex mutex;
    farcaster::transport::async_http::Operation operation;

    void cancel()
    {
        wanted.store(false);
        const std::scoped_lock lock{mutex};
        operation.cancel();
    }

    void attach(farcaster::transport::async_http::Operation value)
    {
        const std::scoped_lock lock{mutex};
        operation = std::move(value);
        if (!wanted.load()) operation.cancel();
    }
};

// Runs exclusively on the media worker. The resulting PNG contains the whole
// animation, so the picker never parses vector paths or decodes GIF frames.
[[nodiscard]] farcaster::foundation::result::Result<QByteArray> prepareSprite(
    const std::string& body, const std::atomic<bool>& wanted)
{
    const auto invalid = [](const QString& detail) {
        return std::unexpected(farcaster::foundation::error::Error::make(
            farcaster::foundation::error::ErrorCode::ProtocolFailure, detail.toStdString()));
    };
    if (body.size() < 10U || body.size() > MaximumRasterBytes)
        return invalid(QStringLiteral("decode: invalid GIF body size"));
    const QByteArray input = QByteArray::fromRawData(body.data(), static_cast<qsizetype>(body.size()));
    if (!input.startsWith("GIF87a") && !input.startsWith("GIF89a"))
        return invalid(QStringLiteral("decode: response is not a GIF"));
    const auto dimension = [&input](int offset) {
        return static_cast<unsigned char>(input[offset])
            | (static_cast<unsigned char>(input[offset + 1]) << 8);
    };
    if (dimension(6) < 8 || dimension(8) < 8
        || dimension(6) > 512 || dimension(8) > 512)
        return invalid(QStringLiteral("decode: GIF canvas exceeds bounds"));
    QBuffer buffer;
    buffer.setData(input);
    if (!buffer.open(QIODevice::ReadOnly)) return invalid(QStringLiteral("decode: input buffer unavailable"));
    QImageReader reader{&buffer, "gif"};
    const int count = reader.imageCount();
    if (count < 1 || count > MaximumInputFrames)
        return invalid(QStringLiteral("decode: invalid frame count %1 (%2)").arg(count).arg(reader.errorString()));
    std::vector<QImage> frames;
    std::vector<int> ends;
    frames.reserve(static_cast<std::size_t>(count));
    ends.reserve(static_cast<std::size_t>(count));
    int duration{};
    for (int index = 0; index < count; ++index) {
        if (!wanted.load()) return {};
        const auto size = reader.size();
        if (size.width() > 512 || size.height() > 512)
            return invalid(QStringLiteral("decode: GIF frame exceeds bounds"));
        const QImage image = reader.read();
        if (image.isNull()) return invalid(QStringLiteral("decode: frame %1: %2").arg(index).arg(reader.errorString()));
        if (image.width() > 512 || image.height() > 512)
            return invalid(QStringLiteral("decode: decoded frame exceeds bounds"));
        duration += std::max(10, reader.nextImageDelay());
        if (duration > MaximumDurationMs) return invalid(QStringLiteral("decode: animation exceeds duration limit"));
        frames.push_back(image.scaled(SpriteFrameSize, SpriteFrameSize,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ends.push_back(duration);
    }
    const int frameCount = std::clamp((duration + 39) / 40, 1, MaximumSpriteFrames);
    const int frameDuration = std::max(40, (duration + frameCount - 1) / frameCount);
    QImage sheet{SpriteColumns * SpriteFrameSize,
        ((frameCount + SpriteColumns - 1) / SpriteColumns) * SpriteFrameSize,
        QImage::Format_ARGB32_Premultiplied};
    if (sheet.isNull()) return invalid(QStringLiteral("atlas: image allocation failed"));
    sheet.fill(Qt::transparent);
    {
        QPainter painter{&sheet};
        std::size_t inputIndex{};
        for (int index = 0; index < frameCount; ++index) {
            const int time = index * frameDuration;
            while (inputIndex + 1 < ends.size() && ends[inputIndex] <= time) ++inputIndex;
            const QImage& frame = frames[inputIndex];
            painter.drawImage((index % SpriteColumns) * SpriteFrameSize + (SpriteFrameSize - frame.width()) / 2,
                (index / SpriteColumns) * SpriteFrameSize + (SpriteFrameSize - frame.height()) / 2, frame);
        }
    }
    sheet.setText(QStringLiteral("frameCount"), QString::number(frameCount));
    sheet.setText(QStringLiteral("frameDuration"), QString::number(frameDuration));
    QByteArray result;
    QBuffer output{&result};
    if (!wanted.load()) return QByteArray{};
    if (!output.open(QIODevice::WriteOnly) || !sheet.save(&output, "PNG"))
        return invalid(QStringLiteral("atlas: PNG encoding failed"));
    return result;
}

[[nodiscard]] QVariantMap spriteMetadata(const std::filesystem::path& path)
{
    const QString fileName = QString::fromStdString(path.string());
    QImageReader reader{fileName, "png"};
    const auto size = reader.size();
    if (size.width() != SpriteColumns * SpriteFrameSize || size.height() <= 0
        || size.height() > (MaximumSpriteFrames / SpriteColumns) * SpriteFrameSize) return {};
    const int count = reader.text(QStringLiteral("frameCount")).toInt();
    const int duration = reader.text(QStringLiteral("frameDuration")).toInt();
    if (count < 1 || count > MaximumSpriteFrames || duration < 40
        || duration > MaximumDurationMs || count * duration > MaximumDurationMs + 120
        || ((count + SpriteColumns - 1) / SpriteColumns) * SpriteFrameSize != size.height()) return {};
    return {
        {QStringLiteral("source"), QUrl::fromLocalFile(fileName).toString(QUrl::FullyEncoded)},
        {QStringLiteral("frameCount"), count},
        {QStringLiteral("frameDuration"), duration},
        {QStringLiteral("frameSize"), SpriteFrameSize},
    };
}

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

} // namespace

namespace farcaster::runtime::emoji {
class EmojiRuntime::Impl final {
public:
    Impl(EmojiController& controller, std::filesystem::path cacheRoot)
        : m_owner(controller),
          m_provider(m_catalog, noto::NotoEmojiConfig{
                             .bundledPath = [](const std::string_view key) {
                                 return bundledPathFor(key);
                             },
                         }),
          m_diskCache([&] {
              cache::EmojiCacheConfig config{};
              config.rootDirectory = cacheRoot.empty()
                  ? std::filesystem::path{cacheDirectory().toStdString()} : cacheRoot;
              config.maxCacheBytes = 96U * 1024U * 1024U;
              config.maxEntryBytes = MaximumAssetBytes + 64U * 1024U;
              return config;
          }()),
          m_repository(m_provider, m_diskCache, m_catalog),
          m_spriteDiskCache([&] {
              cache::EmojiCacheConfig config{};
              config.rootDirectory = std::filesystem::path{
                  QDir{cacheRoot.empty() ? cacheDirectory() : QString::fromStdString(cacheRoot.string())}
                      .filePath(QStringLiteral("sprites-v1")).toStdString()};
              config.maxCacheBytes = 128U * 1024U * 1024U;
              config.maxEntryBytes = MaximumRasterBytes;
              config.format = cache::EmojiCacheFormat::SpritePng;
              return config;
          }())
    {
        m_owner.setAssetBackend({
            .source = [this](const QString& sequence) { return sourceFor(sequence); },
            .request = [this](const QString& sequence) { requestAsset(sequence); },
            .pending = [this] {
                const std::scoped_lock lock{m_pendingMutex};
                return static_cast<int>(m_pendingKeys.size()) + static_cast<int>(m_spriteTickets.size());
            },
            .sprite = [this](const QString& sequence) {
                const auto* value = m_sprites.object(sequence);
                return value ? *value : QVariantMap{};
            },
            .requestSprite = [this](const QString& sequence, bool retry) { return requestSprite(sequence, retry); },
            .releaseSprite = [this](const QString& sequence) { releaseSprite(sequence); },
            .requestCached = [this](const QString& sequence) { requestCachedAsset(sequence); },
        });
    }

private:
    EmojiController& m_owner;
    QObject m_dispatchContext{};
    catalog::EmojiCatalog m_catalog{};
    noto::NotoAnimatedEmojiProvider m_provider;
    cache::EmojiAssetCache m_diskCache;
    repository::EmojiAssetRepository m_repository;
    cache::EmojiAssetCache m_spriteDiskCache;
    QCache<QString, QVariantMap> m_sprites{512};
    QHash<QString, int> m_spriteInterests;
    QHash<QString, std::shared_ptr<SpriteTicket>> m_spriteTickets;
    QHash<QString, std::chrono::steady_clock::time_point> m_spriteFailures;
    QHash<QString, QString> m_spriteFailureReasons;
    std::deque<QString> m_spriteQueue;
    int m_activeSprites{};
    QSet<QString> m_cachedPreviewPending;

    // Shutdown gate. ~Impl sets this before any member is destroyed so that
    // reactor-thread completion callbacks fired while the transport shuts down
    // (see ~AsyncQtHttpTransport) cannot touch a dying owner or its state.
    std::atomic<bool> m_shuttingDown{false};

    // Cross-thread request state. Must be declared before (so it is destroyed
    // after) the network infrastructure below: executor draining and transport
    // shutdown can still run completion paths that lock pendingMutex and touch
    // the pending request state.
    std::mutex m_networkMutex{};
    std::mutex m_pendingMutex{};
    std::unordered_set<std::string> m_pendingKeys{};

public:
    ~Impl()
    {
        {
            const std::scoped_lock lock{m_networkMutex};
            m_shuttingDown = true;
        }
        m_owner.setAssetBackend({});
        for (const auto& ticket : m_spriteTickets) ticket->cancel();
        m_executor.reset();
        m_transport.reset();
    }

private:
    // Network infrastructure. Declared last so it is destroyed first: the
    // executor drains queued jobs while every dependency is still alive, and
    // the transport is torn down while the state above (which its shutdown
    // callbacks may still touch) is still alive.
    std::unique_ptr<farcaster::transport::async_http::IAsyncHttpTransport> m_transport{};
    std::unique_ptr<farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor> m_executor{};

    void ensureExecutor()
    {
        if (!m_executor) {
            m_executor = std::make_unique<farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor>(
                1U, 48U, 32U * 1024U * 1024U);
        }
    }

    bool requestSprite(const QString& sequence, bool retry)
    {
        if (auto ticket = m_spriteTickets.value(sequence)) {
            if (!ticket->wanted.load()) return false;
            ++m_spriteInterests[sequence];
            return true;
        }
        if (m_spriteTickets.size() >= 32) return false;
        ++m_spriteInterests[sequence];
        const auto key = normalizedKeyOf(sequence);
        if (!m_owner.spriteAvailable() || !key || !m_provider.supports(*key)) {
            emit m_owner.spriteFailed(sequence, QStringLiteral("availability: GIF decoder or catalog key unavailable"));
            return true;
        }
        const auto failure = m_spriteFailures.constFind(sequence);
        if (!retry && failure != m_spriteFailures.cend()
            && std::chrono::steady_clock::now() - failure.value() < std::chrono::seconds{30}) {
            emit m_owner.spriteFailed(sequence, m_spriteFailureReasons.value(sequence));
            return true;
        }
        ensureExecutor();
        m_spriteTickets.insert(sequence, std::make_shared<SpriteTicket>());
        m_spriteQueue.push_back(sequence);
        // qDebug().noquote() << "Genycaster emoji" << sequence << "queue: sprite admitted";
        pumpSprites();
        emit m_owner.fetchStateChanged();
        return true;
    }

    void releaseSprite(const QString& sequence)
    {
        auto interest = m_spriteInterests.find(sequence);
        if (interest == m_spriteInterests.end()) return;
        if (--interest.value() > 0) return;
        m_spriteInterests.erase(interest);
        if (auto ticket = m_spriteTickets.value(sequence)) ticket->cancel();
        // Remove cancelled queued jobs immediately, even with both active slots
        // occupied. A rapid category switch must not inherit a stale full queue.
        if (std::erase(m_spriteQueue, sequence) > 0) m_spriteTickets.remove(sequence);
        emit m_owner.fetchStateChanged();
    }

    void pumpSprites()
    {
        // Two network/cache jobs, one decoder worker. A category change cancels
        // unused queued work and checks the active decoder between input frames.
        while (m_activeSprites < 2 && !m_spriteQueue.empty() && !m_shuttingDown) {
            const QString sequence = m_spriteQueue.front();
            m_spriteQueue.pop_front();
            auto ticket = m_spriteTickets.value(sequence);
            if (!ticket || !ticket->wanted.load()) {
                m_spriteTickets.remove(sequence);
                continue;
            }
            ++m_activeSprites;
            const auto status = m_executor->post(0U, [this, sequence, ticket] {
                resolveSprite(sequence, ticket);
            });
            if (!status) finishSprite(sequence, ticket, {}, QStringLiteral("queue: worker admission failed"));
        }
    }

    void finishSprite(const QString& sequence, const std::shared_ptr<SpriteTicket>& ticket,
                      QVariantMap metadata, QString failure = {})
    {
        if (m_shuttingDown) return;
        QMetaObject::invokeMethod(&m_dispatchContext,
            [this, sequence, ticket, metadata = std::move(metadata), failure = std::move(failure)] {
                --m_activeSprites;
                m_spriteTickets.remove(sequence);
                if (!metadata.isEmpty()) {
                    m_sprites.insert(sequence, new QVariantMap{metadata});
                    m_spriteFailures.remove(sequence);
                    m_spriteFailureReasons.remove(sequence);
                    if (ticket->wanted.load()) emit m_owner.spriteReady(sequence, metadata);
                } else if (ticket->wanted.load()) {
                    m_sprites.remove(sequence);
                    m_spriteFailures.insert(sequence, std::chrono::steady_clock::now());
                    const QString reason = failure.isEmpty() ? QStringLiteral("atlas: preparation returned no frames") : failure;
                    m_spriteFailureReasons.insert(sequence, reason);
                    // qWarning().noquote() << "Genycaster emoji" << sequence << reason;
                    emit m_owner.spriteFailed(sequence, reason);
                }
                pumpSprites();
                emit m_owner.fetchStateChanged();
            }, Qt::QueuedConnection);
    }

    void resolveSprite(const QString& sequence, const std::shared_ptr<SpriteTicket>& ticket)
    {
        if (m_shuttingDown || !ticket->wanted.load()) {
            finishSprite(sequence, ticket, {});
            return;
        }
        const auto key = normalizedKeyOf(sequence);
        if (!key) {
            finishSprite(sequence, ticket, {}, QStringLiteral("catalog: invalid Unicode sequence"));
            return;
        }
        if (const auto cached = m_spriteDiskCache.read(SpriteProvider, *key, Revision); cached) {
            auto metadata = spriteMetadata(cached->path);
            if (!metadata.isEmpty()) {
                finishSprite(sequence, ticket, std::move(metadata));
                return;
            }
        }
        ensureNetwork();
        if (m_shuttingDown || !m_transport || !ticket->wanted.load()) {
            finishSprite(sequence, ticket, {}, QStringLiteral("network: transport unavailable"));
            return;
        }
        farcaster::transport::async_http::Request request{};
        request.method = farcaster::transport::async_http::Method::Get;
        request.url = m_provider.rasterDownloadUrlFor(*key);
        // Raster animations are larger than Lottie JSON; the shared deadline
        // must leave room for the transport's second transient-read attempt.
        request.timeout = std::chrono::milliseconds{30'000};
        request.maxBodyBytes = MaximumRasterBytes;
        request.priority = farcaster::transport::async_http::Priority::Visible;
        request.retryPolicy = farcaster::transport::async_http::RetryPolicy::TransientRead;
        request.maximumAttempts = 2U;
        // qDebug().noquote() << "Genycaster emoji" << sequence << "network: fetching GIF";
        auto operation = m_transport->start(std::move(request),
            [this, sequence, ticket, key = *key](
                farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse> result) {
                if (m_shuttingDown) return;
                if (!ticket->wanted.load()) {
                    finishSprite(sequence, ticket, {});
                    return;
                }
                if (!result) {
                    finishSprite(sequence, ticket, {}, QStringLiteral("network: ")
                        + QString::fromStdString(result.error().message));
                    return;
                }
                if (result->statusCode < 200U || result->statusCode >= 300U) {
                    finishSprite(sequence, ticket, {}, QStringLiteral("http: status %1").arg(result->statusCode));
                    return;
                }
                if (result->body.empty() || result->body.size() > MaximumRasterBytes) {
                    finishSprite(sequence, ticket, {}, QStringLiteral("http: empty or oversized GIF response"));
                    return;
                }
                const std::scoped_lock lock{m_networkMutex};
                if (m_shuttingDown) return;
                const auto bodySize = result->body.size();
                const auto status = m_executor->post(bodySize,
                    [this, sequence, ticket, key, body = std::move(result->body)] {
                        const auto png = prepareSprite(body, ticket->wanted);
                        if (!ticket->wanted.load()) {
                            finishSprite(sequence, ticket, {});
                            return;
                        }
                        if (!png) {
                            finishSprite(sequence, ticket, {}, QString::fromStdString(png.error().message));
                            return;
                        }
                        const auto bytes = std::span<const std::byte>{
                            reinterpret_cast<const std::byte*>(png->constData()),
                            static_cast<std::size_t>(png->size())};
                        const auto stored = m_spriteDiskCache.store(SpriteProvider, key, Revision,
                            bytes, validator::sha256(bytes));
                        if (!stored) {
                            finishSprite(sequence, ticket, {}, QStringLiteral("cache: ")
                                + QString::fromStdString(stored.error().message));
                            return;
                        }
                        finishSprite(sequence, ticket,
                            spriteMetadata(m_spriteDiskCache.pathFor(SpriteProvider, key, Revision)),
                            QStringLiteral("atlas: stored PNG metadata is invalid"));
                    });
                if (!status) finishSprite(sequence, ticket, {}, QStringLiteral("queue: decoder admission failed"));
            });
        if (!operation) finishSprite(sequence, ticket, {}, QStringLiteral("network: ")
            + QString::fromStdString(operation.error().message));
        else ticket->attach(std::move(*operation));
    }

    // Network infrastructure is created lazily on the first asset fetch. Emoji
    // playback never needs it (bundled assets and the disk cache are always
    // available), so startup and offline sessions never spawn a reactor thread.
    void ensureNetwork()
    {
        const std::scoped_lock lock{m_networkMutex};
        if (m_transport || m_shuttingDown) {
            return;
        }
        farcaster::transport::qtnetwork::async::NetworkLimits limits{};
        limits.maximumActiveRequests = 4U;
        limits.maximumActiveRequestsPerOrigin = 2U;
        limits.maximumBackgroundActiveRequests = 2U;
        limits.maximumBackgroundActiveRequestsPerOrigin = 1U;
        // Special-picker cells resolve automatically on viewport entry. Keep
        // active work small while admitting one wide desktop viewport so cells
        // are not forced into a hover-only retry path.
        limits.maximumAcceptedRequests = 64U;
        limits.maximumAcceptedRequestBodyBytes = 1U * 1024U * 1024U;
        limits.maximumBufferedResponseBytes = MaximumRasterBytes * 2U;
        limits.replyReadBufferBytes = 64U * 1024U;
        auto transportInstance = farcaster::transport::qtnetwork::async::makeAsyncTransport(
            farcaster::transport::qtnetwork::ProxyPolicy::System,
            farcaster::transport::qtnetwork::ProtocolPolicy::Automatic,
            limits);
        if (!transportInstance) {
            return;
        }
        m_transport = std::move(*transportInstance);
    }

    [[nodiscard]] std::optional<std::string> normalizedKeyOf(const QString& sequence) const
    {
        const auto utf8 = sequence.toUtf8();
        return m_repository.normalizedKey(std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())});
    }

    [[nodiscard]] QString sourceFor(const QString& sequence) const
    {
        const auto utf8 = sequence.toUtf8();
        const auto decision = m_repository.decide(
            std::string_view{utf8.constData(), static_cast<std::size_t>(utf8.size())},
            Revision);
        if (decision.asset && !decision.asset->localPath.empty()) {
            const QString path = QString::fromUtf8(decision.asset->localPath.string());
            return path.startsWith(QLatin1Char(':'))
                ? QStringLiteral("qrc") + path
                : QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
        }
        return {};
    }

    void requestCachedAsset(const QString& sequence)
    {
        if (const QString source = sourceFor(sequence); !source.isEmpty()) {
            emit m_owner.assetReady(sequence, source);
            return;
        }
        const auto key = normalizedKeyOf(sequence);
        if (!key || !m_provider.supports(*key) || m_cachedPreviewPending.contains(sequence)
            || m_cachedPreviewPending.size() >= 8) return;
        ensureExecutor();
        m_cachedPreviewPending.insert(sequence);
        const auto status = m_executor->post(0U, [this, sequence, key = *key] {
            if (m_shuttingDown) return;
            const auto cached = m_diskCache.read(m_provider.providerId(), key, Revision);
            const auto path = cached ? cached->path : std::filesystem::path{};
            QMetaObject::invokeMethod(&m_dispatchContext, [this, sequence, key, path] {
                m_cachedPreviewPending.remove(sequence);
                if (path.empty()) return; // Cache miss must never start networking.
                farcaster::domain::emoji::AnimatedEmojiAsset asset{};
                asset.unicodeSequence = sequence.toUtf8().toStdString();
                asset.provider = std::string{m_provider.providerId()};
                asset.assetKey = key;
                asset.localPath = path;
                asset.revision = Revision;
                asset.animated = true;
                asset.availableOffline = true;
                m_repository.registerValidatedAsset(asset);
                emit m_owner.assetReady(sequence, sourceFor(sequence));
            }, Qt::QueuedConnection);
        });
        if (!status) m_cachedPreviewPending.remove(sequence);
    }

    void requestAsset(const QString& sequence)
    {
        if (sequence.trimmed().isEmpty()) {
            return;
        }
        const auto key = normalizedKeyOf(sequence);
        if (!key || !m_provider.supports(*key)) {
            return;
        }
        const auto sequenceBytes = sequence.toUtf8();
        const auto utf8SequenceView = std::string_view{
            sequenceBytes.constData(), static_cast<std::size_t>(sequenceBytes.size())};
        if (m_repository.isAvailable(utf8SequenceView, Revision)) {
            const QString source = sourceFor(sequence);
            if (!source.isEmpty()) {
                emit m_owner.assetReady(sequence, source);
            }
            return;
        }
        if (m_repository.recentlyFailed(utf8SequenceView)) {
            emit m_owner.assetFailed(sequence);
            return;
        }

        if (!m_executor) {
            m_executor = std::make_unique<farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor>(
                1U, 48U, 32U * 1024U * 1024U);
        }

        {
            const std::scoped_lock lock{m_pendingMutex};
            if (m_pendingKeys.contains(*key)) {
                return; // deduplicated concurrent request
            }
            m_pendingKeys.insert(*key);
        }
        emit m_owner.fetchStateChanged();

        const std::string providerId{m_provider.providerId()};
        const std::string assetKey = *key;
        const std::string utf8Sequence = sequence.toUtf8().toStdString();

        // Disk-cache probe and any network fetch run off the UI thread.
        const auto self = this;
        const auto status = m_executor->post(
            0U,
            [self, providerId, assetKey, utf8Sequence] {
                self->resolveOffUiThread(providerId, assetKey, utf8Sequence);
            });
        if (!status) {
            markFailed(utf8Sequence);
        }
    }

    void resolveOffUiThread(
        const std::string& providerId,
        const std::string& assetKey,
        const std::string& utf8Sequence)
    {
        if (m_shuttingDown) {
            return; // m_owner is being destroyed; do not start new work.
        }
        // 1. Disk cache hit (already validated at store time; cache re-checks
        //    the digest on read and evicts corrupt entries).
        auto cached = m_diskCache.read(providerId, assetKey, Revision);
        if (cached) {
            registerValidated(providerId, assetKey, Revision, cached->path, utf8Sequence);
            return;
        }

        // 2. Create networking only after the offline cache misses.
        ensureNetwork();
        if (m_shuttingDown || !m_transport) {
            markFailed(utf8Sequence);
            return;
        }
        const QString urlText = QString::fromUtf8(m_provider.downloadUrlFor(assetKey));
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

        auto operation = m_transport->start(
            std::move(request),
            [self = this, providerId, assetKey, utf8Sequence](
                farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse> result) {
                if (!result || result->statusCode < 200U || result->statusCode >= 300U ||
                    result->body.empty() || result->body.size() > MaximumAssetBytes) {
                    self->markFailed(utf8Sequence);
                    return;
                }
                const std::scoped_lock lock{self->m_networkMutex};
                if (self->m_shuttingDown) {
                    return;
                }
                const auto status = self->m_executor->post(result->body.size(),
                    [self, providerId, assetKey, utf8Sequence, body = std::move(result->body)] {
                        self->validateAndStore(providerId, assetKey, utf8Sequence, body);
                    });
                if (!status) {
                    self->markFailed(utf8Sequence);
                }
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
        const auto stored = m_diskCache.store(providerId, assetKey, Revision, bytes, validated->assetSha256);
        if (!stored) {
            markFailed(utf8Sequence);
            return;
        }
        registerValidated(providerId, assetKey, Revision, m_diskCache.pathFor(providerId, assetKey, Revision), utf8Sequence);
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
        finishRequest(assetKey, [this, asset = std::move(asset)] {
            m_repository.registerValidatedAsset(asset);

            const QString sequence = QString::fromUtf8(asset.unicodeSequence);
            emit m_owner.assetReady(sequence, sourceFor(sequence));
        });
    }

    void markFailed(const std::string& utf8Sequence)
    {
        if (m_shuttingDown) {
            return;
        }
        const std::optional<std::string> key = normalizedKeyOf(QString::fromUtf8(utf8Sequence));

        finishRequest(key.value_or(""), [this, sequence = QString::fromUtf8(utf8Sequence)] {
            m_repository.markUnavailable(sequence.toUtf8().toStdString());
            emit m_owner.assetFailed(sequence);
        });
    }

    void finishRequest(const std::string& key, std::function<void()> onUiThread)
    {
        if (m_shuttingDown) {
            return; // m_owner is going away; never hop back to the UI thread.
        }
        // Hop to the Qt main thread before touching QObject state or signals.
        QMetaObject::invokeMethod(&m_dispatchContext, [this, key, callback = std::move(onUiThread)] {
            {
                const std::scoped_lock lock{m_pendingMutex};
                m_pendingKeys.erase(key);
            }
            callback();
            emit m_owner.fetchStateChanged();
        }, Qt::QueuedConnection);
    }

};

EmojiRuntime::EmojiRuntime(EmojiController& controller, std::filesystem::path cacheRoot)
    : m_impl(std::make_unique<Impl>(controller, std::move(cacheRoot)))
{
}

EmojiRuntime::~EmojiRuntime() = default;

} // namespace farcaster::runtime::emoji
