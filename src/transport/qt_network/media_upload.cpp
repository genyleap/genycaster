module;

#include <QByteArray>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMimeDatabase>
#include <QString>
#include <QUuid>

#include "media_json_numbers.hpp"
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.transport.qtnetwork.media;

import farcaster.capability.media;
import farcaster.capability.write_authority;
import farcaster.domain.identity;
import farcaster.domain.media;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;

namespace farcaster::transport::qtnetwork::media {
namespace {

using Header = std::pair<QByteArray, QByteArray>;

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] QString toQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] std::string toString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string{utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

[[nodiscard]] QString toNativeQString(const std::filesystem::path& path)
{
#if defined(Q_OS_WIN)
    return QString::fromStdWString(path.native());
#else
    const auto& native = path.native();
    return QFile::decodeName(QByteArray{native.data(), static_cast<qsizetype>(native.size())});
#endif
}

[[nodiscard]] std::string hex(const QByteArray& bytes)
{
    return toString(QString::fromLatin1(bytes.toHex()));
}

[[nodiscard]] std::string signatureHex(const domain::signer::Signature& signature)
{
    const QByteArray bytes{signature.bytes.data(), static_cast<qsizetype>(signature.bytes.size())};
    return hex(bytes);
}

[[nodiscard]] foundation::result::Result<QUrl> normalizedBaseUrl(const std::string& raw)
{
    QUrl url{toQString(raw)};
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http")) ||
        !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Media gateway base URL must be a credential-free HTTP(S) origin."));
    }
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    url.setPath(path);
    return url;
}

[[nodiscard]] QUrl endpointUrl(const QUrl& base, const QString& path)
{
    QUrl url = base;
    QString joined = base.path();
    if (!joined.endsWith(QLatin1Char('/'))) {
        joined += QLatin1Char('/');
    }
    QString suffix = path;
    while (suffix.startsWith(QLatin1Char('/'))) {
        suffix.remove(0, 1);
    }
    joined += suffix;
    url.setPath(joined);
    return url;
}

[[nodiscard]] QByteArray jsonBody(const QJsonObject& object)
{
    return QJsonDocument{object}.toJson(QJsonDocument::Compact);
}

[[nodiscard]] foundation::result::Result<QJsonObject> parseObject(const QByteArray& body)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Media gateway returned invalid JSON."));
    }
    return document.object();
}

[[nodiscard]] foundation::error::Error httpError(const int status, const QByteArray& body)
{
    foundation::error::ErrorCode code = foundation::error::ErrorCode::ProtocolFailure;
    if (status == 401 || status == 403) {
        code = foundation::error::ErrorCode::Unauthorized;
    } else if (status == 408) {
        code = foundation::error::ErrorCode::Timeout;
    } else if (status == 413 || status == 422) {
        code = foundation::error::ErrorCode::InvalidArgument;
    } else if (status == 429) {
        code = foundation::error::ErrorCode::RateLimited;
    } else if (status >= 500) {
        code = foundation::error::ErrorCode::Unavailable;
    }

    std::string message = "Media gateway request failed with HTTP " + std::to_string(status) + ".";
    auto parsed = parseObject(body);
    if (parsed) {
        const QString detail = parsed->value(QStringLiteral("message")).toString(
            parsed->value(QStringLiteral("error")).toString()).trimmed();
        if (!detail.isEmpty()) {
            message += " ";
            message += toString(detail.left(320));
        }
    }
    return makeError(code, std::move(message));
}

struct HttpResult final {
    int status{};
    QByteArray body{};
};

[[nodiscard]] farcaster::transport::async_http::Method asyncMethod(const QByteArray& method)
{
    if (method == QByteArrayLiteral("POST")) {
        return farcaster::transport::async_http::Method::Post;
    }
    if (method == QByteArrayLiteral("PUT")) {
        return farcaster::transport::async_http::Method::Put;
    }
    if (method == QByteArrayLiteral("DELETE")) {
        return farcaster::transport::async_http::Method::Delete;
    }
    return farcaster::transport::async_http::Method::Get;
}

[[nodiscard]] std::vector<std::string> asyncHeaders(const std::vector<Header>& headers)
{
    std::vector<std::string> result{};
    result.reserve(headers.size());
    for (const auto& [name, value] : headers) {
        std::string header{name.constData(), static_cast<std::size_t>(name.size())};
        header.append(": ");
        header.append(value.constData(), static_cast<std::size_t>(value.size()));
        result.push_back(std::move(header));
    }
    return result;
}

