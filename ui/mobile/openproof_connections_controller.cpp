#include "openproof_connections_controller.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QSettings>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

#include <algorithm>
#include <utility>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

// Opening the popup must happen in the original user-click stack: opening
// after the asynchronous OpenProof handoff would be blocked in many browsers.
// Neither tokens nor account claims enter JavaScript; only an opaque handoff
// destination is passed after the backend creates its one-use ticket.
EM_JS(int, prepareGenycasterConnectionWindow, (), {
    try {
        const previous = window.__genycasterConnectionApproval;
        if (previous && !previous.closed) previous.close();
        const page = window.open('about:blank', '_blank', 'width=540,height=700');
        if (!page) return 0;
        page.opener = null;
        page.document.title = 'Genycaster - Connecting account';
        page.document.body.textContent = 'Connecting your account securely…';
        window.__genycasterConnectionApproval = page;
        return 1;
    } catch (_) {
        return 0;
    }
});
EM_JS(int, navigateGenycasterConnectionWindow, (const char* target), {
    try {
        const page = window.__genycasterConnectionApproval;
        if (!page || page.closed) return 0;
        page.location.replace(UTF8ToString(target));
        return 1;
    } catch (_) {
        return 0;
    }
});
EM_JS(void, closeGenycasterConnectionWindow, (), {
    const page = window.__genycasterConnectionApproval;
    window.__genycasterConnectionApproval = null;
    if (page && !page.closed) {
        try { page.close(); } catch (_) {}
    }
});
#endif

namespace {

[[nodiscard]] QUrl configuredUrl(const char* overrideName, const QString& jsonKey)
{
    const QString overridden = qEnvironmentVariable(overrideName).trimmed();
    if (!overridden.isEmpty()) {
        return QUrl{overridden};
    }
    QFile config{QStringLiteral(":/genycaster-config/public_endpoints.json")};
    if (!config.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto doc = QJsonDocument::fromJson(config.readAll());
    return QUrl{doc.object().value(jsonKey).toString()};
}

[[nodiscard]] bool trustedHttpsEndpoint(const QUrl& url)
{
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && !url.host().isEmpty() && url.userInfo().isEmpty()
        && url.fragment().isEmpty() && url.port() == -1;
}

// QNetworkReply::abort() may emit finished() synchronously on Qt/macOS.
// Detach the QPointer and its callbacks before calling abort(), so neither
// a reentrant completion nor a cancelled request can overwrite another
// account's session or invalidate the pointer used for deferred deletion.
void cancelNetworkReply(QPointer<QNetworkReply>& tracked, QObject* receiver)
{
    QPointer<QNetworkReply> pending = tracked;
    tracked.clear();
    if (!pending) return;
    QObject::disconnect(pending.data(), nullptr, receiver, nullptr);
    pending->abort();
    if (pending) pending->deleteLater();
}

[[nodiscard]] bool validProviderName(const QString& value)
{
    static const QRegularExpression allowed{QStringLiteral("^[a-z][a-z0-9-]{0,39}$")};
    return allowed.match(value).hasMatch();
}

[[nodiscard]] QVariantList offlineTopicCatalog()
{
    QVariantList result;
    for (const auto& slug : {QStringLiteral("art"), QStringLiteral("development"),
             QStringLiteral("photography"), QStringLiteral("design"),
             QStringLiteral("technology"), QStringLiteral("music"),
             QStringLiteral("writing"), QStringLiteral("science"),
             QStringLiteral("business"), QStringLiteral("gaming")}) {
        result.push_back(QVariantMap{{QStringLiteral("slug"), slug}});
    }
    return result;
}

} // namespace

OpenProofConnectionsController::OpenProofConnectionsController(QObject* parent, QSettings* settings)
    : QObject(parent),
      m_settings(settings),
      m_cloudUrl(configuredUrl("GENYCASTER_CLOUD_API_URL",
          QStringLiteral("genycasterCloudApiUrl"))),
      m_browserConnectionBaseUrl(configuredUrl("GENYCASTER_OPENPROOF_BROWSER_CONNECTION_BASE_URL",
          QStringLiteral("openproofBrowserConnectionBaseUrl"))),
      m_catalogUrl(configuredUrl("GENYCASTER_OPENPROOF_PROVIDERS_URL",
          QStringLiteral("openproofProviderCatalogUrl"))),
      m_issuerUrl(configuredUrl("GENYCASTER_OPENPROOF_ISSUER_URL",
          QStringLiteral("openproofIssuerUrl")))
{
    m_gatewayUrl = configuredUrl("GENYCASTER_OPENPROOF_CONNECTIONS_GATEWAY_URL",
        QStringLiteral("openproofConnectionsGatewayUrl"));
    m_grantExpiry.setSingleShot(true);
    connect(&m_grantExpiry, &QTimer::timeout, this, [this] {
        clearAccountGrant();
        restoreCachedConnections();
        if(m_cloudSessionVerified)m_bookmarkPoll.start();
        m_error.clear();
        emit stateChanged();
    });
    m_bookmarkPoll.setInterval(25'000);
    connect(&m_bookmarkPoll, &QTimer::timeout, this, [this] {
        if(!m_cloudSessionVerified && !m_sessionRequest
            && !m_accountBearer.isEmpty()
            && (m_verified || m_accountBearer.startsWith("recovery.")))
            bootstrapAccountSession();
        if (!m_cloudServiceAvailable) checkCloudService();
        else {
            // Connections are account data too: poll for changes made on
            // another signed-in device, not only at first authorization.
            m_linkedInReconciled = false;
            reconcileLinkedIn();
            publishPortableConnectionLabels();
            refreshAccountConnectionsSnapshot();
            pumpBookmarkChanges();
            pumpDraftChanges();
            pumpTopicWrites();
            pumpStreakWrites();
            // Rehydration may have raced the initial account view. Retry a
            // missing server snapshot on the normal account-data poll.
            if (m_streak.isEmpty() && !m_streakRead && !m_streakWrite)
                fetchStreak();
        }
    });
    m_connectionPoll.setInterval(3'000);
    connect(&m_connectionPoll, &QTimer::timeout, this, [this] {
        if (!m_verified || m_pendingProvider.isEmpty()
            || --m_remainingPolls < 0) {
            finishAction(QStringLiteral("Connection confirmation timed out. Refresh to check status."));
            return;
        }
        refreshConnections();
    });
    // A safe, last-known public capability catalog, not linked-account status.
    // These are public-facing social reputation links, not sign-in methods.
    // Google, wallet and Farcaster identity remain managed by OpenProof and
    // are not advertised as a public social profile.
    m_providers = {
        QStringLiteral("x"), QStringLiteral("github"),
        QStringLiteral("linkedin"), QStringLiteral("telegram")
    };
    // These are display-only offline suggestions. A verified account's
    // PostgreSQL catalog replaces them, but changing viewers must never
    // strand the composer with an empty default Qt "unavailable" surface.
    m_topicCatalog = offlineTopicCatalog();

}

QStringList OpenProofConnectionsController::providers() const { return m_providers; }
bool OpenProofConnectionsController::loading() const noexcept { return m_loading; }
bool OpenProofConnectionsController::catalogFallback() const noexcept { return m_catalogFallback; }
QString OpenProofConnectionsController::error() const { return m_error; }
bool OpenProofConnectionsController::authorized() const noexcept { return m_verified; }
bool OpenProofConnectionsController::authorizing() const noexcept { return m_authorizing; }
QString OpenProofConnectionsController::approvalUrl() const { return m_approvalUrl; }
QVariantList OpenProofConnectionsController::connections() const { return m_connections; }
bool OpenProofConnectionsController::actionBusy() const noexcept { return m_actionBusy; }
bool OpenProofConnectionsController::cachedSnapshot() const noexcept { return m_cachedSnapshot; }
QString OpenProofConnectionsController::pendingProvider() const { return m_pendingProvider; }

QString OpenProofConnectionsController::manualLinkedInSettingKey() const
{
    if (m_accountSubject.isEmpty() || !trustedHttpsEndpoint(m_issuerUrl)) return {};
    const QByteArray owner = m_issuerUrl.toString(QUrl::FullyEncoded).toUtf8()
        + ':' + m_accountSubject.toUtf8();
    return QStringLiteral("genycaster/manualLinkedIn/")
        + QString::fromLatin1(QCryptographicHash::hash(owner, QCryptographicHash::Sha256).toHex());
}

QString OpenProofConnectionsController::manualLinkedInUrl() const
{
    if (!m_verified || !isConnected(QStringLiteral("linkedin")) || !m_settings) return {};
    const QString key = manualLinkedInSettingKey();
    return key.isEmpty() ? QString{} : m_settings->value(key).toString();
}

void OpenProofConnectionsController::setManualLinkedInUrl(const QString& candidate)
{
    if (!m_verified || !isConnected(QStringLiteral("linkedin")) || !m_settings) return;
    const QString value = candidate.trimmed();
    const QUrl parsed{value};
    static const QRegularExpression validPath{QStringLiteral("^/in/[A-Za-z0-9_-]{3,100}/?$")};
    if (!value.isEmpty() && !(parsed.isValid() && parsed.scheme() == QStringLiteral("https")
        && parsed.host() == QStringLiteral("www.linkedin.com")
        && parsed.port() == -1 && parsed.userInfo().isEmpty()
        && parsed.query().isEmpty() && parsed.fragment().isEmpty()
        && validPath.match(parsed.path()).hasMatch())) {
        m_error = QStringLiteral("Enter the full LinkedIn URL: https://www.linkedin.com/in/username");
        emit stateChanged();
        return;
    }
    const QString key = manualLinkedInSettingKey();
    if (key.isEmpty()) return;
    if (value.isEmpty()) m_settings->remove(key);
    else m_settings->setValue(key, QStringLiteral("https://www.linkedin.com") + parsed.path());
    m_settings->sync();
    if (!m_applyingCloudLinkedIn)
        m_settings->setValue(key + QStringLiteral(".cloudDirty"), true);
    for (auto& entry : m_connections) {
        auto item = entry.toMap();
        if (item.value(QStringLiteral("provider")).toString() != QStringLiteral("linkedin")) continue;
        item[QStringLiteral("manualProfileUrl")] = value.isEmpty()
            ? QString{} : QStringLiteral("https://www.linkedin.com") + parsed.path();
        entry = item;
    }
    saveCachedConnections();
    m_error.clear();
    emit stateChanged();
    // Every local edit is durable first, then propagated automatically.
    // Offline changes remain queued under the verified account.
    if (!m_applyingCloudLinkedIn && m_cloudServiceAvailable
        && !m_cloudRequest) uploadManualLinkedInCloud();
}


void OpenProofConnectionsController::authorizeCloudRequest(
    QNetworkRequest& request) const
{
    if (m_cloudSessionVerified && !m_cloudCsrf.isEmpty())
        request.setRawHeader("X-Genycaster-CSRF", m_cloudCsrf);
    else if (!m_accountBearer.isEmpty()
             && (m_verified || m_accountBearer.startsWith("recovery.")))
        request.setRawHeader("Authorization", "Bearer " + m_accountBearer);
}

void OpenProofConnectionsController::restoreAccountSession()
{
    if (!m_viewerFid || !trustedHttpsEndpoint(m_cloudUrl)
        || m_sessionRevoking || m_sessionRequest) return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/session"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    auto* reply=m_network.get(request);
    m_sessionRequest=reply;
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,viewer] {
        // An obsolete request can never adopt a new viewer/session.
        if (m_sessionRequest != reply) {
            reply->deleteLater();
            return;
        }
        m_sessionRequest.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // A previous account-session GET may be cancelled when a freshly
        // verified recovery bearer arrives. Do not read a closed reply:
        // QNetworkReply::abort() can synchronously emit finished() on macOS.
        const auto bytes=status==200 && reply->isReadable()
            && reply->bytesAvailable()<8192 ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if(generation!=m_grantGeneration || viewer!=m_viewerFid || status!=200)
            return;
        const auto doc=QJsonDocument::fromJson(bytes);
        if(!doc.isObject())return;
        const auto data=doc.object();
        const auto csrf=data.value(QStringLiteral("csrf_token")).toString().toUtf8();
        if(data.value(QStringLiteral("fid")).toString()!=QString::number(viewer)
            || csrf.size()!=43)return;
        m_cloudSessionVerified=true;
        m_cloudCsrf=csrf;
        fetchTopics();
        refreshBookmarkCollections();
        m_bookmarkPoll.start();
        publishPortableConnectionLabels();
        if(!m_portableConnectionsRequest)refreshAccountConnectionsSnapshot();
        fetchStreak();
        fetchStreakPreferences();
        emit stateChanged();
        if(m_cloudServiceAvailable) {
            pumpBookmarkChanges();
            pumpNotificationSeenAt();
            pumpDraftChanges();
        } else checkCloudService();
    });
}

void OpenProofConnectionsController::cancelAccountSessionRequest()
{
    cancelNetworkReply(m_sessionRequest, this);
}

void OpenProofConnectionsController::bootstrapAccountSession()
{
    if(!m_viewerFid || m_accountBearer.isEmpty()
        || !(m_verified || m_accountBearer.startsWith("recovery."))
        || m_sessionRevoking || m_cloudSessionVerified
        || !trustedHttpsEndpoint(m_cloudUrl))return;
    cancelAccountSessionRequest();
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/session"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    request.setRawHeader("Authorization","Bearer "+m_accountBearer);
    auto* reply=m_network.post(request,QByteArray{});
    m_sessionRequest=reply;
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,viewer] {
        // An obsolete request can never adopt a new viewer/session.
        if (m_sessionRequest != reply) {
            reply->deleteLater();
            return;
        }
        m_sessionRequest.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // A previous account-session GET may be cancelled when a freshly
        // verified recovery bearer arrives. Do not read a closed reply:
        // QNetworkReply::abort() can synchronously emit finished() on macOS.
        const auto bytes=status==200 && reply->isReadable()
            && reply->bytesAvailable()<8192 ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if(generation!=m_grantGeneration || viewer!=m_viewerFid || status!=200)
            return;
        const auto doc=QJsonDocument::fromJson(bytes);
        if(!doc.isObject())return;
        const auto data=doc.object();
        const auto csrf=data.value(QStringLiteral("csrf_token")).toString().toUtf8();
        if(data.value(QStringLiteral("fid")).toString()!=QString::number(viewer)
            || csrf.size()!=43)return;
        // A successful Set-Cookie response is not proof that a browser
        // actually retained an HttpOnly cookie (privacy policy or storage
        // restrictions may reject it). Require a same-origin GET roundtrip.
        m_cloudSessionVerified=false;
        m_cloudCsrf.clear();
        emit stateChanged();
        restoreAccountSession();
    });
}

void OpenProofConnectionsController::acceptBrowserAccountSession(
    const qulonglong fid, QByteArray csrf)
{
    static const QRegularExpression csrfFormat{
        QStringLiteral("^[A-Za-z0-9_-]{43}$")};
    if(!fid || fid!=m_viewerFid || !csrfFormat.match(
            QString::fromLatin1(csrf)).hasMatch()) {
        csrf.fill('\0');
        return;
    }
    // The browser only emits this after a verified signed challenge AND a
    // same-origin GET that proves the HttpOnly cookie was actually stored.
    m_cloudSessionVerified=true;
    m_cloudCsrf=std::move(csrf);
    fetchTopics();
    refreshBookmarkCollections();
    m_bookmarkPoll.start();
    publishPortableConnectionLabels();
    if(!m_portableConnectionsRequest)refreshAccountConnectionsSnapshot();
    fetchStreak();
    fetchStreakPreferences();
    emit stateChanged();
    if(m_cloudServiceAvailable) {
        pumpBookmarkChanges();
        pumpNotificationSeenAt();
        pumpDraftChanges();
        pumpTopicWrites();
        pumpStreakWrites();
    } else checkCloudService();
}

void OpenProofConnectionsController::revokeAccountSession()
{
    if(!m_viewerFid || !m_cloudSessionVerified || m_cloudCsrf.isEmpty()
        || !trustedHttpsEndpoint(m_cloudUrl))return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/session"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    request.setRawHeader("X-Genycaster-CSRF",m_cloudCsrf);
    m_sessionRevoking=true;
    auto* reply=m_network.deleteResource(request);
    connect(reply,&QNetworkReply::finished,this,[this,reply] {
        reply->deleteLater();
        m_sessionRevoking=false;
        // Never allow an old account's late cookie clear to race a newly
        // verified account's Set-Cookie response during account switching.
        if(!m_viewerFid)return;
        if(m_verified && !m_accountBearer.isEmpty())bootstrapAccountSession();
        else restoreAccountSession();
    });
}

void OpenProofConnectionsController::checkCloudService()
{
    if (m_cloudProbe || !trustedHttpsEndpoint(m_cloudUrl)
        || m_cloudUrl.path() != QStringLiteral("/api/v1/cloud/profile_extras")) return;
    QUrl probe = m_cloudUrl;
    probe.setPath(QStringLiteral("/api/v1/cloud/app_updates"));
    auto request = QNetworkRequest{probe};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(6'000);
    request.setRawHeader("Accept", "application/json");
    auto* reply = m_network.get(request);
    m_cloudProbe = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_cloudProbe == reply) m_cloudProbe = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto payload = reply->bytesAvailable() <= 12 * 1024
            ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        const auto doc = QJsonDocument::fromJson(payload);
        // Production SPA fallbacks can return HTTP 200 with HTML. Never
        // mistake that for a configured, healthy cloud JSON API.
        m_cloudServiceAvailable = status == 200 && doc.isObject()
            && doc.object().value(QStringLiteral("items")).isArray();
        emit cloudStateChanged();
        if (m_cloudServiceAvailable && cloudAuthorizationReady()) {
            m_bookmarkPoll.start();
            reconcileLinkedIn();
            refreshAccountConnectionsSnapshot();
            pumpBookmarkChanges();
            pumpDraftChanges();
            pumpTopicWrites();
            pumpStreakWrites();
            // Rehydration may have raced the initial account view. Retry a
            // missing server snapshot on the normal account-data poll.
            if (m_streak.isEmpty() && !m_streakRead && !m_streakWrite)
                fetchStreak();
        }
    });
}

