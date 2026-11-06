module;

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
// Qt WebAssembly delegates TLS to the browser; Qt builds without the SSL
// feature also omit QSslSocket and QNetworkReply handshake diagnostics.
#include <QSslError>
#include <QSslSocket>
#endif
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

module farcaster.transport.qtnetwork;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.http;

namespace farcaster::transport::qtnetwork {
namespace {

std::once_flag systemProxyConfigurationFlag{};

struct RequestShutdownState final {
    std::mutex mutex{};
    bool isStopping{};
    std::unordered_map<QObject*, std::function<void()>> requests{};
};

// This registry only binds factories to the composition-owned application scope.
// Transports retain shared state, never a borrowed pointer to the scope itself.
std::mutex shutdownScopeMutex{};
std::weak_ptr<RequestShutdownState> activeShutdownScope{};

class RequestRegistration final {
public:
    RequestRegistration(std::shared_ptr<RequestShutdownState> state,
                        QObject& context, std::function<void()> cancel)
        : m_state(std::move(state)), m_context(context)
    {
        if (m_state) {
            std::scoped_lock lock{m_state->mutex};
            m_isAccepted = !m_state->isStopping;
            if (m_isAccepted) {
                m_state->requests.emplace(&m_context, std::move(cancel));
            }
        }
    }

    ~RequestRegistration()
    {
        if (m_state && m_isAccepted) {
            std::scoped_lock lock{m_state->mutex};
            m_state->requests.erase(&m_context);
        }
    }

    RequestRegistration(const RequestRegistration&) = delete;
    auto operator=(const RequestRegistration&) -> RequestRegistration& = delete;

    [[nodiscard]] bool isAccepted() const noexcept { return m_isAccepted; }

private:
    std::shared_ptr<RequestShutdownState> m_state;
    QObject& m_context;
    bool m_isAccepted{true};
};

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

#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
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
#endif

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


[[nodiscard]] std::vector<std::string> responseHeaders(const QNetworkReply& reply)
{
    std::vector<std::string> headers{};
    const auto rawHeaders = reply.rawHeaderPairs();
    headers.reserve(static_cast<std::size_t>(rawHeaders.size()));
    for (const auto& [name, value] : rawHeaders) {
        std::string header{name.constData(), static_cast<std::size_t>(name.size())};
        header.append(": ");
        header.append(value.constData(), static_cast<std::size_t>(value.size()));
        headers.push_back(std::move(header));
    }
    return headers;
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
        const ProtocolPolicy protocolPolicy,
        std::shared_ptr<RequestShutdownState> shutdownState) noexcept
        : proxyPolicy_{proxyPolicy},
          protocolPolicy_{protocolPolicy},
          m_shutdownState(std::move(shutdownState))
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

#if defined(__EMSCRIPTEN__)
        // Qt for WebAssembly uses browser fetch/XHR. A nested QEventLoop on
        // the browser's main thread cannot synchronously wait for a network
        // result; doing so can deadlock the browser event loop. All WASM
        // reads must use the non-blocking IAsyncHttpTransport instead.
        return std::unexpected(transportError(
            foundation::error::ErrorCode::Unavailable,
            "Synchronous HTTP is unavailable in WebAssembly; use the async "
            "network transport so the browser can deliver the response."));
#endif

