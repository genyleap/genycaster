#include "remote_image.hpp"

#include <QBuffer>
#include <QCoreApplication>
#include <QByteArray>
#include <QCache>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <QMetaObject>
#include <QQuickTextureFactory>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QStringList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.result;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;
import farcaster.transport.qtnetwork.async;
import farcaster.presentation.media.storage_policy;

namespace {

// Keep recently visited feed media decoded across ListView delegate recycling.
// Bound memory explicitly: WebAssembly has a smaller memory budget than native.
#if defined(Q_OS_WASM)
constexpr int imageCacheCapacityKiB = 96 * 1024;
#else
constexpr int imageCacheCapacityKiB = 128 * 1024;
#endif
constexpr int failedRequestCacheCapacity = 256;
constexpr auto failedRequestBackoff = std::chrono::seconds{30};
constexpr auto forbiddenOriginBackoff = std::chrono::minutes{10};
constexpr int forbiddenOriginCapacity = 64;
constexpr std::size_t imageDecodeWorkerCount = 2U;
constexpr qint64 maximumDecodedPixelCount = 20LL * 1024LL * 1024LL;
constexpr int maximumImageDimension = 16'384;
constexpr qint64 maximumDiskCacheEntryBytes = 64LL * 1024LL * 1024LL;
constexpr std::uint64_t maximumNetworkImageBytes = 64ULL * 1024ULL * 1024ULL;
constexpr int decodedSizeQuantum = 128;

namespace storage_policy = farcaster::presentation::media::storage_policy;

struct RemoteRequestDescriptor final {
    QString urlText{};
    storage_policy::MediaCategory category{storage_policy::MediaCategory::Other};
    bool manual{false};
};

[[nodiscard]] QString categoryId(const storage_policy::MediaCategory category)
{
    const auto id = storage_policy::mediaCategoryId(category);
    return QString::fromLatin1(id.data(), static_cast<qsizetype>(id.size()));
}

[[nodiscard]] QString diskCacheRoot()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return root.isEmpty() ? QString{} : QDir{root}.filePath(QStringLiteral("remote-media"));
}

[[nodiscard]] QString legacyDiskCacheDirectory()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return root.isEmpty() ? QString{} : QDir{root}.filePath(QStringLiteral("remote-images"));
}

[[nodiscard]] QString diskCacheDirectory(const storage_policy::MediaCategory category)
{
    const QString root = diskCacheRoot();
    return root.isEmpty() ? QString{} : QDir{root}.filePath(categoryId(category));
}

[[nodiscard]] QString cacheDigestName(const QString& url)
{
    const QByteArray digest = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(digest) + QStringLiteral(".img");
}

[[nodiscard]] QString diskCachePathForUrl(
    const QString& url, const storage_policy::MediaCategory category)
{
    const QString directory = diskCacheDirectory(category);
    return directory.isEmpty() ? QString{} : QDir{directory}.filePath(cacheDigestName(url));
}

[[nodiscard]] QString legacyDiskCachePathForUrl(const QString& url)
{
    const QString directory = legacyDiskCacheDirectory();
    return directory.isEmpty() ? QString{} : QDir{directory}.filePath(cacheDigestName(url));
}

[[nodiscard]] bool expiredByRetention(
    const QFileInfo& info,
    const storage_policy::PolicySnapshot& policy,
    const storage_policy::MediaCategory category)
{
    const int days = storage_policy::retentionDays(policy, category);
    return days > 0 && info.lastModified().toUTC().daysTo(QDateTime::currentDateTimeUtc()) >= days;
}

[[nodiscard]] QByteArray readOneDiskFile(
    const QString& path,
    const storage_policy::PolicySnapshot& policy,
    const storage_policy::MediaCategory category)
{
    if (path.isEmpty()) {
        return {};
    }
    QFileInfo info{path};
    if (!info.exists() || !info.isFile()) {
        return {};
    }
    if (expiredByRetention(info, policy, category)) {
        QFile::remove(path);
        return {};
    }
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    if (file.size() <= 0 || file.size() > maximumDiskCacheEntryBytes) {
        file.close();
        QFile::remove(path);
        return {};
    }
    const QByteArray bytes = file.readAll();
    // Coarse access-time approximation for LRU without writing metadata on every paint.
    if (info.lastModified().secsTo(QDateTime::currentDateTimeUtc()) > 6 * 60 * 60) {
        file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
    }
    return bytes;
}

void writeDiskCachedBytes(
    const QString& url,
    const storage_policy::MediaCategory category,
    const QByteArray& bytes,
    const storage_policy::PolicySnapshot& policy)
{
    if (!policy.diskCacheEnabled || bytes.isEmpty() || bytes.size() > maximumDiskCacheEntryBytes) {
        return;
    }
    const QString path = diskCachePathForUrl(url, category);
    if (path.isEmpty()) {
        return;
    }
    QDir directory = QFileInfo{path}.dir();
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        return;
    }
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return;
    }
    (void)file.commit();
}

[[nodiscard]] QByteArray readDiskCachedBytes(
    const QString& url,
    const storage_policy::MediaCategory category,
    const storage_policy::PolicySnapshot& policy)
{
    if (!policy.diskCacheEnabled) {
        return {};
    }
    const QString currentPath = diskCachePathForUrl(url, category);
    QByteArray bytes = readOneDiskFile(currentPath, policy, category);
    if (!bytes.isEmpty()) {
        return bytes;
    }

    // v21.5 used one flat remote-images directory. Migrate lazily only when disk caching is
    // enabled; disabling cache must never create or move files as a side effect of a read.
    const QString legacyPath = legacyDiskCachePathForUrl(url);
    bytes = readOneDiskFile(legacyPath, policy, category);
    if (!bytes.isEmpty()) {
        writeDiskCachedBytes(url, category, bytes, policy);
        QFile::remove(legacyPath);
    }
    return bytes;
}

