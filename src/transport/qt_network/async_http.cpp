module;

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QRandomGenerator>
#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
#include <QSslSocket>
#endif
#include <QString>
#if !defined(__EMSCRIPTEN__)
#include <QThread>
#endif
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <cctype>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

module farcaster.transport.qtnetwork.async;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.async_http;
import farcaster.transport.http;
import farcaster.transport.qtnetwork;

namespace farcaster::transport::qtnetwork::async {
namespace {

using Clock = std::chrono::steady_clock;
using Error = foundation::error::Error;
using ErrorCode = foundation::error::ErrorCode;
using Request = async_http::Request;
using Completion = async_http::Completion;
using HttpResponse = http::HttpResponse;

constexpr std::chrono::milliseconds retryBaseDelay{150};
constexpr std::chrono::milliseconds retryMaximumDelay{1'200};

[[nodiscard]] Error makeError(const ErrorCode code, std::string message)
{
    return Error::make(code, std::move(message));
}

[[nodiscard]] std::string toUtf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string{bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] bool isBackgroundPriority(const async_http::Priority priority) noexcept
{
    return priority == async_http::Priority::Background ||
        priority == async_http::Priority::Prefetch;
}

[[nodiscard]] int priorityRank(const async_http::Priority priority) noexcept
{
    switch (priority) {
    case async_http::Priority::Interactive:
        return 0;
    case async_http::Priority::Visible:
        return 1;
    case async_http::Priority::Normal:
        return 2;
    case async_http::Priority::Background:
        return 3;
    case async_http::Priority::Prefetch:
        return 4;
    }
    return 4;
}

[[nodiscard]] bool retryableStatus(const std::uint16_t statusCode) noexcept
{
    return statusCode == 408U || statusCode == 429U || statusCode == 500U ||
        statusCode == 502U || statusCode == 503U || statusCode == 504U;
}

[[nodiscard]] bool retryableNetworkError(const QNetworkReply::NetworkError error) noexcept
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::UnknownProxyError:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::vector<std::string> responseHeaders(const QNetworkReply& reply)
{
    const auto rawHeaders = reply.rawHeaderPairs();
    std::vector<std::string> headers{};
    headers.reserve(static_cast<std::size_t>(rawHeaders.size()));
    for (const auto& pair : rawHeaders) {
        const auto& name = pair.first;
        const auto& value = pair.second;
        std::string header{name.constData(), static_cast<std::size_t>(name.size())};
        header.append(": ");
        header.append(value.constData(), static_cast<std::size_t>(value.size()));
        headers.push_back(std::move(header));
    }
    return headers;
}

[[nodiscard]] std::optional<std::string_view> headerValue(
    const std::vector<std::string>& headers,
    const std::string_view requestedName)
{
    for (const auto& header : headers) {
        const auto separator = header.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        std::string_view name{header.data(), separator};
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.remove_suffix(1U);
        }
        if (name.size() != requestedName.size()) {
            continue;
        }
        bool equal = true;
        for (std::size_t index = 0; index < name.size(); ++index) {
            auto left = name[index];
            auto right = requestedName[index];
            if (left >= 'A' && left <= 'Z') {
                left = static_cast<char>(left - 'A' + 'a');
            }
            if (right >= 'A' && right <= 'Z') {
                right = static_cast<char>(right - 'A' + 'a');
            }
            if (left != right) {
                equal = false;
                break;
            }
        }
        if (!equal) {
            continue;
        }
        std::string_view value{header};
        value.remove_prefix(separator + 1U);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1U);
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.remove_suffix(1U);
        }
        return value;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::chrono::milliseconds> retryAfterDelay(
    const std::vector<std::string>& headers)
{
    const auto value = headerValue(headers, "Retry-After");
    if (!value || value->empty()) {
        return std::nullopt;
    }
    std::uint64_t seconds{};
    for (const char character : *value) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(character - '0');
        if (seconds > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) {
            return std::nullopt;
        }
        seconds = seconds * 10U + digit;
    }
    constexpr std::uint64_t maximumMilliseconds =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (seconds > maximumMilliseconds / 1'000U) {
        return std::nullopt;
    }
    return std::chrono::milliseconds{static_cast<std::int64_t>(seconds * 1'000U)};
}

[[nodiscard]] std::chrono::milliseconds jitteredBackoff(const std::size_t attempts)
{
    const auto exponent = std::min<std::size_t>(attempts > 0U ? attempts - 1U : 0U, 3U);
    const auto scaled = retryBaseDelay * static_cast<std::int64_t>(1ULL << exponent);
    const auto cap = std::min(scaled, retryMaximumDelay);
    const auto upper = static_cast<quint32>(std::clamp<std::int64_t>(
        cap.count(),
        1,
        static_cast<std::int64_t>(std::numeric_limits<quint32>::max())));
    return std::chrono::milliseconds{
        static_cast<std::int64_t>(QRandomGenerator::global()->bounded(upper))};
}

[[nodiscard]] foundation::result::Status applyHeaders(
    QNetworkRequest& request,
    const std::vector<std::string>& headers)
{
    for (const auto& header : headers) {
        if (header.find_first_of("\r\n") != std::string::npos) {
            return std::unexpected(makeError(
                ErrorCode::InvalidArgument,
                "HTTP header must not contain line breaks."));
        }
        const auto separator = header.find(':');
        if (separator == std::string::npos || separator == 0U) {
            return std::unexpected(makeError(
                ErrorCode::InvalidArgument,
                "HTTP header must use the 'Name: value' form."));
        }
        const QByteArray name{header.data(), static_cast<qsizetype>(separator)};
        const QByteArray value{
            header.data() + separator + 1U,
            static_cast<qsizetype>(header.size() - separator - 1U)};
        request.setRawHeader(name.trimmed(), value.trimmed());
    }
    return {};
}

struct RequestContext final {
    std::uint64_t id{};
    Request request{};
    Completion completion{};
    std::string origin{};
    Clock::time_point deadline{};
    std::size_t attempts{};
    bool isCancelled{false};
    bool isQueued{false};
    bool isRetryWaiting{false};
};

struct ActiveReply final {
    QPointer<QNetworkReply> reply{};
    QPointer<QTimer> timer{};
    std::string body{};
    std::size_t reservedResponseBytes{};
    bool timedOut{false};
    bool bodyLimitExceeded{false};
    bool responseBudgetContended{false};
};

class ReactorWorker final : public QObject {
public:
    ReactorWorker(
        const ProxyPolicy proxyPolicy,
        const ProtocolPolicy protocolPolicy,
        NetworkLimits limits)
        : m_proxyPolicy(proxyPolicy),
          m_protocolPolicy(protocolPolicy),
          m_limits(std::move(limits))
    {
    }

    void initialize()
    {
        m_manager = std::make_unique<QNetworkAccessManager>();
        if (m_proxyPolicy == ProxyPolicy::Direct) {
            m_manager->setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
        }
    }

    void enqueue(std::shared_ptr<RequestContext> context)
    {
        if (m_isStopping || !m_manager) {
            complete(context, std::unexpected(makeError(
                ErrorCode::Unavailable,
                "Qt asynchronous network reactor is stopping.")));
            return;
        }
        if (m_contexts.size() >= m_limits.maximumAcceptedRequests) {
            complete(context, std::unexpected(makeError(
                ErrorCode::RateLimited,
                "Qt asynchronous network admission queue is full.")));
            return;
        }
        const std::size_t bodyBytes = context->request.body.size();
        if (bodyBytes > m_limits.maximumAcceptedRequestBodyBytes ||
            m_acceptedRequestBodyBytes > m_limits.maximumAcceptedRequestBodyBytes - bodyBytes) {
            complete(context, std::unexpected(makeError(
                ErrorCode::RateLimited,
                "Qt asynchronous network request-body budget is exhausted.")));
            return;
        }

        m_acceptedRequestBodyBytes += bodyBytes;
        context->isQueued = true;
        m_contexts.emplace(context->id, context);
        m_queue.push_back(context->id);
        pump();
    }

    void cancel(const std::uint64_t id)
    {
        const auto iterator = m_contexts.find(id);
        if (iterator == m_contexts.end()) {
            return;
        }
        const auto context = iterator->second;
        context->isCancelled = true;
        if (const auto active = m_active.find(id); active != m_active.end()) {
            if (!active->second.reply) {
                finishCancelled(context);
            }
            // Image delegates are frequently recycled while a TLS reply is already
            // delivering bytes. Aborting that socket from the cancellation path can
            // race Qt's SSL backend and emit "QIODevice::read (QSslSocket): device
            // not open". Keep the reply alive, drain/discard its remaining body, and
            // complete cancellation when finished. This preserves bounded buffering
            // without tearing down a live TLS device mid-read.
            return;
        }
        if (context->isQueued || context->isRetryWaiting) {
            finishCancelled(context);
        }
    }

    [[nodiscard]] std::vector<std::shared_ptr<RequestContext>> shutdown()
    {
        m_isStopping = true;
        // Pending completions may borrow services and Qt presentation state.
        // Never invoke application callbacks while the transport is stopping.
        // Transfer request ownership back to the transport's owning thread,
        // so captured references are released after its reactor is joined.
        // Retaining the contexts (rather than vector<Completion>) also avoids
        // Emscripten libc++'s std::expected comparison constraint recursion
        // when relocating std::function completion objects.
        std::vector<std::shared_ptr<RequestContext>> abandoned{};
        abandoned.reserve(m_contexts.size());
        for (auto& [id, context] : m_contexts) {
            static_cast<void>(id);
            if (context) {
                abandoned.push_back(std::move(context));
            }
        }
        for (auto& [id, active] : m_active) {
            static_cast<void>(id);
            if (active.reply) {
                QObject::disconnect(active.reply, nullptr, this, nullptr);
                active.reply->abort();
                active.reply->deleteLater();
            }
            if (active.timer) {
                active.timer->stop();
                active.timer->deleteLater();
            }
        }
        m_active.clear();
        m_queue.clear();
        m_contexts.clear();
        m_originActive.clear();
        m_originBackgroundActive.clear();
        m_activeBackground = 0U;
        m_bufferedResponseBytes = 0U;
        m_acceptedRequestBodyBytes = 0U;
        m_manager.reset();
        return abandoned;
    }

private:
    [[nodiscard]] bool canStart(const RequestContext& context) const
    {
        if (m_active.size() >= m_limits.maximumActiveRequests) {
            return false;
        }
        const auto originCount = m_originActive.contains(context.origin)
            ? m_originActive.at(context.origin) : 0U;
        if (originCount >= m_limits.maximumActiveRequestsPerOrigin) {
            return false;
        }
        if (!isBackgroundPriority(context.request.priority)) {
            return true;
        }
        if (m_activeBackground >= m_limits.maximumBackgroundActiveRequests) {
            return false;
        }
        const auto backgroundOriginCount = m_originBackgroundActive.contains(context.origin)
            ? m_originBackgroundActive.at(context.origin) : 0U;
        return backgroundOriginCount < m_limits.maximumBackgroundActiveRequestsPerOrigin;
    }

    void pump()
    {
        while (!m_queue.empty() && m_active.size() < m_limits.maximumActiveRequests) {
            auto selected = m_queue.end();
            int selectedRank = std::numeric_limits<int>::max();
            for (auto iterator = m_queue.begin(); iterator != m_queue.end(); ++iterator) {
                const auto contextIterator = m_contexts.find(*iterator);
                if (contextIterator == m_contexts.end()) {
                    continue;
                }
                const auto& context = *contextIterator->second;
                if (context.isCancelled) {
                    continue;
                }
                if (!canStart(context)) {
                    continue;
                }
                const int rank = priorityRank(context.request.priority);
                if (rank < selectedRank) {
                    selected = iterator;
                    selectedRank = rank;
                    if (rank == 0) {
                        break;
                    }
                }
            }
            if (selected == m_queue.end()) {
                break;
            }
            const auto id = *selected;
            m_queue.erase(selected);
            const auto contextIterator = m_contexts.find(id);
            if (contextIterator == m_contexts.end()) {
                continue;
            }
            const auto context = contextIterator->second;
            if (context->isCancelled) {
                finishCancelled(context);
                continue;
            }
            context->isQueued = false;
            startAttempt(context);
        }
    }

    void startAttempt(const std::shared_ptr<RequestContext>& context)
    {
        if (Clock::now() >= context->deadline) {
            complete(context, std::unexpected(makeError(
                ErrorCode::Timeout,
                "HTTP operation deadline expired before another attempt could start.")));
            return;
        }

        const QUrl url{QString::fromUtf8(
            context->request.url.data(),
            static_cast<qsizetype>(context->request.url.size()))};
        QNetworkRequest networkRequest{url};
        // Provider credentials have no prescribed header name. Treat all
        // non-standard headers as potentially sensitive and never follow a
        // redirect with them, regardless of the selected read provider.
        const bool credentialedApiRequest = std::any_of(
            context->request.headers.begin(), context->request.headers.end(),
            [](const std::string& header) {
                const auto separator = header.find(':');
                if (separator == std::string::npos) return true;
                std::string name = header.substr(0U, separator);
                std::ranges::transform(name, name.begin(), [](const unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                return name != "accept" && name != "content-type";
            });
        networkRequest.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute,
            credentialedApiRequest ? QNetworkRequest::ManualRedirectPolicy
                                   : QNetworkRequest::NoLessSafeRedirectPolicy);
        if (m_protocolPolicy == ProtocolPolicy::Http1Only) {
            networkRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        }
        if (auto status = applyHeaders(networkRequest, context->request.headers); !status) {
            complete(context, std::unexpected(std::move(status.error())));
            return;
        }

        QNetworkReply* reply = nullptr;
        const QByteArray payload = context->request.body.empty()
            ? QByteArray{}
            : QByteArray::fromRawData(
                  context->request.body.data(),
                  static_cast<qsizetype>(context->request.body.size()));
        switch (context->request.method) {
        case async_http::Method::Head:
            reply = m_manager->head(networkRequest);
            break;
        case async_http::Method::Get:
            reply = m_manager->get(networkRequest);
            break;
        case async_http::Method::Post:
            reply = m_manager->post(networkRequest, payload);
            break;
        case async_http::Method::Put:
            reply = m_manager->put(networkRequest, payload);
            break;
        case async_http::Method::Delete:
            reply = context->request.body.empty()
                ? m_manager->deleteResource(networkRequest)
                : m_manager->sendCustomRequest(networkRequest, QByteArrayLiteral("DELETE"), payload);
            break;
        }
        if (reply == nullptr) {
            complete(context, std::unexpected(makeError(
                ErrorCode::TransportFailure,
                "Qt Network could not create an asynchronous HTTP reply.")));
            return;
        }

        ++context->attempts;
        auto& active = m_active[context->id];
        active.reply = reply;
        active.body.reserve(std::min<std::size_t>(context->request.maxBodyBytes, 64U * 1024U));
        ++m_originActive[context->origin];
        if (isBackgroundPriority(context->request.priority)) {
            ++m_activeBackground;
            ++m_originBackgroundActive[context->origin];
        }

        reply->setReadBufferSize(static_cast<qint64>(std::min<std::size_t>(
            m_limits.replyReadBufferBytes,
            static_cast<std::size_t>(std::numeric_limits<qint64>::max()))));

        auto* timer = new QTimer{this};
        timer->setSingleShot(true);
        active.timer = timer;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            context->deadline - Clock::now());
        const auto milliseconds = std::clamp<std::int64_t>(
            remaining.count(),
            1,
            static_cast<std::int64_t>(std::numeric_limits<int>::max()));
        timer->start(static_cast<int>(milliseconds));

        QObject::connect(timer, &QTimer::timeout, this, [this, id = context->id] {
            const auto iterator = m_active.find(id);
            if (iterator == m_active.end()) {
                return;
            }
            iterator->second.timedOut = true;
            if (iterator->second.reply) {
                iterator->second.reply->abort();
            }
        });
        QObject::connect(reply, &QNetworkReply::metaDataChanged, this, [this, id = context->id] {
            const auto contextIterator = m_contexts.find(id);
            const auto activeIterator = m_active.find(id);
            if (contextIterator == m_contexts.end() || activeIterator == m_active.end() ||
                !activeIterator->second.reply) {
                return;
            }
            bool valid = false;
            const qlonglong contentLength = activeIterator->second.reply
                ->header(QNetworkRequest::ContentLengthHeader).toLongLong(&valid);
            if (valid && contentLength >= 0 &&
                static_cast<std::uint64_t>(contentLength) >
                    static_cast<std::uint64_t>(contextIterator->second->request.maxBodyBytes)) {
                activeIterator->second.bodyLimitExceeded = true;
                activeIterator->second.reply->abort();
            }
        });
        if (context->request.uploadProgress) {
            QObject::connect(
                reply,
                &QNetworkReply::uploadProgress,
                this,
                [progress = context->request.uploadProgress](const qint64 sent, const qint64 total) {
                    const auto safeSent = static_cast<std::uint64_t>(std::max<qint64>(0, sent));
                    const auto safeTotal = static_cast<std::uint64_t>(std::max<qint64>(0, total));
                    progress(safeSent, safeTotal);
                });
        }
        QObject::connect(reply, &QNetworkReply::readyRead, this, [this, id = context->id] {
            consume(id);
        });
        QObject::connect(reply, &QNetworkReply::finished, this, [this, id = context->id] {
            finishAttempt(id);
        });
    }

    void consume(const std::uint64_t id)
    {
        const auto contextIterator = m_contexts.find(id);
        const auto activeIterator = m_active.find(id);
        if (contextIterator == m_contexts.end() || activeIterator == m_active.end()) {
            return;
        }
        auto& active = activeIterator->second;
        // A timeout abort (or an interrupted connection) closes the underlying
        // QIODevice before the reply necessarily flips its open flag. Avoid reads
        // after timeout; normal caller cancellation is instead drained below so
        // Qt can tear down the TLS device in order.
        if (!active.reply || active.timedOut || !active.reply->isOpen()
            || !active.reply->isReadable() || active.reply->bytesAvailable() <= 0) {
            return;
        }
        const QByteArray bytes = active.reply->readAll();
        if (bytes.isEmpty()) {
            return;
        }
        // Cancelled requests and already-over-budget responses are drained only.
        // No body bytes are retained after the caller has stopped caring.
        if (contextIterator->second->isCancelled || active.bodyLimitExceeded) {
            return;
        }
        const auto chunkBytes = static_cast<std::size_t>(bytes.size());
        const auto maximumBodyBytes = contextIterator->second->request.maxBodyBytes;
        const bool perRequestExceeded = chunkBytes > maximumBodyBytes ||
            active.body.size() > maximumBodyBytes - chunkBytes;
        const bool processExceeded = chunkBytes > m_limits.maximumBufferedResponseBytes ||
            m_bufferedResponseBytes > m_limits.maximumBufferedResponseBytes - chunkBytes;
        if (perRequestExceeded || processExceeded) {
            active.bodyLimitExceeded = true;
            // A response that cannot fit even by itself is not retryable.
            // Only contention with concurrent responses is temporary.
            active.responseBudgetContended = !perRequestExceeded && processExceeded
                && chunkBytes <= m_limits.maximumBufferedResponseBytes
                && active.body.size() <= m_limits.maximumBufferedResponseBytes - chunkBytes;
            active.reply->abort();
            return;
        }
        active.body.append(bytes.constData(), chunkBytes);
        active.reservedResponseBytes += chunkBytes;
        m_bufferedResponseBytes += chunkBytes;
    }

    void finishAttempt(const std::uint64_t id)
    {
        const auto contextIterator = m_contexts.find(id);
        auto activeIterator = m_active.find(id);
        if (contextIterator == m_contexts.end() || activeIterator == m_active.end()) {
            return;
        }
        const auto context = contextIterator->second;
        consume(id);
        ActiveReply active = std::move(activeIterator->second);
        m_active.erase(activeIterator);
        releaseActiveCounts(*context);
        if (active.timer) {
            active.timer->stop();
            active.timer->deleteLater();
        }

        QNetworkReply* reply = active.reply.data();
        const QVariant statusAttribute = reply
            ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
            : QVariant{};
        bool statusValid = false;
        const int statusCode = statusAttribute.toInt(&statusValid);
        const auto networkError = reply ? reply->error() : QNetworkReply::UnknownNetworkError;
        const auto headers = reply ? responseHeaders(*reply) : std::vector<std::string>{};
        const QString errorString = reply ? reply->errorString() : QStringLiteral("Unknown network failure");
        // Qt's generic HTTP/2 warning omits the host. Emit only the hostname
        // and status here: never log query strings, paths, cookies or tokens.
        if (reply && (networkError == QNetworkReply::ProtocolFailure
                      || networkError == QNetworkReply::InternalServerError)) {
            // A Qt HTTP/2 RST_STREAM diagnostic contains only a stream number.
            // Log the host and status to distinguish remote service failures
            // from image/CDN responses without exposing URL path, query or
            // authorization headers.
            qWarning().noquote() << "[Genycaster] Network protocol failure at host:"
                                  << reply->url().host() << "(HTTP status" << statusCode << ")";
        }
        if (reply && ((statusValid && statusCode == 401)
                      || networkError == QNetworkReply::AuthenticationRequiredError)) {
            qWarning().noquote() << "[Genycaster] Authentication required by host:"
                                  << reply->url().host() << "(HTTP status" << statusCode << ")";
        }
        if (reply) {
            reply->deleteLater();
        }

        m_bufferedResponseBytes -= active.reservedResponseBytes;

        if (context->isCancelled) {
            finishCancelled(context);
            pump();
            return;
        }
        if (active.bodyLimitExceeded) {
            complete(context, std::unexpected(makeError(
                active.responseBudgetContended ? ErrorCode::RateLimited : ErrorCode::Unavailable,
                active.responseBudgetContended
                    ? "HTTP response buffer budget is temporarily contended."
                    : "HTTP response exceeded the configured memory/body budget.")));
            pump();
            return;
        }
        if (active.timedOut || Clock::now() >= context->deadline) {
            if (shouldRetry(*context, 0U, QNetworkReply::TimeoutError)) {
                scheduleRetry(context, jitteredBackoff(context->attempts));
            } else {
                complete(context, std::unexpected(makeError(
                    ErrorCode::Timeout,
                    "HTTP operation deadline expired.")));
            }
            pump();
            return;
        }

        // An HTTP status is authoritative even when Qt maps 4xx/5xx to a
        // QNetworkReply::NetworkError. This prevents HTTP failures from being
        // collapsed into TransportFailure diagnostics.
        if (statusValid && statusCode >= 100 && statusCode <= 65'535) {
            const auto normalizedStatus = static_cast<std::uint16_t>(statusCode);
            if (shouldRetry(*context, normalizedStatus, QNetworkReply::NoError)) {
                const auto delay = retryAfterDelay(headers).value_or(
                    jitteredBackoff(context->attempts));
                scheduleRetry(context, delay);
            } else {
                complete(context, HttpResponse{
                    .statusCode = normalizedStatus,
                    .body = std::move(active.body),
                    .headers = headers,
                });
            }
            pump();
            return;
        }

        if (networkError != QNetworkReply::NoError) {
            if (shouldRetry(*context, 0U, networkError)) {
                scheduleRetry(context, jitteredBackoff(context->attempts));
            } else {
                const auto code = networkError == QNetworkReply::TimeoutError
                    ? ErrorCode::Timeout : ErrorCode::TransportFailure;
                complete(context, std::unexpected(makeError(
                    code,
                    "Qt asynchronous network request failed: " + toUtf8(errorString) + ".")));
            }
            pump();
            return;
        }

        complete(context, std::unexpected(makeError(
            ErrorCode::TransportFailure,
            "Qt asynchronous network request completed without an HTTP status.")));
        pump();
    }

    [[nodiscard]] bool shouldRetry(
        const RequestContext& context,
        const std::uint16_t statusCode,
        const QNetworkReply::NetworkError networkError) const noexcept
    {
        if (context.request.retryPolicy != async_http::RetryPolicy::TransientRead ||
            context.attempts >= std::max<std::size_t>(context.request.maximumAttempts, 1U) ||
            Clock::now() >= context.deadline) {
            return false;
        }
        if (statusCode != 0U) {
            return retryableStatus(statusCode);
        }
        return retryableNetworkError(networkError);
    }

    void scheduleRetry(
        const std::shared_ptr<RequestContext>& context,
        std::chrono::milliseconds requestedDelay)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            context->deadline - Clock::now());
        if (remaining <= std::chrono::milliseconds::zero() || requestedDelay >= remaining) {
            complete(context, std::unexpected(makeError(
                ErrorCode::Timeout,
                "HTTP retry budget expired.")));
            return;
        }
        requestedDelay = std::max(requestedDelay, std::chrono::milliseconds::zero());
        context->isRetryWaiting = true;
        const auto milliseconds = std::clamp<std::int64_t>(
            requestedDelay.count(),
            0,
            static_cast<std::int64_t>(std::numeric_limits<int>::max()));
        QTimer::singleShot(static_cast<int>(milliseconds), this, [this, id = context->id] {
            const auto iterator = m_contexts.find(id);
            if (iterator == m_contexts.end()) {
                return;
            }
            const auto retryContext = iterator->second;
            retryContext->isRetryWaiting = false;
            if (retryContext->isCancelled) {
                finishCancelled(retryContext);
                return;
            }
            retryContext->isQueued = true;
            m_queue.push_back(id);
            pump();
        });
    }

