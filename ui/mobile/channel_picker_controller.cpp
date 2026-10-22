#include "channel_picker_controller.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr qsizetype maxPayloadBytes = 2 * 1024 * 1024;
constexpr int searchLimit = 70;
constexpr int membershipLimit = 100;
constexpr int maximumMembershipPages = 25;
constexpr int maximumRows = 90;
constexpr int searchTtlMs = 90 * 1000;
constexpr int membershipTtlMs = 70 * 1000;

[[nodiscard]] QString firstString(const QJsonObject& obj,
                                  std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QString value = obj.value(QLatin1StringView{key}).toString().trimmed();
        if (!value.isEmpty()) return value;
    }
    return {};
}

[[nodiscard]] QVariantMap homeRow()
{
    return QVariantMap{
        {QStringLiteral("id"), QString{}},
        {QStringLiteral("name"), QStringLiteral("Home")},
        {QStringLiteral("isHome"), true},
        {QStringLiteral("isMember"), true},
        {QStringLiteral("selectable"), true},
        {QStringLiteral("memberCount"), -1},
    };
}

[[nodiscard]] qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

// Bounded edit distance for short names. This provides typo tolerance without
// fuzzy-matching arbitrary CAIP paths or doing quadratic work on long strings.
[[nodiscard]] int nearDistance(const QString& a, const QString& b)
{
    if (a.size() > 32 || b.size() > 32) return 100;
    if (std::abs(a.size() - b.size()) > 2) return 100;
    QList<int> before(b.size() + 1), after(b.size() + 1);
    for (qsizetype j = 0; j <= b.size(); ++j) before[j] = static_cast<int>(j);
    for (qsizetype i = 1; i <= a.size(); ++i) {
        after[0] = static_cast<int>(i);
        for (qsizetype j = 1; j <= b.size(); ++j) {
            after[j] = std::min({before[j] + 1, after[j - 1] + 1,
                before[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        }
        before.swap(after);
    }
    return before.back();
}

[[nodiscard]] QString usageSettingsKey(qulonglong fid)
{
    return QStringLiteral("channels/composeUsage/%1").arg(fid);
}
}

ChannelPickerController::ChannelPickerController(
    QSettings& settings, QString readNodeBaseUrl, QString directoryBaseUrl,
    bool directProxy, bool networkEnabled, QObject* parent)
    : QObject(parent),
      m_settings(settings),
      m_baseUrl(std::move(readNodeBaseUrl)),
      m_directoryBaseUrl(std::move(directoryBaseUrl)),
      m_networkEnabled(networkEnabled)
{
    // Both endpoints are injected at the composition boundary. The node is
    // exactly the one selected for the core Farcaster runtime, not a fixed host.
    m_baseUrl = m_baseUrl.trimmed();
    m_directoryBaseUrl = m_directoryBaseUrl.trimmed();
    while (m_baseUrl.endsWith(QLatin1Char('/'))) m_baseUrl.chop(1);
    while (m_directoryBaseUrl.endsWith(QLatin1Char('/'))) m_directoryBaseUrl.chop(1);
    if (directProxy) m_network.setProxy(QNetworkProxy{QNetworkProxy::NoProxy});
    m_debounce.setInterval(190);
    m_debounce.setSingleShot(true);
    connect(&m_debounce, &QTimer::timeout, this, &ChannelPickerController::requestSearchNow);
    rebuildRows();
}

QVariantList ChannelPickerController::rows() const { return m_rows; }
bool ChannelPickerController::busy() const noexcept
{
    return m_membershipBusy || m_searchBusy || m_trendingBusy;
}
bool ChannelPickerController::membershipReady() const noexcept { return m_membershipReady; }
QString ChannelPickerController::errorText() const { return m_error; }
QString ChannelPickerController::query() const { return m_query; }

void ChannelPickerController::setBusy(bool value)
{
    const bool previous = busy();
    m_searchBusy = value;
    if (previous != busy()) emit busyChanged();
}

void ChannelPickerController::setError(QString value)
{
    if (m_error == value) return;
    m_error = std::move(value);
    emit errorTextChanged();
}

bool ChannelPickerController::validParentUrl(const QString& url)
{
    return !url.isEmpty() && url.toUtf8().size() <= 256
        && (url.startsWith(QStringLiteral("https://"))
            || url.startsWith(QStringLiteral("chain://")));
}

QVariantMap ChannelPickerController::channelProjection(const QJsonObject& channel) const
{
    const QString id = firstString(channel, {"id"});
    const QString parentUrl = firstString(channel, {"parent_url", "parentUrl", "url"});
    if (id.isEmpty() || !validParentUrl(parentUrl)) return {};
    // Only HyperSnap-supplied imagery is eligible. Missing image_url means
    // show the channel's initial, not a fabricated asset from a static JSON.
    const QString image = firstString(channel,
        {"image_url", "imageUrl", "icon_url", "iconUrl"});
    QVariantMap row{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), firstString(channel, {"name", "id"})},
        {QStringLiteral("parentUrl"), parentUrl},
        {QStringLiteral("description"), firstString(channel, {"description"})},
        {QStringLiteral("imageUrl"), image.startsWith(QStringLiteral("https://"))
            ? image : QString{}},
        {QStringLiteral("memberCount"),
            channel.value(QStringLiteral("member_count")).toInteger(-1)},
        {QStringLiteral("followerCount"),
            channel.value(QStringLiteral("follower_count")).toInteger(-1)},
        {QStringLiteral("isHome"), false},
    };
    return row;
}

