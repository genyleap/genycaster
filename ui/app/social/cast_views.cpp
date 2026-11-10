#include "genycaster_deployment.hpp"
#include "cast_views.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

#if defined(Q_OS_WASM) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
// WASM and backgroundable mobile processes need a smaller retained footprint;
// the per-cast semantics, network deadline and batch API are identical.
constexpr int maxEntries = 192;
constexpr int maxPending = 48;
#else
constexpr int maxEntries = 384;
constexpr int maxPending = 96;
#endif
constexpr int maxBatch = 16;
constexpr qint64 validForMs = 3 * 60 * 1000;
// The gateway's 6-second logical operation budget must finish before the
// client's end-to-end deadline, including response transfer and scheduling.
constexpr qint64 requestDeadlineMs = 8 * 1000;
constexpr qint64 maxSafeFid = 9'007'199'254'740'991LL;
constexpr qint64 maxResponseBytes = 64 * 1024;

}

CastViewEntry::CastViewEntry(QString castId, QObject* parent)
    : QObject(parent), m_castId(std::move(castId))
{
}

CastViewController::CastViewController(
    const bool useDirectProxy, const bool networkEnabled, QObject* parent, QUrl endpoint,
    const qint64 retryIntervalMs)
    : QObject(parent),
      m_endpoint(endpoint.isValid() && !endpoint.isEmpty()
          ? std::move(endpoint)
          : QUrl{(QString::fromUtf8(genycaster::deployment::browserGateway.data())
              + QStringLiteral("/v1/browser/cast-views"))}),
      m_networkEnabled(networkEnabled),
      m_retryIntervalMs(std::max<qint64>(1, retryIntervalMs))
{
    if (useDirectProxy) {
        m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
    }
    m_dispatchTimer.setSingleShot(true);
    m_dispatchTimer.setInterval(220);
    connect(&m_dispatchTimer, &QTimer::timeout, this, &CastViewController::dispatch);
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &CastViewController::queueInterested);
    m_clock.start();
}

CastViewController::~CastViewController()
{
    m_dispatchTimer.stop();
    m_retryTimer.stop();
    if (m_activeReply) {
        disconnect(m_activeReply, nullptr, this, nullptr);
        // Only one bounded derived-analytics request can remain in flight.
        m_activeReply->abort();
        m_activeReply->deleteLater();
    }
}

QString CastViewController::normalizeHash(const QString& input)
{
    QString hash = input.trimmed().toLower();
    if (!hash.startsWith(QLatin1String("0x"))) {
        hash.prepend(QLatin1String("0x"));
    }
    if (hash.size() != 42) {
        return {};
    }
    for (int index = 2; index < hash.size(); ++index) {
        const QChar ch = hash.at(index);
        if ((ch < QLatin1Char('0') || ch > QLatin1Char('9'))
            && (ch < QLatin1Char('a') || ch > QLatin1Char('f'))) {
            return {};
        }
    }
    return hash;
}

CastViewEntry* CastViewController::getOrCreate(const QString& hash)
{
    if (auto* value = m_entries.value(hash, nullptr)) {
        value->m_touched = m_clock.elapsed();
        return value;
    }
    evictUnused();
    if (m_entries.size() >= maxEntries) {
        return nullptr;
    }
    auto* entry = new CastViewEntry(hash, this);
    entry->m_touched = m_clock.elapsed();
    m_entries.insert(hash, entry);
    return entry;
}

void CastViewController::evictUnused()
{
    while (m_entries.size() >= maxEntries) {
        auto oldest = m_entries.end();
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            const auto* entry = it.value();
            if (entry->m_consumers != 0 || entry->m_inFlight
                || m_pending.contains(it.key())) {
                continue;
            }
            if (oldest == m_entries.end()
                || entry->m_touched < oldest.value()->m_touched) {
                oldest = it;
            }
        }
        if (oldest == m_entries.end()) {
            return;
        }
        auto* removed = oldest.value();
        m_entries.erase(oldest);
        removed->deleteLater();
    }
}

QObject* CastViewController::entryFor(const QString& castId)
{
    const QString key = normalizeHash(castId);
    return key.isEmpty() ? nullptr : getOrCreate(key);
}