    void releaseActiveCounts(const RequestContext& context)
    {
        if (auto iterator = m_originActive.find(context.origin); iterator != m_originActive.end()) {
            if (iterator->second > 0U) {
                --iterator->second;
            }
            if (iterator->second == 0U) {
                m_originActive.erase(iterator);
            }
        }
        if (isBackgroundPriority(context.request.priority)) {
            if (m_activeBackground > 0U) {
                --m_activeBackground;
            }
            if (auto iterator = m_originBackgroundActive.find(context.origin);
                iterator != m_originBackgroundActive.end()) {
                if (iterator->second > 0U) {
                    --iterator->second;
                }
                if (iterator->second == 0U) {
                    m_originBackgroundActive.erase(iterator);
                }
            }
        }
    }

    void finishCancelled(const std::shared_ptr<RequestContext>& context)
    {
        complete(context, std::unexpected(makeError(
            ErrorCode::Unavailable,
            "HTTP operation was cancelled.")));
        pump();
    }

    void complete(
        const std::shared_ptr<RequestContext>& context,
        foundation::result::Result<HttpResponse> result)
    {
        if (!context) {
            return;
        }
        const auto iterator = m_contexts.find(context->id);
        if (iterator != m_contexts.end()) {
            const auto bodyBytes = iterator->second->request.body.size();
            if (bodyBytes <= m_acceptedRequestBodyBytes) {
                m_acceptedRequestBodyBytes -= bodyBytes;
            } else {
                m_acceptedRequestBodyBytes = 0U;
            }
            m_contexts.erase(iterator);
        }
        context->isQueued = false;
        context->isRetryWaiting = false;
        auto completion = std::move(context->completion);
        // Release potentially large request bodies before invoking arbitrary
        // application code from the reactor thread.
        std::string{}.swap(context->request.body);
        if (completion) {
#if defined(__cpp_exceptions)
            try {
                completion(std::move(result));
            } catch (...) {
                // A consumer callback cannot unwind out of Qt's network
                // reactor event loop and strand all unrelated requests.
                // Context and byte accounting were released above.
            }
#else
            completion(std::move(result));
#endif
        }
    }