QString OpenProofConnectionsController::bookmarkStorageKey() const
{
    if (!m_viewerFid) return {};
    return QStringLiteral("genycaster/bookmark-service/fid_%1").arg(m_viewerFid);
}

void OpenProofConnectionsController::recordBookmarkMutation(
    const QString& castId, const bool deleted, const QVariantMap& snapshot)
{
    if (!m_settings || !m_viewerFid) return;
    static const QRegularExpression valid{
        QStringLiteral("^0x[0-9a-fA-F]{40}$")};
    if (!valid.match(castId).hasMatch()) return;
    const auto priority=std::clamp(snapshot.value(QStringLiteral("priority")).toInt(),0,2);
    QVariantMap preview;
    if(!deleted) {
        for(const auto& key : {QStringLiteral("authorFid"),QStringLiteral("author"),
            QStringLiteral("username"),QStringLiteral("displayName"),
            QStringLiteral("avatarUrl"),QStringLiteral("createdAtMs"),
            QStringLiteral("text")}) {
            if(snapshot.contains(key))preview.insert(key,snapshot.value(key));
        }
        const QString image=preview.value(QStringLiteral("avatarUrl")).toString();
        if(!image.isEmpty() && (!image.startsWith(QStringLiteral("https://"))
                               || image.size()>512))
            preview.remove(QStringLiteral("avatarUrl"));
    }
    const QVariantMap item{
        {QStringLiteral("cast_hash"), castId.toLower()},
        {QStringLiteral("is_deleted"), deleted},
        {QStringLiteral("priority"), deleted ? 0 : priority},
        {QStringLiteral("preview"), preview.isEmpty() ? QVariant{} : QVariant{preview}},
        {QStringLiteral("mutation_id"), QUuid::createUuid().toString(QUuid::WithoutBraces)}
    };
    m_bookmarkOutbox.push_back(item);
    m_bookmarkError.clear();
    m_settings->setValue(bookmarkStorageKey() + QStringLiteral("/outbox"), m_bookmarkOutbox);
    m_settings->sync();
    emit cloudStateChanged();
#if defined(__EMSCRIPTEN__)
    EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
    pumpBookmarkChanges();
}

void OpenProofConnectionsController::migrateLocalBookmarks(const QStringList& ids)
{
    if (!m_settings || !m_viewerFid
        || !m_cloudServiceAvailable || !cloudAuthorizationReady()) return;
    const QString key=bookmarkStorageKey() + QStringLiteral("/migrated");
    if (m_settings->value(key).toBool()) return;
    if (m_legacyBookmarksPending) return;
    // Never blindly upload old device bookmarks: another device might have
    // deleted one already. First replay *all* server receipts, including
    // tombstones, then enqueue only hashes unseen by the service.
    m_legacyBookmarksPending=true;
    m_legacyBookmarkCandidates=ids;
    m_bookmarkHistoryKeys.clear();
    m_bookmarkCursor=QStringLiteral("0");
    pumpBookmarkChanges();
}

void OpenProofConnectionsController::pumpBookmarkChanges()
{
    if (!m_settings || !m_viewerFid
        || !cloudAuthorizationReady() || !m_cloudServiceAvailable
        || m_cloudRequest || !trustedHttpsEndpoint(m_cloudUrl)) return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/bookmarks"));
    const bool uploading=!m_bookmarkOutbox.isEmpty();
    const QString fromCursor=uploading ? QString{} : m_bookmarkCursor;
    if (!uploading) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("since"),m_bookmarkCursor);
        url.setQuery(query);
    }
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    QVariantMap pending;
    QNetworkReply* reply{};
    if (uploading) {
        pending=m_bookmarkOutbox.constFirst().toMap();
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json"));
        reply=m_network.put(request,
              QJsonDocument{QJsonObject::fromVariantMap(pending)}.toJson(QJsonDocument::Compact));
    } else {
        reply=m_network.get(request);
    }
    m_cloudRequest=reply;
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,
            [this,reply,generation,viewer,uploading,pending,fromCursor] {
        if (m_cloudRequest==reply) m_cloudRequest=nullptr;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable() <= 100 * 1024
            ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if (generation!=m_grantGeneration || viewer!=m_viewerFid) return;
        if (status!=200) {
            // A rejected preview used to block the entire durable queue
            // forever. Retry its original toggle without optional artwork;
            // retain the canonical hash, priority and idempotency key.
            if (uploading && (status==400 || status==413) &&
                    !m_bookmarkOutbox.isEmpty() &&
                    m_bookmarkOutbox.constFirst().toMap().value(
                        QStringLiteral("mutation_id"))==pending.value(
                        QStringLiteral("mutation_id"))) {
                auto first=m_bookmarkOutbox.first().toMap();
                if (first.value(QStringLiteral("preview")).isValid() &&
                    !first.value(QStringLiteral("preview")).isNull()) {
                    first.remove(QStringLiteral("preview"));
                    m_bookmarkOutbox[0]=first;
                    m_settings->setValue(bookmarkStorageKey()+
                        QStringLiteral("/outbox"),m_bookmarkOutbox);
                    m_settings->sync();
                    QMetaObject::invokeMethod(this,[this]{pumpBookmarkChanges();},
                                              Qt::QueuedConnection);
                    return;
                }
            }
            m_bookmarkError=(status==401 || status==403)
                ? QStringLiteral("Bookmark account authorization expired. Your changes are saved on this device.")
                : (status==400 || status==413)
                ? QStringLiteral("A saved cast was rejected by the account API; its local copy is preserved.")
                : QStringLiteral("Cannot update bookmarks on the account server. Changes are kept locally.");
            emit cloudStateChanged();
            return; // Never discard an unacknowledged bookmark mutation.
        }
        m_bookmarkError.clear();
        emit cloudStateChanged();
        const auto json=QJsonDocument::fromJson(bytes);
        if (!json.isObject()) return;
        if (uploading) {
            if (!json.object().value(QStringLiteral("item")).isObject()
                || m_bookmarkOutbox.isEmpty()
                || m_bookmarkOutbox.constFirst().toMap()
                     .value(QStringLiteral("mutation_id"))!=pending.value(QStringLiteral("mutation_id")))
                return;
            m_bookmarkOutbox.removeFirst();
            emit cloudStateChanged();
            m_settings->setValue(bookmarkStorageKey()+QStringLiteral("/outbox"),m_bookmarkOutbox);
            m_settings->sync();
#if defined(__EMSCRIPTEN__)
            EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
            QMetaObject::invokeMethod(this,[this] { pumpBookmarkChanges(); },
                                      Qt::QueuedConnection);
        } else {
            const auto items=json.object().value(QStringLiteral("items"));
            const QString next=json.object().value(QStringLiteral("next_cursor")).toString();
            static const QRegularExpression cursorFormat{
                QStringLiteral("^(0|[1-9][0-9]{0,17})$")};
            if (!items.isArray() || !cursorFormat.match(next).hasMatch()) return;
            if (m_legacyBookmarksPending && fromCursor!=QStringLiteral("0")
                && m_bookmarkHistoryKeys.isEmpty()) {
                // Initial legacy reconciliation began while another cursor
                // request was in flight. Restart from zero, not from an
                // incomplete tail that could miss older deletion receipts.
                QMetaObject::invokeMethod(this,[this]{pumpBookmarkChanges();},
                                          Qt::QueuedConnection);
                return;
            }
            QVariantList changes{};
            // A GET may have started before a local toggle. Its server-side
            // deletion/addition receipt is then older than our queued edit.
            // Never replay it over the optimistic bookmark state. Advance
            // the cursor normally; the outbox later commits the newer edit.
            QSet<QString> pendingHashes;
            for (const auto& outboxEntry : std::as_const(m_bookmarkOutbox))
                pendingHashes.insert(outboxEntry.toMap()
                    .value(QStringLiteral("cast_hash")).toString().toLower());
            for (const auto& value : items.toArray()) {
                if (!value.isObject()) continue;
                const QVariantMap change=value.toObject().toVariantMap();
                const QString hash=change.value(QStringLiteral("cast_hash"))
                    .toString().toLower();
                if (m_legacyBookmarksPending)
                    m_bookmarkHistoryKeys.insert(hash);
                if (pendingHashes.contains(hash)) continue;
                changes.push_back(change);
            }
            // Synchronously update the per-account local reading list before
            // advancing the cursor; a crash can replay but cannot lose rows.
            emit cloudBookmarkChangesReceived(changes);
            const QString previous=m_bookmarkCursor;
            m_bookmarkCursor=next;
            if (next!=previous) {
                m_settings->setValue(bookmarkStorageKey()+QStringLiteral("/cursor"),next);
                m_settings->sync();
#if defined(__EMSCRIPTEN__)
                EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
            }
            if (items.toArray().size()>=200 && next!=previous) {
                QMetaObject::invokeMethod(this,[this] { pumpBookmarkChanges(); },
                                          Qt::QueuedConnection);
                return;
            }
            if (m_legacyBookmarksPending) {
                static const QRegularExpression validHash{
                    QStringLiteral("^0x[0-9a-f]{40}$")};
                for (const auto& original : std::as_const(m_legacyBookmarkCandidates)) {
                    const QString hash=original.toLower();
                    if (!validHash.match(hash).hasMatch()
                        || m_bookmarkHistoryKeys.contains(hash)) continue;
                    bool queued=false;
                    for (const auto& pendingValue : std::as_const(m_bookmarkOutbox)) {
                        if (pendingValue.toMap().value(QStringLiteral("cast_hash"))
                                .toString()==hash) { queued=true; break; }
                    }
                    if (!queued) m_bookmarkOutbox.push_back(QVariantMap{
                        {QStringLiteral("cast_hash"),hash},
                        {QStringLiteral("is_deleted"),false},
                        {QStringLiteral("mutation_id"),
                         QUuid::createUuid().toString(QUuid::WithoutBraces)}
                    });
                }
                m_legacyBookmarksPending=false;
                m_legacyBookmarkCandidates.clear();
                m_bookmarkHistoryKeys.clear();
                m_settings->setValue(bookmarkStorageKey()+QStringLiteral("/migrated"),true);
                m_settings->setValue(bookmarkStorageKey()+QStringLiteral("/outbox"),m_bookmarkOutbox);
                m_settings->sync();
#if defined(__EMSCRIPTEN__)
                EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
            }
            if (!m_bookmarkOutbox.isEmpty())
                QMetaObject::invokeMethod(this,[this] { pumpBookmarkChanges(); },
                                          Qt::QueuedConnection);
            else
                QMetaObject::invokeMethod(this,[this] { pumpNotificationSeenAt(); },
                                          Qt::QueuedConnection);
        }
    });
}

void OpenProofConnectionsController::recordNotificationSeenAt(
    const qulonglong fid, const qlonglong observedAtMs)
{
    if (!m_settings || !m_viewerFid || fid != m_viewerFid
        || observedAtMs <= m_notificationSeenAtMs) return;
    m_notificationSeenAtMs=observedAtMs;
    m_settings->setValue(QStringLiteral("notifications/seen_at/%1").arg(fid),
                         m_notificationSeenAtMs);
    m_settings->sync();
#if defined(__EMSCRIPTEN__)
    EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
    pumpNotificationSeenAt();
}

void OpenProofConnectionsController::pumpNotificationSeenAt(const bool upload)
{
    if (!m_viewerFid || !m_settings || !cloudAuthorizationReady()
        || !m_cloudServiceAvailable || m_cloudRequest
        || !trustedHttpsEndpoint(m_cloudUrl)) return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/notification_seen"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    const qlonglong uploadedAt=upload ? m_notificationSeenAtMs : 0;
    QNetworkReply* reply{};
    if (upload) {
        if (m_notificationSeenAtMs <= m_notificationRemoteAtMs) return;
        // Only the timestamp is submitted. The account API owns the
        // reserved record and atomically keeps the maximum across devices.
        const QJsonObject body{
            {QStringLiteral("seen_at_ms"),QString::number(uploadedAt)}
        };
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json"));
        reply=m_network.put(request,QJsonDocument{body}.toJson(QJsonDocument::Compact));
    } else {
        reply=m_network.get(request);
    }
    m_cloudRequest=reply;
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,
        [this,reply,generation,viewer,upload,uploadedAt] {
        if (m_cloudRequest==reply) m_cloudRequest=nullptr;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=64*1024
            ? reply->readAll():QByteArray{};
        reply->deleteLater();
        if (generation!=m_grantGeneration || viewer!=m_viewerFid) return;
        if (status!=200) return;
        const auto doc=QJsonDocument::fromJson(bytes);
        if (!doc.isObject()) return;
        // Send millisecond integers as strings: no floating-point rounding
        // at the wire/SQL boundary, and no client-controlled account key.
        const QString raw=doc.object().value(QStringLiteral("seen_at_ms")).toString();
        static const QRegularExpression validStamp{
            QStringLiteral("^(0|[1-9][0-9]{0,15})$")};
        if (!validStamp.match(raw).hasMatch()) return;
        bool valid=false;
        const auto observed=raw.toLongLong(&valid);
        if (!valid || observed<0) return;
        m_notificationRemoteAtMs=observed;
        if (m_notificationRemoteAtMs>m_notificationSeenAtMs) {
            m_notificationSeenAtMs=m_notificationRemoteAtMs;
            m_settings->setValue(QStringLiteral("notifications/seen_at/%1").arg(viewer),
                                 m_notificationSeenAtMs);
            m_settings->sync();
#if defined(__EMSCRIPTEN__)
            EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
            emit cloudNotificationSeenAtReceived(viewer,m_notificationSeenAtMs);
        }
        if (m_notificationSeenAtMs>m_notificationRemoteAtMs
            && (!upload || uploadedAt!=m_notificationSeenAtMs))
            QMetaObject::invokeMethod(this,[this]{pumpNotificationSeenAt(true);},
                                      Qt::QueuedConnection);
    });
}

QString OpenProofConnectionsController::draftStorageKey() const
{
    return m_viewerFid
        ? QStringLiteral("genycaster/draft-service/fid_%1").arg(m_viewerFid)
        : QString{};
}

void OpenProofConnectionsController::recordDraftMutation(
    const qulonglong fid, const bool deleted)
{
    if (!m_settings || !fid || fid!=m_viewerFid) return;
    const auto key=draftStorageKey();
    m_draftMutationSerial++;
    m_settings->setValue(key+QStringLiteral("/serial"),m_draftMutationSerial);
    m_settings->setValue(key+QStringLiteral("/dirty"),true);
    m_settings->setValue(key+QStringLiteral("/deleted"),deleted);
    m_settings->sync();
#if defined(__EMSCRIPTEN__)
    EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
    if (m_draftRemoteInitialized && !m_draftConflict) uploadDraftChange();
    else pumpDraftChanges();
}

void OpenProofConnectionsController::confirmDraftSnapshot(
    const qulonglong fid, const int revision)
{
    if (!m_settings || !fid || fid!=m_viewerFid || revision<=0
        || m_settings->value(draftStorageKey()+QStringLiteral("/dirty")).toBool())
        return;
    m_draftRevision=revision;
    m_settings->setValue(draftStorageKey()+QStringLiteral("/revision"),revision);
    m_settings->setValue(draftStorageKey()+QStringLiteral("/seen"),true);
    m_settings->sync();
#if defined(__EMSCRIPTEN__)
    EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
}

void OpenProofConnectionsController::refreshAccountDraft()
{
    // Explicit screen entry is not a mutation and never authorizes by FID
    // alone. This read is independent from the normal 25-second poll.
    pumpDraftChanges();
}