void CastViewController::retain(const QString& castId)
{
    const QString key = normalizeHash(castId);
    if (key.isEmpty()) {
        return;
    }
    if (auto* entry = getOrCreate(key)) {
        ++entry->m_consumers;
        queueInterested();
    }
}

void CastViewController::release(const QString& castId)
{
    const QString key = normalizeHash(castId);
    auto* entry = m_entries.value(key, nullptr);
    if (!entry) {
        return;
    }
    entry->m_consumers = std::max(0, entry->m_consumers - 1);
    if (entry->m_consumers == 0) {
        if (m_pending.remove(key) != 0) {
            entry->m_state = CastViewEntry::Lifecycle::CanceledBeforeDispatch;
            entry->m_retryAfter = 0; // No request was made.
        }
        entry->m_metadataFollowUpUsed = false;
    }
    queueInterested();
}

void CastViewController::ensure(
    const QString& castId, const double authorFid, const QString& text)
{
    if (!m_networkEnabled || !std::isfinite(authorFid) || authorFid < 1
        || authorFid > static_cast<double>(maxSafeFid)
        || std::trunc(authorFid) != authorFid) {
        return;
    }
    const QString key = normalizeHash(castId);
    if (key.isEmpty()) {
        return;
    }
    auto* entry = getOrCreate(key);
    if (!entry || entry->m_consumers == 0) {
        return;
    }
    const auto newFid = static_cast<qint64>(authorFid);
    const QString compact = text.simplified().left(160);
    const bool changed = (newFid > 0 && entry->m_desiredAuthorFid != newFid)
        || (!compact.isEmpty() && compact != entry->m_desiredText);
    // Preserve newer usable metadata through transient QML null/empty states.
    if (newFid > 0) {
        entry->m_desiredAuthorFid = newFid;
    }
    // A transient empty QML text binding cannot destroy usable metadata.
    if (!compact.isEmpty()) {
        entry->m_desiredText = compact;
    }
    if (changed && entry->m_state == CastViewEntry::Lifecycle::Invalid) {
        entry->m_state = CastViewEntry::Lifecycle::Idle;
        entry->m_retryAfter = 0;
    }
    if (auto it = m_pending.find(key); it != m_pending.end()) {
        it->authorFid = entry->m_desiredAuthorFid;
        it->text = entry->m_desiredText;
    }
    queueInterested();
}