    ProxyPolicy m_proxyPolicy{ProxyPolicy::System};
    ProtocolPolicy m_protocolPolicy{ProtocolPolicy::Automatic};
    NetworkLimits m_limits{};
    std::unique_ptr<QNetworkAccessManager> m_manager{};
    std::unordered_map<std::uint64_t, std::shared_ptr<RequestContext>> m_contexts{};
    std::unordered_map<std::uint64_t, ActiveReply> m_active{};
    std::deque<std::uint64_t> m_queue{};
    std::unordered_map<std::string, std::size_t> m_originActive{};
    std::unordered_map<std::string, std::size_t> m_originBackgroundActive{};
    std::size_t m_activeBackground{};
    std::size_t m_acceptedRequestBodyBytes{};
    std::size_t m_bufferedResponseBytes{};
    bool m_isStopping{false};
};

struct SharedCancellationState final {
    std::mutex mutex{};
    QPointer<ReactorWorker> worker{};
    bool isAccepting{true};
};

class AsyncQtHttpTransport final : public async_http::IAsyncHttpTransport {
public:
    AsyncQtHttpTransport(
        const ProxyPolicy proxyPolicy,
        const ProtocolPolicy protocolPolicy,
        NetworkLimits limits)
        : m_shared(std::make_shared<SharedCancellationState>()),
#if !defined(__EMSCRIPTEN__)
          m_thread(std::make_unique<QThread>()),
#endif
          m_worker(new ReactorWorker(proxyPolicy, protocolPolicy, std::move(limits)))
    {
#if !defined(__EMSCRIPTEN__)
        m_thread->setObjectName(QStringLiteral("GenycasterNetworkReactor"));
        m_worker->moveToThread(m_thread.get());
        QObject::connect(m_thread.get(), &QThread::finished, m_worker, &QObject::deleteLater);
#endif
        {
            std::scoped_lock lock{m_shared->mutex};
            m_shared->worker = m_worker;
        }
#if defined(__EMSCRIPTEN__)
        // Browser networking must remain on the JS/Qt main event loop:
        // neither pthread creation nor BlockingQueuedConnection is safe in
        // the single-threaded Qt WebAssembly configuration.
        m_worker->initialize();
#else
        m_thread->start();
        QMetaObject::invokeMethod(
            m_worker,
            [worker = m_worker] { worker->initialize(); },
            Qt::BlockingQueuedConnection);
#endif
    }