struct CachedImage final {
    QImage image{};
    QSize originalSize{};
};

struct FailedRequest final {
    std::chrono::steady_clock::time_point retryAfter{};
};

QMutex imageCacheMutex{};
QCache<QString, CachedImage> imageCache{imageCacheCapacityKiB};
QCache<QString, FailedRequest> failedRequests{failedRequestCacheCapacity};
QCache<QString, FailedRequest> forbiddenOrigins{forbiddenOriginCapacity};

[[nodiscard]] QString imageOriginKey(const QString& source)
{
    const QUrl url{source};
    if (!url.isValid() || url.host().isEmpty()) return {};
    // An r2.dev hostname identifies one development bucket, so an HTTP
    // authorization denial is meaningful for its whole origin. Generic CDNs
    // may return 403 for one object while serving others publicly; continue
    // using their per-URL negative cache instead of blocking the full host.
    if (!url.host().endsWith(QStringLiteral(".r2.dev"), Qt::CaseInsensitive))
        return {};
    return url.scheme().toLower() + QStringLiteral("://") + url.host().toLower()
        + QLatin1Char(':') + QString::number(url.port(
            url.scheme() == QStringLiteral("https") ? 443 : 80));
}

[[nodiscard]] bool originTemporarilyForbidden(const QString& source)
{
    const QString key = imageOriginKey(source);
    if (key.isEmpty()) return false;
    const QMutexLocker lock{&imageCacheMutex};
    const auto* entry = forbiddenOrigins.object(key);
    if (!entry) return false;
    if (entry->retryAfter > std::chrono::steady_clock::now()) return true;
    forbiddenOrigins.remove(key);
    return false;
}

void rememberForbiddenOrigin(const QString& source)
{
    const QString key = imageOriginKey(source);
    if (key.isEmpty()) return;
    const QMutexLocker lock{&imageCacheMutex};
    forbiddenOrigins.insert(key, new FailedRequest{
        .retryAfter = std::chrono::steady_clock::now() + forbiddenOriginBackoff}, 1);
}

void clearForbiddenOrigin(const QString& source)
{
    const QString key = imageOriginKey(source);
    if (key.isEmpty()) return;
    const QMutexLocker lock{&imageCacheMutex};
    forbiddenOrigins.remove(key);
}

[[nodiscard]] QImage transparentFailureImage(QSize* const size)
{
    const QSize sentinelSize{1, 1};
    if (size != nullptr) {
        *size = sentinelSize;
    }
    QImage image{sentinelSize, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::transparent);
    return image;
}

[[nodiscard]] QString decodeBase64RemoteUrl(QString encodedText)
{
    QByteArray encoded = encodedText.toUtf8();
    while ((encoded.size() % 4) != 0) {
        encoded.append('=');
    }
    return QString::fromUtf8(QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding));
}

[[nodiscard]] RemoteRequestDescriptor decodeRemoteRequest(const QString& id)
{
    if (!id.startsWith(QStringLiteral("v22/"))) {
        return {.urlText = decodeBase64RemoteUrl(id).trimmed()};
    }
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() < 4) {
        return {};
    }
    const QByteArray categoryUtf8 = parts.at(1).toUtf8();
    const auto category = storage_policy::mediaCategoryFromId(
        std::string_view{categoryUtf8.constData(), static_cast<std::size_t>(categoryUtf8.size())});
    return {
        .urlText = decodeBase64RemoteUrl(parts.mid(3).join(QLatin1Char('/'))).trimmed(),
        .category = category,
        .manual = parts.at(2).compare(QStringLiteral("manual"), Qt::CaseInsensitive) == 0,
    };
}

[[nodiscard]] std::string mediaAcceptHeader()
{
    static const std::string header = [] {
        const QList<QByteArray> supportedMimeTypes = QImageReader::supportedMimeTypes();
        QStringList accepted{};

        const auto appendIfSupported = [&](const char* const mimeType) {
            const QByteArray candidate{mimeType};
            if (supportedMimeTypes.contains(candidate)) {
                accepted.push_back(QString::fromLatin1(candidate));
            }
        };

        // Advertise only formats the active Qt installation can actually decode.
        // Cloudflare Images performs content negotiation from Accept and may
        // otherwise return AVIF to a build that has no AVIF image plugin.
        // Keep the negotiated set deliberately conservative for social media.
        // Some Qt installations advertise optional AVIF/HEIC readers that
        // still fail on CDN-produced variants. Asking a CDN for a stable WebP,
        // PNG or JPEG representation avoids a transparent 1x1 failure sentinel
        // for otherwise valid profile banners.
        appendIfSupported("image/webp");
        appendIfSupported("image/png");
        appendIfSupported("image/jpeg");
        appendIfSupported("image/gif");
        appendIfSupported("image/svg+xml");

        if (accepted.isEmpty()) {
            accepted.push_back(QStringLiteral("image/png"));
            accepted.push_back(QStringLiteral("image/jpeg"));
        }
        accepted.push_back(QStringLiteral("*/*;q=0.1"));
        return (QStringLiteral("Accept: ") + accepted.join(QLatin1Char(','))).toStdString();
    }();
    return header;
}