void OpenProofConnectionsController::pumpDraftChanges()
{
    if (!m_settings || !m_viewerFid ||
        !cloudAuthorizationReady() || !m_cloudServiceAvailable ||
        m_draftRequest || !trustedHttpsEndpoint(m_cloudUrl)) return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/drafts"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"),
                       QStringLiteral("00000000-0000-4000-8000-000000000002"));
    url.setQuery(query);
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* reply=m_network.get(request);
    m_draftRequest=reply;
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,viewer] {
        if (m_draftRequest==reply) m_draftRequest=nullptr;
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=384*1024
            ? reply->readAll():QByteArray{};
        reply->deleteLater();
        if (generation!=m_grantGeneration || viewer!=m_viewerFid
            || status!=200) return;
        const auto response=QJsonDocument::fromJson(bytes);
        if (!response.isObject() ||
            !response.object().value(QStringLiteral("items")).isArray()) return;
        const auto key=draftStorageKey();
        constexpr auto draftId="00000000-0000-4000-8000-000000000002";
        QJsonObject remote;
        for (const auto& value:response.object().value(QStringLiteral("items")).toArray()) {
            if (!value.isObject()) continue;
            const auto row=value.toObject();
            if (row.value(QStringLiteral("id")).toString()==QString::fromLatin1(draftId)) {
                remote=row;
                break;
            }
        }
        if (!remote.isEmpty()
            && remote.value(QStringLiteral("kind")).toString()!=QStringLiteral("cast")) {
            m_draftConflict=true;
            return; // A foreign record cannot be silently replaced by this adapter.
        }
        const int remoteRevision=remote.isEmpty()?0:
            remote.value(QStringLiteral("revision")).toInt(-1);
        if (remoteRevision<0) return;
        const bool deleted=!remote.isEmpty()
            && !remote.value(QStringLiteral("deleted_at")).isNull();
        const auto localDocumentKey=QStringLiteral(
            "composer/accounts/fid_%1/richDraftV1").arg(viewer);
        const bool hasLocalDraft=m_settings->contains(localDocumentKey);
        bool dirty=m_settings->value(key+QStringLiteral("/dirty")).toBool();
        // The database is canonical for an existing account draft. A
        // pre-account local document or an older pending edit must never
        // prevent another device's newer revision from being displayed.
        // Preserve any differing local work *before* accepting the server
        // version. An active editor still refuses remote application.
        const bool seen=m_settings->value(key+QStringLiteral("/seen")).toBool();
        const auto previous=m_settings->value(localDocumentKey).toByteArray();
        const auto localDocument=QJsonDocument::fromJson(previous);
        const auto serverDocument=remote.value(QStringLiteral("content")).toObject()
            .value(QStringLiteral("document")).toObject();
        const bool differs=hasLocalDraft &&
            (deleted || !localDocument.isObject()
                || localDocument.object()!=serverDocument);
        const bool staleDirty=dirty && remoteRevision>m_draftRevision;
        if (remoteRevision==0 && !seen && hasLocalDraft && !dirty) {
            dirty=true;
            m_settings->setValue(key+QStringLiteral("/dirty"),true);
            m_settings->setValue(key+QStringLiteral("/deleted"),false);
            m_settings->sync();
        } else if (remoteRevision>0 &&
                   (!seen || staleDirty || (differs && remoteRevision==m_draftRevision))) {
            if (differs && !previous.isEmpty()) {
                const auto backupKey=key+QStringLiteral("/legacy-device-backup");
                if (!m_settings->contains(backupKey))
                    m_settings->setValue(backupKey,previous);
                if (staleDirty) {
                    const auto conflictKey=key+QStringLiteral("/conflict-device-backup/")
                        +QString::number(m_draftMutationSerial);
                    if (!m_settings->contains(conflictKey))
                        m_settings->setValue(conflictKey,previous);
                }
            }
            if (staleDirty) {
                // This older local version is safe in its own backup; it is
                // not entitled to overwrite a newer database document.
                dirty=false;
                m_draftConflict=false;
                m_settings->setValue(key+QStringLiteral("/dirty"),false);
            }
            m_settings->setValue(key+QStringLiteral("/seen"),true);
            m_settings->sync();
        }
        m_draftRemoteRevision=remoteRevision;
        m_draftRemoteInitialized=true;
        if (dirty && remoteRevision>m_draftRevision) {
            // A network acknowledgement may be lost after PostgreSQL has
            // committed the write. Recognize an identical server snapshot
            // rather than incorrectly locking the outbox in conflict state.
            const bool pendingDelete=m_settings->value(
                key+QStringLiteral("/deleted")).toBool();
            const auto serverDoc=remote.value(QStringLiteral("content")).toObject()
                .value(QStringLiteral("document")).toObject();
            const auto localDoc=QJsonDocument::fromJson(
                m_settings->value(localDocumentKey).toByteArray());
            if (pendingDelete==deleted
                && (deleted || (localDoc.isObject() && !serverDoc.isEmpty()
                    && localDoc.object()==serverDoc))) {
                m_draftRevision=remoteRevision;
                m_draftConflict=false;
                m_settings->setValue(key+QStringLiteral("/revision"),remoteRevision);
                m_settings->setValue(key+QStringLiteral("/seen"),true);
                m_settings->setValue(key+QStringLiteral("/dirty"),false);
                m_settings->sync();
#if defined(__EMSCRIPTEN__)
                EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
                return;
            }
        }
        if (dirty) {
            if (m_draftRevision!=remoteRevision) {
                // Both versions remain intact: the unsent local draft stays
                // durable, and the other device's draft stays on the server.
                m_draftConflict=true;
                return;
            }
            m_draftConflict=false;
            QMetaObject::invokeMethod(this,[this]{uploadDraftChange();},
                                      Qt::QueuedConnection);
            return;
        }
        m_draftConflict=false;
        if (remoteRevision==0 || (remoteRevision==m_draftRevision
            && (deleted ? !hasLocalDraft : !differs))) {
            m_settings->setValue(key+QStringLiteral("/seen"),true);
            return;
        }
        if (!deleted && !remote.value(QStringLiteral("content")).isObject())
            return;
        const auto content=remote.value(QStringLiteral("content")).toObject();
        if (!deleted && !content.value(QStringLiteral("document")).isObject())
            return;
        const QByteArray serialized=deleted ? QByteArray{}
            : QJsonDocument{content.value(QStringLiteral("document")).toObject()}
                  .toJson(QJsonDocument::Compact);
        // The UI adapter rejects delivery while the user is editing. Only
        // its explicit acknowledgement advances the persistent revision.
        emit cloudDraftSnapshotReceived(viewer,serialized,deleted,remoteRevision);
    });
}

void OpenProofConnectionsController::uploadDraftChange()
{
    if (!m_settings || !m_draftRemoteInitialized || m_draftConflict ||
        !m_viewerFid || !cloudAuthorizationReady() ||
        !m_cloudServiceAvailable || m_draftRequest ||
        m_draftRemoteRevision!=m_draftRevision ||
        !m_settings->value(draftStorageKey()+QStringLiteral("/dirty")).toBool())
        return;
    const auto key=draftStorageKey();
    const bool deleted=m_settings->value(key+QStringLiteral("/deleted")).toBool();
    QJsonObject content;
    if (!deleted) {
        const auto localKey=QStringLiteral(
            "composer/accounts/fid_%1/richDraftV1").arg(m_viewerFid);
        const auto doc=QJsonDocument::fromJson(m_settings->value(localKey).toByteArray());
        if (!doc.isObject()) return;
        content.insert(QStringLiteral("document"),doc.object());
    }
    const QJsonObject body{
        {QStringLiteral("id"),QStringLiteral("00000000-0000-4000-8000-000000000002")},
        {QStringLiteral("kind"),QStringLiteral("cast")},
        {QStringLiteral("title"),QString{}},
        {QStringLiteral("content"),content},
        {QStringLiteral("is_deleted"),deleted},
        {QStringLiteral("revision"),m_draftRevision}
    };
    const auto data=QJsonDocument{body}.toJson(QJsonDocument::Compact);
    if (data.size()>188*1024) return; // Do not abandon the local pending draft.
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/drafts"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/json"));
    auto* reply=m_network.put(request,data);
    m_draftRequest=reply;
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    const auto serial=m_draftMutationSerial;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,viewer,serial] {
        if (m_draftRequest==reply) m_draftRequest=nullptr;
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=256*1024
            ? reply->readAll():QByteArray{};
        reply->deleteLater();
        if (generation!=m_grantGeneration || viewer!=m_viewerFid) return;
        if (status==409) {
            m_draftRemoteInitialized=false;
            QMetaObject::invokeMethod(this,[this]{pumpDraftChanges();},
                                      Qt::QueuedConnection);
            return;
        }
        if (status!=200) return;
        const auto response=QJsonDocument::fromJson(bytes);
        const int next=response.isObject()
            ? response.object().value(QStringLiteral("item")).toObject()
                .value(QStringLiteral("revision")).toInt(-1):-1;
        if (next<=m_draftRevision) return;
        m_draftRevision=next;
        m_draftRemoteRevision=next;
        const auto stateKey=draftStorageKey();
        m_settings->setValue(stateKey+QStringLiteral("/revision"),next);
        m_settings->setValue(stateKey+QStringLiteral("/seen"),true);
        if (serial==m_draftMutationSerial)
            m_settings->setValue(stateKey+QStringLiteral("/dirty"),false);
        m_settings->sync();
#if defined(__EMSCRIPTEN__)
        EM_ASM({ Module.genycasterSyncBrowserData?.().catch(function() {}); });
#endif
        if (serial!=m_draftMutationSerial)
            QMetaObject::invokeMethod(this,[this]{uploadDraftChange();},
                                      Qt::QueuedConnection);
    });
}

void OpenProofConnectionsController::reconcileLinkedIn()
{
    if (m_linkedInReconciled || !m_settings || !m_verified
        || !m_cloudServiceAvailable || !cloudAuthorizationReady()
        || m_cloudRequest || !isConnected(QStringLiteral("linkedin"))) return;
    const QString key=manualLinkedInSettingKey();
    if (key.isEmpty()) return;
    m_linkedInReconciled=true;
    if (m_settings->value(key+QStringLiteral(".cloudDirty")).toBool())
        uploadManualLinkedInCloud();
    else
        restoreManualLinkedInCloud();
}

void OpenProofConnectionsController::uploadManualLinkedInCloud()
{
    sendCloudLinkedInRequest(true);
}

void OpenProofConnectionsController::restoreManualLinkedInCloud()
{
    sendCloudLinkedInRequest(false);
}

void OpenProofConnectionsController::sendCloudLinkedInRequest(const bool upload)
{
    if (!m_cloudServiceAvailable || !m_verified || !m_viewerFid || !m_settings
        || !isConnected(QStringLiteral("linkedin")) || m_cloudRequest
        || m_accountBearer.isEmpty()) {
        m_cloudError = QStringLiteral("Verified OpenProof authorization and active cloud storage are required.");
        emit cloudStateChanged();
        return;
    }
    QUrl url = m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/connections"));
    auto request = QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_accountBearer);
    const QString uploadedUrl=upload ? manualLinkedInUrl() : QString{};
    QNetworkReply* reply{};
    if (upload) {
        QJsonObject body{
            {QStringLiteral("provider"), QStringLiteral("linkedin")},
            {QStringLiteral("publicly_visible"), false}
        };
        body.insert(QStringLiteral("manual_profile_url"),
                    uploadedUrl.isEmpty() ? QJsonValue{QJsonValue::Null} : QJsonValue{uploadedUrl});
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_network.put(request, QJsonDocument{body}.toJson(QJsonDocument::Compact));
    } else {
        reply = m_network.get(request);
    }
    m_cloudRequest = reply;
    m_cloudError.clear();
    const auto generation = m_grantGeneration;
    emit cloudStateChanged();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation, upload, uploadedUrl] {
        if (generation != m_grantGeneration) {
            reply->deleteLater();
            return;
        }
        if (m_cloudRequest == reply) m_cloudRequest = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->bytesAvailable() < 64 * 1024
            ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        // LinkedIn refresh must release the shared request lane for bookmarks
        // and read-watermark changes, including when the refresh fails.
        QMetaObject::invokeMethod(this, [this] { pumpBookmarkChanges(); },
                                  Qt::QueuedConnection);
        const auto response = QJsonDocument::fromJson(bytes);
        if (status != 200 || !response.isObject()) {
            m_linkedInReconciled = false;
            m_cloudError = status == 401
                ? QStringLiteral("Reauthorize OpenProof to sync LinkedIn.")
                : QStringLiteral("LinkedIn cloud sync is unavailable. The local URL is unchanged.");
            emit cloudStateChanged();
            return;
        }
        if (upload) {
            if (!response.object().value(QStringLiteral("item")).isObject()) {
                m_linkedInReconciled = false;
                m_cloudError = QStringLiteral("LinkedIn cloud update was not confirmed.");
            } else {
                const QString key = manualLinkedInSettingKey();
                const bool superseded = uploadedUrl != manualLinkedInUrl();
                if (!key.isEmpty() && !superseded)
                    m_settings->setValue(key + QStringLiteral(".cloudDirty"), false);
                m_settings->sync();
                m_cloudError.clear();
                if (superseded)
                    QMetaObject::invokeMethod(this,[this]{uploadManualLinkedInCloud();},
                                              Qt::QueuedConnection);
            }
        } else {
            const QString key=manualLinkedInSettingKey();
            if (!key.isEmpty()
                && m_settings->value(key+QStringLiteral(".cloudDirty")).toBool()) {
                QMetaObject::invokeMethod(this,[this]{uploadManualLinkedInCloud();},
                                          Qt::QueuedConnection);
                emit cloudStateChanged();
                return;
            }
            const auto items = response.object().value(QStringLiteral("items")).toArray();
            bool foundLinkedIn = false;
            for (const auto& value : items) {
                if (!value.isObject()) continue;
                const auto object = value.toObject();
                if (object.value(QStringLiteral("provider")).toString()
                    != QStringLiteral("linkedin")) continue;
                foundLinkedIn = true;
                const QString manual = object.value(QStringLiteral("manual_profile_url")).toString();
                if (object.contains(QStringLiteral("manual_profile_url"))
                    && object.value(QStringLiteral("manual_profile_url")).isNull()) {
                    // Explicit restore applies the cloud deletion as well.
                    m_applyingCloudLinkedIn = true;
                    setManualLinkedInUrl({});
                    m_applyingCloudLinkedIn = false;
                } else if (!manual.isEmpty()) {
                    const QString existing = manualLinkedInUrl();
                    if (existing != manual) {
                        m_applyingCloudLinkedIn = true;
                        setManualLinkedInUrl(manual);
                        m_applyingCloudLinkedIn = false;
                    }
                }
                break;
            }
            // Existing self-authored URLs on older devices are migrated on
            // first verified login, never discarded just because cloud is empty.
            if (!foundLinkedIn && !manualLinkedInUrl().isEmpty())
                QMetaObject::invokeMethod(this, [this] { uploadManualLinkedInCloud(); },
                                          Qt::QueuedConnection);
        }
        emit cloudStateChanged();
    });
}


void OpenProofConnectionsController::fetchPublicActivitySource(qulonglong fid)
{
    if(!fid || !trustedHttpsEndpoint(m_cloudUrl))return;
    cancelNetworkReply(m_publicActivityRead,this);
    m_publicActivityFid=fid;
    m_publicActivitySource=QStringLiteral("casts");
    m_publicActivityGithubLogin.clear();
    emit streakChanged();
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/profile_activity"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fid"),QString::number(fid));
    url.setQuery(query);
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9000);
    request.setRawHeader("Accept","application/json");
    auto* reply=m_network.get(request);
    m_publicActivityRead=reply;
    connect(reply,&QNetworkReply::finished,this,[this,reply,fid] {
        if(m_publicActivityRead!=reply){reply->deleteLater();return;}
        m_publicActivityRead.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=1024?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(m_publicActivityFid!=fid)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        const auto object=doc.object();
        if(status==200 && object.value(QStringLiteral("fid")).toString()
            ==QString::number(fid)) {
            const auto source=object.value(QStringLiteral("activity_source")).toString();
            const auto githubLogin=object.value(QStringLiteral("github_login")).toString();
            static const QRegularExpression publicLogin{
                QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")};
            m_publicActivityGithubLogin=publicLogin.match(githubLogin).hasMatch()
                ?githubLogin:QString{};
            m_publicActivitySource=source==QStringLiteral("github")
                && !m_publicActivityGithubLogin.isEmpty()
                ?source:QStringLiteral("casts");
        }
        emit streakChanged();
    });
}

// Account-owned mute policy: no Farcaster protocol messages or wallet
// signatures are issued here. It is private to the verified OpenProof FID.
void OpenProofConnectionsController::fetchMutedProfiles()
{
    if (!m_viewerFid || !cloudAuthorizationReady()
        || !trustedHttpsEndpoint(m_cloudUrl) || m_mutedProfilesWrite
        || m_mutedProfilesRead) return;
    const auto now=QDateTime::currentMSecsSinceEpoch();
    if(m_mutedProfilesFetchedAt > 0
       && now-m_mutedProfilesFetchedAt < (m_mutedProfilesLoaded ? 60'000 : 10'000))
        return;
    m_mutedProfilesFetchedAt=now;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/muted_profiles"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* const reply=m_network.get(request);
    m_mutedProfilesRead=reply;
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation]{
        if(m_mutedProfilesRead!=reply){reply->deleteLater();return;}
        m_mutedProfilesRead.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=8192?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        const auto preferences=doc.object().value(QStringLiteral("preferences")).toObject();
        const auto ids=preferences.value(QStringLiteral("muted_fids")).toArray();
        const auto revision=preferences.value(QStringLiteral("revision")).toInteger(-1);
        if(status!=200 || !doc.isObject() || !preferences.contains(QStringLiteral("muted_fids"))
            || !preferences.value(QStringLiteral("muted_fids")).isArray()
            || revision<0 || ids.size()>256){
            m_mutedProfilesError=QStringLiteral("Could not load muted accounts");
            emit mutedProfilesChanged();
            return;
        }
        QStringList next{};
        bool valid=true;
        for(const auto& fid:ids){
            const auto value=fid.toString();
            if(value.isEmpty() || value==QString::number(viewer)
               || !QRegularExpression{QStringLiteral("^[1-9][0-9]{0,18}$")}
                       .match(value).hasMatch() || next.contains(value)){
                valid=false;break;
            }
            next.push_back(value);
        }
        if(!valid){
            m_mutedProfilesError=QStringLiteral("Invalid mute preferences");
            emit mutedProfilesChanged();
            return;
        }
        m_mutedProfileFids=std::move(next);
        m_mutedProfilesRevision=revision;
        m_mutedProfilesLoaded=true;
        m_mutedProfilesError.clear();
        emit mutedProfilesChanged();
    });
}

void OpenProofConnectionsController::setProfileMuted(
    const qulonglong targetFid,const bool muted)
{
    if(!m_viewerFid || !m_mutedProfilesLoaded || !cloudAuthorizationReady()
       || !trustedHttpsEndpoint(m_cloudUrl) || m_mutedProfilesWrite
       || targetFid==0 || targetFid==m_viewerFid
       || targetFid>9'007'199'254'740'991ULL)return;
    const QString target=QString::number(targetFid);
    QStringList next=m_mutedProfileFids;
    const bool present=next.contains(target);
    if(present==muted)return;
    if(muted){
        if(next.size()>=256)return;
        next.push_back(target);
    }else next.removeAll(target);
    std::sort(next.begin(),next.end());
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/muted_profiles"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10'000);
    request.setRawHeader("Accept","application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/json"));
    authorizeCloudRequest(request);
    const auto payload=QJsonDocument{QJsonObject{
        {QStringLiteral("muted_fids"),QJsonArray::fromStringList(next)},
        {QStringLiteral("revision"),m_mutedProfilesRevision}
    }}.toJson(QJsonDocument::Compact);
    auto* const reply=m_network.put(request,payload);
    m_mutedProfilesWrite=reply;
    m_mutedProfilesError.clear();
    emit mutedProfilesChanged();
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation]{
        if(m_mutedProfilesWrite!=reply){reply->deleteLater();return;}
        m_mutedProfilesWrite.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=8192?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        const auto preferences=doc.object().value(QStringLiteral("preferences")).toObject();
        const auto ids=preferences.value(QStringLiteral("muted_fids")).toArray();
        const auto revision=preferences.value(QStringLiteral("revision")).toInteger(-1);
        if(status==200 && preferences.value(QStringLiteral("muted_fids")).isArray()
           && revision>=0 && ids.size()<=256){
            QStringList confirmed;
            for(const auto& id:ids){
                const auto value=id.toString();
                if(value.isEmpty()) { confirmed.clear();break; }
                confirmed.push_back(value);
            }
            m_mutedProfileFids=std::move(confirmed);
            m_mutedProfilesRevision=revision;
            m_mutedProfilesLoaded=true;
            m_mutedProfilesFetchedAt=QDateTime::currentMSecsSinceEpoch();
            m_mutedProfilesError.clear();
        }else{
            m_mutedProfilesError=status==409
                ?QStringLiteral("Mute preferences changed on another device")
                :QStringLiteral("Could not update muted accounts");
            m_mutedProfilesFetchedAt=0;
            m_mutedProfilesLoaded=false;
            fetchMutedProfiles(); // Refresh the canonical revision; never overwrite blindly.
        }
        emit mutedProfilesChanged();
    });
}

