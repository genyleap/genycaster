#include "media_upload_controller.hpp"
#include "stats_share_card_renderer.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <QFileInfo>
#include <QFile>
#include <QBuffer>
#include <QImageReader>
#include <QQuickImageProvider>
#include <QQuickTextureFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QUrlQuery>
#include <QStringView>
#include <utility>

namespace {

struct SupportedMedia final {
    QStringView extension;
    QStringView contentType;
    QStringView previewKind;
};

constexpr std::array SupportedMediaTypes{
    SupportedMedia{u"jpg", u"image/jpeg", u"image"},
    SupportedMedia{u"jpeg", u"image/jpeg", u"image"},
    SupportedMedia{u"png", u"image/png", u"image"},
    SupportedMedia{u"webp", u"image/webp", u"image"},
    SupportedMedia{u"gif", u"image/gif", u"image"},
    SupportedMedia{u"avif", u"image/avif", u"image"},
    SupportedMedia{u"mp4", u"video/mp4", u"video"},
    SupportedMedia{u"webm", u"video/webm", u"video"},
    SupportedMedia{u"mov", u"video/quicktime", u"video"},
};

[[nodiscard]] const SupportedMedia* supportedMediaFor(const QFileInfo& fileInfo) noexcept
{
    const QString suffix = fileInfo.suffix().toLower();
    const auto iterator = std::ranges::find_if(
        SupportedMediaTypes,
        [&suffix](const SupportedMedia& candidate) { return candidate.extension == suffix; });
    return iterator == SupportedMediaTypes.end() ? nullptr : std::addressof(*iterator);
}

[[nodiscard]] QString unsupportedMediaMessage()
{
    return qtTrId("genycaster.media.unsupported_file_type");
}

} // namespace

MediaUploadController::MediaUploadController(QObject* parent)
    : QObject{parent}
{
}

MediaUploadController::~MediaUploadController()
{
    for (const auto& path : std::as_const(m_generatedStatsImages))
        QFile::remove(path);
}

QVariantMap MediaUploadController::renderStatsCard(
    const QVariantMap& stats, const QString& displayName,
    const QString& username, const QString& avatarUrl, const int periodDays,
    const QString& style, const double capturedAtMs)
{
    // Never reuse the previous identity's portrait during account switching.
    const QImage currentAvatar = avatarUrl == m_statsAvatarUrl && m_statsAvatar
        ? *m_statsAvatar : QImage{};
    const auto result = genycaster::presentation::renderStatsShareCard(
        stats, displayName, username, currentAvatar, periodDays, style, capturedAtMs);
    if (!result.error.isEmpty() || !result.fileUrl.isLocalFile()) {
        return {{QStringLiteral("error"),
                 result.error.isEmpty() ? QStringLiteral("share_card_png_failed") : result.error}};
    }
    m_generatedStatsImages.insert(result.fileUrl.toLocalFile());
    return {{QStringLiteral("imageFileUrl"), result.fileUrl.toString(QUrl::FullyEncoded)}};
}