void CastViewController::queueInterested()
{
    m_retryTimer.stop();
    if (!m_networkEnabled) {
        return;
    }
    const qint64 now = m_clock.elapsed();
    qint64 nextWake = std::numeric_limits<qint64>::max();
    for (auto* entry : m_entries) {
        if (entry->m_consumers == 0 || entry->m_desiredAuthorFid <= 0
            || entry->m_inFlight || entry->m_state == CastViewEntry::Lifecycle::Invalid
            || m_pending.contains(entry->m_castId)) {
            continue;
        }
        const qint64 eligibleAt = std::max(entry->m_freshUntil, entry->m_retryAfter);
        if (eligibleAt > now) {
            nextWake = std::min(nextWake, eligibleAt - now);
        } else if (m_pending.size() < maxPending) {
            m_pending.insert(entry->m_castId, Pending{
                .castId = entry->m_castId,
                .authorFid = entry->m_desiredAuthorFid,
                .text = entry->m_desiredText,
            });
            entry->m_state = CastViewEntry::Lifecycle::Queued;
        } else {
            // Demand remains active even when the bounded queue is full.
            nextWake = std::min<qint64>(nextWake, 220);
        }
    }
    if (!m_pending.isEmpty() && !m_activeReply && !m_dispatchTimer.isActive()) {
        m_dispatchTimer.start();
    }
    if (nextWake != std::numeric_limits<qint64>::max()) {
        m_retryTimer.start(static_cast<int>(std::clamp<qint64>(nextWake, 1, 2'147'483'647)));
    }
}

void CastViewController::dispatch()
{
    if (!m_networkEnabled || m_activeReply || m_pending.isEmpty()) {
        return;
    }
    QJsonArray casts{};
    QStringList hashes{};
    hashes.reserve(maxBatch);
    for (auto it = m_pending.begin(); it != m_pending.end()
         && hashes.size() < maxBatch;) {
        const QString key = it.key();
        const Pending pending = it.value();
        it = m_pending.erase(it);
        auto* entry = m_entries.value(key, nullptr);
        if (!entry || entry->m_consumers == 0) {
            continue;
        }
        entry->m_inFlight = true;
        entry->m_state = CastViewEntry::Lifecycle::Active;
        entry->m_sentAuthorFid = pending.authorFid;
        entry->m_sentText = pending.text;
        hashes.append(key);
        QJsonObject row{};
        row.insert(QStringLiteral("castId"), pending.castId);
        row.insert(QStringLiteral("authorFid"), pending.authorFid);
        row.insert(QStringLiteral("text"), pending.text);
        casts.append(row);
    }
    if (hashes.isEmpty()) {
        return;
    }
    const QJsonObject payload{{QStringLiteral("casts"), casts}};
    QNetworkRequest request{m_endpoint};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(static_cast<int>(requestDeadlineMs));
    auto* reply = m_network.post(request, QJsonDocument{payload}.toJson(
        QJsonDocument::Compact));
    m_activeReply = reply;
    // setTransferTimeout is an inactivity watchdog, not a total deadline.
    QTimer::singleShot(requestDeadlineMs, reply, [reply] {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, hashes] { finish(reply, hashes); });
    connect(reply, &QNetworkReply::downloadProgress, this,
        [reply](const qint64 received, const qint64) {
            if (received > maxResponseBytes && reply->isRunning()) {
                reply->abort();
            }
        });
}

void CastViewController::finish(QNetworkReply* reply, const QStringList& hashes)
{
    if (reply != m_activeReply) {
        reply->deleteLater();
        return;
    }
    m_activeReply = nullptr;
    const qint64 now = m_clock.elapsed();
    QJsonObject counts{};
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool transportOk = reply->error() == QNetworkReply::NoError
        && httpStatus == 200;
    if (transportOk) {
        const QByteArray body = reply->read(maxResponseBytes + 1);
        if (body.size() <= maxResponseBytes) {
            QJsonParseError error{};
            const auto parsed = QJsonDocument::fromJson(body, &error);
            if (error.error == QJsonParseError::NoError && parsed.isObject()) {
                counts = parsed.object().value(QStringLiteral("counts")).toObject();
            }
        }
    }
    for (const QString& hash : hashes) {
        auto* entry = m_entries.value(hash, nullptr);
        if (!entry) {
            continue;
        }
        entry->m_inFlight = false;
        const QJsonValue raw = counts.value(hash);
        bool verified = false;
        if (raw.isDouble()) {
            const double number = raw.toDouble();
            if (std::isfinite(number) && number >= 0.0
                && std::trunc(number) == number
                && number <= static_cast<double>(maxSafeFid)) {
                const QVariant count{static_cast<qulonglong>(number)};
                verified = true;
                entry->m_state = CastViewEntry::Lifecycle::Resolved;
                entry->m_freshUntil = now + validForMs;
                entry->m_retryAfter = 0;
                entry->m_metadataFollowUpUsed = false;
                if (entry->m_count != count) {
                    entry->m_count = count;
                    emit entry->countChanged();
                }
            }
        }
        if (!verified) {
            const bool permanent = httpStatus == 400 || httpStatus == 422;
            entry->m_state = permanent ? CastViewEntry::Lifecycle::Invalid
                : transportOk ? CastViewEntry::Lifecycle::Unavailable
                              : CastViewEntry::Lifecycle::Failed;
            const bool metadataImproved = entry->m_sentAuthorFid != entry->m_desiredAuthorFid
                || entry->m_sentText != entry->m_desiredText;
            // One immediate follow-up is permitted when a better descriptor
            // arrived during the unresolved request. Repeated changes obey
            // ordinary backoff to bound provider load.
            if (!permanent && metadataImproved && !entry->m_metadataFollowUpUsed
                && entry->m_consumers > 0) {
                entry->m_metadataFollowUpUsed = true;
                entry->m_retryAfter = 0;
            } else {
                entry->m_retryAfter = permanent
                    ? std::numeric_limits<qint64>::max() : now + m_retryIntervalMs;
            }
            // Existing verified values remain present across refresh failures.
        }
    }
    reply->deleteLater();
    queueInterested();
}
