#include "genycaster_deployment.hpp"
#include "reactions.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
#if defined(Q_OS_WASM) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
constexpr qsizetype maximumEntries = 192;
#else
constexpr qsizetype maximumEntries = 256;
#endif
constexpr qsizetype maximumPending = 32;
constexpr qsizetype maximumBatch = 8;
constexpr qsizetype maximumLocal = 512;
constexpr qsizetype maximumResponseBytes = 64 * 1024;
constexpr qint64 maximumSafeFid = 9'007'199'254'740'991LL;
constexpr qint64 positiveFreshMs = 5 * 60 * 1000;
constexpr qint64 unavailableRetryMs = 15 * 1000;
constexpr int requestDeadlineMs = 8 * 1000;
}

BrowserReactionController::BrowserReactionController(
    const bool useDirectProxy, const bool networkEnabled, QObject* parent,
    QUrl endpoint, const int dispatchDelayMs)
    : QObject(parent),
      m_endpoint(endpoint.isValid() && !endpoint.isEmpty()
          ? std::move(endpoint)
          : QUrl{(QString::fromUtf8(genycaster::deployment::browserGateway.data())
              + QStringLiteral("/v1/browser/cast-reactions"))}),
      m_enabled(networkEnabled)
{
    if (useDirectProxy) {
        m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
    }
    m_clock.start();
    m_dispatchTimer.setSingleShot(true);
    m_dispatchTimer.setInterval(std::max(0, dispatchDelayMs));
    connect(&m_dispatchTimer, &QTimer::timeout, this, &BrowserReactionController::dispatch);
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &BrowserReactionController::queueInterested);
    m_interestScanTimer.setSingleShot(true);
    m_interestScanTimer.setInterval(0);
    connect(&m_interestScanTimer, &QTimer::timeout, this, &BrowserReactionController::queueInterested);
}

BrowserReactionController::~BrowserReactionController()
{
    m_dispatchTimer.stop();
    m_retryTimer.stop();
    m_interestScanTimer.stop();
    if (m_activeReply) {
        disconnect(m_activeReply, nullptr, this, nullptr);
        m_activeReply->abort();
        m_activeReply->deleteLater();
    }
}

QString BrowserReactionController::canonicalKey(const QString& castId, const double viewerFid)
{
    if (!std::isfinite(viewerFid) || viewerFid < 1
        || viewerFid > static_cast<double>(maximumSafeFid)
        || std::trunc(viewerFid) != viewerFid) {
        return {};
    }
    QString hash = castId.trimmed().toLower();
    if (!hash.startsWith(QLatin1String("0x"))) {
        hash.prepend(QLatin1String("0x"));
    }
    if (hash.size() != 42) {
        return {};
    }
    for (qsizetype index = 2; index < hash.size(); ++index) {
        const QChar ch = hash.at(index);
        if ((ch < QLatin1Char('0') || ch > QLatin1Char('9'))
            && (ch < QLatin1Char('a') || ch > QLatin1Char('f'))) {
            return {};
        }
    }
    return QString::number(static_cast<qint64>(viewerFid)) + QLatin1Char(':') + hash;
}

BrowserReactionController::Entry* BrowserReactionController::getOrCreate(const QString& key)
{
    if (auto it = m_entries.find(key); it != m_entries.end()) {
        it->lastUsed = m_clock.elapsed();
        return &it.value();
    }
    while (m_entries.size() >= maximumEntries) {
        auto oldest = m_entries.end();
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->consumers > 0 || it->inFlight || m_pending.contains(it.key())) continue;
            if (oldest == m_entries.end() || it->lastUsed < oldest->lastUsed) oldest = it;
        }
        if (oldest == m_entries.end()) return nullptr;
        m_entries.erase(oldest);
    }
    const qsizetype separator = key.indexOf(QLatin1Char(':'));
    Entry entry{};
    entry.viewerFid = key.left(separator).toLongLong();
    entry.castId = key.mid(separator + 1);
    entry.lastUsed = m_clock.elapsed();
    return &m_entries.insert(key, std::move(entry)).value();
}