struct AsyncWaitState final {
    std::mutex mutex{};
    std::condition_variable ready{};
    std::optional<foundation::result::Result<farcaster::transport::http::HttpResponse>> result{};
    std::atomic_uint64_t sentBytes{};
};

[[nodiscard]] foundation::result::Result<HttpResult> performRequest(
    farcaster::transport::async_http::IAsyncHttpTransport& transport,
    const QByteArray& method,
    const QUrl& url,
    const std::vector<Header>& headers,
    const QByteArray& body,
    const std::chrono::milliseconds timeout,
    const std::size_t maxResponseBytes,
    const capability::media::CancellationProbe& cancelled,
    const capability::media::ProgressObserver& progress,
    const std::uint64_t totalBytes,
    const std::uint64_t progressBaseBytes = 0U)
{
    if (cancelled && cancelled()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Media upload was cancelled."));
    }

    auto state = std::make_shared<AsyncWaitState>();
    farcaster::transport::async_http::Request request{};
    request.method = asyncMethod(method);
    request.url = toString(url.toString(QUrl::FullyEncoded));
    request.headers = asyncHeaders(headers);
    request.body.assign(body.constData(), static_cast<std::size_t>(body.size()));
    request.timeout = timeout;
    request.maxBodyBytes = maxResponseBytes;
    request.priority = farcaster::transport::async_http::Priority::Interactive;
    request.retryPolicy = farcaster::transport::async_http::RetryPolicy::None;
    request.maximumAttempts = 1U;
    if (method == QByteArrayLiteral("PUT") && progress) {
        request.uploadProgress = [state](const std::uint64_t sent, const std::uint64_t) {
            state->sentBytes.store(sent, std::memory_order_relaxed);
            state->ready.notify_one();
        };
    }

    auto operation = transport.start(
        std::move(request),
        [state](foundation::result::Result<farcaster::transport::http::HttpResponse> response) mutable {
            {
                const std::scoped_lock lock{state->mutex};
                state->result.emplace(std::move(response));
            }
            state->ready.notify_one();
        });
    if (!operation) {
        return std::unexpected(std::move(operation.error()));
    }

    std::uint64_t lastReported = 0U;
    while (true) {
        std::unique_lock lock{state->mutex};
        if (state->result) {
            break;
        }
        state->ready.wait_for(lock, std::chrono::milliseconds{60});
        if (state->result) {
            break;
        }
        lock.unlock();

        if (cancelled && cancelled()) {
            // Do not wait for QtNetwork to deliver a cancellation callback.
            // The operation owns its async cancellation; the shared wait
            // state remains alive until its eventual callback.
            operation->cancel();
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unavailable,
                "Media upload was cancelled."));
        }
        if (progress && method == QByteArrayLiteral("PUT")) {
            const auto sent = state->sentBytes.load(std::memory_order_relaxed);
            if (sent != lastReported) {
                lastReported = sent;
                progress(
                    capability::media::UploadStage::Uploading,
                    std::min(totalBytes, progressBaseBytes + sent),
                    totalBytes);
            }
        }
    }

    auto response = std::move(*state->result);
    if (!response) {
        return std::unexpected(std::move(response.error()));
    }
    const int status = static_cast<int>(response->statusCode);
    const QByteArray responseBody{
        response->body.data(), static_cast<qsizetype>(response->body.size())};
    if (status < 200 || status >= 300) {
        return std::unexpected(httpError(status, responseBody));
    }
    return HttpResult{.status = status, .body = responseBody};
}

[[nodiscard]] std::string uploadChallenge(
    const QByteArray& method,
    const QString& path,
    const std::uint64_t fid,
    const std::string& publicKey,
    const qint64 timestampSeconds,
    const QString& nonce,
    const QByteArray& body)
{
    const QByteArray digest = QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex();
    QByteArray challenge{};
    challenge += "genycaster-media-upload-v1\n";
    challenge += method;
    challenge += '\n';
    challenge += path.toUtf8();
    challenge += '\n';
    challenge += QByteArray::number(fid);
    challenge += '\n';
    challenge += QByteArray::fromStdString(publicKey);
    challenge += '\n';
    challenge += QByteArray::number(timestampSeconds);
    challenge += '\n';
    challenge += nonce.toUtf8();
    challenge += '\n';
    challenge += digest;
    return std::string{challenge.constData(), static_cast<std::size_t>(challenge.size())};
}

