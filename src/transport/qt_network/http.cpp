module;

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSslError>
#include <QSslSocket>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.transport.qtnetwork;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.http;

namespace farcaster::transport::qtnetwork {
namespace {

std::once_flag systemProxyConfigurationFlag{};

struct ThreadNetworkManagers final {
    std::unique_ptr<QNetworkAccessManager> system{};
    std::unique_ptr<QNetworkAccessManager> direct{};
};

[[nodiscard]] QNetworkAccessManager& networkManagerFor(const ProxyPolicy proxyPolicy)
{
    thread_local ThreadNetworkManagers managers{};
    auto& selected = proxyPolicy == ProxyPolicy::Direct
        ? managers.direct : managers.system;
    if (!selected) {
        selected = std::make_unique<QNetworkAccessManager>();
        if (proxyPolicy == ProxyPolicy::Direct) {
            selected->setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
        }
    }
    return *selected;
}

enum class RequestStage : std::uint8_t {
    Starting,
    Connecting,
    TlsEstablished,
    RequestSent,
    ResponseHeaders,
    ResponseBody,
};

[[nodiscard]] std::string_view stageDescription(const RequestStage stage) noexcept
{
    switch (stage) {
    case RequestStage::Starting:
        return "Starting";
    case RequestStage::Connecting:
        return "Connecting";
    case RequestStage::TlsEstablished:
        return "TlsEstablished";
    case RequestStage::RequestSent:
        return "WaitingForHeaders";
    case RequestStage::ResponseHeaders:
        return "ResponseHeaders";
    case RequestStage::ResponseBody:
        return "ResponseBody";
    }
    return "Unknown";
}

[[nodiscard]] foundation::error::Error transportError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] std::string toUtf8(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string{utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

[[nodiscard]] std::string_view proxyPolicySummary(const ProxyPolicy proxyPolicy) noexcept
{
    switch (proxyPolicy) {
    case ProxyPolicy::Direct:
        return "direct (no proxy)";
    case ProxyPolicy::System:
        return "system configuration";
    }
    return "unknown";
}

[[nodiscard]] std::string sslErrorSummary(const QList<QSslError>& errors)
{
    QStringList descriptions{};
    descriptions.reserve(errors.size());
    for (const QSslError& error : errors) {
        const QString description = error.errorString().trimmed();
        if (!description.isEmpty() && !descriptions.contains(description)) {
            descriptions.push_back(description);
        }
    }
    return toUtf8(descriptions.join(QStringLiteral("; ")));
}

[[nodiscard]] bool appendBodyChunk(
    std::string& body,
    const QByteArray& chunk,
    const std::size_t maxBytes)
{
    if (chunk.isEmpty()) {
        return true;
    }

    const auto chunkSize = static_cast<std::size_t>(chunk.size());
    if (chunkSize > maxBytes || body.size() > maxBytes - chunkSize) {
        return false;
    }

    body.append(chunk.constData(), chunkSize);
    return true;
}

[[nodiscard]] foundation::result::Status applyHeaders(
    QNetworkRequest& networkRequest,
    const std::vector<std::string>& headers)
{
    for (const std::string& header : headers) {
        if (header.find_first_of("\r\n") != std::string::npos) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP header must not contain line breaks."));
        }

        const std::size_t separator = header.find(':');
        if (separator == std::string::npos || separator == 0) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP header must use the 'Name: value' form."));
        }

        const QByteArray name{header.data(), static_cast<qsizetype>(separator)};
        const QByteArray value{
            header.data() + separator + 1,
            static_cast<qsizetype>(header.size() - separator - 1)};
        networkRequest.setRawHeader(name.trimmed(), value.trimmed());
    }
    return {};
}

class QtHttpTransport final : public farcaster::transport::http::IHttpTransport {
public:
    QtHttpTransport(
        const ProxyPolicy proxyPolicy,
        const ProtocolPolicy protocolPolicy) noexcept
        : proxyPolicy_{proxyPolicy},
          protocolPolicy_{protocolPolicy}
    {
    }