void BrowserReactionController::retain(const QString& castId, const double viewerFid)
{
    if (!m_enabled) return;
    const QString key = canonicalKey(castId, viewerFid);
    if (key.isEmpty()) return;
    if (auto* entry = getOrCreate(key)) {
        ++entry->consumers;
        scheduleInterestScan();
    }
}

void BrowserReactionController::release(const QString& castId, const double viewerFid)
{
    const auto key = canonicalKey(castId, viewerFid);
    auto it = m_entries.find(key);
    if (it == m_entries.end()) return;
    it->consumers = std::max(0, it->consumers - 1);
    if (it->consumers == 0) {
        // Releasing an unsent cast must not impose a failed-request cooldown.
        m_pending.removeAll(key);
    }
    scheduleInterestScan();
}

void BrowserReactionController::ensure(const QString& castId, const double viewerFid)
{
    if (!m_enabled) return;
    const auto key = canonicalKey(castId, viewerFid);
    if (auto it = m_entries.find(key); it != m_entries.end() && it->consumers > 0) {
        scheduleInterestScan();
    }
}

bool BrowserReactionController::reacted(
    const QString& castId, const double viewerFid, const QString& kind) const
{
    const QString key = canonicalKey(castId, viewerFid);
    if (key.isEmpty() || (kind != QLatin1String("like")
        && kind != QLatin1String("recast"))) return false;
    if (const auto override = m_local.constFind(key + QLatin1Char(':') + kind);
        override != m_local.cend()) return override.value();
    const auto entry = m_entries.constFind(key);
    return entry != m_entries.cend()
        && (kind == QLatin1String("like") ? entry->liked : entry->recasted);
}

QVariant BrowserReactionController::localValue(
    const QString& castId, const double viewerFid, const QString& kind) const
{
    const auto key = canonicalKey(castId, viewerFid);
    if (key.isEmpty()) return {};
    const auto it = m_local.constFind(key + QLatin1Char(':') + kind);
    return it == m_local.cend() ? QVariant{} : QVariant{it.value()};
}

void BrowserReactionController::setLocal(
    const QString& castId, const double viewerFid, const QString& kind, const bool active)
{
    const auto key = canonicalKey(castId, viewerFid);
    if (key.isEmpty() || (kind != QLatin1String("like")
        && kind != QLatin1String("recast"))) return;
    const auto localKey = key + QLatin1Char(':') + kind;
    if (m_local.contains(localKey) && m_local.value(localKey) == active) return;
    if (m_local.size() >= maximumLocal && !m_local.contains(localKey)) {
        m_local.erase(m_local.begin());
    }
    m_local.insert(localKey, active);
    notifyChange();
}

void BrowserReactionController::notifyChange()
{
    ++m_revision;
    emit revisionChanged();
}

void BrowserReactionController::scheduleInterestScan()
{
    // Many delegates acquire/release interest in one model transaction.
    // Scan the bounded cache once on the next event turn, not once per
    // delegate. Ownership and cancellation still change synchronously.
    if (m_enabled && !m_interestScanTimer.isActive())
        m_interestScanTimer.start();
}