void MediaUploadController::prepareStatsAvatar(const QString& avatarUrl)
{
    if (m_statsAvatarUrl == avatarUrl
        && ((m_statsAvatar && !m_statsAvatar->isNull())
            || m_statsAvatarReply || avatarUrl.isEmpty()))
        return;

    ++m_statsAvatarGeneration;
    m_statsAvatarUrl = avatarUrl;
    m_statsAvatar.reset();
    if (m_statsAvatarImageResponse) {
        auto* const previous = static_cast<QQuickImageResponse*>(
            m_statsAvatarImageResponse.data());
        QObject::disconnect(previous, nullptr, this, nullptr);
        previous->cancel();
        previous->deleteLater();
        m_statsAvatarImageResponse = nullptr;
    }
    if (m_statsAvatarReply) {
        m_statsAvatarReply->abort();
        m_statsAvatarReply->deleteLater();
        m_statsAvatarReply = nullptr;
    }
    // The proxy enforces an explicit CDN allowlist, pinned public DNS, raster
    // MIME and a small body cap. Never fetch an arbitrary URL from Qt directly.
    const QUrl source{avatarUrl};
    if (avatarUrl.size() > 2048 || source.scheme() != QStringLiteral("https")
        || source.host().isEmpty() || !source.userInfo().isEmpty()
        || source.hasFragment() || source.port(-1) != -1) {
        emit statsAvatarChanged();
        return;
    }
    // Reuse the very same asynchronous image provider as Avatar.qml. It
    // shares its bounded CDN routing, decode, disk/memory cache and retry
    // policy. A separate fixed-host filter wrongly discarded valid account
    // photos that the profile itself could already display.
    if (m_statsAvatarImageProvider) {
        const QByteArray encoded = source.toString(QUrl::FullyEncoded).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        QQuickImageResponse* const response =
            m_statsAvatarImageProvider->requestImageResponse(
                QStringLiteral("v22/avatars/manual/") + QString::fromLatin1(encoded),
                QSize{256, 256});
        if (response) {
            const quint64 generation = m_statsAvatarGeneration;
            m_statsAvatarImageResponse = response;
            const auto completed = [this, response, generation] {
                if (generation != m_statsAvatarGeneration
                    || m_statsAvatarImageResponse != response) return;
                m_statsAvatarImageResponse = nullptr;
                std::unique_ptr<QQuickTextureFactory> factory{response->textureFactory()};
                if (factory) {
                    QImage decoded = factory->image();
                    if (!decoded.isNull() && decoded.width() > 1 && decoded.height() > 1
                        && static_cast<qint64>(decoded.width()) * decoded.height()
                            <= 8'000'000)
                        m_statsAvatar = std::make_unique<QImage>(std::move(decoded));
                }
                response->deleteLater();
                if (!statsAvatarReady()) {
                    // The provider's 1x1 failure sentinel (or an undecodable
                    // source) must not become a permanent Stats avatar failure.
                    // Re-fetch JPEG directly, with a pinned public proxy as
                    // a final fallback for CORS/CDN restrictions.
                    startStatsAvatarFallback(generation, false);
                    return;
                }
                emit statsAvatarChanged();
            };
            connect(response, &QQuickImageResponse::finished,
                    this, completed, Qt::QueuedConnection);
            emit statsAvatarChanged();
            // A warm image or a cached failure may complete synchronously
            // before the signal connection above has been installed.
            std::unique_ptr<QQuickTextureFactory> cached{response->textureFactory()};
            if (cached && !cached->image().isNull())
                QMetaObject::invokeMethod(this, completed, Qt::QueuedConnection);
            return;
        }
    }

    // Tests without a composed provider, or a provider that refuses the
    // request, still fetch the public portrait via the bounded JPEG path.
    startStatsAvatarFallback(m_statsAvatarGeneration, false);
}