        const QUrl url{QString::fromUtf8(
            requestUrl.data(), static_cast<qsizetype>(requestUrl.size()))};
        if (!url.isValid() || url.host().isEmpty() ||
            (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP request URL must be a valid http or https URL."));
        }

        QNetworkRequest networkRequest{url};
        // IHttpTransport is stateless: callers carry cookie state explicitly.
        // This also prevents a prior OpenProof session cookie from being mixed
        // with a later OAuth bearer credential by QNetworkAccessManager.
        networkRequest.setAttribute(
            QNetworkRequest::CookieLoadControlAttribute,
            static_cast<int>(QNetworkRequest::Manual));
        networkRequest.setAttribute(
            QNetworkRequest::CookieSaveControlAttribute,
            static_cast<int>(QNetworkRequest::Manual));
        // A signed submission must never be re-POSTed to a redirect target.
        // Also keep credentials on the explicitly selected host: provider
        // adapters may use arbitrary custom authentication header names.
        const bool carriesCustomHeaders = std::any_of(
            requestHeaders.begin(), requestHeaders.end(), [](const std::string& rawHeader) {
                const auto separator = rawHeader.find(':');
                if (separator == std::string::npos) return true;
                std::string name = rawHeader.substr(0U, separator);
                std::ranges::transform(name, name.begin(), [](const unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                return name != "accept" && name != "content-type";
            });
        networkRequest.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute,
            requestBody != nullptr || carriesCustomHeaders
                ? QNetworkRequest::ManualRedirectPolicy
                : QNetworkRequest::NoLessSafeRedirectPolicy);
        if (protocolPolicy_ == ProtocolPolicy::Http1Only) {
            networkRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        }
        auto headerStatus = applyHeaders(networkRequest, requestHeaders);
        if (!headerStatus) {
            return std::unexpected(std::move(headerStatus.error()));
        }

        std::unique_ptr<QNetworkReply> reply{};
        QEventLoop loop{};
        bool cancelled{false};
        // Registration is destroyed before the loop/reply. Holding its registry
        // lock while queuing cancellation makes completion versus stop safe;
        // deleting the loop also removes any still-queued cancellation callback.
        RequestRegistration registration{m_shutdownState, loop, [&] {
            cancelled = true;
            if (reply && reply->isRunning()) {
                // As with the deadline path, let finished() close the loop so
                // the network backend can complete abort on its owning thread.
                reply->abort();
            } else {
                loop.quit();
            }
        }};
        if (!registration.isAccepted()) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::Unavailable,
                "HTTP request stopped during application shutdown."));
        }

        const std::string configuredProxyPolicy{proxyPolicySummary(proxyPolicy_)};
        QNetworkAccessManager& manager = networkManagerFor(proxyPolicy_);
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

        QTimer timeoutTimer{};
        timeoutTimer.setSingleShot(true);

        std::string responseBody{};
        responseBody.reserve(std::min<std::size_t>(maxBodyBytes, 64U * 1024U));
        bool bodyLimitExceeded{false};
        bool timedOut{false};
#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
        QList<QSslError> sslErrors{};
#endif
        RequestStage requestStage{RequestStage::Starting};
        QElapsedTimer elapsedTimer{};
        elapsedTimer.start();

        const auto consumeAvailableBody = [&]() {
            // A timeout abort closes the underlying QSslSocket before the
            // QNetworkReply necessarily flips its QIODevice open flag. Never
            // drain after cancellation: calling readAll() in that narrow window
            // makes Qt ask a closed QSslSocket to read and emits
            // "QIODevice::read (QSslSocket): device not open".
            if (cancelled || timedOut || !reply->isReadable() || reply->bytesAvailable() <= 0) {
                return;
            }
            const QByteArray chunk = reply->readAll();
            if (bodyLimitExceeded) {
                return;
            }
            if (!appendBodyChunk(responseBody, chunk, maxBodyBytes)) {
                // Keep draining the response until QNetworkReply finishes instead
                // of aborting a live TLS socket. The caller still receives
                // BodyTooLarge below, but QSslSocket gets an orderly teardown.
                bodyLimitExceeded = true;
            }
        };

