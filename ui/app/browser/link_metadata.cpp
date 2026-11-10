#include "genycaster_deployment.hpp"
#include "link_metadata.hpp"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace {
constexpr qsizetype maximumQueued{32};
constexpr qsizetype maximumActive{2};
constexpr qsizetype maximumCached{192};
constexpr qsizetype maximumResponseBytes{96 * 1024};
constexpr qint64 verifiedFreshMs{3 * 60 * 1000};
constexpr qint64 unavailableRetryMs{15 * 1000};
constexpr int requestDeadlineMs{7 * 1000};
// A cold Mini App lookup may require manifest + one or two same-origin
// redirects. Give that path more time without slowing normal link previews.
constexpr int miniAppRequestDeadlineMs{12 * 1000};

[[nodiscard]] bool safeHost(const QUrl& url)
{
    if (url.scheme() != QLatin1String("https")
        || !url.isValid() || url.host().isEmpty()
        || !url.userName().isEmpty() || !url.password().isEmpty()
        || url.port() != -1) {
        return false;
    }
    const QString host = url.host().toLower();
    if (host == QLatin1String("localhost")
        || host.endsWith(QLatin1String(".localhost"))
        || host.endsWith(QLatin1String(".local"))
        || host.endsWith(QLatin1String(".internal"))) {
        return false;
    }
    const QHostAddress literal{host};
    // Reject literal private addresses: only the preconfigured public relay
    // receives a URL. The gateway independently enforces its SSRF policy.
    if (!literal.isNull()
        && (literal.isLoopback() || literal.isLinkLocal()
            || literal.isInSubnet(QHostAddress{QStringLiteral("10.0.0.0")}, 8)
            || literal.isInSubnet(QHostAddress{QStringLiteral("172.16.0.0")}, 12)
            || literal.isInSubnet(QHostAddress{QStringLiteral("192.168.0.0")}, 16))) {
        return false;
    }
    return true;
}
}

BrowserLinkMetadataController::BrowserLinkMetadataController(
    const bool useDirectProxy, const bool enabled, QObject* parent, QUrl relayBase)
    : QObject(parent),
      m_relayBase(relayBase.isValid() && !relayBase.isEmpty()
          ? std::move(relayBase)
          : QUrl{QString::fromUtf8(genycaster::deployment::browserGateway.data())}),
      m_enabled(enabled)
{
    if (useDirectProxy) m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
    m_clock.start();
}

