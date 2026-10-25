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
#include <QSettings>
#include <QUrlQuery>

#include <algorithm>

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

[[nodiscard]] bool validProviderName(const QString& value)
{
    static const QRegularExpression allowed{QStringLiteral("^[a-z][a-z0-9-]{0,39}$")};
    return allowed.match(value).hasMatch();
}

} // namespace

OpenProofConnectionsController::OpenProofConnectionsController(QObject* parent, QSettings* settings)
    : QObject(parent),
      m_settings(settings),
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
        m_error.clear();
        emit stateChanged();
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
            {QStringLiteral("profileUrl"), value.value(QStringLiteral("profileUrl")).toString()}});
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
            {QStringLiteral("profileUrl"), item.value(QStringLiteral("profileUrl")).toString().left(256)}});
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
    m_grantExpiry.stop();
    m_connectionPoll.stop();
    if (m_request) {
        m_request->abort();
        m_request->deleteLater();
        m_request = nullptr;
    }
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

void OpenProofConnectionsController::setViewerFid(const qulonglong fid)
{
    if (m_viewerFid == fid) return;
    m_authorization.cancel();
    clearAccountGrant();
    m_authorizing = false;
    m_approvalUrl.clear();
    m_error.clear();
    m_viewerFid = fid;
    restoreCachedConnections();
    emit stateChanged();
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
    refreshConnections();
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
    refreshConnections();
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
                const QString handle = json.value(QStringLiteral("preferred_username")).toString();
                if (provider == QStringLiteral("linkedin")) {
                    const QString value = json.value(QStringLiteral("profile_url")).toString();
                    const QUrl url{value};
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
            saveCachedConnections();
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
        || provider == QStringLiteral("farcaster") || subject.isEmpty()) return;
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