void MediaUploadController::startStatsAvatarFallback(
    const quint64 generation, const bool viaGateway)
{
    if (generation != m_statsAvatarGeneration) return;
    const QUrl source{m_statsAvatarUrl};
    // These are public raster CDNs, not an arbitrary fetch/SSRF primitive.
    // The gateway additionally enforces public DNS and decoded raster limits.
    static const QStringList allowedHosts{
        QStringLiteral("imagedelivery.net"), QStringLiteral("cdn.warpcast.com"),
        QStringLiteral("wrpcd.net"), QStringLiteral("cdn.farcaster.xyz"),
        QStringLiteral("media-farcaster.genyleap.com"),
        QStringLiteral("i.imgur.com"), QStringLiteral("pbs.twimg.com"),
        QStringLiteral("res.cloudinary.com"), QStringLiteral("ipfs.io"),
        QStringLiteral("gateway.pinata.cloud"), QStringLiteral("imagekit.io"),
        QStringLiteral("lh3.googleusercontent.com"),
        QStringLiteral("arweave.net"), QStringLiteral("media.discordapp.net"),
        QStringLiteral("cdn.discordapp.com")
    };
    const QString host = source.host().toLower();
    bool allowed = false;
    for (const QString& domain : allowedHosts) {
        if (host == domain
            || (domain != QStringLiteral("media-farcaster.genyleap.com")
                && host.endsWith(QLatin1Char('.') + domain))) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        qWarning() << "Stats avatar unsupported source host:" << host;
        emit statsAvatarChanged();
        return;
    }
    QUrl fetchUrl = source;
    if (viaGateway) {
        fetchUrl = QUrl{QStringLiteral(
            "https://storage-farcaster.genyleap.com/v1/browser/images")};
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("url"), m_statsAvatarUrl);
        fetchUrl.setQuery(query);
    }
    QNetworkRequest request{fetchUrl};
    // Many Cloudflare Images /original URLs negotiate WebP by default. A
    // JPEG-only request is portable across native and Qt/WASM image readers.
    request.setRawHeader("Accept", "image/jpeg");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(10000);
    QNetworkReply* const reply = m_statsAvatarNetwork.get(request);
    m_statsAvatarReply = reply;
    emit statsAvatarChanged();

    constexpr qsizetype MaxAvatarBytes = 1024 * 1024;
    auto payload = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, payload] {
        if (reply->bytesAvailable() + payload->size() > MaxAvatarBytes) {
            reply->abort();
            return;
        }
        payload->append(reply->readAll());
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, payload, generation, viaGateway] {
        if (generation != m_statsAvatarGeneration) {
            reply->deleteLater();
            return;
        }
        m_statsAvatarReply = nullptr;
        // Collect any tail bytes delivered together with finished().
        if (reply->bytesAvailable() + payload->size() <= MaxAvatarBytes)
            payload->append(reply->readAll());
        if (reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && payload->size() > 16 && payload->size() <= MaxAvatarBytes) {
            QBuffer buffer{payload.get()};
            if (buffer.open(QIODevice::ReadOnly)) {
                QImageReader reader{&buffer};
                reader.setAutoTransform(true);
                const QSize original = reader.size();
                if (original.isValid() && original.width() <= 4096
                    && original.height() <= 4096
                    && static_cast<qint64>(original.width()) * original.height()
                         <= 8'000'000) {
                    reader.setScaledSize(original.scaled(
                        QSize{256, 256}, Qt::KeepAspectRatio));
                    QImage decoded = reader.read();
                    if (!decoded.isNull())
                        m_statsAvatar = std::make_unique<QImage>(std::move(decoded));
                }
            }
        }
        if (!statsAvatarReady() && viaGateway) {
            qWarning() << "Stats avatar JPEG/proxy failed for public host:"
                       << QUrl{m_statsAvatarUrl}.host()
                       << "HTTP" << reply->attribute(
                              QNetworkRequest::HttpStatusCodeAttribute).toInt();
        }
        reply->deleteLater();
        if (!statsAvatarReady() && !viaGateway) {
            startStatsAvatarFallback(generation, true);
            return;
        }
        emit statsAvatarChanged();
    });
}

bool MediaUploadController::statsAvatarLoading() const noexcept
{
    return !m_statsAvatarReply.isNull() || !m_statsAvatarImageResponse.isNull();
}

void MediaUploadController::setStatsAvatarImageProvider(
    QQuickAsyncImageProvider* const provider) noexcept
{
    m_statsAvatarImageProvider = provider;
}

bool MediaUploadController::statsAvatarReady() const noexcept
{
    return m_statsAvatar && !m_statsAvatar->isNull();
}

