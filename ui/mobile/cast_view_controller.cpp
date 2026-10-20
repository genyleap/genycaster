#include "cast_view_controller.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace {

#if defined(__EMSCRIPTEN__) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
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
constexpr qint64 retryForMs = 90 * 1000;
constexpr qint64 requestDeadlineMs = 6 * 1000;
constexpr qint64 maxSafeFid = 9'007'199'254'740'991LL;
constexpr qint64 maxResponseBytes = 64 * 1024;

}

CastViewEntry::CastViewEntry(QString castId, QObject* parent)
    : QObject(parent), m_castId(std::move(castId))
{
}

CastViewController::CastViewController(
    const bool useDirectProxy, const bool networkEnabled, QObject* parent, QUrl endpoint)
    : QObject(parent),
      m_endpoint(endpoint.isValid() && !endpoint.isEmpty()
          ? std::move(endpoint)
          : QUrl{QStringLiteral(
              "https://storage-farcaster.genyleap.com/v1/browser/cast-views")}),
      m_networkEnabled(networkEnabled)
{
    if (useDirectProxy) {
        m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
    }
    m_dispatchTimer.setSingleShot(true);
    m_dispatchTimer.setInterval(220);
    connect(&m_dispatchTimer, &QTimer::timeout, this, &CastViewController::dispatch);
    m_clock.start();
}

CastViewController::~CastViewController()
{
    m_dispatchTimer.stop();
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
        m_pending.remove(key);
    }
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
    const qint64 now = m_clock.elapsed();
    if (entry->m_inFlight || entry->m_freshUntil > now
        || entry->m_retryAfter > now || m_pending.contains(key)
        || m_pending.size() >= maxPending) {
        return;
    }
    QString compact = text.simplified().left(160);
    m_pending.insert(key, Pending{
        .castId = key,
        .authorFid = static_cast<qint64>(authorFid),
        .text = std::move(compact),
    });
    entry->m_retryAfter = now + retryForMs;
    if (!m_activeReply && !m_dispatchTimer.isActive()) {
        m_dispatchTimer.start();
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
    if (reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
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
        if (raw.isDouble()) {
            const double number = raw.toDouble();
            if (std::isfinite(number) && number >= 0.0
                && std::trunc(number) == number
                && number <= static_cast<double>(maxSafeFid)) {
                const QVariant count{static_cast<qulonglong>(number)};
                entry->m_freshUntil = now + validForMs;
                entry->m_retryAfter = entry->m_freshUntil;
                if (entry->m_count != count) {
                    entry->m_count = count;
                    emit entry->countChanged();
                }
            }
        }
        // No value is not zero. Keep any older verified count untouched and
        // use the retry deadline already recorded at admission.
    }
    reply->deleteLater();
    if (!m_pending.isEmpty()) {
        m_dispatchTimer.start();
    }
}