QVariantList ChannelPickerController::channelsFromResponse(
    const QJsonObject& response, const ChannelPickerController* self)
{
    QVariantList channels;
    const QJsonArray items = response.value(QStringLiteral("channels")).toArray();
    channels.reserve(std::min<qsizetype>(items.size(), 500));
    for (const QJsonValue& value : items) {
        if (!value.isObject()) continue;
        const QVariantMap row = self->channelProjection(value.toObject());
        if (!row.isEmpty()) channels.append(row);
    }
    return channels;
}

void ChannelPickerController::request(
    const QString& path, const QVariantMap& args, JsonCallback done)
{
    if (!m_networkEnabled) {
        done({}, QStringLiteral("Network unavailable"));
        return;
    }
    QUrl url{m_baseUrl + QStringLiteral("/v2/farcaster/") + path};
    QUrlQuery query;
    for (auto it = args.cbegin(); it != args.cend(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    url.setQuery(query);
    // Use the same HTTP/HTTPS scheme as the configured read node. In
    // particular, do not reject explicitly configured private/local nodes.
    if (!url.isValid() || (url.scheme() != QStringLiteral("https")
                           && url.scheme() != QStringLiteral("http"))) {
        done({}, QStringLiteral("Invalid configured channel read endpoint"));
        return;
    }
    QNetworkRequest request{url};
    request.setTransferTimeout(11000);
    request.setRawHeader("Accept", "application/json");
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
        [reply, done = std::move(done)]() {
            reply->deleteLater();
            const QByteArray payload = reply->readAll();
            if (reply->error() != QNetworkReply::NoError) {
                done({}, reply->errorString());
                return;
            }
            if (payload.size() > maxPayloadBytes) {
                done({}, QStringLiteral("Channel response exceeds size limit"));
                return;
            }
            QJsonParseError error{};
            const QJsonDocument doc = QJsonDocument::fromJson(payload, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) {
                done({}, QStringLiteral("Invalid HyperSnap channel response"));
                return;
            }
            done(doc.object(), {});
        });
}

void ChannelPickerController::requestDirectory(
    const QString& path, const QVariantMap& args, JsonCallback done)
{
    if (!m_networkEnabled || m_directoryBaseUrl.isEmpty()) {
        done({}, QStringLiteral("Farcaster directory not configured"));
        return;
    }
    QUrl url{m_directoryBaseUrl + QLatin1Char('/') + path};
    QUrlQuery query;
    for (auto it = args.cbegin(); it != args.cend(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    url.setQuery(query);
    if (url.scheme() != QStringLiteral("https")) {
        done({}, QStringLiteral("Directory endpoint requires HTTPS"));
        return;
    }
    QNetworkRequest req{url};
    req.setTransferTimeout(9500);
    req.setRawHeader("Accept", "application/json");
    auto* reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this,
        [reply, done = std::move(done)] {
            reply->deleteLater();
            const QByteArray body = reply->readAll();
            if (reply->error() != QNetworkReply::NoError) {
                done({}, reply->errorString());
                return;
            }
            if (body.size() > maxPayloadBytes) {
                done({}, QStringLiteral("Directory response is too large"));
                return;
            }
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) {
                done({}, QStringLiteral("Invalid channel directory response"));
                return;
            }
            done(doc.object(), {});
        });
}

bool ChannelPickerController::isSocialChannel(const QString& id) const
{
    // FIP-2 permits parent URLs for collectibles/tokens, but these are NOT
    // human-facing social channels. Exclude them from the channel picker.
    // Do not alter parent_url for a genuine slug-based channel.
    static const QRegularExpression slug{
        QStringLiteral("^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$")};
    return slug.match(id).hasMatch();
}

void ChannelPickerController::mergeMetadata(QVariantMap& channel) const
{
    const QString id = channel.value(QStringLiteral("id")).toString();
    const auto it = m_directoryMetadata.constFind(id);
    if (it == m_directoryMetadata.cend()) return;
    const QVariantMap& meta = it.value();
    for (const char* field : {"imageUrl", "description", "followerCount", "memberCount"}) {
        const auto key = QString::fromLatin1(field);
        const QVariant value = meta.value(key);
        if (value.isValid() && !value.isNull()
            && (value.typeId() != QMetaType::QString || !value.toString().isEmpty()))
            channel.insert(key, value);
    }
    // Prefer the user-facing display name, not the opaque parent URL/name
    // sometimes returned by an indexed chain-asset read.
    if (!meta.value(QStringLiteral("name")).toString().isEmpty())
        channel.insert(QStringLiteral("name"), meta.value(QStringLiteral("name")));
}

void ChannelPickerController::retrieveDirectoryResults(
    QString query, quint64 nodeGeneration, quint64 searchGeneration)
{
    if (m_directoryBaseUrl.isEmpty()) return;
    requestDirectory(QStringLiteral("search-channels"),
        {{QStringLiteral("q"), query}, {QStringLiteral("limit"), 40}},
        [this, nodeGeneration, searchGeneration](
            const QJsonObject& body, const QString& error) {
            if (nodeGeneration != m_generation
                || searchGeneration != m_searchGeneration) return;
            if (!error.isEmpty()) return; // node remains functional
            const QJsonArray results = body.value(QStringLiteral("result"))
                                           .toObject().value(QStringLiteral("channels")).toArray();
            QVariantList directoryRows;
            for (const auto& value : results) {
                if (!value.isObject()) continue;
                const QJsonObject item = value.toObject();
                const QString id = firstString(item, {"key", "id"});
                if (!isSocialChannel(id)) continue;
                const QString picture = firstString(item, {"imageUrl", "fastImageUrl"});
                QVariantMap meta{
                    {QStringLiteral("id"), id},
                    {QStringLiteral("name"), firstString(item, {"name", "key", "id"})},
                    {QStringLiteral("description"), firstString(item, {"description"})},
                    {QStringLiteral("imageUrl"), picture.startsWith(QStringLiteral("https://"))
                        ? picture : QString{}},
                    {QStringLiteral("followerCount"),
                        item.value(QStringLiteral("followerCount")).toInteger(-1)},
                    {QStringLiteral("memberCount"),
                        item.value(QStringLiteral("memberCount")).toInteger(-1)},
                };
                m_directoryMetadata.insert(id, meta);
                directoryRows.push_back(meta);
            }
            m_directoryResults = std::move(directoryRows);
            rebuildRows();
        });
}

void ChannelPickerController::hydrateMemberImages()
{
    if (m_directoryBaseUrl.isEmpty() || m_members.isEmpty()) return;
    // Fetch metadata only for visible/recent joined channels; bounded and
    // deduplicated. Keep caches in C++ and let Qt's image provider cache bytes.
    QList<QString> ids = m_members.keys();
    std::stable_sort(ids.begin(), ids.end(), [this](const QString& a, const QString& b) {
        return m_usage.value(a) > m_usage.value(b);
    });
    const quint64 generation = m_generation;
    int scheduled = 0;
    for (const QString& id : ids) {
        if (scheduled >= 12) break;
        if (!isSocialChannel(id) || m_directoryMetadata.contains(id)
            || m_pendingImageLookups.contains(id) || m_triedImageLookups.contains(id)) continue;
        ++scheduled;
        m_triedImageLookups.insert(id);
        m_pendingImageLookups.insert(id);
        requestDirectory(QStringLiteral("channel"), {{QStringLiteral("key"), id}},
            [this, id, generation](const QJsonObject& body, const QString& error) {
                if (generation != m_generation) return;
                m_pendingImageLookups.remove(id);
                if (!error.isEmpty()) return;
                const QJsonObject item = body.value(QStringLiteral("result"))
                                             .toObject().value(QStringLiteral("channel")).toObject();
                const QString key = firstString(item, {"key", "id"});
                if (key != id) return;
                const QString picture = firstString(item, {"imageUrl", "fastImageUrl"});
                QVariantMap meta{
                    {QStringLiteral("name"), firstString(item, {"name", "key"})},
                    {QStringLiteral("imageUrl"), picture.startsWith(QStringLiteral("https://"))
                        ? picture : QString{}},
                    {QStringLiteral("followerCount"),
                        item.value(QStringLiteral("followerCount")).toInteger(-1)},
                    {QStringLiteral("memberCount"),
                        item.value(QStringLiteral("memberCount")).toInteger(-1)},
                };
                m_directoryMetadata.insert(id, std::move(meta));
                rebuildRows();
            });
    }
}

void ChannelPickerController::loadUsage()
{
    m_usage.clear();
    if (m_viewerFid == 0) return;
    const QVariantMap used = m_settings.value(usageSettingsKey(m_viewerFid)).toMap();
    for (auto it = used.cbegin(); it != used.cend(); ++it) {
        const int count = it.value().toInt();
        if (count > 0 && count <= 1'000'000) m_usage.insert(it.key(), count);
    }
}

void ChannelPickerController::saveUsage()
{
    if (m_viewerFid == 0) return;
    QVariantMap counts;
    // Cap retained usage entries. No channel metadata, tokens, or API
    // responses are persisted in user settings.
    QList<QString> ids = m_usage.keys();
    std::sort(ids.begin(), ids.end(), [this](const QString& a, const QString& b) {
        return m_usage.value(a) > m_usage.value(b);
    });
    for (const QString& id : ids.mid(0, 100))
        counts.insert(id, m_usage.value(id));
    m_settings.setValue(usageSettingsKey(m_viewerFid), counts);
    m_settings.sync();
}

void ChannelPickerController::openForViewer(qulonglong fid)
{
    const bool viewerChanged = fid != m_viewerFid;
    if (viewerChanged) {
        ++m_generation;
        ++m_membershipGeneration;
        ++m_searchGeneration;
        m_debounce.stop();
        m_viewerFid = fid;
        m_members.clear();
        m_searchCache.clear();
        m_searchResults.clear();
        m_directoryResults.clear();
        m_directoryMetadata.clear();
        m_pendingImageLookups.clear();
        m_triedImageLookups.clear();
        m_trendingResults.clear();
        m_membershipReady = false;
        m_membershipValidUntil = 0;
        m_membershipBusy = false;
        m_searchBusy = false;
        m_trendingBusy = false;
        loadUsage();
        emit membershipStateChanged();
        emit busyChanged();
    }
    if (!m_query.isEmpty()) {
        m_query.clear();
        emit queryChanged();
    }
    setError({});
    rebuildRows();
    if (!m_networkEnabled) return;
    if (m_viewerFid > 0 && (!m_membershipReady || nowMs() >= m_membershipValidUntil))
        refreshMembership();
    // No unrelated /channel/trending items in the default composer list.
    // Show Home and actual joined channels; discovery starts when typing.
    if (m_membershipReady)
        hydrateMemberImages();
}

void ChannelPickerController::refreshMembership()
{
    const quint64 currentGeneration = ++m_membershipGeneration;
    const bool previousBusy = busy();
    m_membershipReady = false;
    m_membershipBusy = m_viewerFid > 0 && m_networkEnabled;
    m_members.clear();
    m_membershipValidUntil = 0;
    emit membershipStateChanged();
    if (previousBusy != busy()) emit busyChanged();
    rebuildRows();
    if (m_membershipBusy)
        requestMembershipPage({}, 0, currentGeneration);
}

void ChannelPickerController::requestMembershipPage(
    const QString& cursor, int page, quint64 generation)
{
    QVariantMap args{
        {QStringLiteral("fid"), QString::number(m_viewerFid)},
        {QStringLiteral("limit"), membershipLimit},
    };
    if (!cursor.isEmpty()) args.insert(QStringLiteral("cursor"), cursor);
    request(QStringLiteral("user/memberships/list"), args,
        [this, generation, page](const QJsonObject& body, const QString& error) {
            if (generation != m_membershipGeneration) return;
            if (!error.isEmpty() || !body.contains(QStringLiteral("channels"))) {
                m_members.clear();
                m_membershipReady = false; // fail closed, never infer membership
                const bool wasBusy = busy();
                m_membershipBusy = false;
                emit membershipStateChanged();
                if (wasBusy != busy()) emit busyChanged();
                setError(QStringLiteral("Could not verify channel memberships"));
                rebuildRows();
                return;
            }
            for (const QVariant& item : channelsFromResponse(body, this)) {
                const QVariantMap row = item.toMap();
                m_members.insert(row.value(QStringLiteral("id")).toString(), row);
            }
            const QString next = body.value(QStringLiteral("next")).toObject()
                                     .value(QStringLiteral("cursor")).toString();
            if (!next.isEmpty() && page + 1 < maximumMembershipPages) {
                requestMembershipPage(next, page + 1, generation);
                rebuildRows();
                hydrateMemberImages();
                return;
            }
            const bool wasBusy = busy();
            m_membershipBusy = false;
            // Treat exceeding the bounded page limit as incomplete, so no
            // non-verified candidate becomes inadvertently selectable.
            m_membershipReady = next.isEmpty();
            m_membershipValidUntil = nowMs() + membershipTtlMs;
            if (!m_membershipReady)
                setError(QStringLiteral("Channel membership list is incomplete"));
            emit membershipStateChanged();
            if (wasBusy != busy()) emit busyChanged();
            rebuildRows();
            hydrateMemberImages();
        });
}

void ChannelPickerController::requestTrending()
{
    if (m_trendingBusy || !m_networkEnabled) return;
    const quint64 generation = m_generation;
    const bool wasBusy = busy();
    m_trendingBusy = true;
    if (wasBusy != busy()) emit busyChanged();
    request(QStringLiteral("channel/trending"),
            {{QStringLiteral("limit"), 30}},
        [this, generation](const QJsonObject& body, const QString& error) {
            if (generation != m_generation) return;
            const bool wasBusyAtCompletion = busy();
            m_trendingBusy = false;
            if (wasBusyAtCompletion != busy()) emit busyChanged();
            if (error.isEmpty()) m_trendingResults = channelsFromResponse(body, this);
            rebuildRows();
        });
}

void ChannelPickerController::search(const QString& rawQuery)
{
    const QString next = rawQuery.trimmed().left(64);
    if (m_query == next) return;
    m_query = next;
    ++m_searchGeneration;
    m_debounce.stop();
    m_directoryResults.clear();
    if (m_searchBusy) setBusy(false);
    emit queryChanged();
    setError({});
    const auto cached = m_searchCache.constFind(m_query.toCaseFolded());
    if (cached != m_searchCache.cend() && cached->expiresAtMs > nowMs()) {
        m_searchResults = cached->results;
    } else {
        m_searchResults.clear();
        if (!m_query.isEmpty() && m_networkEnabled) m_debounce.start();
    }
    rebuildRows();
}

void ChannelPickerController::requestSearchNow()
{
    if (m_query.isEmpty() || !m_networkEnabled) return;
    const QString requestedQuery = m_query.toCaseFolded();
    const quint64 generation = m_generation;
    const quint64 searchGeneration = m_searchGeneration;
    setBusy(true);
    retrieveDirectoryResults(m_query, generation, searchGeneration);
    request(QStringLiteral("channel/search"),
            {{QStringLiteral("q"), m_query}, {QStringLiteral("limit"), searchLimit}},
        [this, generation, searchGeneration, requestedQuery](
            const QJsonObject& body, const QString& error) {
            if (generation != m_generation || searchGeneration != m_searchGeneration) return;
            setBusy(false);
            if (!error.isEmpty() || !body.contains(QStringLiteral("channels"))) {
                setError(QStringLiteral("Channel search unavailable"));
                rebuildRows();
                return;
            }
            m_searchResults = channelsFromResponse(body, this);
            m_searchCache.insert(requestedQuery, CachedSearch{
                .results = m_searchResults,
                .expiresAtMs = nowMs() + searchTtlMs,
            });
            if (m_searchCache.size() > 40) m_searchCache.clear();
            rebuildRows();
        });
}

int ChannelPickerController::scoreForQuery(const QVariantMap& channel) const
{
    if (m_query.isEmpty()) return 0;
    const QString q = m_query.toCaseFolded();
    const QString id = channel.value(QStringLiteral("id")).toString().toCaseFolded();
    const QString name = channel.value(QStringLiteral("name")).toString().toCaseFolded();
    if (q == id || q == name) return 0;
    if (id.startsWith(q) || name.startsWith(q)) return 1;
    if (id.contains(q) || name.contains(q)) return 2;
    if (nearDistance(q, id) <= 2 || nearDistance(q, name) <= 2) return 3;
    return 100;
}

void ChannelPickerController::rebuildRows()
{
    QVariantList list{homeRow()};
    QHash<QString, QVariantMap> candidates = m_members;
    const auto insertRows = [&candidates](const QVariantList& source) {
        for (const QVariant& item : source) {
            const QVariantMap row = item.toMap();
            const QString id = row.value(QStringLiteral("id")).toString();
            if (!id.isEmpty() && !candidates.contains(id)) candidates.insert(id, row);
        }
    };
    insertRows(m_searchResults);
    insertRows(m_directoryResults);
    // Never pollute the default selection list with arbitrary trending assets.

    QList<QVariantMap> found;
    for (auto it = candidates.cbegin(); it != candidates.cend(); ++it) {
        QVariantMap row = it.value();
        const QString id = it.key();
        // A positive membership entry is verified immediately, even while
        // later pages are loading. An incomplete directory must not prevent
        // selecting channels already confirmed for this FID.
        const bool isJoined = m_viewerFid > 0 && m_members.contains(id);
        const int relevance = scoreForQuery(row);
        if (!isSocialChannel(id) || relevance >= 100) continue;
        // URL must be canonical from the selected node's channel response.
        // Directory-only rows are visible but never publishable.
        if (isJoined)
            row.insert(QStringLiteral("parentUrl"),
                       m_members.value(id).value(QStringLiteral("parentUrl")));
        mergeMetadata(row);
        row.insert(QStringLiteral("isMember"), isJoined);
        row.insert(QStringLiteral("selectable"), isJoined
            && validParentUrl(row.value(QStringLiteral("parentUrl")).toString()));
        row.insert(QStringLiteral("usedCount"), m_usage.value(id));
        row.insert(QStringLiteral("matchRank"), relevance);
        found.append(row);
    }
    std::stable_sort(found.begin(), found.end(), [](const QVariantMap& a, const QVariantMap& b) {
        const int rankA = a.value(QStringLiteral("matchRank")).toInt();
        const int rankB = b.value(QStringLiteral("matchRank")).toInt();
        if (rankA != rankB) return rankA < rankB;
        const bool memberA = a.value(QStringLiteral("isMember")).toBool();
        const bool memberB = b.value(QStringLiteral("isMember")).toBool();
        if (memberA != memberB) return memberA;
        const int countA = a.value(QStringLiteral("usedCount")).toInt();
        const int countB = b.value(QStringLiteral("usedCount")).toInt();
        if (countA != countB) return countA > countB;
        return QString::localeAwareCompare(
            a.value(QStringLiteral("name")).toString(),
            b.value(QStringLiteral("name")).toString()) < 0;
    });
    for (const QVariantMap& row : std::as_const(found)) {
        if (list.size() >= maximumRows) break;
        list.append(row);
    }
    if (list == m_rows) return;
    m_rows = std::move(list);
    emit rowsChanged();
}

QVariantMap ChannelPickerController::chooseChannel(const QString& id)
{
    if (id.isEmpty()) {
        const QVariantMap home = homeRow();
        const quint64 generation = m_generation;
        QTimer::singleShot(0, this, [this, home, generation] {
            if (generation == m_generation) emit channelChosen(home);
        });
        return home;
    }
    if (m_viewerFid == 0) return {};
    const auto it = m_members.constFind(id);
    if (it == m_members.cend()) return {};
    QVariantMap row = it.value();
    mergeMetadata(row);
    if (!validParentUrl(row.value(QStringLiteral("parentUrl")).toString())) return {};
    m_usage.insert(id, std::min(m_usage.value(id) + 1, 1'000'000));
    saveUsage();
    QVariantMap selected = row;
    selected.insert(QStringLiteral("selectable"), true);
    selected.insert(QStringLiteral("isMember"), true);
    // Changing rows here destroys the QML ListView delegate inside onClicked
    // before it can apply the selection. Emit from the next event loop turn.
    const quint64 generation = m_generation;
    QTimer::singleShot(0, this, [this, selected, generation] {
        if (generation != m_generation) return;
        emit channelChosen(selected);
        rebuildRows();
    });
    return selected;
}