bool MediaUploadController::shareStatsImage(const QUrl& localFile,
                                            const QString& caption)
{
    if (!localFile.isLocalFile() || !m_generatedStatsImages.contains(localFile.toLocalFile()))
        return false;
#if defined(Q_OS_WASM)
    emit statsImageShareRequested(localFile.toLocalFile(), caption.left(1800));
    return true;
#else
    Q_UNUSED(caption);
    // Desktop OS share sheets vary; export to Pictures then open the exact
    // raster so it can be shared to X, Telegram or Instagram by the user.
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (folder.isEmpty() || !QDir{}.mkpath(folder)) return false;
    const QString name = QStringLiteral("Genycaster-Stats-")
        + QString::number(QDateTime::currentMSecsSinceEpoch()) + QStringLiteral(".png");
    const QString dest = QDir{folder}.filePath(name);
    if (!QFile::copy(localFile.toLocalFile(), dest)) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dest));
#endif
}

bool MediaUploadController::discardStatsCardImage(const QUrl& localFile)
{
    if (!localFile.isLocalFile()) return false;
    const QString path = localFile.toLocalFile();
    if (m_busy && m_localPath == path) return false;
    if (!m_generatedStatsImages.remove(path)) return false;
    return QFile::remove(path);
}

bool MediaUploadController::available() const noexcept
{
    return m_available;
}

bool MediaUploadController::busy() const noexcept
{
    return m_busy;
}

bool MediaUploadController::canRetry() const noexcept
{
    return m_canRetry;
}

double MediaUploadController::progress() const noexcept
{
    return m_progress;
}

QString MediaUploadController::phase() const
{
    return m_phase;
}

QString MediaUploadController::errorText() const
{
    return m_errorText;
}

QString MediaUploadController::fileName() const
{
    return m_fileName;
}

QUrl MediaUploadController::localPreviewUrl() const
{
    return m_localPreviewUrl;
}

QString MediaUploadController::previewKind() const
{
    return m_previewKind;
}

QString MediaUploadController::selectedContentType() const
{
    return m_selectedContentType;
}

qulonglong MediaUploadController::transferredBytes() const noexcept
{
    return m_transferredBytes;
}

qulonglong MediaUploadController::totalBytes() const noexcept
{
    return m_totalBytes;
}

qulonglong MediaUploadController::sourceBytes() const noexcept
{
    return m_sourceBytes;
}

bool MediaUploadController::optimizationSeen() const noexcept
{
    return m_optimizationSeen;
}

qulonglong MediaUploadController::maxImageBytes() const noexcept
{
    return m_maxImageBytes;
}

void MediaUploadController::setMaxImageBytes(const qulonglong limit)
{
    // A UI property cannot grant an arbitrary unbounded media upload.
    const qulonglong bounded = limit > 2U * 1024U * 1024U
        ? 5U * 1024U * 1024U : 2U * 1024U * 1024U;
    if (m_maxImageBytes == bounded) return;
    m_maxImageBytes = bounded;
    emit stateChanged();
}

quint64 MediaUploadController::uploadGeneration() const noexcept
{
    return m_uploadGeneration;
}

