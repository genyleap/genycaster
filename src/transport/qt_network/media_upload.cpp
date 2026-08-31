module;

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
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
import farcaster.transport.qtnetwork;

namespace farcaster::transport::qtnetwork::media {
namespace {

using Header = std::pair<QByteArray, QByteArray>;

struct ThreadNetworkManagers final {
    std::unique_ptr<QNetworkAccessManager> system{};
    std::unique_ptr<QNetworkAccessManager> direct{};
};

[[nodiscard]] QNetworkAccessManager& networkManagerFor(const ProxyPolicy proxyPolicy)
{
    thread_local ThreadNetworkManagers managers{};
    auto& selected = proxyPolicy == ProxyPolicy::Direct ? managers.direct : managers.system;
    if (!selected) {
        selected = std::make_unique<QNetworkAccessManager>();
        if (proxyPolicy == ProxyPolicy::Direct) {
            selected->setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
        }
    }
    return *selected;
}

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

[[nodiscard]] foundation::result::Result<HttpResult> performRequest(
    QNetworkAccessManager& manager,
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

    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    for (const auto& [name, value] : headers) {
        request.setRawHeader(name, value);
    }

    QNetworkReply* rawReply = nullptr;
    if (method == QByteArrayLiteral("POST")) {
        rawReply = manager.post(request, body);
    } else if (method == QByteArrayLiteral("PUT")) {
        rawReply = manager.put(request, body);
    } else if (method == QByteArrayLiteral("DELETE")) {
        rawReply = manager.sendCustomRequest(request, QByteArrayLiteral("DELETE"), body);
    } else {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Unsupported media HTTP request mode."));
    }
    if (rawReply == nullptr) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::TransportFailure,
            "Qt Network could not create the media request."));
    }
    std::unique_ptr<QNetworkReply> reply{rawReply};

    bool timedOut = false;
    bool cancelledDuringRequest = false;
    bool responseTooLarge = false;
    QByteArray responseBody{};
    QEventLoop loop{};
    QTimer timeoutTimer{};
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, reply.get(), [&] {
        timedOut = true;
        reply->abort();
    });
    QTimer cancellationTimer{};
    cancellationTimer.setInterval(80);
    QObject::connect(&cancellationTimer, &QTimer::timeout, reply.get(), [&] {
        if (cancelled && cancelled()) {
            cancelledDuringRequest = true;
            reply->abort();
        }
    });
    const auto drainResponse = [&] {
        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty() || responseTooLarge) {
            return;
        }
        const auto currentSize = static_cast<std::size_t>(responseBody.size());
        const auto chunkSize = static_cast<std::size_t>(chunk.size());
        if (currentSize > maxResponseBytes || chunkSize > maxResponseBytes - currentSize) {
            responseTooLarge = true;
            reply->abort();
            return;
        }
        responseBody.append(chunk);
    };
    QObject::connect(reply.get(), &QNetworkReply::readyRead, reply.get(), drainResponse);
    QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (method == QByteArrayLiteral("PUT") && progress) {
        QObject::connect(reply.get(), &QNetworkReply::uploadProgress, reply.get(),
            [&](const qint64 sent, const qint64) {
                const auto safeSent = static_cast<std::uint64_t>(std::max<qint64>(0, sent));
                const auto absoluteSent = std::min(totalBytes, progressBaseBytes + safeSent);
                progress(capability::media::UploadStage::Uploading, absoluteSent, totalBytes);
            });
    }

    timeoutTimer.start(static_cast<int>(std::clamp<long long>(
        timeout.count(), 1LL, static_cast<long long>(std::numeric_limits<int>::max()))));
    cancellationTimer.start();
    loop.exec();
    cancellationTimer.stop();
    timeoutTimer.stop();
    drainResponse();

    if (responseTooLarge) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Media gateway response exceeded the configured limit."));
    }
    if (cancelledDuringRequest || (cancelled && cancelled())) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Media upload was cancelled."));
    }
    if (timedOut) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Timeout,
            "Media upload request timed out."));
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 100 && (status < 200 || status >= 300)) {
        return std::unexpected(httpError(status, responseBody));
    }
    if (reply->error() != QNetworkReply::NoError) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::TransportFailure,
            "Media upload network request failed: " + toString(reply->errorString())));
    }
    if (status < 200 || status >= 300) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Media gateway did not return a valid HTTP success status."));
    }
    return HttpResult{.status = status, .body = std::move(responseBody)};
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
    auto signature = authority.sign(domain::signer::Scope::CastAdd, challenge);
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
        const ProxyPolicy proxyPolicy,
        Config config,
        QUrl baseUrl) noexcept
        : m_authority{authority},
          m_proxyPolicy{proxyPolicy},
          m_config{std::move(config)},
          m_baseUrl{std::move(baseUrl)}
    {
    }

    [[nodiscard]] foundation::result::Result<capability::media::ManagedAsset> upload(
        const domain::identity::Fid fid,
        const std::filesystem::path& localPath,
        capability::media::ProgressObserver progress,
        capability::media::CancellationProbe cancelled) override
    {
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
        const auto sizeBytes = static_cast<std::uint64_t>(signedSize);
        QMimeDatabase mimeDatabase{};
        const QString contentType = mimeDatabase.mimeTypeForFile(info, QMimeDatabase::MatchContent).name();
        const bool image = contentType.startsWith(QStringLiteral("image/"));
        const bool video = contentType.startsWith(QStringLiteral("video/"));
        if (!image && !video) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Genycaster media upload accepts image and video files only."));
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

        const QJsonObject createObject{
            {QStringLiteral("fileName"), info.fileName()},
            {QStringLiteral("contentType"), contentType},
            {QStringLiteral("sizeBytes"), static_cast<double>(sizeBytes)},
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
        QNetworkAccessManager& manager = networkManagerFor(m_proxyPolicy);
        auto created = performRequest(
            manager,
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
        const auto partSizeBytes = static_cast<std::uint64_t>(
            ticket->value(QStringLiteral("partSizeBytes")).toDouble(0.0));
        const auto partCount = static_cast<std::uint64_t>(
            ticket->value(QStringLiteral("partCount")).toDouble(0.0));
        const auto expectedPartCount = partSizeBytes > 0U
            ? (sizeBytes + partSizeBytes - 1U) / partSizeBytes
            : 0U;
        if (uploadId.isEmpty() || mode != QStringLiteral("gateway-multipart") ||
            !validHttpUrl(assetUrlText) || partSizeBytes == 0U || partCount == 0U ||
            partCount != expectedPartCount || partCount > 10000U ||
            partSizeBytes > 64U * 1024U * 1024U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Media gateway returned an invalid multipart upload ticket."));
        }

        QFile file{info.absoluteFilePath()};
        if (!file.open(QIODevice::ReadOnly)) {
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
                manager,
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
            const QByteArray partBody = file.read(static_cast<qint64>(expectedBytes));
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
                manager,
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
            const auto returnedPartNumber = static_cast<std::uint64_t>(
                partResponse->value(QStringLiteral("partNumber")).toDouble(0.0));
            const auto returnedSizeBytes = static_cast<std::uint64_t>(
                partResponse->value(QStringLiteral("sizeBytes")).toDouble(0.0));
            const QString etag = partResponse->value(QStringLiteral("etag")).toString().trimmed();
            if (returnedPartNumber != partNumber || returnedSizeBytes != expectedBytes || etag.isEmpty()) {
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
        if (transferred != sizeBytes || !file.atEnd()) {
            abortMultipart();
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Media upload part accounting did not match the selected file."));
        }

        if (progress) {
            progress(capability::media::UploadStage::Verifying, sizeBytes, sizeBytes);
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
            manager,
            QByteArrayLiteral("POST"),
            endpointUrl(m_baseUrl, completePath),
            *completeHeaders,
            completeBody,
            m_config.requestTimeout,
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
        const auto finalSize = static_cast<std::uint64_t>(
            completion->value(QStringLiteral("sizeBytes")).toDouble(static_cast<double>(sizeBytes)));
        if (!validHttpUrl(finalAssetUrl) || finalSize != sizeBytes || managementToken.isEmpty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Media gateway completion response did not match the uploaded file."));
        }

        return capability::media::ManagedAsset{
            .asset = domain::media::Asset{
                .url = toString(finalAssetUrl),
                .contentType = toString(finalContentType),
                .fileName = toString(finalFileName),
                .sizeBytes = finalSize,
                .kind = image ? domain::media::Kind::Image : domain::media::Kind::Video,
            },
            .managementToken = toString(managementToken),
        };
    }

private:
    capability::write::IWriteAuthority& m_authority;
    ProxyPolicy m_proxyPolicy{ProxyPolicy::System};
    Config m_config{};
    QUrl m_baseUrl{};
};

class QtMediaAssetLifecycleGateway final : public capability::media::IMediaAssetLifecycleGateway {
public:
    QtMediaAssetLifecycleGateway(
        capability::write::IWriteAuthority& authority,
        const ProxyPolicy proxyPolicy,
        Config config,
        QUrl baseUrl) noexcept
        : m_authority{authority},
          m_proxyPolicy{proxyPolicy},
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

        QNetworkAccessManager& manager = networkManagerFor(m_proxyPolicy);
        auto deleted = performRequest(
            manager,
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
    ProxyPolicy m_proxyPolicy{ProxyPolicy::System};
    Config m_config{};
    QUrl m_baseUrl{};
};

} // namespace

foundation::result::Result<std::unique_ptr<capability::media::IMediaUploadGateway>>
makeGateway(
    capability::write::IWriteAuthority& authority,
    const ProxyPolicy proxyPolicy,
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
        std::make_unique<QtMediaUploadGateway>(authority, proxyPolicy, std::move(config), std::move(*baseUrl));
    return gateway;
}

foundation::result::Result<std::unique_ptr<capability::media::IMediaAssetLifecycleGateway>>
makeLifecycleGateway(
    capability::write::IWriteAuthority& authority,
    const ProxyPolicy proxyPolicy,
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
            authority, proxyPolicy, std::move(config), std::move(*baseUrl));
    return gateway;
}

} // namespace farcaster::transport::qtnetwork::media