[[nodiscard]] std::string mediaCompatibilityAcceptHeader()
{
    // A second decode attempt asks content-negotiating CDNs for the most
    // conservative raster formats first. This recovers valid social-media
    // images when a CDN's preferred WebP variant is malformed for the active
    // platform plugin without sending network I/O through QML.
    return "Accept: image/jpeg,image/png;q=0.95,image/webp;q=0.90,*/*;q=0.1";
}

[[nodiscard]] std::string mediaUserAgentHeader()
{
    return "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X) AppleWebKit/537.36 "
           "(KHTML, like Gecko) Genycaster/1.0 Safari/537.36";
}

[[nodiscard]] QString responseHeaderValue(
    const std::vector<std::string>& headers,
    const std::string_view wantedName)
{
    const QByteArray wanted{wantedName.data(), static_cast<qsizetype>(wantedName.size())};
    for (const auto& header : headers) {
        const QByteArray line = QByteArray::fromStdString(header);
        const qsizetype separator = line.indexOf(':');
        if (separator <= 0) {
            continue;
        }
        if (line.first(separator).trimmed().compare(wanted, Qt::CaseInsensitive) == 0) {
            return QString::fromLatin1(line.sliced(separator + 1).trimmed());
        }
    }
    return {};
}

[[nodiscard]] bool recognizedImagePayload(const QByteArray& bytes)
{
    if (bytes.isEmpty()) {
        return false;
    }
    QBuffer buffer{};
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return false;
    }
    return !QImageReader::imageFormat(&buffer).isEmpty();
}

[[nodiscard]] bool looksLikeMarkupPayload(const QByteArray& bytes)
{
    const QByteArray prefix = bytes.left(512).trimmed().toLower();
    return prefix.startsWith("<!doctype html") || prefix.startsWith("<html")
        || prefix.startsWith("<?xml") || prefix.contains("<head")
        || prefix.contains("<meta") || prefix.contains("<body");
}

[[nodiscard]] bool responseMayContainImage(
    const farcaster::transport::http::HttpResponse& response)
{
    if (response.body.empty()) {
        return false;
    }

    const QByteArray bytes = QByteArray::fromRawData(
        response.body.data(), static_cast<qsizetype>(response.body.size()));
    if (recognizedImagePayload(bytes)) {
        return true;
    }
    // Do not trust a remote Content-Type over the actual payload. Some
    // image URLs resolve to HTML (including branded 200/error pages) while
    // still advertising image/*. Those pages cannot recover through a
    // compatibility Accept retry and must never reach QImageReader.
    if (looksLikeMarkupPayload(bytes)) {
        return false;
    }

    const QString contentType = responseHeaderValue(response.headers, "Content-Type")
                                    .section(QLatin1Char(';'), 0, 0)
                                    .trimmed()
                                    .toLower();
    if (contentType.startsWith(QStringLiteral("image/"))) {
        // The active Qt image plugin may still reject the negotiated image
        // format. Let the normal compatibility Accept retry handle that case.
        return true;
    }

    if (contentType.isEmpty() || contentType == QStringLiteral("application/octet-stream")) {
        return !looksLikeMarkupPayload(bytes);
    }

    // Never send HTML, JSON, XML, text, video or other known non-image bodies
    // through QImageReader. Mini App metadata in the wild can legally be wrong
    // and occasionally points imageUrl at the app page itself.
    return false;
}

[[nodiscard]] int quantizedRequestedDimension(const int value) noexcept
{
    if (value <= 0) {
        return 0;
    }
    const qint64 widened = value;
    const qint64 quantized = ((widened + decodedSizeQuantum - 1) / decodedSizeQuantum)
        * decodedSizeQuantum;
    return static_cast<int>(std::min<qint64>(quantized, std::numeric_limits<int>::max()));
}

[[nodiscard]] QSize canonicalRequestedSize(const QSize& requestedSize) noexcept
{
    return {
        quantizedRequestedDimension(requestedSize.width()),
        quantizedRequestedDimension(requestedSize.height()),
    };
}

[[nodiscard]] QString imageCacheKey(const QString& url, const QSize& requestedSize)
{
    return url + QStringLiteral("#") + QString::number(requestedSize.width()) +
        QStringLiteral("x") + QString::number(requestedSize.height());
}

[[nodiscard]] CachedImage cachedImage(const QString& key)
{
    const QMutexLocker lock{&imageCacheMutex};
    if (const auto* const cached = imageCache.object(key); cached != nullptr) {
        return *cached;
    }
    return {};
}

[[nodiscard]] bool requestIsBackedOff(const QString& url)
{
    const auto now = std::chrono::steady_clock::now();
    const QMutexLocker lock{&imageCacheMutex};
    const auto* const failed = failedRequests.object(url);
    if (failed == nullptr) {
        return false;
    }
    if (failed->retryAfter > now) {
        return true;
    }
    failedRequests.remove(url);
    return false;
}

void rememberFailure(const QString& url)
{
    const QMutexLocker lock{&imageCacheMutex};
    failedRequests.insert(
        url,
        new FailedRequest{.retryAfter = std::chrono::steady_clock::now() + failedRequestBackoff},
        1);
}

void clearFailure(const QString& url)
{
    const QMutexLocker lock{&imageCacheMutex};
    failedRequests.remove(url);
}