bool MediaUploadController::beginUpload(const QUrl& localFile, const int afterIndex)
{
    if (!m_available || m_busy || !localFile.isValid() || !localFile.isLocalFile()) {
        return false;
    }

    const QString localPath = localFile.toLocalFile();
    const QFileInfo fileInfo{localPath};
    if (localPath.trimmed().isEmpty() || !fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        m_busy = false;
        m_canRetry = false;
        m_phase = QStringLiteral("failed");
        m_errorText = qtTrId("genycaster.media.file_unreadable");
        m_fileName = localFile.fileName();
        m_localPreviewUrl.clear();
        m_previewKind.clear();
        m_selectedContentType.clear();
        m_localPath.clear();
        resetProgress();
        emit stateChanged();
        return false;
    }

    const SupportedMedia* media = supportedMediaFor(fileInfo);
    if (media == nullptr) {
        m_busy = false;
        m_canRetry = false;
        m_phase = QStringLiteral("failed");
        m_errorText = unsupportedMediaMessage();
        m_fileName = fileInfo.fileName();
        m_localPreviewUrl.clear();
        m_previewKind.clear();
        m_selectedContentType.clear();
        m_localPath.clear();
        m_afterIndex = afterIndex;
        resetProgress();
        emit stateChanged();
        return false;
    }

    const qint64 size = fileInfo.size();
    // Source bytes may exceed the upload tier: transcode on-device first.
    // A bounded 25 MiB ingress cap protects RAM; 2/5 MiB applies to the
    // optimized file before a signed upload ticket is issued.
    if (size <= 0 || (media->previewKind == QStringView{u"image"}
            && static_cast<qulonglong>(size) > 25U * 1024U * 1024U)) {
        m_busy = false;
        m_canRetry = false;
        m_phase = QStringLiteral("failed");
        m_errorText = qtTrId("genycaster.media.image_size_limit")
                          .arg(m_maxImageBytes / (1024U * 1024U));
        m_fileName = fileInfo.fileName();
        m_localPreviewUrl.clear();
        m_localPath.clear();
        m_previewKind.clear();
        m_selectedContentType.clear();
        resetProgress();
        emit stateChanged();
        return false;
    }
    ++m_uploadGeneration;
    m_busy = true;
    m_canRetry = false;
    m_errorText.clear();
    m_phase = QStringLiteral("queued");
    m_localPreviewUrl = localFile;
    m_previewKind = media->previewKind.toString();
    m_selectedContentType = media->contentType.toString();
    m_localPath = localPath;
    m_fileName = fileInfo.fileName();
    m_afterIndex = afterIndex;
    resetProgress();
    const qint64 localSize = fileInfo.size();
    m_totalBytes = localSize > 0 ? static_cast<qulonglong>(localSize) : 0U;
    m_sourceBytes = m_totalBytes;
    emit stateChanged();
    emit uploadRequested(m_localPath, m_afterIndex);
    return true;
}

bool MediaUploadController::browseBrowserImage(const int afterIndex)
{
    if (!m_available || m_busy) return false;
    emit browserImagePickRequested(afterIndex);
    return true;
}

void MediaUploadController::applyBrowserSelection(
    QString fileName, QString contentType, const qulonglong sizeBytes, const int afterIndex)
{
    if (!m_available || m_busy) return;
    if (sizeBytes == 0 || sizeBytes > 25U * 1024U * 1024U) {
        m_busy = false;
        m_canRetry = false;
        m_phase = QStringLiteral("failed");
        m_errorText = qtTrId("genycaster.media.image_size_limit")
                          .arg(m_maxImageBytes / (1024U * 1024U));
        m_fileName = std::move(fileName);
        emit stateChanged();
        return;
    }
    ++m_uploadGeneration;
    m_busy = true;
    m_canRetry = false;
    m_errorText.clear();
    m_phase = QStringLiteral("preparing");
    m_fileName = std::move(fileName);
    m_selectedContentType = std::move(contentType);
    m_previewKind = QStringLiteral("image");
    m_localPreviewUrl.clear(); // Never pass a browser File or blob: URL into Qt.
    m_localPath.clear();
    m_afterIndex = afterIndex;
    resetProgress();
    m_totalBytes = sizeBytes;
    m_sourceBytes = sizeBytes;
    emit stateChanged();
}

void MediaUploadController::applyBrowserPreview(const QString& dataUrl)
{
    // A small JPEG data URL (never a File or blob: URL) is safe for Qt's
    // image renderer and must not replace the persisted public asset URL.
    if (!m_busy || m_previewKind != QStringLiteral("image")
        || !dataUrl.startsWith(QStringLiteral("data:image/jpeg;base64,"))
        || dataUrl.size() > 6200) {
        return;
    }
    m_localPreviewUrl = QUrl{dataUrl};
    emit stateChanged();
}