void OpenProofConnectionsController::fetchStreakPreferences()
{
    if(!m_viewerFid || !cloudAuthorizationReady() ||
       !trustedHttpsEndpoint(m_cloudUrl) || m_streakPreferencesWrite)return;
    cancelNetworkReply(m_streakPreferencesRead,this);
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/streak/preferences"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* reply=m_network.get(request);
    m_streakPreferencesRead=reply;
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        if(m_streakPreferencesRead!=reply){reply->deleteLater();return;}
        m_streakPreferencesRead.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto body=reply->bytesAvailable()<=2048?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(m_viewerFid!=viewer || m_grantGeneration!=generation)return;
        const auto doc=QJsonDocument::fromJson(body);
        if(status==200 && doc.isObject()
           && doc.object().value(QStringLiteral("preferences")).isObject()) {
            m_streakPreferences=doc.object().value(QStringLiteral("preferences"))
                .toObject().toVariantMap();
            emit streakChanged();
        }
    });
}

void OpenProofConnectionsController::setStreakPreferences(
    const QString& activitySource,bool leaderboardVisible)
{
    if(activitySource!=QStringLiteral("casts") &&
       activitySource!=QStringLiteral("github"))return;
    if(!m_viewerFid || !cloudAuthorizationReady() ||
       !trustedHttpsEndpoint(m_cloudUrl) || m_streakPreferencesWrite)return;
    const auto revision=m_streakPreferences.value(QStringLiteral("revision")).toLongLong();
    if(revision<0 || revision>9007199254740990LL)return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/streak/preferences"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10000);
    request.setRawHeader("Accept","application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    authorizeCloudRequest(request);
    const auto payload=QJsonDocument{QJsonObject{
        {QStringLiteral("activity_source"),activitySource},
        {QStringLiteral("leaderboard_visible"),leaderboardVisible},
        {QStringLiteral("revision"),static_cast<double>(revision)}
    }}.toJson(QJsonDocument::Compact);
    auto* reply=m_network.put(request,payload);
    m_streakPreferencesWrite=reply;
    emit streakChanged();
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        if(m_streakPreferencesWrite!=reply){reply->deleteLater();return;}
        m_streakPreferencesWrite.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=2048?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(m_viewerFid!=viewer || m_grantGeneration!=generation)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        if(status==200 && doc.isObject()
           && doc.object().value(QStringLiteral("preferences")).isObject()) {
            m_streakPreferences=doc.object().value(QStringLiteral("preferences"))
                .toObject().toVariantMap();
            fetchStreakLeaderboard(m_streakLeaderboardTrack);
        } else {
            // A conflict means the account was updated on another device.
            fetchStreakPreferences();
        }
        emit streakChanged();
    });
}

void OpenProofConnectionsController::retryProEntitlements()
{
    if (!m_viewerFid || !trustedHttpsEndpoint(m_cloudUrl) || m_sessionRevoking)
        return;
    if (cloudAuthorizationReady()) {
        fetchProEntitlements(true);
        return;
    }
    // A signed-in Farcaster user may already have a verified GenyCaster
    // HttpOnly session. Restore it before interpreting a missing Pro response
    // as Basic. This never opens a signer/approval window.
    if (m_sessionRequest)
        return;
    if (!m_accountBearer.isEmpty()
        && (m_verified || m_accountBearer.startsWith("recovery.")))
        bootstrapAccountSession();
    else
        restoreAccountSession();
}

void OpenProofConnectionsController::fetchProEntitlements(const bool force)
{
    if (!m_viewerFid || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl)) {
        // Network/session restoration is not evidence of revoked membership.
        // A viewer switch resets this snapshot in clearAccountGrant().
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && m_proEntitlementsFid == m_viewerFid &&
        m_proEntitlementsFetchedAt > 0 && now - m_proEntitlementsFetchedAt < 60'000)
        return;
    if (m_proEntitlementsRead) return;
    QUrl url = m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/entitlements"));
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(14000);
    request.setRawHeader("Accept", "application/json");
    authorizeCloudRequest(request);
    auto* reply = m_network.get(request);
    m_proEntitlementsRead = reply;
    emit proEntitlementsChanged(); // QML busy indicator starts with the HTTP request.
    const auto viewer = m_viewerFid;
    const auto generation = m_grantGeneration;
    connect(reply, &QNetworkReply::finished, this, [this,reply,viewer,generation] {
        if (m_proEntitlementsRead != reply) { reply->deleteLater(); return; }
        m_proEntitlementsRead.clear();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->bytesAvailable() <= 8192 ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if (viewer != m_viewerFid || generation != m_grantGeneration) return;
        const auto doc = QJsonDocument::fromJson(bytes);
        bool verified = false;
        if (status == 200 && doc.isObject()
            && doc.object().value(QStringLiteral("entitlements")).isObject()) {
            const auto parsed = doc.object().value(QStringLiteral("entitlements"))
                                    .toObject().toVariantMap();
            verified = parsed.value(QStringLiteral("status")).toString()
                       == QStringLiteral("verified");
            if (verified) {
                // A new verified Basic response may revoke an earlier Pro
                // grant. An unavailable balance provider must not do so.
                m_proEntitlements = parsed;
                m_proEntitlementsFid = viewer;
            }
        }
        if (!verified && (m_proEntitlementsFid != viewer
            || m_proEntitlements.value(QStringLiteral("status")).toString()
                != QStringLiteral("verified"))) {
            m_proEntitlements = {{QStringLiteral("status"),
                                  QStringLiteral("unavailable")}};
            m_proEntitlementsFid = viewer;
        }
        // Failed checks are retryable immediately; only server-verified
        // entitlements qualify for the normal short cache interval.
        m_proEntitlementsFetchedAt = verified
            ? QDateTime::currentMSecsSinceEpoch() : 0;
        emit proEntitlementsChanged();
    });
}

void OpenProofConnectionsController::fetchGenyAssets(const bool force)
{
    if (!m_viewerFid || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl)) {
        m_genyAssets.clear();
        m_genyAssetsFid = 0;
        emit genyAssetsChanged();
        return;
    }
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!force && m_genyAssetsFid == m_viewerFid
        && m_genyAssetsFetchedAt > 0 && now - m_genyAssetsFetchedAt < 60000) return;
    if (m_genyAssetsRead) return;
    QUrl url = m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/geny/assets"));
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(20000);
    request.setRawHeader("Accept", "application/json");
    authorizeCloudRequest(request);
    auto* reply = m_network.get(request);
    m_genyAssetsRead = reply;
    const auto viewer = m_viewerFid;
    const auto generation = m_grantGeneration;
    m_genyAssets = QVariantMap{
        {QStringLiteral("loading"), true},
        {QStringLiteral("wallets"), QVariantList{}},
        {QStringLiteral("addresses"), QVariantList{}}
    };
    m_genyAssetsFid = viewer;
    emit genyAssetsChanged();
    connect(reply, &QNetworkReply::finished, this, [this, reply, viewer, generation] {
        if (m_genyAssetsRead != reply) { reply->deleteLater(); return; }
        m_genyAssetsRead.clear();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->bytesAvailable() <= 32768 ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if (viewer != m_viewerFid || generation != m_grantGeneration) return;
        m_genyAssets = QVariantMap{
            {QStringLiteral("loading"), false},
            {QStringLiteral("available"), false},
            {QStringLiteral("wallets"), QVariantList{}},
            {QStringLiteral("addresses"), QVariantList{}},
            {QStringLiteral("error"), QStringLiteral("Balance check unavailable")}
        };
        const auto doc = QJsonDocument::fromJson(bytes);
        if (status == 200 && doc.isObject() &&
            doc.object().value(QStringLiteral("holding")).isObject()) {
            const auto parsed = doc.object().value(QStringLiteral("holding")).toObject().toVariantMap();
            const auto statusCode = parsed.value(QStringLiteral("status")).toString();
            if (statusCode == QStringLiteral("verified") ||
                statusCode == QStringLiteral("unavailable")) {
                m_genyAssets = parsed;
                m_genyAssets.insert(QStringLiteral("loading"), false);
            }
        }
        m_genyAssetsFid = viewer;
        m_genyAssetsFetchedAt = QDateTime::currentMSecsSinceEpoch();
        emit genyAssetsChanged();
    });
}

void OpenProofConnectionsController::fetchStreakLeaderboard(const QString& track)
{
    if(track!=QStringLiteral("activity") && track!=QStringLiteral("creator"))return;
    if(!trustedHttpsEndpoint(m_cloudUrl))return;
    cancelNetworkReply(m_streakLeaderboardRead,this);
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/streak/leaderboard"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track"),track);
    url.setQuery(query);
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    request.setRawHeader("Accept","application/json");
    // The ranking is public. No account token or membership is required.
    auto* reply=m_network.get(request);
    m_streakLeaderboardRead=reply;
    m_streakLeaderboardTrack=track;
    m_streakLeaderboard.clear();
    emit streakChanged();
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        if(m_streakLeaderboardRead!=reply){reply->deleteLater();return;}
        m_streakLeaderboardRead.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=65536?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(m_viewerFid!=viewer || m_grantGeneration!=generation)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        if(status==200 && doc.isObject() &&
           doc.object().value(QStringLiteral("leaders")).isArray())
            m_streakLeaderboard=doc.object().value(QStringLiteral("leaders"))
                .toArray().toVariantList();
        emit streakChanged();
    });
}

void OpenProofConnectionsController::fetchStreak()
{
    if (!m_viewerFid || !cloudAuthorizationReady()
        || !trustedHttpsEndpoint(m_cloudUrl) || m_streakWrite
        || m_streakRead) return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/streak"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* reply=m_network.get(request);
    m_streakRead=reply;
    m_streakError.clear();
    emit streakChanged();
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        if(m_streakRead!=reply){reply->deleteLater();return;}
        m_streakRead.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=8192?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        if(status==200 && doc.isObject()
            && doc.object().value(QStringLiteral("streak")).isObject()) {
            m_streak=doc.object().value(QStringLiteral("streak")).toObject().toVariantMap();
            // Only store a server-validated, account-scoped public snapshot.
            if (m_settings)
                m_settings->setValue(QStringLiteral("streak/snapshot/%1").arg(viewer), m_streak);
            m_streakError.clear();
        } else {
            m_streakError=QStringLiteral("Streak is temporarily unavailable.");
        }
        emit streakChanged();
    });
}

void OpenProofConnectionsController::recordStreakCast(const QString& hash)
{
    static const QRegularExpression castHash{QStringLiteral("^0x[0-9a-fA-F]{40}$")};
    if(!castHash.match(hash).hasMatch() || !m_viewerFid || !m_settings)return;
    const QString normalized=hash.toLower();
    if(!m_pendingStreakCasts.contains(normalized)) {
        // Indexing can lag behind protocol confirmation. Persist the exact
        // verified-publish hash before attempting the account API write.
        m_pendingStreakCasts.push_back(normalized);
        while(m_pendingStreakCasts.size()>64)m_pendingStreakCasts.removeFirst();
        m_settings->setValue(QStringLiteral("streak/outbox/%1").arg(m_viewerFid),
                             m_pendingStreakCasts);
        m_settings->sync();
    }
    pumpStreakWrites();
}

void OpenProofConnectionsController::pumpStreakWrites()
{
    if(!m_viewerFid || m_pendingStreakCasts.isEmpty() || m_streakWrite
       || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl))return;
    const QString hash=m_pendingStreakCasts.first();
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/streak/cast"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(12'000);
    request.setRawHeader("Accept","application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/json"));
    authorizeCloudRequest(request);
    const auto payload=QJsonDocument{QJsonObject{
        {QStringLiteral("cast_hash"),hash}
    }}.toJson(QJsonDocument::Compact);
    auto* reply=m_network.put(request,payload);
    m_streakWrite=reply;
    emit streakChanged();
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation,hash] {
        if(m_streakWrite!=reply){reply->deleteLater();return;}
        m_streakWrite.clear();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=8192?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        const bool accepted=status==200 && doc.isObject()
            && doc.object().value(QStringLiteral("streak")).isObject();
        if(accepted || status==400 || status==409) {
            if(accepted) {
                m_streak=doc.object().value(QStringLiteral("streak")).toObject().toVariantMap();
                if (m_settings)
                    m_settings->setValue(QStringLiteral("streak/snapshot/%1").arg(viewer), m_streak);
                m_streakError.clear();
            } else {
                m_streakError=QStringLiteral("Unable to record this streak cast.");
            }
            // Success or a permanent malformed/conflicting write must never
            // block later casts queued while the network was unavailable.
            m_pendingStreakCasts.removeAll(hash);
            if(m_settings) {
                m_settings->setValue(QStringLiteral("streak/outbox/%1").arg(m_viewerFid),
                                     m_pendingStreakCasts);
                m_settings->sync();
            }
            m_streakRetryDelayMs=10'000;
            emit streakChanged();
            pumpStreakWrites();
            return;
        }
        m_streakError=status==422
            ? QStringLiteral("Cast confirmation is pending on the network.")
            : QStringLiteral("Streak update is temporarily unavailable.");
        emit streakChanged();
        // Retry after either provider indexes the cast; never award locally.
        const int delay=m_streakRetryDelayMs;
        m_streakRetryDelayMs=std::min(m_streakRetryDelayMs*2,120'000);
        QTimer::singleShot(delay,this,[this,viewer,generation] {
            if(viewer==m_viewerFid && generation==m_grantGeneration)
                pumpStreakWrites();
        });
    });
}

void OpenProofConnectionsController::fetchProfileStatus(qulonglong profileFid)
{
    if (!profileFid || !trustedHttpsEndpoint(m_cloudUrl) || m_profileStatusWrite) return;
    cancelNetworkReply(m_profileStatusRead, this);
    // Retain the last confirmed state for the SAME FID while refreshing.
    // A temporary timeout must never erase a successfully saved status.
    if(m_profileStatusFid!=profileFid) {
        m_profileStatus.clear();
        m_profileStatusRevision=0;
    }
    m_profileStatusFid=profileFid;
    m_profileStatusError.clear();
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/profile_status"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    if(profileFid==m_viewerFid && cloudAuthorizationReady())
        authorizeCloudRequest(request);
    else {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("fid"),QString::number(profileFid));
        url.setQuery(query);
        request.setUrl(url);
    }
    auto* reply=m_network.get(request);
    m_profileStatusRead=reply;
    emit profileStatusChanged();
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,profileFid,generation]{
        // An aborted/superseded GET must not clear the newer result or show
        // an error after another read has replaced it.
        if(m_profileStatusRead!=reply) {
            reply->deleteLater();
            return;
        }
        m_profileStatusRead=nullptr;
        const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto data=reply->bytesAvailable()<=4096?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(m_profileStatusFid!=profileFid || generation!=m_grantGeneration)return;
        const auto doc=QJsonDocument::fromJson(data);
        const auto item=doc.object().value(QStringLiteral("item"));
        if(code==200 && doc.isObject() && (item.isNull() || item.isObject())){
            m_profileStatus=item.isObject()?item.toObject().toVariantMap():QVariantMap{};
            bool valid=false;
            m_profileStatusRevision=m_profileStatus.value(QStringLiteral("revision"))
                .toString().toLongLong(&valid);
            if(!valid)m_profileStatusRevision=0;
            m_profileStatusError.clear();
        } else {
            m_profileStatusError=QStringLiteral("Status is temporarily unavailable.");
        }
        emit profileStatusChanged();
    });
}

