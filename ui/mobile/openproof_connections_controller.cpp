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
#include <QUrlQuery>

#include <algorithm>

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

OpenProofConnectionsController::OpenProofConnectionsController(QObject* parent)
    : QObject(parent),
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
        m_error = QStringLiteral("OpenProof account authorization expired. Authorize again.");
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
    m_providers = {
        QStringLiteral("x"), QStringLiteral("github"), QStringLiteral("google"),
        QStringLiteral("linkedin"), QStringLiteral("telegram"),
        QStringLiteral("farcaster"), QStringLiteral("ethereum-wallet")
    };
}

QStringList OpenProofConnectionsController::providers() const { return m_providers; }
bool OpenProofConnectionsController::loading() const noexcept { return m_loading; }
QString OpenProofConnectionsController::error() const { return m_error; }
bool OpenProofConnectionsController::authorized() const noexcept { return m_verified; }
bool OpenProofConnectionsController::authorizing() const noexcept { return m_authorizing; }
QString OpenProofConnectionsController::approvalUrl() const { return m_approvalUrl; }
QString OpenProofConnectionsController::consentUrl() const { return m_consentUrl; }
QVariantList OpenProofConnectionsController::connections() const { return m_connections; }
bool OpenProofConnectionsController::actionBusy() const noexcept { return m_actionBusy; }
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
        if (latest.isEmpty()) {
            // An offline/CORS failure must not blank already visible choices.
            m_error = QStringLiteral("Provider catalog unavailable; showing known options.");
        } else {
            m_providers = latest;
            m_error.clear();
        }
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
            if (validProviderName(provider) && provider != QStringLiteral("local")
                && provider != QStringLiteral("passkey") && !result.contains(provider)) {
                result.push_back(provider);
            }
        }
    }
    // Local password and passkey management belong to account settings,
    // not the social/external identity picker.
    result.sort(Qt::CaseInsensitive);
    return result;
}

void OpenProofConnectionsController::clearAccountGrant()
{
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
    m_actionBusy = false;
    m_pendingProvider.clear();
    m_consentUrl.clear();
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
    m_authorization.begin(
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
                m_authorizing = false;
                m_approvalUrl.clear(); break;
            case BrowserOpenProofState::Phase::SignedOut:
                m_authorizing = false;
                m_approvalUrl.clear(); break;
            case BrowserOpenProofState::Phase::Starting:
                break;
            }
            emit stateChanged();
        },
        [this, requiredFid, generation](QByteArray bearer, QString subject,
                                        const std::uint64_t fid, const int expirySeconds) {
            if (generation != m_grantGeneration || !requiredFid
                || m_viewerFid != requiredFid || fid != requiredFid
                || !trustedHttpsEndpoint(m_gatewayUrl) || bearer.size() > 8192
                || bearer.isEmpty() || subject.isEmpty()) {
                bearer.fill('\0');
                m_error = QStringLiteral(
                    "OpenProof authorization must match your active Farcaster profile.");
                emit stateChanged();
                return;
            }
            m_accountBearer = std::move(bearer);
            m_accountSubject = std::move(subject);
            m_grantExpiry.start(std::clamp(expirySeconds - 10, 1, 3'590) * 1'000);
            refreshConnections();
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
                m_error = QStringLiteral("OpenProof account session expired. Authorize again.");
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
                current.push_back(QVariantMap{
                    {QStringLiteral("provider"), provider},
                    {QStringLiteral("subject"), subject},
                    {QStringLiteral("displayName"),
                        json.value(QStringLiteral("display_name")).toString()},
                    {QStringLiteral("username"),
                        json.value(QStringLiteral("preferred_username")).toString()},
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
            m_error.clear();
            if (!m_pendingProvider.isEmpty() && isConnected(m_pendingProvider)) {
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
    m_actionBusy = false;
    m_connectionPoll.stop();
    m_pendingProvider.clear();
    m_consentUrl.clear();
    m_remainingPolls = 0;
    m_error = std::move(error);
    emit stateChanged();
}

void OpenProofConnectionsController::connectProvider(const QString& provider)
{
    if (!m_verified || m_actionBusy || m_request
        || !m_providers.contains(provider) || isConnected(provider)) return;
    // Signed web3 connections have a distinct SIWE/SIWF contract; never
    // pretend their provider is a redirect OAuth account.
    if (!QStringList{QStringLiteral("x"), QStringLiteral("github"),
                     QStringLiteral("google"), QStringLiteral("linkedin"),
                     QStringLiteral("telegram")}.contains(provider)) {
        m_error = QStringLiteral("This provider requires a separate signed wallet or Farcaster proof.");
        emit stateChanged();
        return;
    }
    m_actionBusy = true;
    m_error.clear();
    emit stateChanged();
    sendAccountRequest(gatewayActionUrl(QStringLiteral("/handoff")),
        QJsonDocument(QJsonObject{{QStringLiteral("provider"), provider}})
            .toJson(QJsonDocument::Compact),
        [this, provider](const int status, const QByteArray& payload) {
            const auto parsed = QJsonDocument::fromJson(payload).object();
            const QUrl url{parsed.value(QStringLiteral("handoff_url")).toString()};
            if (status != 200 || url.scheme() != m_issuerUrl.scheme() || url.host() != m_issuerUrl.host() || url.port() != m_issuerUrl.port()
                || url.path() != QStringLiteral("/account/connections/handoff")
                || !url.hasQuery() || !QUrlQuery{url}.hasQueryItem(QStringLiteral("ticket"))
                || !trustedHttpsEndpoint(url.adjusted(QUrl::RemoveQuery))) {
                finishAction(QStringLiteral("Could not create a secure OpenProof connection."));
                return;
            }
            // Opening the provider URL must be a direct user interaction in
            // WASM, otherwise browser popup blockers can discard it.
            m_pendingProvider = provider;
            m_consentUrl = url.toString(QUrl::FullyEncoded);
            m_remainingPolls = 40;
            emit stateChanged();
        });
}

void OpenProofConnectionsController::openConsent()
{
    if (!m_verified || m_consentUrl.isEmpty() || !m_actionBusy
        || m_pendingProvider.isEmpty()) return;
    const QUrl url{m_consentUrl};
    m_consentUrl.clear();
    if (!QDesktopServices::openUrl(url)) {
        finishAction(QStringLiteral("Could not open secure provider approval."));
        return;
    }
    m_connectionPoll.start();
    emit stateChanged();
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