[[nodiscard]] foundation::result::Result<std::vector<Header>> authorizationHeaders(
    capability::write::IWriteAuthority& authority,
    const domain::identity::Fid fid,
    const QByteArray& method,
    const QString& path,
    const QByteArray& body,
    const QByteArray& contentType = QByteArrayLiteral("application/json"))
{
    auto publicKey = authority.requireSigner(fid, domain::signer::Scope::CastAdd);
    if (!publicKey) {
        return std::unexpected(std::move(publicKey.error()));
    }
    const qint64 timestampSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
    const std::string challenge = uploadChallenge(
        method, path, fid.value, publicKey->hex, timestampSeconds, nonce, body);
    auto signature = authority.sign(fid, *publicKey, domain::signer::Scope::CastAdd, challenge);
    if (!signature) {
        return std::unexpected(std::move(signature.error()));
    }

    return std::vector<Header>{
        {QByteArrayLiteral("content-type"), contentType},
        {QByteArrayLiteral("accept"), QByteArrayLiteral("application/json")},
        {QByteArrayLiteral("x-genycaster-fid"), QByteArray::number(fid.value)},
        {QByteArrayLiteral("x-genycaster-signer-key"), QByteArray::fromStdString(publicKey->hex)},
        {QByteArrayLiteral("x-genycaster-timestamp"), QByteArray::number(timestampSeconds)},
        {QByteArrayLiteral("x-genycaster-nonce"), nonce.toUtf8()},
        {QByteArrayLiteral("x-genycaster-signature"), QByteArray::fromStdString(signatureHex(*signature))},
    };
}