void MediaUploadController::cancel()
{
    if (!m_busy) {
        return;
    }
    // User cancellation is immediate even if a Qt network reply takes a
    // while to acknowledge the abort or the provider cleanup times out.
    ++m_uploadGeneration;
    emit cancelRequested();
    applyCancelled();
}

bool MediaUploadController::retry()
{
    if (!m_available || m_busy || !m_canRetry || m_localPath.isEmpty()) {
        return false;
    }
    return beginUpload(QUrl::fromLocalFile(m_localPath), m_afterIndex);
}

void MediaUploadController::clear()
{
    if (m_busy) {
        return;
    }
    m_canRetry = false;
    m_errorText.clear();
    m_phase.clear();
    if (m_generatedStatsImages.remove(m_localPath))
        QFile::remove(m_localPath);
    clearSelection();
    resetProgress();
    emit stateChanged();
}

void MediaUploadController::setAvailable(const bool available, QString unavailableReason)
{
    if (m_available == available && m_unavailableReason == unavailableReason) {
        return;
    }
    const QString previousReason = m_unavailableReason;
    m_available = available;
    m_unavailableReason = std::move(unavailableReason);
    if (!m_available && !m_busy && !m_unavailableReason.isEmpty()) {
        m_errorText = m_unavailableReason;
    } else if (m_available && !m_busy && m_errorText == previousReason) {
        m_errorText.clear();
    }
    emit stateChanged();
}

void MediaUploadController::applyProgress(QString phase, const qulonglong transferred, const qulonglong total)
{
    if (!m_busy) {
        return;
    }
    if (phase == QStringLiteral("optimizing"))
        m_optimizationSeen = true;
    m_phase = std::move(phase);
    m_transferredBytes = transferred;
    m_totalBytes = total > 0U ? total : m_totalBytes;
    m_progress = m_totalBytes == 0U
        ? 0.0
        : std::clamp(static_cast<double>(transferred) / static_cast<double>(m_totalBytes), 0.0, 1.0);
    emit stateChanged();
}

void MediaUploadController::applyCompleted(
    QString assetUrl,
    QString contentType,
    QString fileName,
    QString managementToken,
    const qulonglong sizeBytes)
{
    if (!m_busy) {
        return;
    }
    const int afterIndex = m_afterIndex;
    m_busy = false;
    m_canRetry = false;
    m_errorText.clear();
    m_phase = QStringLiteral("completed");
    m_transferredBytes = sizeBytes;
    m_totalBytes = sizeBytes;
    m_progress = 1.0;
    if (m_generatedStatsImages.remove(m_localPath))
        QFile::remove(m_localPath);
    if (!fileName.isEmpty()) {
        m_fileName = std::move(fileName);
    }
    if (!contentType.isEmpty()) {
        m_selectedContentType = contentType;
    }
    emit stateChanged();
    emit assetReady(std::move(assetUrl), std::move(contentType), std::move(managementToken), afterIndex);
}

void MediaUploadController::applyFailed(QString message)
{
    if (!m_busy) return;
    m_busy = false;
    m_canRetry = !m_localPath.isEmpty();
    m_phase = QStringLiteral("failed");
    m_errorText = std::move(message);
    emit stateChanged();
}

void MediaUploadController::applyCancelled()
{
    if (!m_busy) return;
    m_busy = false;
    m_canRetry = !m_localPath.isEmpty();
    m_phase = QStringLiteral("cancelled");
    m_errorText.clear();
    emit stateChanged();
}

void MediaUploadController::resetProgress()
{
    m_progress = 0.0;
    m_transferredBytes = 0U;
    m_totalBytes = 0U;
    m_sourceBytes = 0U;
    m_optimizationSeen = false;
}

void MediaUploadController::clearSelection()
{
    m_fileName.clear();
    m_localPreviewUrl.clear();
    m_previewKind.clear();
    m_selectedContentType.clear();
    m_localPath.clear();
    m_sourceBytes = 0U;
    m_optimizationSeen = false;
    m_afterIndex = -1;
}
