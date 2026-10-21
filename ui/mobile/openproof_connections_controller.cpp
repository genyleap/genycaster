#include "openproof_connections_controller.hpp"

#include <QCoreApplication>
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
          QStringLiteral("openproofIssuerUrl"))),
      m_accountUrl(configuredUrl("GENYCASTER_ACCOUNT_MANAGEMENT_URL",
          QStringLiteral("accountManagementUrl")))
{
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

bool OpenProofConnectionsController::openManager(const QString& provider)
{
    if (!m_providers.contains(provider) || !validProviderName(provider)
        || !trustedHttpsEndpoint(m_accountUrl)) {
        m_error = QStringLiteral("Unknown provider or insecure account-management URL.");
        emit stateChanged();
        return false;
    }
    // Do NOT invent a direct OAuth link or infer that the browser session has
    // the same identity as the native device grant. Open the established
    // first-party account ceremony, which checks browser identity, connection
    // status, CSRF and provider-specific consent before any mutation.
    QUrl url = m_accountUrl;
    QUrlQuery query{url};
    query.removeAllQueryItems(QStringLiteral("section"));
    query.addQueryItem(QStringLiteral("section"), QStringLiteral("methods"));
    url.setQuery(query);
    const bool opened = QDesktopServices::openUrl(url);
    if (!opened) {
        m_error = QStringLiteral("Could not open the OpenProof account portal.");
        emit stateChanged();
    }
    return opened;
}