void OpenProofConnectionsController::saveProfileStatus(
    const QString& mood,const QString& note,bool visible)
{
    static const QSet<QString> moods={
        QStringLiteral("none"),QStringLiteral("available"),QStringLiteral("busy"),
        QStringLiteral("idle"),QStringLiteral("away"),QStringLiteral("focusing"),QStringLiteral("creating"),
        QStringLiteral("dnd"),QStringLiteral("custom")
    };
    if(!moods.contains(mood) || note.size()>72 || !m_viewerFid ||
       m_profileStatusFid!=m_viewerFid || !cloudAuthorizationReady() ||
       !trustedHttpsEndpoint(m_cloudUrl) || m_profileStatusWrite ||
       m_profileStatusRead || (mood==QStringLiteral("custom") && note.trimmed().isEmpty())) {
        m_profileStatusError=QStringLiteral("Status cannot be saved until account verification is ready.");
        emit profileStatusChanged();
        emit profileStatusSaveFinished(false,m_profileStatusError);
        return;
    }
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/profile_status"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/json"));
    authorizeCloudRequest(request);
    const QJsonObject data{
        {QStringLiteral("mood"),mood},
        {QStringLiteral("note"),note.trimmed()},
        {QStringLiteral("visible"),visible},
        {QStringLiteral("revision"),static_cast<double>(m_profileStatusRevision)}
    };
    auto* reply=m_network.put(request,QJsonDocument{data}.toJson(QJsonDocument::Compact));
    m_profileStatusWrite=reply;
    m_profileStatusError.clear();
    emit profileStatusChanged();
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,viewer]{
        if(m_profileStatusWrite==reply)m_profileStatusWrite=nullptr;
        const auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->bytesAvailable()<=4096?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(generation!=m_grantGeneration || viewer!=m_viewerFid)return;
        const auto doc=QJsonDocument::fromJson(bytes);
        const auto item=doc.object().value(QStringLiteral("item"));
        const bool saved=code==200 && item.isObject()
            && reply->error()==QNetworkReply::NoError;
        if(saved){
            m_profileStatus=item.toObject().toVariantMap();
            m_profileStatusRevision=m_profileStatus.value(QStringLiteral("revision"))
                .toString().toLongLong();
            m_profileStatusError.clear();
        } else {
            m_profileStatusError=code==409
                ?QStringLiteral("This status changed on another device. Review your selection and save again.")
                :code==401 || code==403
                    ?QStringLiteral("Your account session has expired. Sign in again to save your status.")
                    :QStringLiteral("Could not save status. Your selection has not changed. Please try again.");
        }
        emit profileStatusChanged();
        // A profileStatusChanged event also fires for GETs and in-flight PUTs.
        // Only this terminal result may dismiss the editor.
        emit profileStatusSaveFinished(saved,m_profileStatusError);
        if(!saved && code==409)
            QMetaObject::invokeMethod(this,[this,viewer]{
                if(m_viewerFid==viewer)fetchProfileStatus(viewer);
            },Qt::QueuedConnection);
    });
}

void OpenProofConnectionsController::fetchCloudProfileExtras()
{
    sendCloudProfileRequest(false);
}

void OpenProofConnectionsController::pushCloudProfileExtras(const QVariantMap& fields)
{
    sendCloudProfileRequest(true, fields);
}

void OpenProofConnectionsController::sendCloudProfileRequest(
    const bool upload, const QVariantMap& fields)
{
    // A previously restored FID, local profile or opaque recovery ticket is
    // not a cloud-authentication grant. Only a freshly FID-verified OpenProof
    // bearer can access account-owned private state.
    if (!m_cloudServiceAvailable) {
        m_cloudError = QStringLiteral("Account data service is unavailable. Your local edits are preserved.");
        emit cloudStateChanged();
        return;
    }
    if (!m_viewerFid || !cloudAuthorizationReady() || m_cloudRequest) {
        m_cloudError = QStringLiteral("Account data is temporarily unavailable. Local edits are preserved.");
        emit cloudStateChanged();
        return;
    }
    if (!trustedHttpsEndpoint(m_cloudUrl)
        || m_cloudUrl.path() != QStringLiteral("/api/v1/cloud/profile_extras")
        || m_cloudUrl.hasQuery()) {
        m_cloudError = QStringLiteral("Account data service is not configured.");
        emit cloudStateChanged();
        return;
    }
    auto request = QNetworkRequest{m_cloudUrl};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept", "application/json");
    authorizeCloudRequest(request);
    QNetworkReply* reply{};
    if (upload) {
        auto payload = QJsonObject::fromVariantMap(fields);
        // The caller cannot choose a different owner or choose its own
        // expected revision. The server independently authenticates identity.
        payload.remove(QStringLiteral("issuer"));
        payload.remove(QStringLiteral("subject"));
        payload.remove(QStringLiteral("identity_id"));
        payload[QStringLiteral("revision")] = m_cloudRevision;
        payload[QStringLiteral("publicly_visible")] = false;
        const auto body = QJsonDocument{payload}.toJson(QJsonDocument::Compact);
        if (body.size() > 20 * 1024) {
            m_cloudError = QStringLiteral("Profile details exceed the storage limit.");
            emit cloudStateChanged();
            return;
        }
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_network.put(request, body);
    } else {
        reply = m_network.get(request);
    }
    m_cloudRequest = reply;
    m_cloudError.clear();
    const auto generation = m_grantGeneration;
    emit cloudStateChanged();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation, upload] {
        if (generation != m_grantGeneration) {
            reply->deleteLater();
            return;
        }
        if (m_cloudRequest == reply) m_cloudRequest = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->bytesAvailable() <= 24 * 1024
            ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        const auto doc = QJsonDocument::fromJson(bytes);
        if (status == 409) {
            m_cloudError = QStringLiteral(
                "Another device saved newer profile details. Your local edits remain preserved.");
            // Immediately retrieve the winning revision so the UI can
            // reconcile the pending edit; do not wait for the next poll.
            QMetaObject::invokeMethod(this, [this] { fetchCloudProfileExtras(); },
                                      Qt::QueuedConnection);
        } else if (status == 400 || status == 413) {
            m_cloudError = QStringLiteral(
                "Profile details were rejected. Check the selected category, role and links.");
        } else if (status == 401 || status == 403) {
            m_cloudError = QStringLiteral("Your account authorization is unavailable. Local edits are preserved.");
        } else if (status == 503 || status == 404 || status == 0) {
            m_cloudError = QStringLiteral("Account data service is temporarily unavailable. Local edits are preserved.");
        } else if (status != 200 || !doc.isObject()) {
            m_cloudError = QStringLiteral("Could not save profile details to your account.");
        } else {
            const auto item = doc.object().value(QStringLiteral("item"));
            if (item.isObject()) {
                const auto object = item.toObject();
                const int revision = object.value(QStringLiteral("revision")).toInt();
                if (revision > 0
                    && object.value(QStringLiteral("links")).isArray()
                    && object.value(QStringLiteral("profession")).isString()
                    && object.value(QStringLiteral("category")).isString()) {
                    m_cloudRevision = revision;
                    m_cloudError.clear();
                    const QVariantMap confirmedFields = object.toVariantMap();
                    m_cloudProfileExtras=confirmedFields;
                    if (upload) emit cloudProfileExtrasSaved(confirmedFields);
                    else emit cloudProfileExtrasReceived(confirmedFields);
                } else {
                    m_cloudError = QStringLiteral("Account data service returned invalid profile details.");
                }
            } else if (!upload && item.isNull()) {
                m_cloudRevision = 0;
                m_cloudProfileExtras.clear();
                m_cloudError.clear();
                emit cloudProfileExtrasReceived({});
            } else {
                m_cloudError = QStringLiteral("Account data service returned an invalid response.");
            }
        }
        emit cloudStateChanged();
    });
}

QString OpenProofConnectionsController::recoveryGatewayOrigin() const
{
    if (!trustedHttpsEndpoint(m_gatewayUrl)) return {};
    return m_gatewayUrl.adjusted(QUrl::RemovePath | QUrl::RemoveQuery)
        .toString(QUrl::FullyEncoded);
}
QString OpenProofConnectionsController::identityIssuer() const
{
    return trustedHttpsEndpoint(m_issuerUrl) ? m_issuerUrl.toString(QUrl::FullyEncoded) : QString{};
}

void OpenProofConnectionsController::refresh()
{
    if (m_loading) return;
    if (!trustedHttpsEndpoint(m_catalogUrl)) {
        m_error = QStringLiteral("OpenProof provider-catalog URL is not configured securely.");
        emit stateChanged();
        return;
    }
    m_loading = true;
    m_error.clear();
    emit stateChanged();
    QNetworkRequest request{m_catalogUrl};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10'000);
    request.setRawHeader("Accept", "application/json");
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_loading = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError
            && status == 200 && reply->bytesAvailable() <= 32 * 1024;
        const QByteArray payload = ok ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        const QStringList latest = parseProviderCatalog(payload);
        // Catalog availability is informational, not an account operation
        // failure: don't flash a red warning or clear a real auth error.
        m_catalogFallback = latest.isEmpty();
        if (!latest.isEmpty()) m_providers = latest;
        emit stateChanged();
    });
}

QStringList OpenProofConnectionsController::parseProviderCatalog(const QByteArray& json)
{
    if (json.size() > 32 * 1024 || json.isEmpty()) return {};
    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return {};
    const auto object = doc.object();
    // The public OpenProof endpoint includes both arrays; the optional
    // first-party Genycaster API may already merge them into "providers".
    if (!object.value(QStringLiteral("providers")).isArray()
        || (object.contains(QStringLiteral("challenge_providers"))
            && !object.value(QStringLiteral("challenge_providers")).isArray())) return {};
    QStringList result{};
    for (const auto* key : {"providers", "challenge_providers"}) {
        for (const auto& entry : object.value(QString::fromLatin1(key)).toArray()) {
            if (!entry.isString()) continue;
            const QString provider = entry.toString();
            if (QStringList{QStringLiteral("x"), QStringLiteral("github"),
                            QStringLiteral("linkedin"), QStringLiteral("telegram")}
                    .contains(provider) && !result.contains(provider)) {
                result.push_back(provider);
            }
        }
    }
    // Local password and passkey management belong to account settings,
    // not the social/external identity picker.
    result.sort(Qt::CaseInsensitive);
    return result;
}

void OpenProofConnectionsController::refreshAccountConnectionsSnapshot()
{
    if (!m_viewerFid || !m_cloudSessionVerified || !m_cloudServiceAvailable
        || m_accountConnectionsRequest || !trustedHttpsEndpoint(m_cloudUrl))
        return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/connections"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* reply=m_network.get(request);
    m_accountConnectionsRequest=reply;
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        if(m_accountConnectionsRequest==reply)m_accountConnectionsRequest=nullptr;
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto body=reply->bytesAvailable()<=32768?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration
           || status!=200)return;
        const auto parsed=QJsonDocument::fromJson(body);
        if(!parsed.isObject() || !parsed.object().value(QStringLiteral("items")).isArray())return;
        const auto items=parsed.object().value(QStringLiteral("items")).toArray();
        QVariantList current;
        QSet<QString> providers;
        bool serverVerifiedFarcaster=false;
        for(const auto& raw : items) {
            if(!raw.isObject())continue;
            const auto row=raw.toObject();
            const auto provider=row.value(QStringLiteral("provider")).toString();
            const auto confirmed=row.value(QStringLiteral("last_confirmed_at"));
            // Only an OpenProof-confirmed row, never an owner-authored
            // portable label, may appear as a linked network.
            if(!confirmed.isString() || confirmed.toString().isEmpty())continue;
            if(provider==QStringLiteral("farcaster")) {
                serverVerifiedFarcaster=true;
                continue;
            }
            if(!m_providers.contains(provider) || providers.contains(provider))continue;
            providers.insert(provider);
            current.push_back(QVariantMap{
                {QStringLiteral("provider"),provider},
                // Informational marker, never a provider OAuth subject.
                {QStringLiteral("subject"),QStringLiteral("server-confirmed:")+provider},
                {QStringLiteral("displayName"),row.value(QStringLiteral("display_name")).toString()},
                {QStringLiteral("username"),row.value(QStringLiteral("preferred_username")).toString()},
                {QStringLiteral("profileUrl"),row.value(QStringLiteral("provider_profile_url")).toString()},
                {QStringLiteral("manualProfileUrl"),row.value(QStringLiteral("manual_profile_url")).toString()}
            });
        }
        if(m_verified) {
            // Another signed-in device may have disconnected a provider
            // since our last OpenProof read. When the database contains a
            // server-refreshed Farcaster identity, retire only the missing
            // public social badges; retain original OpenProof subjects for
            // remaining links and never synthesize authorization from SQL.
            if(!serverVerifiedFarcaster || m_actionBusy || !m_pendingProvider.isEmpty())
                return;
            QVariantList retained;
            QSet<QString> alreadyPresent;
            for(const auto& value:m_connections) {
                auto item=value.toMap();
                const auto provider=item.value(QStringLiteral("provider")).toString();
                if(!m_providers.contains(provider) || providers.contains(provider)) {
                    // Keep the original OAuth subject; confirmed account
                    // metadata may enrich missing display-only fields.
                    for(const auto& confirmed:current) {
                        const auto row=confirmed.toMap();
                        if(row.value(QStringLiteral("provider")).toString()!=provider)
                            continue;
                        for(const auto* field:{"username","profileUrl"}) {
                            const auto key=QString::fromLatin1(field);
                            if(item.value(key).toString().isEmpty()
                               && !row.value(key).toString().isEmpty())
                                item.insert(key,row.value(key));
                        }
                        break;
                    }
                    retained.push_back(item);
                    alreadyPresent.insert(provider);
                }
            }
            // A provider linked on another device may exist in the canonical
            // confirmed database even when the local OpenProof read predates
            // that link. Add display-only confirmed identities; never treat
            // server-confirmed:* labels as OAuth subjects for mutations.
            for(const auto& value:current) {
                const auto item=value.toMap();
                const auto provider=item.value(QStringLiteral("provider")).toString();
                if(!alreadyPresent.contains(provider)) {
                    retained.push_back(value);
                    alreadyPresent.insert(provider);
                }
            }
            if(retained!=m_connections) {
                m_connections=std::move(retained);
                saveCachedConnections();
                emit stateChanged();
            }
            return;
        }
        // No active OpenProof grant: saved labels are display-only. A synthetic
        // server-confirmed subject must never authorize provider unlinking.
        m_connections=std::move(current);
        m_cachedSnapshot=!m_connections.isEmpty();
        emit stateChanged();
    });
}

void OpenProofConnectionsController::publishVerifiedConnectionsSnapshot()
{
    // A verified upstream bearer is checked server-side once more.
    // Client-authored provider claims are never trusted as link evidence.
    if(!m_verified || m_accountBearer.isEmpty()
       || !trustedHttpsEndpoint(m_cloudUrl))return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/connections/refresh"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(12000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept","application/json");
    request.setRawHeader("Authorization","Bearer "+m_accountBearer);
    auto* reply=m_network.post(request,QByteArrayLiteral("{}"));
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        const auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration)return;
        if(code==200)refreshAccountConnectionsSnapshot();
    });
}

void OpenProofConnectionsController::publishBookmarkPreviewIfMissing(
    const QString& castId,const QVariantMap& snapshot)
{
    if(!m_bookmarkPreviewsReady || m_bookmarkPreviewSent.contains(castId)
       || m_bookmarkPreviewSent.size()>=32
       || snapshot.value(QStringLiteral("text")).toString().isEmpty()
       || snapshot.value(QStringLiteral("authorFid")).toULongLong()==0)
        return;
    m_bookmarkPreviewSent.insert(castId);
    recordBookmarkMutation(castId,false,snapshot);
}

void OpenProofConnectionsController::publishPortableConnectionLabels()
{
    if(!m_viewerFid || !m_cloudSessionVerified || m_verified
       || !m_cachedSnapshot || m_portableConnectionsPublished
       || m_portableConnectionsRequest || !trustedHttpsEndpoint(m_cloudUrl))
        return;
    QJsonArray entries;
    QSet<QString> seen;
    for(const auto& value:m_connections) {
        const auto item=value.toMap();
        const auto provider=item.value(QStringLiteral("provider")).toString();
        if(!m_providers.contains(provider)||seen.contains(provider))continue;
        seen.insert(provider);
        QString url=item.value(QStringLiteral("profileUrl")).toString();
        if(!url.isEmpty()) {
            const QUrl parsed{url};
            QStringList allowedHosts;
            if(provider==QStringLiteral("x"))
                allowedHosts={QStringLiteral("x.com"),QStringLiteral("twitter.com")};
            else if(provider==QStringLiteral("github"))
                allowedHosts={QStringLiteral("github.com")};
            else if(provider==QStringLiteral("linkedin"))
                allowedHosts={QStringLiteral("linkedin.com"),
                              QStringLiteral("www.linkedin.com")};
            else if(provider==QStringLiteral("telegram"))
                allowedHosts={QStringLiteral("t.me")};
            if(parsed.scheme()!=QStringLiteral("https")
               || !allowedHosts.contains(parsed.host())
               || parsed.port()!=-1 || !parsed.userInfo().isEmpty()
               || !parsed.query().isEmpty() || !parsed.fragment().isEmpty()
               || url.size()>512)url.clear();
        }
        QString manual;
        if(provider==QStringLiteral("linkedin")) {
            manual=item.value(QStringLiteral("manualProfileUrl")).toString();
            const QUrl candidate{manual};
            if(candidate.scheme()!=QStringLiteral("https")
               || candidate.host()!=QStringLiteral("www.linkedin.com")
               || !candidate.path().startsWith(QStringLiteral("/in/"))
               || candidate.port()!=-1 || !candidate.query().isEmpty()
               || !candidate.fragment().isEmpty() || manual.size()>512)
                manual.clear();
        }
        entries.push_back(QJsonObject{
            {QStringLiteral("provider"),provider},
            {QStringLiteral("display_name"),item.value(
                QStringLiteral("displayName")).toString().left(256)},
            {QStringLiteral("preferred_username"),item.value(
                QStringLiteral("username")).toString().left(128)},
            {QStringLiteral("provider_profile_url"),url},
            {QStringLiteral("manual_profile_url"),
             manual}
        });
    }
    if(entries.isEmpty())return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/connections/portable"));
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    authorizeCloudRequest(request);
    const auto body=QJsonDocument{QJsonObject{{QStringLiteral("items"),entries}}}
        .toJson(QJsonDocument::Compact);
    auto* reply=m_network.put(request,body);
    m_portableConnectionsRequest=reply;
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        if(m_portableConnectionsRequest==reply)m_portableConnectionsRequest=nullptr;
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration)return;
        if(status==200) {
            m_portableConnectionsPublished=true;
            refreshAccountConnectionsSnapshot();
        }
    });
}