BrowserLinkMetadataController::~BrowserLinkMetadataController()
{
    for (auto* reply : std::as_const(m_active)) {
        if (!reply) continue;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

BrowserLinkMetadataController::Request BrowserLinkMetadataController::normalized(
    const QString& pageUrl, const QString& kind)
{
    Request req{};
    if ((kind != QLatin1String("link") && kind != QLatin1String("miniapp"))
        || pageUrl.trimmed().size() > 2048) return req;
    QUrl url{pageUrl.trimmed()};
    if (!safeHost(url)) return req;
    url.setFragment({});
    // One canonical page identity. Mini App lookup uses the origin URL
    // to match the gateway contract, but the signal still names the page.
    // QML compares the original page identity, while the request key uses
    // normalized URI encoding for cross-delegate deduplication.
    req.url = pageUrl.trimmed().section(QLatin1Char('#'), 0, 0);
    req.kind = kind;
    req.key = kind + QLatin1Char(':') + url.toString(QUrl::FullyEncoded);
    return req;
}

QVariantMap BrowserLinkMetadataController::previewFor(
    const QString& pageUrl, const QString& kind) const
{
    const Request request = normalized(pageUrl, kind);
    const auto found = m_cache.constFind(request.key);
    return found != m_cache.cend() && found->freshUntil > m_clock.elapsed()
        ? found->data : QVariantMap{};
}

void BrowserLinkMetadataController::store(
    const QString& key, QVariantMap data, const qint64 ttl)
{
    while (m_cache.size() >= maximumCached && !m_cache.contains(key)) {
        m_cache.erase(m_cache.begin());
    }
    m_cache.insert(key, CacheEntry{
        .data = std::move(data), .freshUntil = m_clock.elapsed() + ttl});
}

void BrowserLinkMetadataController::requestPreview(
    const QString& pageUrl, const QString& kind)
{
    if (!m_enabled) return;
    const Request request = normalized(pageUrl, kind);
    if (request.key.isEmpty()) return;
    if (const auto found = m_cache.constFind(request.key);
        found != m_cache.cend() && found->freshUntil > m_clock.elapsed()) {
        // The cached callback is queued to preserve an identical asynchronous
        // completion contract for pooled/reused delegates.
        const QVariantMap data = found->data;
        QTimer::singleShot(0, this, [this, request, data] {
            emit previewReady(request.url, request.kind, data);
        });
        return;
    }
    if (m_active.contains(request.key)
        || std::ranges::any_of(m_pending, [&](const Request& value) {
            return value.key == request.key;
        })) return;
    if (m_pending.size() >= maximumQueued) {
        // The newest viewport takes precedence, but an evicted consumer still
        // receives a terminal unavailable result instead of being stranded.
        // No negative metadata is cached; reuse can re-admit this URL.
        const Request obsolete = m_pending.dequeue();
        QTimer::singleShot(0, this, [this, obsolete] {
            emit previewReady(obsolete.url, obsolete.kind, {});
        });
    }
    m_pending.enqueue(request);
    dispatch();
}

void BrowserLinkMetadataController::dispatch()
{
    while (m_active.size() < maximumActive && !m_pending.isEmpty()) {
        Request request = m_pending.dequeue();
        QUrl relay{m_relayBase};
        relay.setPath(request.kind == QLatin1String("miniapp")
            ? QStringLiteral("/v1/browser/miniapp-preview")
            : QStringLiteral("/v1/browser/link-preview"));
        const QUrl original{request.url};
        QUrlQuery query{};
        // The shared cast URL carries per-post fc:miniapp metadata, often with
        // an image keyed by query parameters. The gateway separately reads the
        // root manifest for verified app identity and safe image leases.
        query.addQueryItem(QStringLiteral("url"),request.url);
        relay.setQuery(query);
        QNetworkRequest networkRequest{relay};
        networkRequest.setRawHeader("Accept", "application/json");
        networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
            QNetworkRequest::ManualRedirectPolicy);
        const int deadlineMs = request.kind == QLatin1String("miniapp")
            ? miniAppRequestDeadlineMs : requestDeadlineMs;
        networkRequest.setTransferTimeout(deadlineMs);
        QNetworkReply* reply = m_network.get(networkRequest);
        // Limit Qt's socket receive buffer as well as parsed JSON size.
        reply->setReadBufferSize(maximumResponseBytes + 1);
        m_active.insert(request.key, reply);
        QTimer::singleShot(deadlineMs, reply, [reply] {
            if (reply->isRunning()) reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this,
            [this, reply, request = std::move(request)]() mutable {
                complete(reply, std::move(request));
            });
    }
}

void BrowserLinkMetadataController::complete(QNetworkReply* reply, Request request)
{
    if (m_active.value(request.key) != reply) return;
    m_active.remove(request.key);
    QVariantMap value{};
    const auto claimed = reply->header(QNetworkRequest::ContentLengthHeader);
    const qint64 contentLength = claimed.isValid() ? claimed.toLongLong() : -1;
    const bool usable = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
        && reply->header(QNetworkRequest::ContentTypeHeader)
            .toString().startsWith(QLatin1String("application/json"), Qt::CaseInsensitive)
        && (contentLength < 0 || contentLength <= maximumResponseBytes)
        && reply->bytesAvailable() <= maximumResponseBytes;
    if (usable) {
        const QJsonDocument parsed = QJsonDocument::fromJson(
            reply->read(maximumResponseBytes + 1));
        if (parsed.isObject()) {
            const QJsonObject object = parsed.object();
            const QString domain = object.value(QStringLiteral("domain")).toString()
                .trimmed().toLower();
            const QString originHost = QUrl{request.url}.host().toLower();
            const QString normalizedDomain = domain.startsWith(QLatin1String("www."))
                ? domain.mid(4) : domain;
            const QString normalizedHost = originHost.startsWith(QLatin1String("www."))
                ? originHost.mid(4) : originHost;
            const bool accepted = object.value(QStringLiteral("resolved")).toBool()
                && !domain.isEmpty() && normalizedDomain == normalizedHost
                && (request.kind != QLatin1String("miniapp")
                    || object.value(QStringLiteral("providerKind")).toString()
                        == QLatin1String("miniapp"));
            if (accepted) value = object.toVariantMap();
        }
    }
    reply->deleteLater();
    // A valid app manifest with an unreachable per-cast page is provisional.
    // The QML card can render its fallback, but retry after the short
    // negative cooldown instead of caching incomplete art for three minutes.
    store(request.key, value, value.isEmpty()
        || value.value(QStringLiteral("castMetadataPending")).toBool()
        ? unavailableRetryMs : verifiedFreshMs);
    emit previewReady(request.url, request.kind, value);
    dispatch();
}