    ~AsyncQtHttpTransport() override
    {
        {
            std::scoped_lock lock{m_shared->mutex};
            m_shared->isAccepting = false;
        }
        // Keep pending closures alive on the owner thread until after the
        // reactor thread is joined. They must not run during teardown.
        std::vector<std::shared_ptr<RequestContext>> abandoned{};
#if defined(__EMSCRIPTEN__)
        if (m_worker) {
            abandoned = m_worker->shutdown();
        }
#else
        if (m_worker && m_thread && m_thread->isRunning()) {
            QMetaObject::invokeMethod(
                m_worker,
                [worker = m_worker, &abandoned] { abandoned = worker->shutdown(); },
                Qt::BlockingQueuedConnection);
        }
#endif
        {
            std::scoped_lock lock{m_shared->mutex};
            m_shared->worker = nullptr;
        }
#if defined(__EMSCRIPTEN__)
        delete m_worker;
#else
        if (m_thread) {
            m_thread->quit();
            m_thread->wait();
        }
#endif
        m_worker = nullptr;
    }

    [[nodiscard]] foundation::result::Result<async_http::Operation>
    start(Request request, Completion completion) override
    {
        if (!completion) {
            return std::unexpected(makeError(
                ErrorCode::InvalidArgument,
                "Asynchronous HTTP completion must not be empty."));
        }
        if (request.url.empty() || request.timeout <= std::chrono::milliseconds::zero() ||
            request.maxBodyBytes == 0U) {
            return std::unexpected(makeError(
                ErrorCode::InvalidArgument,
                "Asynchronous HTTP URL, deadline and body limit must be valid."));
        }
        const QUrl url{QString::fromUtf8(
            request.url.data(), static_cast<qsizetype>(request.url.size()))};
        if (!url.isValid() || url.host().isEmpty() ||
            (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
            return std::unexpected(makeError(
                ErrorCode::InvalidArgument,
                "Asynchronous HTTP URL must be a valid http or https URL."));
        }

        QPointer<ReactorWorker> worker{};
        {
            std::scoped_lock lock{m_shared->mutex};
            if (!m_shared->isAccepting || !m_shared->worker) {
                return std::unexpected(makeError(
                    ErrorCode::Unavailable,
                    "Qt asynchronous network reactor is not accepting requests."));
            }
            worker = m_shared->worker;
        }

        const auto id = m_nextId.fetch_add(1U, std::memory_order_relaxed);
        auto context = std::make_shared<RequestContext>();
        context->id = id;
        context->origin = toUtf8(url.scheme()) + "://" + toUtf8(url.host()) + ":" +
            std::to_string(url.port(url.scheme() == QStringLiteral("https") ? 443 : 80));
        context->deadline = Clock::now() + request.timeout;
        context->request = std::move(request);
        context->request.maximumAttempts = std::max<std::size_t>(context->request.maximumAttempts, 1U);
        context->completion = std::move(completion);

        if (!QMetaObject::invokeMethod(
                worker,
                [worker, context] {
                    if (worker) {
                        worker->enqueue(context);
                    }
                },
                Qt::QueuedConnection)) {
            return std::unexpected(makeError(
                ErrorCode::Unavailable,
                "Qt could not enqueue the asynchronous HTTP operation."));
        }

        const std::weak_ptr<SharedCancellationState> weakState = m_shared;
        return async_http::Operation{[weakState, id] {
            const auto shared = weakState.lock();
            if (!shared) {
                return;
            }
            QPointer<ReactorWorker> cancellationWorker{};
            {
                std::scoped_lock lock{shared->mutex};
                if (!shared->isAccepting || !shared->worker) {
                    return;
                }
                cancellationWorker = shared->worker;
            }
            QMetaObject::invokeMethod(
                cancellationWorker,
                [cancellationWorker, id] {
                    if (cancellationWorker) {
                        cancellationWorker->cancel(id);
                    }
                },
                Qt::QueuedConnection);
        }};
    }

private:
    std::shared_ptr<SharedCancellationState> m_shared{};
#if !defined(__EMSCRIPTEN__)
    std::unique_ptr<QThread> m_thread{};
#endif
    ReactorWorker* m_worker{};
    std::atomic<std::uint64_t> m_nextId{1U};
};

[[nodiscard]] foundation::result::Status validateLimits(const NetworkLimits& limits)
{
    if (limits.maximumActiveRequests == 0U ||
        limits.maximumActiveRequestsPerOrigin == 0U ||
        limits.maximumAcceptedRequests < limits.maximumActiveRequests ||
        limits.maximumBufferedResponseBytes == 0U ||
        limits.maximumAcceptedRequestBodyBytes == 0U ||
        limits.replyReadBufferBytes == 0U) {
        return std::unexpected(makeError(
            ErrorCode::InvalidArgument,
            "Qt asynchronous network limits must be non-zero and internally consistent."));
    }
    return {};
}

}

foundation::result::Result<std::unique_ptr<async_http::IAsyncHttpTransport>>
makeAsyncTransport(
    const ProxyPolicy proxyPolicy,
    const ProtocolPolicy protocolPolicy,
    NetworkLimits limits)
{
    if (QCoreApplication::instance() == nullptr) {
        return std::unexpected(makeError(
            ErrorCode::TransportFailure,
            "Qt asynchronous network transport requires an active QCoreApplication."));
    }
    if (auto status = validateLimits(limits); !status) {
        return std::unexpected(std::move(status.error()));
    }
    if (proxyPolicy == ProxyPolicy::System) {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
    }
#if !defined(__EMSCRIPTEN__) && QT_CONFIG(ssl)
    if (!QSslSocket::supportsSsl()) {
        return std::unexpected(makeError(
            ErrorCode::TransportFailure,
            "Qt Network has no usable TLS backend. Active backend: " +
                toUtf8(QSslSocket::activeBackend()) + "."));
    }
#elif !defined(__EMSCRIPTEN__)
    // Never create an HTTPS-capable-looking transport without TLS support.
    return std::unexpected(makeError(
        ErrorCode::Unavailable,
        "Qt Network was built without SSL/TLS support. Use an SSL-enabled "
        "Qt Network SDK; HTTPS requests are unavailable."));
#endif
    std::unique_ptr<async_http::IAsyncHttpTransport> transport =
        std::make_unique<AsyncQtHttpTransport>(proxyPolicy, protocolPolicy, std::move(limits));
    return transport;
}

}