void OpenProofConnectionsController::refreshAccountBookmarks()
{
    if(!m_viewerFid || !cloudAuthorizationReady() || !m_cloudServiceAvailable
       || m_bookmarkSnapshotRequest || !trustedHttpsEndpoint(m_cloudUrl))return;
    QUrl url=m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/bookmarks"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("state"),QStringLiteral("1"));
    url.setQuery(query);
    auto request=QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(12000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* reply=m_network.get(request);
    m_bookmarkSnapshotRequest=reply;
    emit cloudStateChanged();
    const auto viewer=m_viewerFid;
    const auto generation=m_grantGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,viewer,generation] {
        if(m_bookmarkSnapshotRequest==reply)m_bookmarkSnapshotRequest=nullptr;
        emit cloudStateChanged();
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto data=reply->bytesAvailable()<=1'048'576?reply->readAll():QByteArray{};
        reply->deleteLater();
        if(viewer!=m_viewerFid || generation!=m_grantGeneration || status!=200)
            return;
        const auto doc=QJsonDocument::fromJson(data);
        if(!doc.isObject() || !doc.object().value(QStringLiteral("items")).isArray())
            return;
        // Apply current canonical server state without discarding local
        // pending mutations. The normal durable outbox still owns their fate.
        QSet<QString> pending;
        for(const auto& item:m_bookmarkOutbox)
            pending.insert(item.toMap().value(QStringLiteral("cast_hash")).toString());
        QVariantList changes;
        const auto items=doc.object().value(QStringLiteral("items")).toArray();
        if(items.size()>500)return;
        m_bookmarkPreviewsReady=doc.object().value(
            QStringLiteral("preview_supported")).toBool();
        for(const auto& item:items) {
            if(!item.isObject())continue;
            auto row=item.toObject().toVariantMap();
            if(pending.contains(row.value(QStringLiteral("cast_hash")).toString()))
                continue;
            changes.push_back(std::move(row));
        }
        emit cloudBookmarkChangesReceived(changes);
        pumpBookmarkChanges();
    });
}

void OpenProofConnectionsController::restoreCachedConnections()
{
    if (!m_settings || !m_viewerFid || m_verified) return;
    const QString key = QStringLiteral("openProofConnections/lastKnown/")
        + QString::number(m_viewerFid);
    const QByteArray raw = m_settings->value(key).toString().toUtf8();
    if (raw.isEmpty() || raw.size() > 16 * 1024) return;
    const auto document = QJsonDocument::fromJson(raw);
    if (!document.isObject()) return;
    const auto snapshot = document.object();
    if (snapshot.value(QStringLiteral("fid")).toString() != QString::number(m_viewerFid)
        || !snapshot.value(QStringLiteral("connections")).isArray()) return;
    const qint64 verifiedAt = snapshot.value(QStringLiteral("verifiedAt")).toVariant().toLongLong();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Saved profile metadata is informational, never authority for mutations.
    // Old or malformed records cannot masquerade as recent verification.
    if (verifiedAt <= 0 || verifiedAt > now || now - verifiedAt > 30LL * 24 * 60 * 60 * 1000) return;
    const auto entries = snapshot.value(QStringLiteral("connections")).toArray();
    if (entries.size() > 4) return;
    QVariantList restored;
    QStringList seen;
    for (const auto& item : entries) {
        if (!item.isObject()) return;
        const auto value = item.toObject();
        const QString provider = value.value(QStringLiteral("provider")).toString();
        const QString subject = value.value(QStringLiteral("subject")).toString();
        const QString name = value.value(QStringLiteral("displayName")).toString();
        const QString username = value.value(QStringLiteral("username")).toString();
        if (!QStringList{QStringLiteral("x"), QStringLiteral("github"),
                         QStringLiteral("linkedin"), QStringLiteral("telegram")}.contains(provider)
            || seen.contains(provider) || subject.isEmpty() || subject.size() > 512
            || name.size() > 256 || username.size() > 128) return;
        seen.push_back(provider);
        restored.push_back(QVariantMap{
            {QStringLiteral("provider"), provider},
            {QStringLiteral("subject"), subject},
            {QStringLiteral("displayName"), name},
            {QStringLiteral("username"), username},
            {QStringLiteral("profileUrl"), value.value(QStringLiteral("profileUrl")).toString()},
            {QStringLiteral("manualProfileUrl"), value.value(QStringLiteral("manualProfileUrl")).toString()}});
    }
    m_connections = std::move(restored);
    m_cachedSnapshot = true;
}

void OpenProofConnectionsController::saveCachedConnections() const
{
    if (!m_settings || !m_verified || !m_viewerFid) return;
    QJsonArray social;
    for (const auto& entry : m_connections) {
        const auto item = entry.toMap();
        const QString provider = item.value(QStringLiteral("provider")).toString();
        if (!QStringList{QStringLiteral("x"), QStringLiteral("github"),
                         QStringLiteral("linkedin"), QStringLiteral("telegram")}.contains(provider))
            continue;
        const QString subject = item.value(QStringLiteral("subject")).toString();
        if (subject.isEmpty() || subject.size() > 512) continue;
        social.push_back(QJsonObject{
            {QStringLiteral("provider"), provider},
            {QStringLiteral("subject"), subject},
            {QStringLiteral("displayName"), item.value(QStringLiteral("displayName")).toString().left(256)},
            {QStringLiteral("username"), item.value(QStringLiteral("username")).toString().left(128)},
            {QStringLiteral("profileUrl"), item.value(QStringLiteral("profileUrl")).toString().left(256)},
            {QStringLiteral("manualProfileUrl"), item.value(QStringLiteral("manualProfileUrl")).toString().left(256)}});
    }
    const QJsonObject snapshot{
        {QStringLiteral("fid"), QString::number(m_viewerFid)},
        {QStringLiteral("verifiedAt"), QDateTime::currentMSecsSinceEpoch()},
        {QStringLiteral("connections"), social}};
    const QString key = QStringLiteral("openProofConnections/lastKnown/")
        + QString::number(m_viewerFid);
    m_settings->setValue(key, QString::fromUtf8(QJsonDocument(snapshot).toJson(QJsonDocument::Compact)));
    m_settings->sync();
#if defined(__EMSCRIPTEN__)
    // The persistent cache contains only public-facing profile labels, never
    // OpenProof cookies or bearer material. IDBFS persistence is serialized
    // with the existing Genycaster browser-data flush contract.
    EM_ASM({
        if (typeof Module.genycasterSyncBrowserData === 'function')
            Module.genycasterSyncBrowserData().catch(function() {});
    });
#endif
}

void OpenProofConnectionsController::clearAccountGrant()
{
#if defined(__EMSCRIPTEN__)
    closeGenycasterConnectionWindow();
#endif
    ++m_grantGeneration;
    clearCastTopic();
    m_grantExpiry.stop();
    m_connectionPoll.stop();
    m_bookmarkPoll.stop();
    m_linkedInReconciled = false;
    cancelNetworkReply(m_request, this);
    cancelNetworkReply(m_bookmarkSnapshotRequest, this);
    cancelNetworkReply(m_portableConnectionsRequest, this);
    cancelNetworkReply(m_accountConnectionsRequest, this);
    cancelNetworkReply(m_cloudRequest, this);
    cancelNetworkReply(m_feedbackRead, this);
    cancelNetworkReply(m_feedbackWrite, this);
    m_feedbackStats.clear();
    m_feedbackReports.clear();
    m_feedbackError.clear();
    m_feedbackSubmitted = false;
    emit feedbackChanged();
    cancelNetworkReply(m_proEntitlementsRead, this);
    cancelNetworkReply(m_genyAssetsRead, this);
    m_genyAssets.clear();
    m_genyAssetsFid = 0;
    m_genyAssetsFetchedAt = 0;
    emit genyAssetsChanged();
    m_proEntitlements.clear();
    m_proEntitlementsFid = 0;
    m_proEntitlementsFetchedAt = 0;
    emit proEntitlementsChanged();
    cancelNetworkReply(m_draftRequest, this);
    m_draftRemoteInitialized=false;
    m_draftConflict=false;
    // Losing the short-lived OpenProof grant does not invalidate a verified
    // Genycaster browser session or its last known profile CAS revision.
    // Account switches clear m_cloudSessionVerified before reaching here.
    if(!m_cloudSessionVerified) {
        m_cloudRevision=0;
        m_cloudProfileExtras.clear();
    }
    m_cloudError.clear();
    emit cloudStateChanged();
    m_accountBearer.fill('\0');
    m_accountBearer.clear();
    m_accountSubject.clear();
    m_connections.clear();
    m_verified = false;
    m_cachedSnapshot = false;
    m_actionBusy = false;
    m_pendingProvider.clear();
    m_reconnectingLinkedIn = false;
    m_remainingPolls = 0;
}

QString OpenProofConnectionsController::topicOutboxKey() const
{
    return m_viewerFid
        ? QStringLiteral("topics/account_%1/outbox").arg(m_viewerFid) : QString{};
}

void OpenProofConnectionsController::fetchTopics()
{
    // Resume the user's last read-only filter after a verified account grant
    // is restored. No signer provisioning, protocol write or login popup.
    if (cloudAuthorizationReady() && !m_activeCastTopic.isEmpty()
        && !m_topicCastsRequest && m_topicCastItems.isEmpty())
        fetchTopicCastPage();
    // Topic metadata belongs to authenticated GenyCaster accounts. Never
    // probe the protected endpoint without a verified bearer or browser session.
    if(m_topicCatalogRequest || !cloudAuthorizationReady()
       || !trustedHttpsEndpoint(m_cloudUrl)) return;
    QUrl endpoint=m_cloudUrl;
    endpoint.setPath(QStringLiteral("/api/v1/cloud/topics"));
    endpoint.setQuery(QString{});
    QNetworkRequest request{endpoint};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* reply=m_network.get(request);
    m_topicCatalogRequest=reply;
    const auto generation=m_grantGeneration;
    emit topicsChanged();
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation] {
        if(m_topicCatalogRequest==reply)m_topicCatalogRequest=nullptr;
        const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(generation==m_grantGeneration && code==200
                && reply->error()==QNetworkReply::NoError) {
            QJsonParseError error;
            const auto document=QJsonDocument::fromJson(reply->readAll(),&error);
            const auto items=document.object().value(QStringLiteral("items")).toArray();
            QVariantList cleaned;
            QSet<QString> seen;
            static const QRegularExpression slugPattern{
                QStringLiteral("^[a-z][a-z0-9-]{1,63}$")};
            for(const auto& value : items) {
                const auto item=value.toObject();
                const auto slug=item.value(QStringLiteral("slug")).toString();
                if(!slugPattern.match(slug).hasMatch() || seen.contains(slug))continue;
                seen.insert(slug);
                cleaned.push_back(QVariantMap{
                    {QStringLiteral("slug"),slug},
                    {QStringLiteral("cast_count"),
                        item.value(QStringLiteral("cast_count")).toInt()}
                });
            }
            if(error.error==QJsonParseError::NoError && !cleaned.isEmpty()) {
                m_topicCatalog=std::move(cleaned);
                m_topicsError.clear();
            } else {
                m_topicsError=QStringLiteral("Invalid topic catalog response");
            }
        } else if(generation==m_grantGeneration)
            m_topicsError=QStringLiteral("Topics could not be refreshed");
        reply->deleteLater();
        emit topicsChanged();
    });
}

void OpenProofConnectionsController::clearCastTopic()
{
    ++m_topicCastsGeneration;
    cancelNetworkReply(m_topicCastsRequest, this);
    m_activeCastTopic.clear();
    m_topicAuthorFid.clear();
    m_topicCastItems.clear();
    m_topicCastOffset=0;
    m_topicCastsHasMore=false;
    m_topicCastsError.clear();
    emit topicCastsChanged();
}

void OpenProofConnectionsController::selectCastTopic(
    const QString& slug, const QString& authorFid)
{
    static const QRegularExpression slugPattern{
        QStringLiteral("^[a-z][a-z0-9-]{1,63}$")};
    static const QRegularExpression fidPattern{
        QStringLiteral("^[1-9][0-9]{0,18}$")};
    clearCastTopic();
    if(!slugPattern.match(slug).hasMatch()
       || (!authorFid.isEmpty() && !fidPattern.match(authorFid).hasMatch()))
        return;
    m_activeCastTopic=slug;
    m_topicAuthorFid=authorFid;
    if (!cloudAuthorizationReady()) {
        // Keep the selected topic and show pending state. fetchTopics()
        // will resume this read when the existing account grant returns.
        // Do not ask the user to re-sign a protocol message.
        emit topicCastsChanged();
        return;
    }
    fetchTopicCastPage();
}

void OpenProofConnectionsController::loadMoreTopicCasts()
{
    if(m_topicCastsHasMore && !m_topicCastsRequest)fetchTopicCastPage();
}

void OpenProofConnectionsController::fetchTopicCastPage()
{
    if(m_activeCastTopic.isEmpty() || m_topicCastsRequest
       || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl))
        return;
    constexpr int pageSize=30;
    const auto offset=m_topicCastOffset;
    QUrl endpoint=m_cloudUrl;
    endpoint.setPath(QStringLiteral("/api/v1/cloud/topic_casts"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("topic"),m_activeCastTopic);
    query.addQueryItem(QStringLiteral("limit"),QString::number(pageSize));
    query.addQueryItem(QStringLiteral("offset"),QString::number(offset));
    if(!m_topicAuthorFid.isEmpty())
        query.addQueryItem(QStringLiteral("author_fid"),m_topicAuthorFid);
    endpoint.setQuery(query);
    QNetworkRequest request{endpoint};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(12'000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    auto* reply=m_network.get(request);
    m_topicCastsRequest=reply;
    const auto requestGeneration=m_topicCastsGeneration;
    const auto viewer=m_viewerFid;
    const auto currentTopic=m_activeCastTopic;
    m_topicCastsError.clear();
    emit topicCastsChanged();
    connect(reply,&QNetworkReply::finished,this,
            [this,reply,requestGeneration,viewer,currentTopic,offset,pageSize] {
        if(m_topicCastsRequest==reply)m_topicCastsRequest=nullptr;
        const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(requestGeneration!=m_topicCastsGeneration
           || viewer!=m_viewerFid || currentTopic!=m_activeCastTopic) {
            reply->deleteLater();
            return;
        }
        const auto body=reply->readAll();
        if(status!=200 || reply->error()!=QNetworkReply::NoError
           || body.size()>64*1024) {
            m_topicCastsError=status==401
                ? QStringLiteral("Sign in to browse GenyCaster topics")
                : QStringLiteral("Unable to load topic casts");
        } else {
            QJsonParseError parseError;
            const auto doc=QJsonDocument::fromJson(body,&parseError);
            const auto rows=doc.object().value(QStringLiteral("items")).toArray();
            static const QRegularExpression hashPattern{
                QStringLiteral("^0x[0-9a-fA-F]{40}$")};
            QSet<QString> known;
            for(const auto& existing : m_topicCastItems)
                known.insert(existing.toMap().value(QStringLiteral("cast_hash")).toString());
            if(parseError.error!=QJsonParseError::NoError || rows.size()>30) {
                m_topicCastsError=QStringLiteral("Invalid topic response");
            } else {
                for(const auto& row : rows) {
                    const auto obj=row.toObject();
                    const auto hash=obj.value(QStringLiteral("cast_hash")).toString().toLower();
                    if(!hashPattern.match(hash).hasMatch() || known.contains(hash))
                        continue;
                    known.insert(hash);
                    m_topicCastItems.push_back(QVariantMap{
                        {QStringLiteral("cast_hash"),hash},
                        {QStringLiteral("author_fid"),
                         obj.value(QStringLiteral("author_fid")).toString()}});
                }
                // The server offset counts raw rows. Invalid or duplicate hashes
                // must not cause the next page to repeat this same window.
                m_topicCastOffset=offset+rows.size();
                m_topicCastsHasMore=rows.size()==pageSize;
            }
        }
        reply->deleteLater();
        emit topicCastsChanged();
    });
}

void OpenProofConnectionsController::saveCastTopics(
    const QString& castHash,const QStringList& slugs)
{
    if(!m_viewerFid || slugs.isEmpty() || slugs.size()>3)return;
    static const QRegularExpression castPattern{
        QStringLiteral("^0x[0-9a-fA-F]{40}$")};
    static const QRegularExpression topicPattern{
        QStringLiteral("^[a-z][a-z0-9-]{1,63}$")};
    if(!castPattern.match(castHash).hasMatch())return;
    QSet<QString> unique;
    for(const auto& slug : slugs) {
        if(!topicPattern.match(slug).hasMatch() || unique.contains(slug))return;
        unique.insert(slug);
    }
    if(!m_settings)return;
    const auto normalized=castHash.toLower();
    QVariantList remaining;
    for(const auto& item : m_topicOutbox) {
        if(item.toMap().value(QStringLiteral("cast_hash")).toString()!=normalized)
            remaining.push_back(item);
    }
    remaining.push_back(QVariantMap{
        {QStringLiteral("cast_hash"),normalized},
        {QStringLiteral("topics"),slugs},
        {QStringLiteral("fid"),QVariant::fromValue<qulonglong>(m_viewerFid)}
    });
    m_topicOutbox=std::move(remaining);
    m_settings->setValue(topicOutboxKey(),m_topicOutbox);
    m_settings->sync();
    pumpTopicWrites();
}

void OpenProofConnectionsController::pumpTopicWrites()
{
    if(m_topicSaveRequest || m_topicOutbox.isEmpty() || !m_viewerFid
        || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl))return;
    const auto item=m_topicOutbox.first().toMap();
    if(item.value(QStringLiteral("fid")).toULongLong()!=m_viewerFid)return;
    QUrl endpoint=m_cloudUrl;
    endpoint.setPath(QStringLiteral("/api/v1/cloud/cast_topics"));
    endpoint.setQuery(QString{});
    QNetworkRequest request{endpoint};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    const auto payload=QJsonObject{
        {QStringLiteral("cast_hash"),item.value(QStringLiteral("cast_hash")).toString()},
        {QStringLiteral("topics"),QJsonArray::fromStringList(
            item.value(QStringLiteral("topics")).toStringList())}
    };
    auto* reply=m_network.put(request,QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_topicSaveRequest=reply;
    const auto generation=m_grantGeneration;
    const auto owner=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,owner,item] {
        if(m_topicSaveRequest==reply)m_topicSaveRequest=nullptr;
        const auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(generation==m_grantGeneration && owner==m_viewerFid) {
            if(code==200 || code==400 || code==409) {
                for(auto it=m_topicOutbox.begin();it!=m_topicOutbox.end();++it) {
                    if(it->toMap()==item) {m_topicOutbox.erase(it);break;}
                }
                if(m_settings) {
                    m_settings->setValue(topicOutboxKey(),m_topicOutbox);
                    m_settings->sync();
                }
            }
            m_topicsError=code==200 ? QString{} :
                QStringLiteral("Unable to save cast topics");
            emit topicsChanged();
        }
        reply->deleteLater();
        if(generation!=m_grantGeneration || owner!=m_viewerFid)return;
        if(code==200 || code==400 || code==409) {
            // A resolved item must not strand the remaining offline queue.
            m_topicRetryDelayMs=10'000;
            if(code==200) {
                // The user may already be viewing the selected topic while
                // this newly published cast finishes indexing. Refresh that
                // server-backed filter immediately after its write commits.
                const auto slugs=item.value(QStringLiteral("topics")).toStringList();
                const auto topic=m_activeCastTopic;
                const auto author=m_topicAuthorFid;
                if(slugs.contains(topic) &&
                   (author.isEmpty() || author==QString::number(owner)))
                    selectCastTopic(topic,author);
                fetchTopics(); // Refresh counts after an accepted association.
            }
            pumpTopicWrites();
        } else if(!m_topicOutbox.isEmpty()) {
            // The protocol's indexing may lag behind a confirmed post.
            // Keep the durable association and retry without user action.
            const int delay=m_topicRetryDelayMs;
            m_topicRetryDelayMs=std::min(m_topicRetryDelayMs*2,120'000);
            QTimer::singleShot(delay,this,[this,owner,generation] {
                if(owner==m_viewerFid && generation==m_grantGeneration)
                    pumpTopicWrites();
            });
        }
    });
}