    [[nodiscard]] foundation::result::Result<farcaster::transport::http::HttpResponse>
    get(const farcaster::transport::http::HttpRequest& request) override
    {
        return perform(
            request.url,
            request.headers,
            request.timeout,
            request.maxBodyBytes,
            nullptr);
    }

    [[nodiscard]] foundation::result::Result<farcaster::transport::http::HttpResponse>
    post(const farcaster::transport::http::HttpPostRequest& request) override
    {
        return perform(
            request.url,
            request.headers,
            request.timeout,
            request.maxBodyBytes,
            &request.body);
    }

private:
    [[nodiscard]] foundation::result::Result<farcaster::transport::http::HttpResponse>
    perform(
        const std::string& requestUrl,
        const std::vector<std::string>& requestHeaders,
        const std::chrono::milliseconds timeout,
        const std::size_t maxBodyBytes,
        const std::string* const requestBody)
    {
        if (QCoreApplication::instance() == nullptr) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::TransportFailure,
                "Qt Network transport requires an active QCoreApplication."));
        }
        if (requestUrl.empty()) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP request URL must not be empty."));
        }
        if (maxBodyBytes == 0) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP maximum response-body size must be greater than zero."));
        }

        const QUrl url{QString::fromUtf8(
            requestUrl.data(), static_cast<qsizetype>(requestUrl.size()))};
        if (!url.isValid() || url.host().isEmpty() ||
            (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP request URL must be a valid http or https URL."));
        }

        QNetworkRequest networkRequest{url};
        networkRequest.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute,
            QNetworkRequest::NoLessSafeRedirectPolicy);
        if (protocolPolicy_ == ProtocolPolicy::Http1Only) {
            networkRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        }
        auto headerStatus = applyHeaders(networkRequest, requestHeaders);
        if (!headerStatus) {
            return std::unexpected(std::move(headerStatus.error()));
        }

        const std::string configuredProxyPolicy{proxyPolicySummary(proxyPolicy_)};
        QNetworkAccessManager& manager = networkManagerFor(proxyPolicy_);
        std::unique_ptr<QNetworkReply> reply{};
        if (requestBody == nullptr) {
            reply.reset(manager.get(networkRequest));
        } else {
            const QByteArray payload{
                requestBody->data(), static_cast<qsizetype>(requestBody->size())};
            reply.reset(manager.post(networkRequest, payload));
        }
        if (!reply) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::TransportFailure,
                "Qt Network could not create an HTTP reply."));
        }

        QEventLoop loop{};
        QTimer timeoutTimer{};
        timeoutTimer.setSingleShot(true);

        std::string responseBody{};
        responseBody.reserve(std::min<std::size_t>(maxBodyBytes, 64U * 1024U));
        bool bodyLimitExceeded{false};
        bool timedOut{false};
        QList<QSslError> sslErrors{};
        RequestStage requestStage{RequestStage::Starting};
        QElapsedTimer elapsedTimer{};
        elapsedTimer.start();

        const auto consumeAvailableBody = [&]() {
            if (bodyLimitExceeded || !reply->isOpen()) {
                return;
            }
            const QByteArray chunk = reply->readAll();
            if (!appendBodyChunk(responseBody, chunk, maxBodyBytes)) {
                bodyLimitExceeded = true;
                reply->abort();
            }
        };

        QObject::connect(reply.get(), &QNetworkReply::socketStartedConnecting, &loop, [&]() {
            requestStage = RequestStage::Connecting;
        });
        QObject::connect(reply.get(), &QNetworkReply::encrypted, &loop, [&]() {
            requestStage = RequestStage::TlsEstablished;
        });
        QObject::connect(reply.get(), &QNetworkReply::requestSent, &loop, [&]() {
            requestStage = RequestStage::RequestSent;
        });
        QObject::connect(reply.get(), &QNetworkReply::metaDataChanged, &loop, [&]() {
            requestStage = RequestStage::ResponseHeaders;
        });
        QObject::connect(reply.get(), &QNetworkReply::readyRead, &loop, [&]() {
            requestStage = RequestStage::ResponseBody;
            consumeAvailableBody();
        });
        QObject::connect(
            reply.get(),
            &QNetworkReply::sslErrors,
            &loop,
            [&](const QList<QSslError>& errors) {
                sslErrors = errors;
                // Deliberately do not call ignoreSslErrors(). Certificate
                // verification is a security invariant of this transport.
            });
        QObject::connect(reply.get(), &QNetworkReply::finished, &loop, [&]() {
            consumeAvailableBody();
            loop.quit();
        });
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            reply->abort();
            loop.quit();
        });

        const auto timeoutCount = timeout.count();
        const auto clampedTimeout = std::clamp<std::int64_t>(
            timeoutCount,
            1,
            static_cast<std::int64_t>(std::numeric_limits<int>::max()));
        timeoutTimer.start(static_cast<int>(clampedTimeout));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        timeoutTimer.stop();

        if (bodyLimitExceeded) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::TransportFailure,
                "HTTP response body exceeded the configured size limit."));
        }
        if (timedOut) {
            std::string message = "Transport: QtNetwork\nStage: ";
            message.append(stageDescription(requestStage));
            message.append("\nElapsed: ");
            message.append(std::to_string(elapsedTimer.elapsed()));
            message.append("ms\nHost: ");
            message.append(toUtf8(url.host()));
            message.append("\nProxy: ");
            message.append(configuredProxyPolicy);
            return std::unexpected(transportError(
                foundation::error::ErrorCode::Timeout,
                std::move(message)));
        }

        const QVariant statusAttribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        bool statusValid{false};
        const int statusCode = statusAttribute.toInt(&statusValid);
        if (statusValid && statusCode >= 100 && statusCode <= 65'535) {
            return farcaster::transport::http::HttpResponse{
                .statusCode = static_cast<std::uint16_t>(statusCode),
                .body = std::move(responseBody),
            };
        }

        if (reply->error() != QNetworkReply::NoError) {
            std::string message = "Qt Network request failed: " + toUtf8(reply->errorString()) + ".";
            const std::string tlsDetails = sslErrorSummary(sslErrors);
            if (!tlsDetails.empty()) {
                message.append(" TLS: ");
                message.append(tlsDetails);
                message.push_back('.');
            }
            if (reply->error() == QNetworkReply::SslHandshakeFailedError || !tlsDetails.empty()) {
                message.append(" TLS backend: ");
                message.append(toUtf8(QSslSocket::activeBackend()));
                message.push_back('.');
            }
            const auto code = reply->error() == QNetworkReply::TimeoutError
                ? foundation::error::ErrorCode::Timeout
                : foundation::error::ErrorCode::TransportFailure;
            return std::unexpected(transportError(code, std::move(message)));
        }

        return std::unexpected(transportError(
            foundation::error::ErrorCode::TransportFailure,
            "Qt Network completed the request without an HTTP status code."));
    }

    ProxyPolicy proxyPolicy_{ProxyPolicy::System};
    ProtocolPolicy protocolPolicy_{ProtocolPolicy::Automatic};
};
}

foundation::result::Result<std::unique_ptr<http::IHttpTransport>> makeTransport(
    const ProxyPolicy proxyPolicy,
    const ProtocolPolicy protocolPolicy)
{
    if (QCoreApplication::instance() == nullptr) {
        return std::unexpected(transportError(
            foundation::error::ErrorCode::TransportFailure,
            "Qt Network transport requires an active QCoreApplication."));
    }

    if (proxyPolicy == ProxyPolicy::System) {
        std::call_once(systemProxyConfigurationFlag, [] {
            QNetworkProxyFactory::setUseSystemConfiguration(true);
        });
    }

    if (!QSslSocket::supportsSsl()) {
        return std::unexpected(transportError(
            foundation::error::ErrorCode::TransportFailure,
            "Qt Network has no usable TLS backend. Active backend: " +
                toUtf8(QSslSocket::activeBackend()) + "."));
    }

    std::unique_ptr<http::IHttpTransport> transport =
        std::make_unique<QtHttpTransport>(proxyPolicy, protocolPolicy);
    return transport;
}

}