[[nodiscard]] bool validHttpUrl(const QString& text)
{
    const QUrl url{text};
    return url.isValid() && !url.host().isEmpty() &&
        (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"));
}

class QtMediaUploadGateway final : public capability::media::IMediaUploadGateway {
public:
    QtMediaUploadGateway(
        capability::write::IWriteAuthority& authority,
        farcaster::transport::async_http::IAsyncHttpTransport& transport,
        Config config,
        QUrl baseUrl) noexcept
        : m_authority{authority},
          m_transport{transport},
          m_config{std::move(config)},
          m_baseUrl{std::move(baseUrl)}
    {
    }

    [[nodiscard]] foundation::result::Result<capability::media::ManagedAsset> upload(
        const domain::identity::Fid fid,
        const std::filesystem::path& localPath,
        const std::uint64_t maxImageBytes,
        capability::media::ProgressObserver progress,
        capability::media::CancellationProbe cancelled) override
    {
        if (maxImageBytes != 2U * 1024U * 1024U
            && maxImageBytes != 5U * 1024U * 1024U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Invalid image account upload limit."));
        }
        if (QCoreApplication::instance() == nullptr) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::TransportFailure,
                "Media upload requires an active QCoreApplication."));
        }
        if (progress) {
            progress(capability::media::UploadStage::Preparing, 0U, 0U);
        }

        const QString nativePath = toNativeQString(localPath);
        QFileInfo info{nativePath};
        if (!info.exists() || !info.isFile() || !info.isReadable()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Selected media file is not readable."));
        }
        const qint64 signedSize = info.size();
        if (signedSize <= 0) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Selected media file is empty."));
        }
        auto sizeBytes = static_cast<std::uint64_t>(signedSize);
        const std::uint64_t originalSourceBytes = sizeBytes;
        QMimeDatabase mimeDatabase{};
        // MatchDefault intentionally considers the filename before falling back to content
        // sniffing. MatchContent alone misclassifies valid ISO-BMFF/QuickTime media as
        // application/octet-stream on some platform MIME databases, which caused valid
        // .mp4/.mov selections to fail before the gateway request.
        QString contentType = mimeDatabase.mimeTypeForFile(info, QMimeDatabase::MatchDefault).name().toLower();
        const QString suffix = info.suffix().toLower();
        const auto canonicalFromSuffix = [&suffix]() -> QString {
            if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) return QStringLiteral("image/jpeg");
            if (suffix == QStringLiteral("png")) return QStringLiteral("image/png");
            if (suffix == QStringLiteral("webp")) return QStringLiteral("image/webp");
            if (suffix == QStringLiteral("gif")) return QStringLiteral("image/gif");
            if (suffix == QStringLiteral("avif")) return QStringLiteral("image/avif");
            if (suffix == QStringLiteral("mp4")) return QStringLiteral("video/mp4");
            if (suffix == QStringLiteral("webm")) return QStringLiteral("video/webm");
            if (suffix == QStringLiteral("mov")) return QStringLiteral("video/quicktime");
            return {};
        }();
        const bool detectedDefault = contentType == QStringLiteral("application/octet-stream");
        if (detectedDefault && !canonicalFromSuffix.isEmpty()) {
            contentType = canonicalFromSuffix;
        }
        const bool image = contentType == QStringLiteral("image/jpeg") ||
            contentType == QStringLiteral("image/png") ||
            contentType == QStringLiteral("image/webp") ||
            contentType == QStringLiteral("image/gif") ||
            contentType == QStringLiteral("image/avif");
        const bool video = contentType == QStringLiteral("video/mp4") ||
            contentType == QStringLiteral("video/webm") ||
            contentType == QStringLiteral("video/quicktime");
        if (!image && !video) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Genycaster media upload accepts JPG, PNG, WebP, GIF, AVIF, MP4, WebM, and MOV media only."));
        }
        if ((image && sizeBytes > m_config.maxImageBytes) ||
            (video && sizeBytes > m_config.maxVideoBytes)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                image ? "Selected image exceeds the configured upload size limit."
                      : "Selected video exceeds the configured upload size limit."));
        }
        if (cancelled && cancelled()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unavailable,
                "Media upload was cancelled."));
        }

        // QtGui compression runs in the existing media executor, not the UI
        // thread. Nothing is uploaded until a valid, size-bounded result is
        // available. Animated GIF and the text-heavy Stats PNG retain their
        // original bytes to preserve motion and fine typography.
        const bool exactStatsPng = image
            && contentType == QStringLiteral("image/png")
            && info.fileName().startsWith(QStringLiteral("genycaster-stats-"));
        QString uploadName = info.fileName();
        QByteArray optimizedBytes{};
        // QImageReader decodes a frame, not the full animated raster. Never
        // flatten WebP/APNG stickers into a still image during optimization.
        bool animatedRaster = contentType == QStringLiteral("image/gif");
        if (contentType == QStringLiteral("image/webp")
            || contentType == QStringLiteral("image/png")) {
            QFile headerFile{info.absoluteFilePath()};
            if (headerFile.open(QIODevice::ReadOnly)) {
                const QByteArray header = headerFile.read(64 * 1024);
                if (contentType == QStringLiteral("image/webp") && header.size() >= 21
                    && header.startsWith("RIFF") && header.mid(8, 4) == "WEBP"
                    && header.mid(12, 4) == "VP8X") {
                    animatedRaster = (static_cast<unsigned char>(header[20]) & 0x02U) != 0U;
                } else if (contentType == QStringLiteral("image/png")
                           && header.size() >= 33 && header.startsWith("\x89PNG\r\n\x1a\n")) {
                    qsizetype offset = 8;
                    while (offset + 12 <= header.size()) {
                        const auto* bytes = reinterpret_cast<const unsigned char*>(
                            header.constData() + offset);
                        const std::uint32_t chunkLength =
                            (static_cast<std::uint32_t>(bytes[0]) << 24)
                            | (static_cast<std::uint32_t>(bytes[1]) << 16)
                            | (static_cast<std::uint32_t>(bytes[2]) << 8)
                            | static_cast<std::uint32_t>(bytes[3]);
                        if (chunkLength > 16U * 1024U * 1024U
                            || chunkLength + 12U
                               > static_cast<std::uint64_t>(header.size() - offset)) break;
                        const QByteArray chunkType = header.mid(offset + 4, 4);
                        if (chunkType == "acTL") { animatedRaster = true; break; }
                        if (chunkType == "IDAT") break;
                        offset += static_cast<qsizetype>(chunkLength) + 12;
                    }
                }
            }
        }
        if (image && !exactStatsPng && !animatedRaster
            && sizeBytes > 256U * 1024U) {
            if (progress) progress(capability::media::UploadStage::Compressing,
                                   0U, originalSourceBytes);
            QImageReader reader{info.absoluteFilePath()};
            reader.setAutoTransform(true);
            const QSize dimensions = reader.size();
            if (!dimensions.isValid() || dimensions.width() > 8192
                || dimensions.height() > 8192
                || static_cast<qint64>(dimensions.width()) * dimensions.height() > 32'000'000) {
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::InvalidArgument,
                    "Source image dimensions exceed the safe local preparation limit."));
            }
            QImage decoded = reader.read();
            if (decoded.isNull()) {
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::InvalidArgument,
                    "Selected image cannot be decoded for local compression."));
            }
            const auto targetBytes = static_cast<qsizetype>(maxImageBytes);
            for (const int edge : {2560, 2140, 1720, 1360, 1024}) {
                if (cancelled && cancelled()) {
                    return std::unexpected(makeError(
                        foundation::error::ErrorCode::Unavailable,
                        "Media upload was cancelled."));
                }
                const QSize target = decoded.size().scaled(QSize{edge, edge},
                                                           Qt::KeepAspectRatio);
                const QImage scaled = target == decoded.size() ? decoded
                    : decoded.scaled(target, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
                for (const int quality : {84, 74, 63, 52, 43}) {
                    optimizedBytes.clear();
                    QBuffer sink{&optimizedBytes};
                    sink.open(QIODevice::WriteOnly);
                    QImageWriter writer{&sink, QByteArrayLiteral("webp")};
                    writer.setQuality(quality);
                    if (!writer.write(scaled)) {
                        optimizedBytes.clear();
                        break;
                    }
                    if (optimizedBytes.size() <= targetBytes) break;
                }
                if (!optimizedBytes.isEmpty()
                    && optimizedBytes.size() <= targetBytes) break;
            }
            if (!optimizedBytes.isEmpty()
                && optimizedBytes.size() <= targetBytes
                && (originalSourceBytes > static_cast<std::uint64_t>(targetBytes)
                    || static_cast<std::uint64_t>(optimizedBytes.size()) * 100
                        < originalSourceBytes * 92)) {
                sizeBytes = static_cast<std::uint64_t>(optimizedBytes.size());
                contentType = QStringLiteral("image/webp");
                uploadName = info.completeBaseName() + QStringLiteral(".webp");
            } else {
                optimizedBytes.clear();
            }
            if (progress) progress(capability::media::UploadStage::Compressing,
                                   sizeBytes, originalSourceBytes);
        }
        if (image && sizeBytes > maxImageBytes) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Image exceeds the account upload limit after local compression."));
        }

        const QJsonObject createObject{
            {QStringLiteral("fileName"), uploadName},
            {QStringLiteral("contentType"), contentType},
            {QStringLiteral("sizeBytes"), static_cast<double>(sizeBytes)},
            // Optimize before persisting images in QStorage. Videos remain
            // on the existing provider multipart path. Future UI may expose
            // a user-selected "original" policy; it is not required here.
            // CPU-painted Stats share cards are already optimized, contain
            // fine text and must retain their exact 1920x1920 pixels. Normal
            // user images still go through Smart Media Pipeline Auto.
            {QStringLiteral("optimizationMode"),
                !image || exactStatsPng || !optimizedBytes.isEmpty()
                    ? QStringLiteral("original") : QStringLiteral("auto")},
        };
        const QByteArray createBody = jsonBody(createObject);
        const QString createPath = QStringLiteral("/v1/uploads");
        if (progress) {
            progress(capability::media::UploadStage::Authorizing, 0U, sizeBytes);
        }
        auto createHeaders = authorizationHeaders(
            m_authority, fid, QByteArrayLiteral("POST"), createPath, createBody);
        if (!createHeaders) {
            return std::unexpected(std::move(createHeaders.error()));
        }
        auto created = performRequest(
            m_transport,
            QByteArrayLiteral("POST"),
            endpointUrl(m_baseUrl, createPath),
            *createHeaders,
            createBody,
            m_config.requestTimeout,
            m_config.maxResponseBytes,
            cancelled,
            progress,
            sizeBytes);
        if (!created) {
            return std::unexpected(std::move(created.error()));
        }
        auto ticket = parseObject(created->body);
        if (!ticket) {
            return std::unexpected(std::move(ticket.error()));
        }
        const QString uploadId = ticket->value(QStringLiteral("uploadId")).toString();
        const QString assetUrlText = ticket->value(QStringLiteral("assetUrl")).toString();
        const QString mode = ticket->value(QStringLiteral("mode")).toString();
        // Validate the JSON number *before* converting it to an unsigned
        // integer. A negative, fractional, NaN or oversized number previously
        // reached a floating-to-uint64 cast with undefined behavior.
        const auto parsedPartSize = detail::boundedJsonUnsigned(
            ticket->value(QStringLiteral("partSizeBytes")), 64U * 1024U * 1024U);
        const auto parsedPartCount = detail::boundedJsonUnsigned(
            ticket->value(QStringLiteral("partCount")), 10000U);
        if (uploadId.isEmpty() || mode != QStringLiteral("gateway-multipart") ||
            !validHttpUrl(assetUrlText) || !parsedPartSize || *parsedPartSize == 0U ||
            !parsedPartCount || *parsedPartCount == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Media gateway returned an invalid multipart upload ticket."));
        }
        // Only the v3 gateway confirms that optimization is supported.
        // Older deployed gateways silently ignore optimizationMode and
        // write originals; never claim to be compressing against v2.
        const QString serverOptimization = ticket->value(
            QStringLiteral("optimizationMode")).toString();
        const bool serverOptimizesImage = image
            && (serverOptimization == QStringLiteral("auto")
                || serverOptimization == QStringLiteral("high"));
        const std::uint64_t partSizeBytes = *parsedPartSize;
        const std::uint64_t partCount = *parsedPartCount;
        // Quotient + remainder avoids overflow from (size + partSize - 1).
        const std::uint64_t expectedPartCount = sizeBytes / partSizeBytes
            + (sizeBytes % partSizeBytes != 0U ? 1U : 0U);
        if (partCount != expectedPartCount) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Media gateway returned an invalid multipart upload ticket."));
        }

        QFile file{info.absoluteFilePath()};
        QBuffer compressedInput{&optimizedBytes};
        QIODevice* const sourceDevice = optimizedBytes.isEmpty()
            ? static_cast<QIODevice*>(&file) : static_cast<QIODevice*>(&compressedInput);
        if (!sourceDevice->open(QIODevice::ReadOnly)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Selected media file could not be opened for upload."));
        }

        const auto abortMultipart = [&]() {
            const QJsonObject abortObject{{QStringLiteral("uploadId"), uploadId}};
            const QByteArray abortBody = jsonBody(abortObject);
            const QString abortPath = QStringLiteral("/v1/uploads");
            auto abortHeaders = authorizationHeaders(
                m_authority, fid, QByteArrayLiteral("DELETE"), abortPath, abortBody);
            if (!abortHeaders) {
                return;
            }
            (void)performRequest(
                m_transport,
                QByteArrayLiteral("DELETE"),
                endpointUrl(m_baseUrl, abortPath),
                *abortHeaders,
                abortBody,
                m_config.requestTimeout,
                m_config.maxResponseBytes,
                {},
                {},
                sizeBytes);
        };

        QJsonArray completionParts{};
        std::uint64_t transferred = 0U;
        if (progress) {
            progress(capability::media::UploadStage::Uploading, transferred, sizeBytes);
        }
        for (std::uint64_t partIndex = 0U; partIndex < partCount; ++partIndex) {
            if (cancelled && cancelled()) {
                abortMultipart();
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::Unavailable,
                    "Media upload was cancelled."));
            }
            const std::uint64_t remaining = sizeBytes - transferred;
            const std::uint64_t expectedBytes = std::min(partSizeBytes, remaining);
            const QByteArray partBody = sourceDevice->read(static_cast<qint64>(expectedBytes));
            if (partBody.size() != static_cast<qsizetype>(expectedBytes)) {
                abortMultipart();
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::TransportFailure,
                    "Selected media file could not be read completely."));
            }

            const std::uint64_t partNumber = partIndex + 1U;
            const QString partPath = QStringLiteral("/v1/uploads/") + uploadId +
                QStringLiteral("/parts/") + QString::number(partNumber);
            auto partHeaders = authorizationHeaders(
                m_authority,
                fid,
                QByteArrayLiteral("PUT"),
                partPath,
                partBody,
                QByteArrayLiteral("application/octet-stream"));
            if (!partHeaders) {
                abortMultipart();
                return std::unexpected(std::move(partHeaders.error()));
            }
            auto uploaded = performRequest(
                m_transport,
                QByteArrayLiteral("PUT"),
                endpointUrl(m_baseUrl, partPath),
                *partHeaders,
                partBody,
                m_config.uploadTimeout,
                m_config.maxResponseBytes,
                cancelled,
                progress,
                sizeBytes,
                transferred);
            if (!uploaded) {
                auto error = std::move(uploaded.error());
                abortMultipart();
                return std::unexpected(std::move(error));
            }
            auto partResponse = parseObject(uploaded->body);
            if (!partResponse) {
                abortMultipart();
                return std::unexpected(std::move(partResponse.error()));
            }
            const auto returnedPartNumber = detail::boundedJsonUnsigned(
                partResponse->value(QStringLiteral("partNumber")), partNumber);
            const auto returnedSizeBytes = detail::boundedJsonUnsigned(
                partResponse->value(QStringLiteral("sizeBytes")), expectedBytes);
            const QString etag = partResponse->value(QStringLiteral("etag")).toString().trimmed();
            if (!returnedPartNumber || *returnedPartNumber != partNumber ||
                !returnedSizeBytes || *returnedSizeBytes != expectedBytes || etag.isEmpty()) {
                abortMultipart();
                return std::unexpected(makeError(
                    foundation::error::ErrorCode::ProtocolFailure,
                    "Media gateway returned invalid multipart part metadata."));
            }
            completionParts.append(QJsonObject{
                {QStringLiteral("partNumber"), static_cast<double>(partNumber)},
                {QStringLiteral("etag"), etag},
            });
            transferred += expectedBytes;
            if (progress) {
                progress(capability::media::UploadStage::Uploading, transferred, sizeBytes);
            }
        }
        if (transferred != sizeBytes || !sourceDevice->atEnd()) {
            abortMultipart();
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Media upload part accounting did not match the selected file."));
        }

        if (progress) {
            // The complete endpoint first optimizes/stores a staged image.
            // Direct video/original uploads only need provider finalization.
            progress(serverOptimizesImage ? capability::media::UploadStage::Optimizing
                                          : capability::media::UploadStage::Finalizing,
                     sizeBytes, sizeBytes);
        }
        const QJsonObject completeObject{
            {QStringLiteral("uploadId"), uploadId},
            {QStringLiteral("parts"), completionParts},
        };
        const QByteArray completeBody = jsonBody(completeObject);
        const QString completePath = QStringLiteral("/v1/uploads/complete");
        auto completeHeaders = authorizationHeaders(
            m_authority, fid, QByteArrayLiteral("POST"), completePath, completeBody);
        if (!completeHeaders) {
            return std::unexpected(std::move(completeHeaders.error()));
        }
        auto completed = performRequest(
            m_transport,
            QByteArrayLiteral("POST"),
            endpointUrl(m_baseUrl, completePath),
            *completeHeaders,
            completeBody,
            // Finalization may run the Smart Media Pipeline and QStorage
            // multipart complete/verification. Its 30s control timeout is
            // appropriate for ticket creation, but too short for this phase.
            std::max(m_config.requestTimeout, std::chrono::milliseconds{90'000}),
            m_config.maxResponseBytes,
            cancelled,
            progress,
            sizeBytes);
        if (!completed) {
            return std::unexpected(std::move(completed.error()));
        }
        auto completion = parseObject(completed->body);
        if (!completion) {
            return std::unexpected(std::move(completion.error()));
        }

        const QString finalAssetUrl = completion->value(QStringLiteral("assetUrl")).toString(assetUrlText);
        const QString finalContentType = completion->value(QStringLiteral("contentType")).toString(contentType);
        const QString finalFileName = completion->value(QStringLiteral("fileName")).toString(info.fileName());
        const QString managementToken = completion->value(QStringLiteral("managementToken")).toString().trimmed();
        const QJsonValue finalSizeField = completion->value(QStringLiteral("sizeBytes"));
        const auto finalSize = finalSizeField.isUndefined()
            ? std::optional<std::uint64_t>{sizeBytes}
            : detail::boundedJsonUnsigned(finalSizeField, sizeBytes);
        const QJsonValue storedSizeField =
            completion->value(QStringLiteral("storedSizeBytes"));
        const auto storedSize = storedSizeField.isUndefined()
            ? finalSize
            : detail::boundedJsonUnsigned(storedSizeField, sizeBytes);
        if (!validHttpUrl(finalAssetUrl) || !finalSize || *finalSize != sizeBytes ||
            !storedSize || *storedSize == 0U ||
            managementToken.isEmpty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Media gateway completion response did not match the uploaded file."));
        }
        if (serverOptimizesImage && progress) {
            progress(capability::media::UploadStage::Finalizing, sizeBytes, sizeBytes);
        }

        return capability::media::ManagedAsset{
            .asset = domain::media::Asset{
                .url = toString(finalAssetUrl),
                .contentType = toString(finalContentType),
                .fileName = toString(finalFileName),
                .sizeBytes = *storedSize,
                .kind = image ? domain::media::Kind::Image : domain::media::Kind::Video,
            },
            .managementToken = toString(managementToken),
        };
    }

private:
    capability::write::IWriteAuthority& m_authority;
    farcaster::transport::async_http::IAsyncHttpTransport& m_transport;
    Config m_config{};
    QUrl m_baseUrl{};
};