void OpenProofConnectionsController::sendBookmarkCollectionRequest(
    const QString& resource, const QVariantMap& body, const bool upload,
    const QString& queryId)
{
    if (m_bookmarkCollectionRequest || !m_viewerFid
        || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl)) return;
    if (resource != QStringLiteral("bookmark_collections")
        && resource != QStringLiteral("bookmark_collection_items")) return;
    QUrl endpoint=m_cloudUrl;
    endpoint.setPath(QStringLiteral("/api/v1/cloud/") + resource);
    endpoint.setQuery(QString{});
    if(!queryId.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("collection_id"),queryId);
        endpoint.setQuery(query);
    }
    QNetworkRequest request{endpoint};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(9'000);
    request.setRawHeader("Accept","application/json");
    authorizeCloudRequest(request);
    QNetworkReply* reply{};
    if(upload) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/json"));
        reply=m_network.put(request,QJsonDocument(QJsonObject::fromVariantMap(body))
                                .toJson(QJsonDocument::Compact));
    } else reply=m_network.get(request);
    m_bookmarkCollectionRequest=reply;
    m_bookmarkCollectionsError.clear();
    emit bookmarkCollectionsChanged();
    const auto generation=m_grantGeneration;
    const auto viewer=m_viewerFid;
    connect(reply,&QNetworkReply::finished,this,
            [this,reply,generation,viewer,resource,upload,queryId] {
        if(m_bookmarkCollectionRequest==reply) m_bookmarkCollectionRequest=nullptr;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes=reply->bytesAvailable()<=96*1024
            ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if(generation!=m_grantGeneration || viewer!=m_viewerFid)return;
        const QJsonDocument document=QJsonDocument::fromJson(bytes);
        if(status!=200 || !document.isObject()) {
            m_bookmarkCollectionsError=QStringLiteral("Could not update bookmark collections");
            emit bookmarkCollectionsChanged();
            return;
        }
        if(!upload) {
            const QJsonValue values=document.object().value(QStringLiteral("items"));
            if(!values.isArray() || values.toArray().size()>500) {
                m_bookmarkCollectionsError=QStringLiteral("Invalid collection response");
                emit bookmarkCollectionsChanged();
                return;
            }
            QVariantList items;
            for(const auto& value : values.toArray())
                if(value.isObject())items.push_back(value.toObject().toVariantMap());
            if(resource==QStringLiteral("bookmark_collections")) {
                m_bookmarkCollections=std::move(items);
            } else if(queryId.isEmpty()) {
                m_bookmarkMembershipItems=std::move(items);
            } else if(queryId==m_activeBookmarkCollection) {
                m_bookmarkCollectionItems=std::move(items);
            }
        }
        m_bookmarkCollectionsError.clear();
        emit bookmarkCollectionsChanged();
        if(upload) {
            // Reads are ordered after the committed write. Avoid optimistic
            // membership state or caching account-wide lists across FIDs.
            QMetaObject::invokeMethod(this,[this] { refreshBookmarkCollections(); },
                                      Qt::QueuedConnection);
        } else if(resource==QStringLiteral("bookmark_collections")) {
            const auto selected=m_activeBookmarkCollection;
            QMetaObject::invokeMethod(this,[this,selected] {
                if(selected!=m_activeBookmarkCollection)return;
                if(selected.isEmpty()) {
                    sendBookmarkCollectionRequest(
                        QStringLiteral("bookmark_collection_items"),{},false);
                } else {
                    selectBookmarkCollection(selected);
                }
            },Qt::QueuedConnection);
        } else if(resource==QStringLiteral("bookmark_collection_items")
                  && !queryId.isEmpty()) {
            QMetaObject::invokeMethod(this,[this] {
                sendBookmarkCollectionRequest(
                    QStringLiteral("bookmark_collection_items"),{},false);
            },Qt::QueuedConnection);
        }
    });
}

void OpenProofConnectionsController::requestBookmarkCollectionChoice(
    const QString& castHash)
{
    static const QRegularExpression pattern{
        QStringLiteral("^0x[0-9a-fA-F]{40}$")};
    if(!m_viewerFid || !cloudAuthorizationReady()
       || !pattern.match(castHash).hasMatch())return;
    emit bookmarkCollectionChoiceRequested(castHash.toLower());
}

void OpenProofConnectionsController::refreshBookmarkCollections()
{
    sendBookmarkCollectionRequest(QStringLiteral("bookmark_collections"),{},false);
}

void OpenProofConnectionsController::selectBookmarkCollection(const QString& id)
{
    static const QRegularExpression uuid{
        QStringLiteral("^[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}$")};
    if(!id.isEmpty() && !uuid.match(id).hasMatch())return;
    if(m_bookmarkCollectionRequest)return;
    m_activeBookmarkCollection=id.toLower();
    m_bookmarkCollectionItems.clear();
    emit bookmarkCollectionsChanged();
    if(!id.isEmpty())
        sendBookmarkCollectionRequest(QStringLiteral("bookmark_collection_items"),
                                      {},false,m_activeBookmarkCollection);
}

void OpenProofConnectionsController::createBookmarkCollection(const QString& name)
{
    if(name.trimmed()!=name || name.isEmpty() || name.size()>80)return;
    sendBookmarkCollectionRequest(QStringLiteral("bookmark_collections"),
        {{QStringLiteral("action"),QStringLiteral("create")},
         {QStringLiteral("id"),QUuid::createUuid().toString(QUuid::WithoutBraces)},
         {QStringLiteral("name"),name}},true);
}

void OpenProofConnectionsController::renameBookmarkCollection(
    const QString& id,const QString& name,const QString& revision)
{
    if(name.trimmed()!=name || name.isEmpty() || name.size()>80)return;
    sendBookmarkCollectionRequest(QStringLiteral("bookmark_collections"),
        {{QStringLiteral("action"),QStringLiteral("rename")},
         {QStringLiteral("id"),id},{QStringLiteral("name"),name},
         {QStringLiteral("expected_revision"),revision}},true);
}

void OpenProofConnectionsController::deleteBookmarkCollection(
    const QString& id,const QString& revision)
{
    if(id==m_activeBookmarkCollection) {
        m_activeBookmarkCollection.clear();
        m_bookmarkCollectionItems.clear();
        emit bookmarkCollectionsChanged();
    }
    sendBookmarkCollectionRequest(QStringLiteral("bookmark_collections"),
        {{QStringLiteral("action"),QStringLiteral("delete")},
         {QStringLiteral("id"),id},{QStringLiteral("expected_revision"),revision}},true);
}

void OpenProofConnectionsController::setBookmarkCollectionMembership(
    const QString& castHash,const QString& collectionId,const bool remove)
{
    static const QRegularExpression hash{QStringLiteral("^0x[0-9a-fA-F]{40}$")};
    if(!hash.match(castHash).hasMatch())return;
    sendBookmarkCollectionRequest(QStringLiteral("bookmark_collection_items"),
        {{QStringLiteral("collection_id"),collectionId},
         {QStringLiteral("cast_hash"),castHash.toLower()},
         {QStringLiteral("remove"),remove}},true);
}

void OpenProofConnectionsController::setViewerFid(const qulonglong fid)
{
    if (m_viewerFid == fid) return;
    if(m_viewerFid)revokeAccountSession();
    cancelAccountSessionRequest();
    m_cloudSessionVerified=false;
    m_cloudCsrf.clear();
    m_authorization.cancel();
    clearAccountGrant();
    m_authorizing = false;
    m_approvalUrl.clear();
    m_error.clear();
    m_viewerFid = fid;
    cancelNetworkReply(m_streakRead, this);
    cancelNetworkReply(m_streakWrite, this);
    m_pendingStreakCasts=m_settings && fid
        ? m_settings->value(QStringLiteral("streak/outbox/%1").arg(fid)).toStringList()
        : QStringList{};
    m_streakRetryDelayMs=10'000;
    cancelNetworkReply(m_publicActivityRead, this);
    m_publicActivityFid=0;
    m_publicActivitySource=QStringLiteral("casts");
    m_publicActivityGithubLogin.clear();
    cancelNetworkReply(m_streakPreferencesRead, this);
    cancelNetworkReply(m_streakPreferencesWrite, this);
    cancelNetworkReply(m_mutedProfilesRead, this);
    cancelNetworkReply(m_mutedProfilesWrite, this);
    m_mutedProfileFids.clear();
    m_mutedProfilesRevision=0;
    m_mutedProfilesFetchedAt=0;
    m_mutedProfilesLoaded=false;
    m_mutedProfilesError.clear();
    emit mutedProfilesChanged();
    cancelNetworkReply(m_streakLeaderboardRead, this);
    m_streakPreferences.clear();
    m_streakLeaderboard.clear();
    m_streak = m_settings && fid
        ? m_settings->value(QStringLiteral("streak/snapshot/%1").arg(fid)).toMap()
        : QVariantMap{};
    m_streakError.clear();
    emit streakChanged();
    cancelNetworkReply(m_bookmarkCollectionRequest, this);
    m_bookmarkCollections.clear();
    m_bookmarkCollectionItems.clear();
    m_bookmarkMembershipItems.clear();
    m_activeBookmarkCollection.clear();
    m_bookmarkCollectionsError.clear();
    emit bookmarkCollectionsChanged();
    cancelNetworkReply(m_topicCatalogRequest, this);
    m_topicCatalog = offlineTopicCatalog();
    cancelNetworkReply(m_topicSaveRequest, this);
    m_topicOutbox=topicOutboxKey().isEmpty() || !m_settings
        ? QVariantList{} : m_settings->value(topicOutboxKey()).toList();
    m_topicRetryDelayMs=10'000;
    m_topicsError.clear();
    emit topicsChanged();
    const QString key=bookmarkStorageKey();
    m_bookmarkOutbox=key.isEmpty() || !m_settings
        ? QVariantList{} : m_settings->value(key+QStringLiteral("/outbox")).toList();
    m_bookmarkError.clear();
    emit cloudStateChanged();
    m_bookmarkCursor=key.isEmpty() || !m_settings
        ? QStringLiteral("0") : m_settings->value(
            key+QStringLiteral("/cursor"),QStringLiteral("0")).toString();
    m_legacyBookmarksPending=false;
    m_legacyBookmarkCandidates.clear();
    m_bookmarkHistoryKeys.clear();
    m_bookmarkPreviewsReady=false;
    m_bookmarkPreviewSent.clear();
    m_portableConnectionsPublished=false;
    m_notificationSeenAtMs=m_settings && fid
        ? m_settings->value(QStringLiteral("notifications/seen_at/%1").arg(fid),
                            0).toLongLong() : 0;
    m_notificationRemoteAtMs=0;
    const auto draftKey=draftStorageKey();
    m_draftRevision=draftKey.isEmpty() || !m_settings ? 0 :
        m_settings->value(draftKey+QStringLiteral("/revision"),0).toInt();
    m_draftMutationSerial=draftKey.isEmpty() || !m_settings ? 0 :
        m_settings->value(draftKey+QStringLiteral("/serial"),0).toULongLong();
    m_draftRemoteRevision=0;
    m_draftRemoteInitialized=false;
    m_draftConflict=false;
    restoreCachedConnections();
    emit stateChanged();
    if(m_viewerFid)restoreAccountSession();
}