void BrowserReactionController::queueInterested()
{
    m_interestScanTimer.stop();
    m_retryTimer.stop();
    if (!m_enabled) return;
    const auto now = m_clock.elapsed();
    qint64 nextWake = std::numeric_limits<qint64>::max();
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it->consumers == 0 || it->inFlight || m_pending.contains(it.key())) continue;
        if (it->retryAfter > now) {
            nextWake = std::min(nextWake, it->retryAfter - now);
        } else if (m_pending.size() < maximumPending) {
            m_pending.append(it.key());
        } else {
            nextWake = std::min<qint64>(nextWake, 220);
        }
    }
    if (!m_activeReply && !m_pending.empty() && !m_dispatchTimer.isActive()) {
        m_dispatchTimer.start();
    }
    if (nextWake != std::numeric_limits<qint64>::max()) {
        m_retryTimer.start(static_cast<int>(
            std::clamp<qint64>(nextWake, 1, 2'147'483'647)));
    }
}

void BrowserReactionController::dispatch()
{
    if (m_activeReply || m_pending.empty()) return;
    QStringList keys{};
    qint64 viewerFid{};
    while (!m_pending.empty() && keys.size() < maximumBatch) {
        const QString key = m_pending.takeFirst();
        auto it = m_entries.find(key);
        if (it == m_entries.end() || it->consumers == 0 || it->inFlight) continue;
        if (viewerFid != 0 && it->viewerFid != viewerFid) {
            m_pending.append(key);
            break;
        }
        viewerFid = it->viewerFid;
        it->inFlight = true;
        keys.append(key);
    }
    if (keys.empty()) {
        queueInterested();
        return;
    }
    QJsonArray casts{};
    for (const auto& key : keys) casts.append(m_entries.value(key).castId);
    const auto payload = QJsonDocument{
        QJsonObject{{QStringLiteral("viewerFid"), viewerFid},
                    {QStringLiteral("casts"), casts}}}.toJson(QJsonDocument::Compact);
    QNetworkRequest request{m_endpoint};
    // This gateway can reset multiplexed Qt HTTP/2 streams after sending a
    // 200 response. Pin its derived read-only POST to HTTP/1.1; preserve the
    // existing deadline, CORS origin and bounded response limits.
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(requestDeadlineMs);
    m_response.clear();
    m_tooLarge = false;
    auto* reply = m_network.post(request, payload);
    m_activeReply = reply;
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        const auto remaining = maximumResponseBytes - m_response.size();
        m_response.append(reply->read(remaining + 1));
        if (m_response.size() > maximumResponseBytes) {
            m_tooLarge = true;
            reply->abort();
        }
    });
    QTimer::singleShot(requestDeadlineMs, reply, [reply] {
        if (reply->isRunning()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, keys = std::move(keys)]() mutable {
        finish(reply, std::move(keys));
    });
}

void BrowserReactionController::finish(QNetworkReply* reply, QStringList keys)
{
    if (m_activeReply != reply) return;
    if (!m_tooLarge) {
        m_response.append(reply->read(maximumResponseBytes - m_response.size() + 1));
        if (m_response.size() > maximumResponseBytes) m_tooLarge = true;
    }
    const bool successful = !m_tooLarge
        && reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
        && reply->header(QNetworkRequest::ContentTypeHeader).toString()
            .startsWith(QLatin1String("application/json"), Qt::CaseInsensitive);
    const QJsonDocument document = successful ? QJsonDocument::fromJson(m_response)
                                             : QJsonDocument{};
    const QJsonObject reactions = document.isObject()
        ? document.object().value(QStringLiteral("reactions")).toObject() : QJsonObject{};
    const auto now = m_clock.elapsed();
    bool changed = false;
    for (const auto& key : keys) {
        auto it = m_entries.find(key);
        if (it == m_entries.end()) continue;
        it->inFlight = false;
        it->retryAfter = now + (successful ? positiveFreshMs : unavailableRetryMs);
        // Omitted or false values are unknown. Only exact hash keyed positive
        // evidence from the authenticated-viewer relay can change the cache.
        if (!successful) continue;
        const auto item = reactions.value(it->castId).toObject();
        if (!it->liked && item.value(QStringLiteral("liked")).toBool(false)) {
            it->liked = true;
            changed = true;
        }
        if (!it->recasted && item.value(QStringLiteral("recasted")).toBool(false)) {
            it->recasted = true;
            changed = true;
        }
    }
    m_activeReply = nullptr;
    reply->deleteLater();
    if (changed) notifyChange();
    queueInterested();
}
