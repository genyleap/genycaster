#include "stats.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QTimeZone>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr int pageSize = 100;
constexpr int maxPages = 20;        // Bounds total requests and memory.
constexpr int maxReplyBytes = 2 * 1024 * 1024;
constexpr int timeoutMs = 12000;
constexpr qint64 cacheMs = 2 * 60 * 1000;
constexpr qint64 farcasterEpochMs = 1609459200000LL;

qint64 nonnegative(const QJsonValue& value) {
    const double n = value.toDouble(-1);
    return std::isfinite(n) && n >= 0 && n < 9007199254740991.0
        ? static_cast<qint64>(n) : 0;
}
QString safeText(const QJsonValue& value) {
    return value.isString() ? value.toString() : QString{};
}
}

StatsController::StatsController(const bool directProxy, const bool networkEnabled, QObject* parent)
    : QObject(parent), m_networkEnabled(networkEnabled) {
    if (directProxy) m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
}

StatsController::~StatsController() { cancel(); }

void StatsController::cancel() {
    ++m_epoch;
    if (m_reply) {
        auto* reply = m_reply.data();
        m_reply.clear();
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

void StatsController::emitChanges() { emit changed(); }

void StatsController::clear() {
    cancel();
    m_state = QStringLiteral("idle");
    m_fid.clear();
    m_key.clear();
    m_lastSuccessfulKey.clear();
    m_summary.clear();
    m_activity.clear();
    m_topCasts.clear();
    m_updatedAt.clear();
    m_seenHashes.clear();
    m_dayCounts.clear();
    m_receivedByPublishedDay.clear();
    m_loadedCount = 0;
    emitChanges();
}

QUrl StatsController::endpointUrl(const QString& route,
                                  const QHash<QString, QString>& query) const {
    QUrl url{m_endpoint};
    url.setPath(route);
    QUrlQuery parameters{};
    for (auto it = query.cbegin(); it != query.cend(); ++it)
        parameters.addQueryItem(it.key(), it.value());
    url.setQuery(parameters);
    return url;
}

void StatsController::load(QString fid, QString contract, QString endpoint,
                           const int days, const bool force) {
    fid = fid.trimmed();
    endpoint = endpoint.trimmed();
    if (days != 7 && days != 30 && days != 90 && days != 365) return;
    bool goodFid = false;
    const qulonglong parsedFid = fid.toULongLong(&goodFid);
    QUrl source{endpoint};
    const bool safeScheme = source.scheme() == QLatin1String("https")
        || (source.scheme() == QLatin1String("http")
            && (source.host() == QLatin1String("localhost")
                || source.host() == QLatin1String("127.0.0.1")));
    const bool raw = contract == QLatin1String("snapchain-v1");
    const bool supported = raw || contract == QLatin1String("hypersnap-v2")
        || contract == QLatin1String("neynar-v2");
    if (!goodFid || parsedFid == 0 || parsedFid > 9007199254740991ULL
        || !source.isValid() || !safeScheme || source.host().isEmpty()
        || !source.userName().isEmpty() || !source.password().isEmpty()
        || source.hasQuery() || source.hasFragment() || !supported) {
        clear();
        fail(QStringLiteral("An active, supported read provider and valid account are required."));
        return;
    }
    const QString key = fid + QLatin1Char('|') + contract
        + QLatin1Char('|') + endpoint + QLatin1Char('|') + QString::number(days);
    if (!force && m_key == key && (m_state == QLatin1String("loading")
        || ((m_state == QLatin1String("ready") || m_state == QLatin1String("partial"))
            && m_cachedAtMs > 0
            && QDateTime::currentMSecsSinceEpoch() - m_cachedAtMs < cacheMs))) return;

    cancel();
    m_fid = std::move(fid);
    m_contract = std::move(contract);
    m_endpoint = std::move(endpoint);
    m_key = key;
    m_days = days;
    m_raw = raw;
    m_state = QStringLiteral("loading");
    m_cursor.clear();
    m_summary.clear();
    m_activity.clear();
    m_topCasts.clear();
    m_updatedAt.clear();
    m_seenHashes.clear();
    m_dayCounts.clear();
    m_receivedByPublishedDay.clear();
    m_likes = m_recasts = m_receivedReplies = 0;
    m_followers = m_following = -1;
    m_casts = m_replies = m_loadedCount = m_pages = 0;
    m_partial = m_fetchedProfile = m_endedByDate = false;
    m_cutoff = days > 0 ? QDateTime(QDate::currentDate().addDays(1 - days),
                                    QTime(0, 0), QTimeZone::LocalTime) : QDateTime{};
    emitChanges();
    if (!m_networkEnabled) {
        fail(QStringLiteral("Network analytics are unavailable in this test session."));
        return;
    }
    if (m_raw) getCasts();
    else getProfile();
}

void StatsController::fail(QString reason) {
    m_state = QStringLiteral("error");
    m_summary = {{QStringLiteral("error"), std::move(reason)}};
    emitChanges();
}

void StatsController::getProfile() {
    const QUrl url = endpointUrl(QStringLiteral("/v2/farcaster/user"),
                                 {{QStringLiteral("fid"), m_fid}});
    QNetworkRequest request{url};
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(timeoutMs);
    auto* reply = m_network.get(request);
    m_reply = reply;
    const quint64 revision = m_epoch;
    connect(reply, &QNetworkReply::finished, this, [this, reply, revision] {
        if (revision != m_epoch) { reply->deleteLater(); return; }
        m_reply.clear();
        if (reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && reply->bytesAvailable() <= maxReplyBytes) {
            const auto payload = QJsonDocument::fromJson(reply->readAll()).object();
            const auto profile = payload.value(QStringLiteral("user")).toObject();
            if (QString::number(static_cast<qulonglong>(nonnegative(profile.value(
                    QStringLiteral("fid"))))) == m_fid) {
                m_followers = nonnegative(profile.value(QStringLiteral("follower_count")));
                m_following = nonnegative(profile.value(QStringLiteral("following_count")));
                m_fetchedProfile = true;
            }
        }
        reply->deleteLater();
        getCasts();
    });
}

void StatsController::getCasts() {
    if (m_pages >= maxPages) { m_partial = true; complete(); return; }
    const QString route = m_raw
        ? QStringLiteral("/v1/castsByFid")
        : QStringLiteral("/v2/farcaster/feed/user/casts");
    QHash<QString, QString> parameters{{QStringLiteral("fid"), m_fid}};
    if (m_raw) {
        parameters.insert(QStringLiteral("pageSize"), QString::number(pageSize));
        parameters.insert(QStringLiteral("reverse"), QStringLiteral("true"));
        if (!m_cursor.isEmpty()) parameters.insert(QStringLiteral("pageToken"), m_cursor);
    } else {
        parameters.insert(QStringLiteral("limit"), QString::number(pageSize));
        if (!m_cursor.isEmpty()) parameters.insert(QStringLiteral("cursor"), m_cursor);
    }
    QNetworkRequest request{endpointUrl(route, parameters)};
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(timeoutMs);
    auto* reply = m_network.get(request);
    m_reply = reply;
    ++m_pages;
    const quint64 revision = m_epoch;
    connect(reply, &QNetworkReply::finished, this, [this, reply, revision] {
        if (revision != m_epoch) { reply->deleteLater(); return; }
        m_reply.clear();
        const bool success = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && reply->bytesAvailable() <= maxReplyBytes;
        const auto payload = success ? QJsonDocument::fromJson(reply->readAll()) : QJsonDocument{};
        reply->deleteLater();
        if (!payload.isObject()) {
            if (m_pages == 1) {
                fail(QStringLiteral("The selected provider did not return valid cast statistics."));
            } else {
                m_partial = true;
                complete();
            }
            return;
        }
        processPage(payload.object());
    });
}

void StatsController::processPage(const QJsonObject& data) {
    const auto messages = data.value(m_raw ? QStringLiteral("messages")
                                           : QStringLiteral("casts"));
    if (!messages.isArray() || messages.toArray().size() > pageSize) {
        if (m_pages == 1) fail(QStringLiteral("Unexpected statistics response."));
        else { m_partial = true; complete(); }
        return;
    }
    const auto array = messages.toArray();
    bool pastCutoff = false;
    bool outOfOrder = false;
    qint64 lastTimestamp = std::numeric_limits<qint64>::max();
    for (const auto& value : array) {
        if (!value.isObject()) { m_partial = true; continue; }
        const auto message = value.toObject();
        const auto rawData = m_raw ? message.value(QStringLiteral("data")).toObject()
                                   : QJsonObject{};
        const auto author = message.value(QStringLiteral("author")).toObject();
        const qulonglong authorFid = static_cast<qulonglong>(nonnegative(
            m_raw ? rawData.value(QStringLiteral("fid"))
                  : author.value(QStringLiteral("fid"))));
        if (QString::number(authorFid) != m_fid) { m_partial = true; continue; }

        QDateTime created{};
        if (m_raw) {
            const qint64 timestamp = nonnegative(rawData.value(QStringLiteral("timestamp")));
            created = QDateTime::fromMSecsSinceEpoch(
                farcasterEpochMs + timestamp * 1000, QTimeZone::UTC);
        } else {
            created = QDateTime::fromString(
                safeText(message.value(QStringLiteral("timestamp"))), Qt::ISODateWithMs);
            if (!created.isValid())
                created = QDateTime::fromString(
                    safeText(message.value(QStringLiteral("timestamp"))), Qt::ISODate);
        }
        if (!created.isValid() || created > QDateTime::currentDateTimeUtc().addSecs(300)) {
            m_partial = true;
            continue;
        }
        if (created.toMSecsSinceEpoch() > lastTimestamp) outOfOrder = true;
        lastTimestamp = created.toMSecsSinceEpoch();
        if (m_cutoff.isValid() && created < m_cutoff) {
            pastCutoff = true;
            continue;
        }
        const QString hash = safeText(message.value(QStringLiteral("hash"))).toLower();
        if (hash.isEmpty() || m_seenHashes.contains(hash)) continue;
        m_seenHashes.insert(hash);
        const auto castBody = rawData.value(QStringLiteral("castAddBody")).toObject();
        if (m_raw && castBody.isEmpty()) { m_partial = true; continue; }
        const bool isReply = m_raw
            ? castBody.value(QStringLiteral("parentCastId")).isObject()
            : message.value(QStringLiteral("type")).toString() == QLatin1String("reply")
                || message.value(QStringLiteral("parent_hash")).isString();
        if (isReply) ++m_replies; else ++m_casts;
        ++m_loadedCount;

        const auto day = created.toLocalTime().date().toString(Qt::ISODate);
        auto count = m_dayCounts.value(day);
        if (isReply) ++count.second; else ++count.first;
        m_dayCounts.insert(day, count);

        if (m_raw || isReply) continue;
        const auto reactions = message.value(QStringLiteral("reactions")).toObject();
        const auto replies = message.value(QStringLiteral("replies")).toObject();
        const qint64 likes = nonnegative(reactions.value(QStringLiteral("likes_count")));
        const qint64 recasts = nonnegative(reactions.value(QStringLiteral("recasts_count")));
        const qint64 responseCount = nonnegative(replies.value(QStringLiteral("count")));
        if (reactions.isEmpty() || replies.isEmpty()) m_partial = true;
        m_likes += likes;
        m_recasts += recasts;
        m_receivedReplies += responseCount;
        auto& perDay = m_receivedByPublishedDay[day];
        perDay.likes += likes;
        perDay.recasts += recasts;
        perDay.receivedReplies += responseCount;
        const QVariantMap entry{
            {QStringLiteral("hash"), hash.startsWith(QLatin1String("0x"))
                ? hash : QStringLiteral("0x") + hash},
            {QStringLiteral("text"), safeText(message.value(QStringLiteral("text"))).left(240)},
            {QStringLiteral("likes"), likes},
            {QStringLiteral("recasts"), recasts},
            {QStringLiteral("replies"), responseCount},
            {QStringLiteral("score"), likes + recasts + responseCount},
            {QStringLiteral("date"), day}
        };
        m_topCasts.append(entry);
        std::sort(m_topCasts.begin(), m_topCasts.end(),
                  [](const QVariant& a, const QVariant& b) {
                      return a.toMap().value(QStringLiteral("score")).toLongLong()
                           > b.toMap().value(QStringLiteral("score")).toLongLong();
                  });
        if (m_topCasts.size() > 5) m_topCasts.removeLast();
    }
    const QString next = m_raw
        ? safeText(data.value(QStringLiteral("nextPageToken")))
        : safeText(data.value(QStringLiteral("next")).toObject().value(QStringLiteral("cursor")));
    if (next.size() > 2048 || (next == m_cursor && !next.isEmpty())) {
        m_partial = true;
        complete();
        return;
    }
    if (pastCutoff && !outOfOrder) {
        m_endedByDate = true;
        complete();
        return;
    }
    if (next.isEmpty()) { complete(); return; }
    m_cursor = next;
    emitChanges();
    getCasts();
}

void StatsController::complete() {
    const bool hasEngagement = !m_raw;
    const qint64 interactions = m_likes + m_recasts + m_receivedReplies;
    m_summary = {
        {QStringLiteral("casts"), m_casts},
        {QStringLiteral("replies"), m_replies},
        {QStringLiteral("likes"), m_likes},
        {QStringLiteral("recasts"), m_recasts},
        {QStringLiteral("receivedReplies"), m_receivedReplies},
        {QStringLiteral("engagement"), interactions},
        {QStringLiteral("average"), m_casts > 0 ? static_cast<double>(interactions) / m_casts : 0.0},
        {QStringLiteral("followers"), m_followers},
        {QStringLiteral("following"), m_following},
        {QStringLiteral("hasAudience"), m_fetchedProfile},
        {QStringLiteral("hasEngagement"), hasEngagement},
        {QStringLiteral("scanned"), m_loadedCount},
        {QStringLiteral("days"), m_days}
    };
    const QDate today = QDate::currentDate();
    const int chartDays = m_days;
    const int bucketSize = chartDays <= 30 ? 1 : 7;
    // Use twelve balanced date ranges over the actual 365-day period.
    // An arbitrary 30/31-day fixed bucket can extend into the future.
    const int bucketCount = chartDays == 365 ? 12
        : (chartDays + bucketSize - 1) / bucketSize;
    m_activity.clear();
    for (int bucket = 0; bucket < bucketCount; ++bucket) {
        const int firstOffset = chartDays == 365
            ? (chartDays * bucket) / bucketCount : bucket * bucketSize;
        const int endOffset = chartDays == 365
            ? (chartDays * (bucket + 1)) / bucketCount
            : std::min(chartDays, (bucket + 1) * bucketSize);
        const QDate start = today.addDays(1 - chartDays + firstOffset);
        const QDate end = today.addDays(1 - chartDays + endOffset - 1);
        int casts = 0;
        int replies = 0;
        qint64 likes = 0;
        qint64 recasts = 0;
        qint64 receivedReplies = 0;
        for (int day = 0; day < endOffset - firstOffset; ++day) {
            const QString key = start.addDays(day).toString(Qt::ISODate);
            const auto counts = m_dayCounts.value(key);
            const auto engagement = m_receivedByPublishedDay.value(key);
            casts += counts.first;
            replies += counts.second;
            likes += engagement.likes;
            recasts += engagement.recasts;
            receivedReplies += engagement.receivedReplies;
        }
        m_activity.append(QVariantMap{
            // Canonical dates let every client render locale-specific month/day
            // names without changing the provider-independent bucket boundaries.
            {QStringLiteral("startDate"), start.toString(Qt::ISODate)},
            {QStringLiteral("endDate"), end.toString(Qt::ISODate)},
            {QStringLiteral("label"), start.toString(QStringLiteral("MMM d"))},
            {QStringLiteral("range"), start == end
                ? start.toString(QStringLiteral("MMM d"))
                : start.toString(QStringLiteral("MMM d"))
                    + QStringLiteral(" – ") + end.toString(QStringLiteral("MMM d"))},
            {QStringLiteral("casts"), casts},
            {QStringLiteral("replies"), replies},
            {QStringLiteral("likes"), likes},
            {QStringLiteral("recasts"), recasts},
            {QStringLiteral("receivedReplies"), receivedReplies}
        });
    }
    // The fetch cap marks partial whenever a provider has more pages; never
    // present a truncated one-year scan as a complete account history.

    m_state = m_partial ? QStringLiteral("partial") : QStringLiteral("ready");
    m_updatedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_cachedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_lastSuccessfulKey = m_key;
    emitChanges();
}