void OpenProofConnectionsController::acceptAuthenticatedGrant(
    QByteArray bearer, QString subject, const std::uint64_t fid,
    const int expiresSeconds)
{
    if (!m_viewerFid || fid != m_viewerFid || bearer.isEmpty()
        || bearer.size() > 8192 || subject.isEmpty() || subject.size() > 512
        || !trustedHttpsEndpoint(m_gatewayUrl)) {
        bearer.fill('\0');
        return;
    }
    // A delegated grant may be delivered immediately before its own
    // Authenticated observer. Do not invalidate that observer's generation
    // unless a previous authenticated grant really needs replacement.
    if (!m_accountBearer.isEmpty()) clearAccountGrant();
    m_accountBearer = std::move(bearer);
    m_accountSubject = std::move(subject);
    m_grantExpiry.start(std::clamp(expiresSeconds - 10, 1, 3'590) * 1'000);
    m_error.clear();
    fetchTopics();
    refreshConnections();
    pumpStreakWrites();
}

void OpenProofConnectionsController::acceptRecoveryGrant(
    QByteArray ticket, QString subject, const std::uint64_t fid,
    const int expiresSeconds)
{
    // The one-time proof established OpenProof identity on the server; only
    // the time-limited account capability enters this process, never cookies
    // or a secret custody key. A stale/out-of-order FID must be rejected.
    static const QRegularExpression ticketFormat{
        QStringLiteral("^recovery\\.[A-Za-z0-9_-]{43}$")};
    if (!m_viewerFid || fid != m_viewerFid ||
        subject.isEmpty() || subject.size() > 512 ||
        !ticketFormat.match(QString::fromLatin1(ticket)).hasMatch() ||
        expiresSeconds < 60 || expiresSeconds > 3600 ||
        !trustedHttpsEndpoint(m_gatewayUrl)) {
        ticket.fill('\0');
        return;
    }
    if (!m_accountBearer.isEmpty()) clearAccountGrant();
    m_accountBearer = std::move(ticket);
    m_accountSubject = std::move(subject);
    m_grantExpiry.start(std::clamp(expiresSeconds - 10, 1, 3'590) * 1'000);
    m_error.clear();
    // The account server independently verifies the short-lived recovery
    // bearer. Native devices need their own persistent authenticated session.
    emit stateChanged();
    m_bookmarkPoll.start();
    bootstrapAccountSession();
    if (!m_cloudServiceAvailable) checkCloudService();
    fetchTopics();
    refreshConnections();
    pumpTopicWrites();
    pumpStreakWrites();
}

void OpenProofConnectionsController::stopAuthorization()
{
    if (!m_authorizing) return;
    m_authorization.cancel();
    m_authorizing = false;
    m_approvalUrl.clear();
    emit stateChanged();
}

void OpenProofConnectionsController::authorizeAccount()
{
    if (!m_viewerFid || m_authorizing) return;
    clearAccountGrant();
    m_authorizing = true;
    m_approvalUrl.clear();
    m_error.clear();
    const auto requiredFid = m_viewerFid;
    const auto generation = m_grantGeneration;
    emit stateChanged();
    // Connections authorization comes from a signed Farcaster proof, not
    // from Genyleap credentials or OpenProof's device-login screen. The same
    // proof is checked against the currently displayed FID on both sides.
    m_authorization.beginFarcaster(
        [this, generation](BrowserOpenProofState state) {
            if (generation != m_grantGeneration) return;
            switch (state.phase) {
            case BrowserOpenProofState::Phase::AwaitingApproval:
                m_approvalUrl = state.approvalUrl; break;
            case BrowserOpenProofState::Phase::Failed:
                m_error = state.error;
                m_authorizing = false;
                m_approvalUrl.clear(); break;
            case BrowserOpenProofState::Phase::Authenticated:
            case BrowserOpenProofState::Phase::SignedOut:
                m_authorizing = false;
                m_approvalUrl.clear(); break;
            case BrowserOpenProofState::Phase::Starting:
                break;
            }
            emit stateChanged();
        },
        [this, requiredFid, generation](QByteArray ticket, QString subject,
                                         const std::uint64_t fid, const int expirySeconds) {
            if (generation != m_grantGeneration || !requiredFid
                || m_viewerFid != requiredFid || fid != requiredFid) {
                ticket.fill('\0');
                m_error = QStringLiteral("Farcaster proof must match your active profile.");
                m_authorizing = false;
                emit stateChanged();
                return;
            }
            acceptRecoveryGrant(std::move(ticket), std::move(subject), fid,
                                expirySeconds);
        });
}

QUrl OpenProofConnectionsController::gatewayActionUrl(const QString& suffix) const
{
    if (!trustedHttpsEndpoint(m_gatewayUrl)) return {};
    QUrl url = m_gatewayUrl;
    url.setPath(m_gatewayUrl.path() + suffix);
    return url;
}

void OpenProofConnectionsController::sendAccountRequest(
    const QUrl& url, QByteArray body, std::function<void(int, QByteArray)> callback)
{
    if (!m_viewerFid || m_accountBearer.isEmpty() || m_request
        || !trustedHttpsEndpoint(url) || url.scheme() != m_gatewayUrl.scheme() || url.host() != m_gatewayUrl.host() || url.port() != m_gatewayUrl.port()) {
        return;
    }
    auto request = QNetworkRequest{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10'000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_accountBearer);
    QNetworkReply* reply{};
    if (body.isNull()) {
        reply = m_network.get(request);
    } else {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_network.post(request, body);
    }
    m_request = reply;
    const auto generation = m_grantGeneration;
    connect(reply, &QNetworkReply::finished, this,
        [this, generation, reply, callback = std::move(callback)] {
            if (m_request == reply) m_request = nullptr;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool valid = reply->error() == QNetworkReply::NoError
                || (status >= 400 && status < 500);
            const QByteArray data = valid && reply->bytesAvailable() <= 64 * 1024
                ? reply->readAll() : QByteArray{};
            reply->deleteLater();
            if (generation != m_grantGeneration) return;
            if (status == 401 || status == 403) {
                clearAccountGrant();
                restoreCachedConnections();
                m_error.clear();
                emit stateChanged();
                return;
            }
            callback(valid ? status : 0, data);
        });
}

void OpenProofConnectionsController::refreshConnections()
{
    if (m_accountBearer.isEmpty() || m_request) return;
    sendAccountRequest(gatewayActionUrl(), {},
        [this](const int status, QByteArray payload) {
            const auto parsed = QJsonDocument::fromJson(payload);
            if (status != 200 || !parsed.isObject()
                || !parsed.object().value(QStringLiteral("connections")).isArray()) {
                if (m_pendingProvider.isEmpty())
                    m_error = QStringLiteral("Unable to read OpenProof connections.");
                emit stateChanged();
                return;
            }
            const auto values = parsed.object().value(QStringLiteral("connections")).toArray();
            bool linkedFid = false;
            int farcasterClaims = 0;
            QVariantList current{};
            for (const auto& value : values) {
                if (!value.isObject()) continue;
                const auto json = value.toObject();
                const QString provider = json.value(QStringLiteral("provider")).toString();
                const QString subject = json.value(QStringLiteral("subject")).toString();
                if (!validProviderName(provider) || subject.size() > 512) continue;
                if (provider == QStringLiteral("farcaster")) {
                    ++farcasterClaims;
                    bool valid{};
                    const auto fidValue = json.value(QStringLiteral("fid"));
                    const qulonglong linked = fidValue.toVariant().toULongLong(&valid);
                    linkedFid = linkedFid || (valid && linked == m_viewerFid);
                }
                // Never synthesize a LinkedIn vanity URL from display_name or
                // the OIDC sub. Prefer provider-backed profile_url and only
                // use preferred_username when that exact claim is available.
                QString publicUrl;
                QString handle = json.value(QStringLiteral("preferred_username")).toString();
                if (provider == QStringLiteral("github")) {
                    // GitHub may return a provider-backed profile URL without
                    // preferred_username. Never infer the login from an
                    // editable bio or the opaque OpenProof subject.
                    const QUrl candidate{json.value(QStringLiteral("profile_url")).toString()};
                    static const QRegularExpression githubPath{
                        QStringLiteral("^/([A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?)/?$")};
                    const auto match=githubPath.match(candidate.path());
                    if(candidate.scheme()==QStringLiteral("https")
                        && candidate.host()==QStringLiteral("github.com")
                        && candidate.port()==-1 && candidate.userInfo().isEmpty()
                        && candidate.query().isEmpty() && candidate.fragment().isEmpty()
                        && match.hasMatch()) {
                        publicUrl=QStringLiteral("https://github.com/")+match.captured(1);
                        if(handle.isEmpty())handle=match.captured(1);
                    }
                }
                if (provider == QStringLiteral("linkedin")) {
                    const QString profileUrl = json.value(QStringLiteral("profile_url")).toString();
                    const QUrl url{profileUrl};
                    static const QRegularExpression profilePath{
                        QStringLiteral("^/in/[A-Za-z0-9_-]{3,100}/?$")};
                    static const QRegularExpression vanity{
                        QStringLiteral("^[A-Za-z0-9_-]{3,100}$")};
                    static const QRegularExpression verifiedRedirect{
                        QStringLiteral("^/profile-thirdparty-redirect/[-A-Za-z0-9_%.]{1,1024}$")};
                    if (url.scheme() == QStringLiteral("https")
                        && (url.host() == QStringLiteral("linkedin.com")
                            || url.host() == QStringLiteral("www.linkedin.com"))
                        && url.port() == -1 && url.userInfo().isEmpty()
                        && url.query().isEmpty() && url.fragment().isEmpty()
                        && (profilePath.match(url.path()).hasMatch()
                            || verifiedRedirect.match(url.path()).hasMatch())) {
                        publicUrl = QStringLiteral("https://www.linkedin.com")
                            + url.path();
                    } else if (vanity.match(handle).hasMatch()) {
                        publicUrl = QStringLiteral("https://www.linkedin.com/in/") + handle;
                    }
                }
                current.push_back(QVariantMap{
                    {QStringLiteral("provider"), provider},
                    {QStringLiteral("subject"), subject},
                    {QStringLiteral("displayName"),
                        json.value(QStringLiteral("display_name")).toString()},
                    {QStringLiteral("username"), handle},
                    {QStringLiteral("profileUrl"), publicUrl},
                    {QStringLiteral("manualProfileUrl"), provider == QStringLiteral("linkedin")
                        ? ([&] { const QString key = manualLinkedInSettingKey();
                            return key.isEmpty() || !m_settings ? QString{} : m_settings->value(key).toString(); }())
                        : QString{}},
                });
            }
            if (!linkedFid || farcasterClaims != 1) {
                clearAccountGrant();
                m_error = QStringLiteral(
                    "This OpenProof identity is not linked to the active Farcaster FID.");
                emit stateChanged();
                return;
            }
            m_connections = std::move(current);
            m_verified = true;
            m_cachedSnapshot = false;
            m_bookmarkPoll.start();
            if(!m_cloudSessionVerified)bootstrapAccountSession();
            if (m_cloudServiceAvailable) {
                reconcileLinkedIn();
                pumpBookmarkChanges();
                pumpDraftChanges();
            }
            else
                checkCloudService();
            saveCachedConnections();
            publishVerifiedConnectionsSnapshot();
            m_error.clear();
            // A LinkedIn reauthorization begins with an already linked account.
            // It succeeds only when the new provider-backed URL is observed,
            // not merely because the old OpenProof subject is still connected.
            bool linkedInUrlReady = false;
            for (const auto& connection : m_connections) {
                const auto item = connection.toMap();
                if (item.value(QStringLiteral("provider")).toString() == QStringLiteral("linkedin")
                    && !item.value(QStringLiteral("profileUrl")).toString().isEmpty()) {
                    linkedInUrlReady = true;
                    break;
                }
            }
            if (!m_pendingProvider.isEmpty() && isConnected(m_pendingProvider)
                && (!m_reconnectingLinkedIn || linkedInUrlReady)) {
#if defined(__EMSCRIPTEN__)
                // Only a server-confirmed OpenProof link closes the browser
                // window; browser redirect/204 alone is not a success signal.
                closeGenycasterConnectionWindow();
#endif
                finishAction();
            }
            emit stateChanged();
        });
}

bool OpenProofConnectionsController::isConnected(
    const QString& provider, const QString& subject) const
{
    for (const auto& entry : m_connections) {
        const auto item = entry.toMap();
        if (item.value(QStringLiteral("provider")).toString() == provider
            && (subject.isEmpty()
                || item.value(QStringLiteral("subject")).toString() == subject)) return true;
    }
    return false;
}

void OpenProofConnectionsController::finishAction(QString error)
{
#if defined(__EMSCRIPTEN__)
    // The result is established by the authenticated connections response,
    // not by any redirect or an untrusted message from the popup.
    closeGenycasterConnectionWindow();
#endif
    m_actionBusy = false;
    m_connectionPoll.stop();
    m_pendingProvider.clear();
    m_reconnectingLinkedIn = false;
    m_remainingPolls = 0;
    m_error = std::move(error);
    emit stateChanged();
}

void OpenProofConnectionsController::connectProvider(const QString& provider)
{
    // Only server-verified GenyCaster Pro may initiate new external links.
    // Disconnect/revoke remains available to every account for safety.
    const auto entitlement = proEntitlements();
    if (entitlement.value(QStringLiteral("status")).toString()
            != QStringLiteral("verified")
        || !entitlement.value(QStringLiteral("eligible")).toBool()) {
        m_error = QStringLiteral("GenyCaster Pro is required to connect an external account.");
        emit stateChanged();
        return;
    }
    const bool alreadyLinked = isConnected(provider);
    if (!m_verified || m_actionBusy || m_request || !m_providers.contains(provider)
        || (alreadyLinked && provider != QStringLiteral("linkedin"))) return;
    // Signed web3 connections have a distinct SIWE/SIWF contract; never
    // pretend their provider is a redirect OAuth account.
    if (!QStringList{QStringLiteral("x"), QStringLiteral("github"),
                     QStringLiteral("google"), QStringLiteral("linkedin"),
                     QStringLiteral("telegram")}.contains(provider)) {
        m_error = QStringLiteral("This provider requires a separate signed wallet or Farcaster proof.");
        emit stateChanged();
        return;
    }
#if defined(__EMSCRIPTEN__)
    if (prepareGenycasterConnectionWindow() == 0) {
        m_error = QStringLiteral("The browser blocked secure account approval. Allow popups for Genycaster and retry.");
        emit stateChanged();
        return;
    }
#endif
    m_actionBusy = true;
    m_reconnectingLinkedIn = alreadyLinked && provider == QStringLiteral("linkedin");
    m_pendingProvider = provider;
    m_error.clear();
    emit stateChanged();
    sendAccountRequest(gatewayActionUrl(QStringLiteral("/handoff")),
        QJsonDocument(QJsonObject{{QStringLiteral("provider"), provider}})
            .toJson(QJsonDocument::Compact),
        [this, provider](const int status, const QByteArray& payload) {
            const auto parsed = QJsonDocument::fromJson(payload).object();
            const QUrl url{parsed.value(QStringLiteral("handoff_url")).toString()};
            if (status != 200 || !trustedHttpsEndpoint(m_browserConnectionBaseUrl)
                || url.scheme() != m_browserConnectionBaseUrl.scheme()
                || url.host() != m_browserConnectionBaseUrl.host()
                || url.port() != m_browserConnectionBaseUrl.port()
                || url.path() != m_browserConnectionBaseUrl.path() + QStringLiteral("/account/connections/handoff")
                || !url.hasQuery() || !QUrlQuery{url}.hasQueryItem(QStringLiteral("ticket"))
                || !trustedHttpsEndpoint(url.adjusted(QUrl::RemoveQuery))) {
                finishAction(QStringLiteral("Could not create a secure OpenProof connection."));
                return;
            }
            // OAuth consent itself belongs to the identity provider; the
            // Genycaster drawer owns initiation, polling and final status.
            // The WASM window was created synchronously on the initial click.
            bool opened = false;
#if defined(__EMSCRIPTEN__)
            const QByteArray destination = url.toString(QUrl::FullyEncoded).toUtf8();
            opened = navigateGenycasterConnectionWindow(destination.constData()) != 0;
#else
            opened = QDesktopServices::openUrl(url);
#endif
            if (!opened) {
                finishAction(QStringLiteral("Could not open the provider. Allow popups and retry."));
                return;
            }
            m_remainingPolls = 40;
            m_connectionPoll.start();
            emit stateChanged();
        });
}

void OpenProofConnectionsController::disconnectProvider(
    const QString& provider, const QString& subject)
{
    if (!m_verified || m_actionBusy || m_request || !isConnected(provider, subject)
        || provider == QStringLiteral("farcaster") || subject.isEmpty()
        || subject.startsWith(QStringLiteral("server-confirmed:"))) return;
    m_actionBusy = true;
    m_error.clear();
    emit stateChanged();
    sendAccountRequest(gatewayActionUrl(QStringLiteral("/disconnect")),
        QJsonDocument(QJsonObject{{QStringLiteral("provider"), provider},
                                 {QStringLiteral("subject"), subject}})
            .toJson(QJsonDocument::Compact),
        [this](const int status, const QByteArray& payload) {
            const auto parsed = QJsonDocument::fromJson(payload).object();
            if (status != 200 || !parsed.value(QStringLiteral("disconnected")).toBool()) {
                finishAction(QStringLiteral("OpenProof refused the disconnect request."));
                return;
            }
            finishAction();
            refreshConnections();
        });
}

void OpenProofConnectionsController::fetchFeedback()
{
    if (m_feedbackRead) return;
    if (!m_viewerFid || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl)) {
        m_feedbackError = QStringLiteral("authentication_required");
        emit feedbackChanged();
        return;
    }
    QUrl url = m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/feedback"));
    url.setQuery(QString{});
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(12'000);
    request.setRawHeader("Accept", "application/json");
    authorizeCloudRequest(request);
    auto* const reply = m_network.get(request);
    m_feedbackRead = reply;
    m_feedbackError.clear();
    emit feedbackChanged();
    const auto generation = m_grantGeneration;
    const auto viewer = m_viewerFid;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, viewer] {
        if (m_feedbackRead != reply) { reply->deleteLater(); return; }
        m_feedbackRead.clear();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // abort() can synchronously finish a closed reply on macOS. Do not
        // read an unreadable QIODevice or adopt another account's results.
        const QByteArray bytes = status == 200 && reply->isReadable()
            // Owner-only history now includes full report text and notes.
            // Up to 30 bounded reports may exceed the old 32 KiB cap.
            && reply->bytesAvailable() <= 512 * 1024 ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if (viewer != m_viewerFid || generation != m_grantGeneration) return;
        const auto doc = QJsonDocument::fromJson(bytes);
        if (status == 200 && doc.isObject()
            && doc.object().value(QStringLiteral("stats")).isObject()
            && doc.object().value(QStringLiteral("reports")).isArray()) {
            m_feedbackStats = doc.object().value(QStringLiteral("stats")).toObject().toVariantMap();
            m_feedbackReports = doc.object().value(QStringLiteral("reports")).toArray().toVariantList();
            m_feedbackError.clear();
        } else {
            m_feedbackError = status == 401 ? QStringLiteral("authentication_required")
                                            : QStringLiteral("feedback_unavailable");
        }
        emit feedbackChanged();
    });
}

void OpenProofConnectionsController::submitFeedback(
    const QString& category, const int sentiment,
    const QString& message, const QString& route)
{
    if (m_feedbackWrite) return;
    m_feedbackSubmitted = false;
    const QString body = message.trimmed();
    const QStringList allowed{QStringLiteral("bug"), QStringLiteral("performance"),
                              QStringLiteral("improvement"), QStringLiteral("idea")};
    if (!allowed.contains(category) || sentiment < 1 || sentiment > 5
        || body.length() < 10 || body.length() > 4000) {
        m_feedbackError = QStringLiteral("invalid_feedback");
        emit feedbackChanged();
        return;
    }
    if (!m_viewerFid || !cloudAuthorizationReady() || !trustedHttpsEndpoint(m_cloudUrl)) {
        m_feedbackError = QStringLiteral("authentication_required");
        emit feedbackChanged();
        return;
    }
    QUrl url = m_cloudUrl;
    url.setPath(QStringLiteral("/api/v1/cloud/feedback"));
    url.setQuery(QString{});
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15'000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    authorizeCloudRequest(request);
    const QJsonObject payload{
        {QStringLiteral("category"), category},
        {QStringLiteral("sentiment"), sentiment},
        {QStringLiteral("message"), body},
        {QStringLiteral("platform"), QSysInfo::productType().left(32)},
        {QStringLiteral("appVersion"), QCoreApplication::applicationVersion().left(40)},
        {QStringLiteral("route"), route.left(96)}
    };
    auto* const reply = m_network.post(request, QJsonDocument{payload}.toJson(QJsonDocument::Compact));
    m_feedbackWrite = reply;
    m_feedbackError.clear();
    emit feedbackChanged();
    const auto generation = m_grantGeneration;
    const auto viewer = m_viewerFid;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, viewer, body] {
        if (m_feedbackWrite != reply) { reply->deleteLater(); return; }
        m_feedbackWrite.clear();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // Only a server-confirmed 201 may place the submitted message in history.
        // This prevents the newly submitted report disappearing while its GET
        // refresh is pending (or a previous history GET is still in flight).
        const QByteArray confirmationBytes = status == 201 && reply->isReadable()
            && reply->bytesAvailable() <= 16 * 1024 ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        if (viewer != m_viewerFid || generation != m_grantGeneration) return;
        m_feedbackSubmitted = status == 201;
        m_feedbackError = status == 201 ? QString{}
            : status == 429 ? QStringLiteral("feedback_limit_reached")
            : status == 400 ? QStringLiteral("invalid_feedback")
            : status == 401 ? QStringLiteral("authentication_required")
            : QStringLiteral("feedback_unavailable");
        if (m_feedbackSubmitted) {
            const auto confirmation = QJsonDocument::fromJson(confirmationBytes);
            if (confirmation.isObject()
                && confirmation.object().value(QStringLiteral("report")).isObject()) {
                auto report = confirmation.object().value(QStringLiteral("report")).toObject().toVariantMap();
                // Older API versions returned the ID/status but omitted the
                // text. The submitted body is safe to show only after 201.
                if (report.value(QStringLiteral("message")).toString().isEmpty())
                    report.insert(QStringLiteral("message"), body);
                const QString reportId = report.value(QStringLiteral("id")).toString();
                if (!reportId.isEmpty()) {
                    bool alreadyPresent = false;
                    for (const auto& existing : m_feedbackReports) {
                        if (existing.toMap().value(QStringLiteral("id")).toString() == reportId) {
                            alreadyPresent = true;
                            break;
                        }
                    }
                    if (!alreadyPresent) m_feedbackReports.prepend(report);
                }
            }
        }
        emit feedbackChanged();
        if (m_feedbackSubmitted) {
            // Do not let an older in-flight GET replace a newly confirmed report.
            cancelNetworkReply(m_feedbackRead, this);
            fetchFeedback();
        }
    });
}
