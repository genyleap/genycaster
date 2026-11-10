#include "genycaster_deployment.hpp"
#include "token_preview.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace {
constexpr qsizetype maximumPending{16};
constexpr qsizetype maximumEntries{128};
constexpr qsizetype maximumResponseBytes{32 * 1024};
constexpr qint64 freshForMs{2 * 60 * 1000};
constexpr qint64 retryAfterMs{15 * 1000};
constexpr int deadlineMs{7 * 1000};
}

BrowserTokenPreviewController::BrowserTokenPreviewController(
    bool useDirectProxy, bool enabled, QObject* parent, QUrl gatewayBase)
    : QObject(parent),
      m_gatewayBase(gatewayBase.isValid() && !gatewayBase.isEmpty()
          ? std::move(gatewayBase)
          : QUrl{QString::fromUtf8(genycaster::deployment::browserGateway.data())}),
      m_enabled(enabled)
{
    if (useDirectProxy) m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
    m_clock.start();
}

BrowserTokenPreviewController::~BrowserTokenPreviewController()
{
    for (auto* reply : std::as_const(m_active)) {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

QString BrowserTokenPreviewController::normalize(const QString& locator)
{
    const QString text = locator.trimmed().toLower();
    static const QRegularExpression pattern{
        QStringLiteral(R"(^(base|eth):(0x[0-9a-f]{40})$)")};
    return pattern.match(text).hasMatch() ? text : QString{};
}

QVariantMap BrowserTokenPreviewController::tokenFor(const QString& locator) const
{
    const auto id = normalize(locator);
    const auto existing = m_cache.constFind(id);
    // The latest verified snapshot remains useful offline even after its
    // refresh TTL has expired. TTL controls network admission, not visibility.
    return existing == m_cache.cend() ? QVariantMap{} : existing->snapshot;
}

void BrowserTokenPreviewController::retain(
    const QString& locator, QVariantMap snapshot)
{
    while (m_cache.size() >= maximumEntries && !m_cache.contains(locator)) {
        m_cache.erase(m_cache.begin());
    }
    m_cache.insert(locator, Stored{
        .snapshot = std::move(snapshot), .freshUntil = m_clock.elapsed() + freshForMs});
    m_retryAfter.remove(locator);
}

void BrowserTokenPreviewController::requestToken(const QString& locator)
{
    if (!m_enabled) return;
    const auto id = normalize(locator);
    if (id.isEmpty()) return;
    const auto now = m_clock.elapsed();
    if (const auto cached = m_cache.constFind(id);
        cached != m_cache.cend() && cached->freshUntil > now) {
        const QVariantMap snapshot = cached->snapshot;
        QTimer::singleShot(0, this, [this, id, snapshot] {
            emit tokenReady(id, snapshot);
        });
        return;
    }
    if (m_retryAfter.value(id, 0) > now) {
        const QVariantMap cached = tokenFor(id);
        QTimer::singleShot(0, this, [this, id, cached] {
            emit tokenReady(id, cached);
        });
        return;
    }
    if (m_active.contains(id) || m_pending.contains(id)) return;
    if (m_pending.size() >= maximumPending) {
        const auto obsolete = m_pending.dequeue();
        QTimer::singleShot(0, this, [this, obsolete] {
            emit tokenReady(obsolete, {});
        });
    }
    m_pending.enqueue(id);
    dispatch();
}

void BrowserTokenPreviewController::dispatch()
{
    // Browser token insight is optional: just one upstream operation at a time.
    if (!m_active.isEmpty() || m_pending.isEmpty()) return;
    const QString locator = m_pending.dequeue();
    const auto separator = locator.indexOf(QLatin1Char(':'));
    QUrl url{m_gatewayBase};
    url.setPath(QStringLiteral("/v1/browser/tokens"));
    QUrlQuery query{};
    query.addQueryItem(QStringLiteral("network"), locator.left(separator));
    query.addQueryItem(QStringLiteral("address"), locator.mid(separator + 1));
    url.setQuery(query);
    QNetworkRequest request{url};
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(deadlineMs);
    auto* reply = m_network.get(request);
    reply->setReadBufferSize(maximumResponseBytes + 1);
    m_active.insert(locator, reply);
    QTimer::singleShot(deadlineMs, reply, [reply] {
        if (reply->isRunning()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, locator] { complete(reply, locator); });
}

void BrowserTokenPreviewController::complete(QNetworkReply* reply, const QString& locator)
{
    if (m_active.value(locator) != reply) return;
    m_active.remove(locator);
    QVariantMap snapshot{};
    const auto lengthHeader = reply->header(QNetworkRequest::ContentLengthHeader);
    const qint64 declared = lengthHeader.isValid() ? lengthHeader.toLongLong() : -1;
    const bool usable = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
        && reply->header(QNetworkRequest::ContentTypeHeader).toString()
            .startsWith(QLatin1String("application/json"), Qt::CaseInsensitive)
        && (declared < 0 || declared <= maximumResponseBytes)
        && reply->bytesAvailable() <= maximumResponseBytes;
    if (usable) {
        const auto data = QJsonDocument::fromJson(reply->read(maximumResponseBytes + 1));
        if (data.isObject()) {
            const auto obj = data.object();
            const auto separator = locator.indexOf(QLatin1Char(':'));
            if (obj.value(QStringLiteral("available")).toBool()
                && obj.value(QStringLiteral("network")).toString().toLower()
                    == locator.left(separator)
                && obj.value(QStringLiteral("address")).toString().toLower()
                    == locator.mid(separator + 1)) {
                snapshot = obj.toVariantMap();
                retain(locator, snapshot);
            }
        }
    }
    if (snapshot.isEmpty()) {
        // Never overwrite a previously verified count/price with unknown.
        m_retryAfter.insert(locator, m_clock.elapsed() + retryAfterMs);
    }
    reply->deleteLater();
    emit tokenReady(locator, snapshot);
    dispatch();
}