class QtMediaAssetLifecycleGateway final : public capability::media::IMediaAssetLifecycleGateway {
public:
    QtMediaAssetLifecycleGateway(
        capability::write::IWriteAuthority& authority,
        farcaster::transport::async_http::IAsyncHttpTransport& transport,
        Config config,
        QUrl baseUrl) noexcept
        : m_authority{authority},
          m_transport{transport},
          m_config{std::move(config)},
          m_baseUrl{std::move(baseUrl)}
    {
    }

    [[nodiscard]] foundation::result::Status remove(
        const domain::identity::Fid fid,
        const std::string_view managementToken) override
    {
        if (QCoreApplication::instance() == nullptr) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::TransportFailure,
                "Managed media cleanup requires an active QCoreApplication."));
        }
        if (managementToken.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Managed media cleanup requires an asset management token."));
        }

        const QJsonObject deleteObject{
            {QStringLiteral("managementToken"),
             QString::fromUtf8(managementToken.data(), static_cast<qsizetype>(managementToken.size()))},
        };
        const QByteArray deleteBody = jsonBody(deleteObject);
        const QString deletePath = QStringLiteral("/v1/assets");
        auto headers = authorizationHeaders(
            m_authority, fid, QByteArrayLiteral("DELETE"), deletePath, deleteBody);
        if (!headers) {
            return std::unexpected(std::move(headers.error()));
        }

        auto deleted = performRequest(
            m_transport,
            QByteArrayLiteral("DELETE"),
            endpointUrl(m_baseUrl, deletePath),
            *headers,
            deleteBody,
            m_config.requestTimeout,
            m_config.maxResponseBytes,
            {},
            {},
            0U);
        if (!deleted) {
            return std::unexpected(std::move(deleted.error()));
        }
        return {};
    }