[[nodiscard]] int imageCostKiB(const QImage& image) noexcept
{
    constexpr qsizetype kibibyte = 1024;
    const qsizetype bytes = image.sizeInBytes();
    if (bytes <= 0) {
        return 1;
    }
    const qsizetype cost = (bytes + kibibyte - 1) / kibibyte;
    return static_cast<int>(std::clamp<qsizetype>(
        cost,
        1,
        std::numeric_limits<int>::max()));
}

void rememberImage(
    const QString& key,
    const QImage& image,
    const QSize& originalSize)
{
    if (image.isNull()) {
        return;
    }
    const QMutexLocker lock{&imageCacheMutex};
    imageCache.insert(
        key,
        new CachedImage{.image = image, .originalSize = originalSize},
        imageCostKiB(image));
}

[[nodiscard]] bool acceptableImageDimensions(const QSize& size) noexcept
{
    if (!size.isValid() || size.isEmpty()) {
        return false;
    }
    if (size.width() > maximumImageDimension || size.height() > maximumImageDimension) {
        return false;
    }
    const qint64 pixelCount = static_cast<qint64>(size.width()) * size.height();
    return pixelCount > 0 && pixelCount <= maximumDecodedPixelCount;
}

[[nodiscard]] QSize boundedDecodeSize(
    const QSize& originalSize,
    const QSize& requestedSize) noexcept
{
    if (!acceptableImageDimensions(originalSize)) {
        return {};
    }

    const int requestedWidth = requestedSize.width();
    const int requestedHeight = requestedSize.height();
    if (requestedWidth <= 0 && requestedHeight <= 0) {
        return originalSize;
    }

    QSize target = originalSize;
    if (requestedWidth > 0 && requestedHeight > 0) {
        target.scale(QSize{requestedWidth, requestedHeight}, Qt::KeepAspectRatio);
    } else if (requestedWidth > 0 && originalSize.width() > requestedWidth) {
        target.setWidth(requestedWidth);
        target.setHeight(std::max(
            1,
            static_cast<int>(std::lround(
                static_cast<double>(originalSize.height()) * requestedWidth /
                originalSize.width()))));
    } else if (requestedHeight > 0 && originalSize.height() > requestedHeight) {
        target.setHeight(requestedHeight);
        target.setWidth(std::max(
            1,
            static_cast<int>(std::lround(
                static_cast<double>(originalSize.width()) * requestedHeight /
                originalSize.height()))));
    }

    if (target.width() > originalSize.width() || target.height() > originalSize.height()) {
        return originalSize;
    }
    return target;
}

[[nodiscard]] CachedImage decodeImage(
    const QByteArray& bytes,
    const QSize& requestedSize,
    QString* const errorText)
{
    QBuffer buffer{};
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Could not open the in-memory image buffer.");
        }
        return {};
    }

    QImageReader reader{&buffer};
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);

    QSize originalSize = reader.size();
    if (acceptableImageDimensions(originalSize)) {
        const QSize decodeSize = boundedDecodeSize(originalSize, requestedSize);
        if (!decodeSize.isValid() || decodeSize.isEmpty()) {
            if (errorText != nullptr) {
                *errorText = QStringLiteral("Unsupported or unsafe image dimensions.");
            }
            return {};
        }
        if (decodeSize != originalSize) {
            reader.setScaledSize(decodeSize);
        }
    }

    QImage image = reader.read();
    if (image.isNull()) {
        if (errorText != nullptr) {
            const QString format = QString::fromLatin1(reader.format());
            const QString readerError = reader.errorString().trimmed();
            *errorText = readerError.isEmpty()
                ? QStringLiteral("QImageReader could not decode format '%1'.").arg(format)
                : QStringLiteral("%1 (format: %2)").arg(readerError, format);
        }
        return {};
    }

    // Some network image formats/plugins cannot expose dimensions before decoding.
    // Accept those formats only after validating the decoded image, then apply the
    // requested display bound without distorting the source aspect ratio.
    if (!acceptableImageDimensions(image.size())) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Decoded image dimensions exceed safety limits.");
        }
        return {};
    }
    if (!originalSize.isValid() || originalSize.isEmpty()) {
        originalSize = image.size();
        const QSize targetSize = boundedDecodeSize(originalSize, requestedSize);
        if (targetSize.isValid() && !targetSize.isEmpty() && targetSize != originalSize) {
            image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    return CachedImage{.image = std::move(image), .originalSize = originalSize};
}

// RemoteImageProvider is declared at global scope in remote_image_provider.hpp,
// so its nested Impl and the response/control types it references must be
// defined here at global scope rather than inside the anonymous namespace.
} // namespace

class RemoteImageResponse;

// Cancellation and a terminal remote/decode failure race on different threads.
// The first terminal actor wins so cancellation can never be reclassified as a
// URL failure merely because it arrived between a cancelled check and cache write.
enum class FailureCommitState : std::uint8_t { Active, Cancelled, Committed };