        QObject::connect(reply.get(), &QNetworkReply::socketStartedConnecting, &loop, [&]() {
            requestStage = RequestStage::Connecting;
        });
#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
        QObject::connect(reply.get(), &QNetworkReply::encrypted, &loop, [&]() {
            requestStage = RequestStage::TlsEstablished;
        });
#endif
        QObject::connect(reply.get(), &QNetworkReply::requestSent, &loop, [&]() {
            requestStage = RequestStage::RequestSent;
        });
        QObject::connect(reply.get(), &QNetworkReply::metaDataChanged, &loop, [&]() {
            requestStage = RequestStage::ResponseHeaders;
        });
        QObject::connect(reply.get(), &QNetworkReply::readyRead, &loop, [&]() {
            if (timedOut) {
                return;
            }
            requestStage = RequestStage::ResponseBody;
            consumeAvailableBody();
        });
#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
        QObject::connect(
            reply.get(),
            &QNetworkReply::sslErrors,
            &loop,
            [&](const QList<QSslError>& errors) {
                sslErrors = errors;
                // Deliberately do not call ignoreSslErrors(). Certificate
                // verification is a security invariant of this transport.
            });
#endif
        QObject::connect(reply.get(), &QNetworkReply::finished, &loop, [&]() {
            consumeAvailableBody();
            loop.quit();
        });
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            // QNetworkReply::abort() completes asynchronously and emits finished().
            // Let that signal close the local event loop so the TLS backend can
            // finish its teardown before the reply is destroyed. Quitting here
            // races QSslSocket cleanup and can produce "device not open" writes.
            if (reply->isRunning()) {
                reply->abort();
            } else {
                loop.quit();
            }
        });

        const auto timeoutCount = timeout.count();
        const auto clampedTimeout = std::clamp<std::int64_t>(
            timeoutCount,
            1,
            static_cast<std::int64_t>(std::numeric_limits<int>::max()));
        timeoutTimer.start(static_cast<int>(clampedTimeout));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        timeoutTimer.stop();

        if (cancelled) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::Unavailable,
                "HTTP request stopped during application shutdown; remote completion is unknown."));
        }
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
        if ((statusValid && statusCode == 401)
            || reply->error() == QNetworkReply::AuthenticationRequiredError) {
            // Report the failing origin without leaking path/query/credentials.
            qWarning().noquote() << "[Genycaster] Authentication required by host:"
                                  << reply->url().host() << "(HTTP status" << statusCode << ")";
        }
        if (statusValid && statusCode >= 100 && statusCode <= 65'535) {
            return farcaster::transport::http::HttpResponse{
                .statusCode = static_cast<std::uint16_t>(statusCode),
                .body = std::move(responseBody),
                .headers = responseHeaders(*reply),
            };
        }

        if (reply->error() != QNetworkReply::NoError) {
            std::string message = "Qt Network request failed: " + toUtf8(reply->errorString()) + ".";
#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
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
#else
            // In WebAssembly the browser enforces TLS and CORS; a failed
            // handshake surfaces through QNetworkReply::NetworkError.
#endif
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
    std::shared_ptr<RequestShutdownState> m_shutdownState;
};
}

class RequestShutdownScope::Impl final {
public:
    std::shared_ptr<RequestShutdownState> state{std::make_shared<RequestShutdownState>()};
};

RequestShutdownScope::RequestShutdownScope() : m_impl(std::make_unique<Impl>())
{
    std::scoped_lock lock{shutdownScopeMutex};
    if (!activeShutdownScope.expired()) {
        throw std::logic_error{"A Qt HTTP shutdown scope is already active."};
    }
    activeShutdownScope = m_impl->state;
}

RequestShutdownScope::~RequestShutdownScope()
{
    stop();
    std::scoped_lock lock{shutdownScopeMutex};
    activeShutdownScope.reset();
}

void RequestShutdownScope::stop()
{
    std::scoped_lock lock{m_impl->state->mutex};
    if (m_impl->state->isStopping) {
        return;
    }
    m_impl->state->isStopping = true;
    for (const auto& [context, cancel] : m_impl->state->requests) {
        QMetaObject::invokeMethod(context, cancel, Qt::QueuedConnection);
    }
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

#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
    if (!QSslSocket::supportsSsl()) {
        return std::unexpected(transportError(
            foundation::error::ErrorCode::TransportFailure,
            "Qt Network has no usable TLS backend. Active backend: " +
                toUtf8(QSslSocket::activeBackend()) + "."));
    }
#elif !defined(__EMSCRIPTEN__)
    // The Qt SDK itself was built without SSL. Adding OpenSSL shared objects
    // to the APK cannot enable APIs disabled when Qt Network was compiled.
    return std::unexpected(transportError(
        foundation::error::ErrorCode::Unavailable,
        "Qt Network was built without SSL/TLS support. Use an SSL-enabled "
        "Qt Network SDK; HTTPS requests are unavailable."));
#endif

    std::shared_ptr<RequestShutdownState> shutdownState{};
    {
        std::scoped_lock lock{shutdownScopeMutex};
        shutdownState = activeShutdownScope.lock();
    }
    std::unique_ptr<http::IHttpTransport> transport =
        std::make_unique<QtHttpTransport>(proxyPolicy, protocolPolicy, std::move(shutdownState));
    return transport;
}

}