private:
    capability::write::IWriteAuthority& m_authority;
    farcaster::transport::async_http::IAsyncHttpTransport& m_transport;
    Config m_config{};
    QUrl m_baseUrl{};
};

} // namespace

foundation::result::Result<std::unique_ptr<capability::media::IMediaUploadGateway>>
makeGateway(
    capability::write::IWriteAuthority& authority,
    farcaster::transport::async_http::IAsyncHttpTransport& transport,
    Config config)
{
    auto baseUrl = normalizedBaseUrl(config.baseUrl);
    if (!baseUrl) {
        return std::unexpected(std::move(baseUrl.error()));
    }
    if (config.requestTimeout <= std::chrono::milliseconds::zero() ||
        config.uploadTimeout <= std::chrono::milliseconds::zero() || config.maxResponseBytes == 0U ||
        config.maxImageBytes == 0U || config.maxVideoBytes == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Media gateway limits and request timeouts must be positive."));
    }
    std::unique_ptr<capability::media::IMediaUploadGateway> gateway =
        std::make_unique<QtMediaUploadGateway>(authority, transport, std::move(config), std::move(*baseUrl));
    return gateway;
}

foundation::result::Result<std::unique_ptr<capability::media::IMediaAssetLifecycleGateway>>
makeLifecycleGateway(
    capability::write::IWriteAuthority& authority,
    farcaster::transport::async_http::IAsyncHttpTransport& transport,
    Config config)
{
    auto baseUrl = normalizedBaseUrl(config.baseUrl);
    if (!baseUrl) {
        return std::unexpected(std::move(baseUrl.error()));
    }
    if (config.requestTimeout <= std::chrono::milliseconds::zero() ||
        config.maxResponseBytes == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Media gateway request timeout and response limit must be positive."));
    }
    std::unique_ptr<capability::media::IMediaAssetLifecycleGateway> gateway =
        std::make_unique<QtMediaAssetLifecycleGateway>(
            authority, transport, std::move(config), std::move(*baseUrl));
    return gateway;
}

} // namespace farcaster::transport::qtnetwork::media