struct ResponseControl final {
    [[nodiscard]] bool commitFailure() noexcept
    {
        FailureCommitState expected = FailureCommitState::Active;
        return failureState.compare_exchange_strong(
            expected,
            FailureCommitState::Committed,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    void cancel() noexcept
    {
        FailureCommitState expected = FailureCommitState::Active;
        static_cast<void>(failureState.compare_exchange_strong(
            expected,
            FailureCommitState::Cancelled,
            std::memory_order_acq_rel,
            std::memory_order_acquire));
        cancelled.store(true, std::memory_order_release);
    }

    std::atomic_bool cancelled{false};
    std::atomic<FailureCommitState> failureState{FailureCommitState::Active};
    std::mutex mutex{};
    RemoteImageResponse* response{};
};

class RemoteImageProvider::Impl final : public std::enable_shared_from_this<RemoteImageProvider::Impl> {
public:
    Impl(const bool useDirectProxy, std::shared_ptr<storage_policy::RuntimeState> storageState)
        : m_storageState(std::move(storageState)),
          m_decodeExecutor(imageDecodeWorkerCount, 48U, 64U * 1024U * 1024U)
    {
        farcaster::transport::qtnetwork::async::NetworkLimits limits{};
        // The decode stage intentionally has only two workers. Letting dozens
        // of images complete HTTP concurrently creates a response/decode backlog
        // that competes with the render thread exactly when a rich timeline settles.
        // Keep network admission close enough to downstream capacity that scrolling
        // sees steady work instead of large completion bursts.
        limits.maximumActiveRequests = 8U;
        limits.maximumActiveRequestsPerOrigin = 4U;
        limits.maximumBackgroundActiveRequests = 4U;
        limits.maximumBackgroundActiveRequestsPerOrigin = 2U;
        limits.maximumAcceptedRequests = 32U;
        limits.maximumAcceptedRequestBodyBytes = 8U * 1024U * 1024U;
        limits.maximumBufferedResponseBytes = 64U * 1024U * 1024U;
        limits.replyReadBufferBytes = 128U * 1024U;

        auto transport = farcaster::transport::qtnetwork::async::makeAsyncTransport(
            useDirectProxy
                ? farcaster::transport::qtnetwork::ProxyPolicy::Direct
                : farcaster::transport::qtnetwork::ProxyPolicy::System,
            farcaster::transport::qtnetwork::ProtocolPolicy::Automatic,
            limits);
        if (transport) {
            m_transport = std::move(*transport);
        }

    }

    Impl(const Impl&) = delete;
    auto operator=(const Impl&) -> Impl& = delete;

    void start(
        std::shared_ptr<ResponseControl> control,
        RemoteRequestDescriptor descriptor,
        QSize requestedSize);

private:
    void startNetwork(
        std::shared_ptr<ResponseControl> control,
        QString urlText,
        storage_policy::MediaCategory category,
        bool manual,
        QSize decodeRequest,
        QString cacheKey,
        bool compatibilityAttempt = false,
        bool gatewayFallbackAttempt = false,
        unsigned int admissionRetryAttempt = 0U);

    void decodeNetworkResponse(
        std::shared_ptr<ResponseControl> control,
        QString urlText,
        storage_policy::MediaCategory category,
        bool manual,
        storage_policy::NetworkClass networkAtStart,
        QSize decodeRequest,
        QString cacheKey,
        farcaster::transport::http::HttpResponse response,
        bool compatibilityAttempt,
        bool gatewayFallbackAttempt,
        unsigned int queueRetryAttempt = 0U);

    std::shared_ptr<storage_policy::RuntimeState> m_storageState{};
    std::unique_ptr<farcaster::transport::async_http::IAsyncHttpTransport> m_transport{};
    farcaster::execution::weighted_thread_pool::WeightedThreadPoolExecutor m_decodeExecutor;
};

class RemoteImageResponse final : public QQuickImageResponse {
public:
    RemoteImageResponse(
        std::shared_ptr<RemoteImageProvider::Impl> impl,
        QString id,
        QSize requestedSize)
        : m_impl(std::move(impl)),
          m_control(std::make_shared<ResponseControl>())
    {
        {
            const std::scoped_lock lock{m_control->mutex};
            m_control->response = this;
        }
        m_impl->start(m_control, decodeRemoteRequest(id), requestedSize);
    }

    ~RemoteImageResponse() override
    {
        // Detach the QObject from provider-side completion before cancelling the
        // transport. A worker may finish concurrently with destruction; once the
        // response is detached, deliverResponse() becomes a no-op for this request.
        {
            const std::scoped_lock lock{m_control->mutex};
            if (m_control->response == this) {
                m_control->response = nullptr;
            }
        }
        cancel();
    }

    [[nodiscard]] QQuickTextureFactory* textureFactory() const override
    {
        const std::scoped_lock lock{m_resultMutex};
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    void cancel() override
    {
        // QQuickImageResponse cancellation is primarily a presentation-lifecycle
        // event: ListView recycling and scroll-time detachment cancel the QML
        // consumer even though the URL itself is still useful. Mark this response
        // cancelled so it can never expose a texture, but let already-admitted
        // bounded network/decode work finish and populate the provider cache.
        // A post-fling request then becomes a cache hit instead of restarting I/O.
        m_cancelled.store(true, std::memory_order_release);
    }

    void setOperation(farcaster::transport::async_http::Operation operation)
    {
        const std::scoped_lock lock{m_operationMutex};
        m_operation = std::move(operation);
    }

    void complete(QImage image)
    {
        if (m_completed.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (!m_cancelled.load(std::memory_order_acquire)) {
            const std::scoped_lock lock{m_resultMutex};
            m_image = std::move(image);
            if (m_image.isNull()) {
                m_image = transparentFailureImage(nullptr);
            }
        }
        // Qt requires a cancelled QQuickImageResponse to finish only after the
        // provider-side work is actually terminal so the engine can release it.
        Q_EMIT finished();
    }

private:
    std::shared_ptr<RemoteImageProvider::Impl> m_impl;
    std::shared_ptr<ResponseControl> m_control;
    mutable std::mutex m_resultMutex{};
    std::mutex m_operationMutex{};
    std::optional<farcaster::transport::async_http::Operation> m_operation{};
    std::atomic_bool m_cancelled{false};
    std::atomic_bool m_completed{false};
    QImage m_image{};
};

void deliverResponse(
    const std::shared_ptr<ResponseControl>& control,
    QImage image)
{
    if (!control) {
        return;
    }
    const std::scoped_lock lock{control->mutex};
    RemoteImageResponse* const response = control->response;
    if (response == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        response,
        [response, image = std::move(image)]() mutable {
            response->complete(std::move(image));
        },
        Qt::QueuedConnection);
}

void setResponseOperation(
    const std::shared_ptr<ResponseControl>& control,
    farcaster::transport::async_http::Operation operation)
{
    if (!control) {
        operation.cancel();
        return;
    }
    const std::scoped_lock lock{control->mutex};
    if (control->response == nullptr) {
        operation.cancel();
        return;
    }
    control->response->setOperation(std::move(operation));
}

void RemoteImageProvider::Impl::start(
    std::shared_ptr<ResponseControl> control,
    RemoteRequestDescriptor descriptor,
    const QSize requestedSize)
{
    if (control->cancelled.load(std::memory_order_acquire)) {
        deliverResponse(control, QImage{});
        return;
    }

    QString urlText = std::move(descriptor.urlText);
    const auto category = descriptor.category;
    const bool manual = descriptor.manual;
    const QUrl url{urlText};
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        deliverResponse(control, transparentFailureImage(nullptr));
        return;
    }

    const QSize decodeRequest = canonicalRequestedSize(requestedSize);
    const QString cacheKey = imageCacheKey(urlText, decodeRequest);
    CachedImage cached = cachedImage(cacheKey);
    if (!cached.image.isNull()) {
        deliverResponse(control, std::move(cached.image));
        return;
    }
    // A deliberate user tap is an explicit retry intent and must not be
    // suppressed by the short automatic-failure backoff window.
    // The 'manual' descriptor also covers UI-critical force-auto media.
    // Neither mode may bypass an origin-wide 401/403 cooldown: otherwise
    // recycled forced-cover/avatar items would continue hammering private R2.
    if (originTemporarilyForbidden(urlText)
        || (!manual && requestIsBackedOff(urlText))) {
        deliverResponse(control, transparentFailureImage(nullptr));
        return;
    }

    // Disk I/O and image decoding are CPU/I/O work, never provider-thread work.
    const auto self = shared_from_this();
    auto queued = m_decodeExecutor.post(
        0U,
        [self, control, urlText, category, manual, decodeRequest, cacheKey]() mutable {
            if (control->cancelled.load(std::memory_order_acquire)) {
                deliverResponse(control, QImage{});
                return;
            }
            const auto policy = self->m_storageState ? self->m_storageState->snapshot() : storage_policy::PolicySnapshot{};
            const QByteArray diskBytes = readDiskCachedBytes(urlText, category, policy);
            if (!diskBytes.isEmpty()) {
                QString decodeError{};
                CachedImage decoded = decodeImage(diskBytes, decodeRequest, &decodeError);
                if (!decoded.image.isNull()) {
                    clearFailure(urlText);
                    rememberImage(cacheKey, decoded.image, decoded.originalSize);
                    deliverResponse(control, std::move(decoded.image));
                    return;
                }
                QFile::remove(diskCachePathForUrl(urlText, category));
            }
            self->startNetwork(
                std::move(control), std::move(urlText), category, manual, decodeRequest, cacheKey);
        });
    if (!queued) {
        // Disk-cache pressure must not turn a valid remote image into a permanent
        // one-pixel failure. Skip the optional disk read and continue with the
        // asynchronous network path; decoding still remains off the UI thread.
        self->startNetwork(control, std::move(urlText), category, manual, decodeRequest, std::move(cacheKey));
    }
}

void RemoteImageProvider::Impl::startNetwork(
    std::shared_ptr<ResponseControl> control,
    QString urlText,
    const storage_policy::MediaCategory category,
    const bool manual,
    const QSize decodeRequest,
    QString cacheKey,
    const bool compatibilityAttempt,
    const bool gatewayFallbackAttempt,
    const unsigned int admissionRetryAttempt)
{
    if (control->cancelled.load(std::memory_order_acquire)) {
        deliverResponse(control, QImage{});
        return;
    }
    const auto policy = m_storageState ? m_storageState->snapshot() : storage_policy::PolicySnapshot{};
    const auto networkAtStart = policy.networkClass;
    const auto rule = storage_policy::downloadRule(policy, category, networkAtStart);
    if (!manual && !rule.enabled) {
        deliverResponse(control, transparentFailureImage(nullptr));
        return;
    }
    // Do not keep retrying other automatic asset URLs on a private R2/CDN origin.
    if (originTemporarilyForbidden(urlText)) {
        deliverResponse(control, transparentFailureImage(nullptr));
        return;
    }
    if (!m_transport) {
        if (!control->commitFailure()) {
            deliverResponse(control, QImage{});
            return;
        }
        rememberFailure(urlText);
        deliverResponse(control, transparentFailureImage(nullptr));
        return;
    }

    farcaster::transport::async_http::Request request{};
    request.method = farcaster::transport::async_http::Method::Get;
// Native profile covers from a few CDNs cannot be fetched/decoded
    // directly. After a terminal direct failure, retry Photos only through
    // the existing public-raster-only Genycaster gateway used by WASM.
    // The same download policy, byte budget, timeout and cache key apply.
#if defined(Q_OS_WASM)
    const bool routeThroughGateway = true;
#else
    const bool routeThroughGateway = gatewayFallbackAttempt;
#endif
    if (routeThroughGateway) {
        QUrl imageGateway{QStringLiteral("https://storage-farcaster.genyleap.com/v1/browser/images")};
        QUrlQuery imageQuery{};
        imageQuery.addQueryItem(QStringLiteral("url"), urlText);
        imageGateway.setQuery(imageQuery);
        request.url = imageGateway.toString(QUrl::FullyEncoded).toStdString();
        request.headers = {
            compatibilityAttempt ? mediaCompatibilityAcceptHeader() : mediaAcceptHeader(),
        };
    } else {
        request.url = urlText.toStdString();
        request.headers = {
            compatibilityAttempt ? mediaCompatibilityAcceptHeader() : mediaAcceptHeader(),
            mediaUserAgentHeader(),
        };
    }
    request.timeout = std::chrono::milliseconds{10'000};
    request.maxBodyBytes = static_cast<std::size_t>(std::min<std::uint64_t>(
        manual ? maximumNetworkImageBytes : rule.maximumBytes, maximumNetworkImageBytes));
    request.priority = farcaster::transport::async_http::Priority::Visible;
    request.retryPolicy = farcaster::transport::async_http::RetryPolicy::TransientRead;
    request.maximumAttempts = 2U;

    const auto self = shared_from_this();
    auto operation = m_transport->start(
        std::move(request),
        [self, control, urlText, category, manual, networkAtStart, decodeRequest, cacheKey,
         compatibilityAttempt, gatewayFallbackAttempt](
            farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse> result) mutable {
            // QQuick delegate recycling routinely cancels image requests while
            // scrolling. The transport completes cancellation as an error, but
            // that is a caller lifecycle event rather than evidence that the
            // remote URL is unhealthy. Never poison the URL negative cache for
            // a cancelled response.
            if (control->cancelled.load(std::memory_order_acquire)) {
                deliverResponse(control, QImage{});
                return;
            }
            if (!result || result->statusCode < 200U || result->statusCode >= 300U) {
                const bool deniedByOrigin = result
                    && (result->statusCode == 401U || result->statusCode == 403U);
                if (deniedByOrigin) rememberForbiddenOrigin(urlText);
#if defined(Q_OS_WASM)
                // Do not log full URLs: CDNs may put identifying tokens in
                // paths or queries. Record only host and bounded status.
                qWarning() << "Browser image request rejected for host"
                           << QUrl{urlText}.host()
                           << (result ? static_cast<int>(result->statusCode) : 0);
#endif

#if !defined(Q_OS_WASM)
                if (!deniedByOrigin && !gatewayFallbackAttempt
                    && category == storage_policy::MediaCategory::Photos) {
                    self->startNetwork(std::move(control), std::move(urlText), category, manual,
                                       decodeRequest, std::move(cacheKey), false, true);
                    return;
                }
#endif
                if (!control->commitFailure()) {
                    deliverResponse(control, QImage{});
                    return;
                }
                rememberFailure(urlText);
                deliverResponse(control, transparentFailureImage(nullptr));
                return;
            }
            clearForbiddenOrigin(urlText);
            if (self->m_storageState) {
                self->m_storageState->recordDownloadedBytes(
                    category, networkAtStart, static_cast<std::uint64_t>(result->body.size()));
            }
            if (!responseMayContainImage(*result)) {
#if !defined(Q_OS_WASM)
                if (!gatewayFallbackAttempt
                    && category == storage_policy::MediaCategory::Photos) {
                    self->startNetwork(std::move(control), std::move(urlText), category, manual,
                                       decodeRequest, std::move(cacheKey), false, true);
                    return;
                }
#endif
                if (!control->commitFailure()) {
                    deliverResponse(control, QImage{});
                    return;
                }
                rememberFailure(urlText);
                deliverResponse(control, transparentFailureImage(nullptr));
                return;
            }
            self->decodeNetworkResponse(
                std::move(control), std::move(urlText), category, manual, networkAtStart, decodeRequest,
                std::move(cacheKey), std::move(*result), compatibilityAttempt,
                gatewayFallbackAttempt, 0U);
        });
    if (!operation) {
        // Synchronous start() failure is transport admission/backpressure, not
        // evidence that this URL is bad. Retry the admission briefly without
        // polluting the URL failure cache used for real HTTP/decode failures.
        constexpr unsigned int maximumAdmissionRetries = 6U;
        if (admissionRetryAttempt < maximumAdmissionRetries && QCoreApplication::instance() != nullptr) {
            const int delayMs = 16 + static_cast<int>(admissionRetryAttempt) * 16;
            QTimer::singleShot(
                delayMs,
                QCoreApplication::instance(),
                [self, control, urlText = std::move(urlText), category, manual, decodeRequest,
                 cacheKey = std::move(cacheKey), compatibilityAttempt, gatewayFallbackAttempt,
                 admissionRetryAttempt]() mutable {
                    if (control->cancelled.load(std::memory_order_acquire)) {
                        deliverResponse(control, QImage{});
                        return;
                    }
                    self->startNetwork(
                        std::move(control), std::move(urlText), category, manual, decodeRequest,
                        std::move(cacheKey), compatibilityAttempt, gatewayFallbackAttempt,
                        admissionRetryAttempt + 1U);
                });
            return;
        }
        deliverResponse(control, transparentFailureImage(nullptr));
        return;
    }
    setResponseOperation(control, std::move(*operation));
}

void RemoteImageProvider::Impl::decodeNetworkResponse(
    std::shared_ptr<ResponseControl> control,
    QString urlText,
    const storage_policy::MediaCategory category,
    const bool manual,
    const storage_policy::NetworkClass networkAtStart,
    const QSize decodeRequest,
    QString cacheKey,
    farcaster::transport::http::HttpResponse response,
    const bool compatibilityAttempt,
    const bool gatewayFallbackAttempt,
    const unsigned int queueRetryAttempt)
{
    if (control->cancelled.load(std::memory_order_acquire)) {
        deliverResponse(control, QImage{});
        return;
    }
    const auto responseHolder = std::make_shared<farcaster::transport::http::HttpResponse>(
        std::move(response));
    const std::size_t retainedBytes = responseHolder->body.size();
    const auto self = shared_from_this();
    auto queued = m_decodeExecutor.post(
        retainedBytes,
        [self, control, urlText, category, manual, decodeRequest, cacheKey, responseHolder,
         compatibilityAttempt, gatewayFallbackAttempt]() mutable {
            if (control->cancelled.load(std::memory_order_acquire)) {
                deliverResponse(control, QImage{});
                return;
            }
            const QByteArray bytes = QByteArray::fromRawData(
                responseHolder->body.data(), static_cast<qsizetype>(responseHolder->body.size()));
            QString decodeError{};
            CachedImage decoded = decodeImage(bytes, decodeRequest, &decodeError);
            if (decoded.image.isNull()) {
                if (control->cancelled.load(std::memory_order_acquire)) {
                    deliverResponse(control, QImage{});
                    return;
                }
                if (!compatibilityAttempt) {
                    self->startNetwork(
                        std::move(control), std::move(urlText), category, manual, decodeRequest,
                        std::move(cacheKey), true, gatewayFallbackAttempt);
                    return;
                }
#if !defined(Q_OS_WASM)
                if (!gatewayFallbackAttempt
                    && category == storage_policy::MediaCategory::Photos) {
                    self->startNetwork(std::move(control), std::move(urlText), category, manual,
                                       decodeRequest, std::move(cacheKey), false, true);
                    return;
                }
#endif
                if (!control->commitFailure()) {
                    deliverResponse(control, QImage{});
                    return;
                }
                rememberFailure(urlText);
                qWarning().noquote()
                    << "Remote image decode failed after compatibility retry:"
                    << QUrl{urlText}.host() << decodeError;
                deliverResponse(control, transparentFailureImage(nullptr));
                return;
            }

            clearFailure(urlText);
            const auto latestPolicy = self->m_storageState ? self->m_storageState->snapshot() : storage_policy::PolicySnapshot{};
            writeDiskCachedBytes(urlText, category, bytes, latestPolicy);
            rememberImage(cacheKey, decoded.image, decoded.originalSize);
            deliverResponse(control, std::move(decoded.image));
        });
    if (!queued) {
        // The HTTP response is already valid. Queue saturation is local
        // backpressure, not an image failure, so retain the response and retry
        // the CPU handoff a few times instead of completing QML with a sentinel.
        constexpr unsigned int maximumQueueRetries = 6U;
        if (queueRetryAttempt < maximumQueueRetries && QCoreApplication::instance() != nullptr) {
            const int delayMs = 18 + static_cast<int>(queueRetryAttempt) * 14;
            QTimer::singleShot(
                delayMs,
                QCoreApplication::instance(),
                [self, control, urlText = std::move(urlText), category, manual, networkAtStart, decodeRequest,
                 cacheKey = std::move(cacheKey), responseHolder, compatibilityAttempt,
                 gatewayFallbackAttempt, queueRetryAttempt]() mutable {
                    if (control->cancelled.load(std::memory_order_acquire)) {
                        deliverResponse(control, QImage{});
                        return;
                    }
                    self->decodeNetworkResponse(
                        std::move(control), std::move(urlText), category, manual, networkAtStart, decodeRequest,
                        std::move(cacheKey), std::move(*responseHolder),
                        compatibilityAttempt, gatewayFallbackAttempt, queueRetryAttempt + 1U);
                });
            return;
        }
        qWarning() << "Remote image decode queue remained saturated after bounded retries.";
        deliverResponse(control, transparentFailureImage(nullptr));
    }
}

RemoteImageProvider::RemoteImageProvider(
    const bool useDirectProxy,
    std::shared_ptr<void> storageStateHandle)
    : m_impl(std::make_shared<Impl>(
          useDirectProxy,
          std::static_pointer_cast<storage_policy::RuntimeState>(std::move(storageStateHandle))))
{
    QImageReader::setAllocationLimit(96);
}

RemoteImageProvider::~RemoteImageProvider() = default;

QQuickImageResponse* RemoteImageProvider::requestImageResponse(
    const QString& id,
    const QSize& requestedSize)
{
    return new RemoteImageResponse{m_impl, id, requestedSize};
}
