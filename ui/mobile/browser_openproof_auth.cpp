#include "browser_openproof_auth.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace {

constexpr auto issuer = "https://openproof.genyleap.com";
constexpr auto clientId = "genycaster-native-v1";

QNetworkRequest secureRequest(const char* path)
{
    QNetworkRequest request{QUrl{QString::fromLatin1(issuer) + QString::fromLatin1(path)}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15'000);
    request.setRawHeader("Accept", "application/json");
    return request;
}

QJsonObject objectFrom(const QByteArray& body)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    return error.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : QJsonObject{};
}

QString safeFailure(const int status)
{
    return QStringLiteral("OpenProof browser authentication failed (HTTP %1).").arg(status);
}

} // namespace

BrowserOpenProofAuth::BrowserOpenProofAuth(QObject* parent)
    : QObject(parent)
{
    m_pollTimer.setSingleShot(true);
    connect(&m_pollTimer, &QTimer::timeout, this, [this] { pollToken(); });
    m_sessionExpiryTimer.setSingleShot(true);
    connect(&m_sessionExpiryTimer, &QTimer::timeout, this, [this] { signOut(); });
}

BrowserOpenProofAuth::~BrowserOpenProofAuth()
{
    reset();
}

void BrowserOpenProofAuth::reset()
{
    ++m_generation;
    m_pollTimer.stop();
    m_sessionExpiryTimer.stop();
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_deviceCode.fill('\0');
    m_deviceCode.clear();
    m_farcasterTicket.fill('\0');
    m_farcasterTicket.clear();
    m_directFarcaster = false;
    m_accessToken.fill('\0');
    m_accessToken.clear();
    m_subject.clear();
    m_approvalUrl.clear();
    m_deadlineMs = 0;
    m_intervalSeconds = 5;
    m_tokenExpiresSeconds = 3600;
}

void BrowserOpenProofAuth::emitState(BrowserOpenProofState state)
{
    if (m_observer) {
        m_observer(std::move(state));
    }
}

void BrowserOpenProofAuth::fail(QString message)
{
    reset();
    BrowserOpenProofState result{};
    result.phase = BrowserOpenProofState::Phase::Failed;
    result.error = std::move(message);
    emitState(std::move(result));
}

void BrowserOpenProofAuth::send(QNetworkRequest request, QByteArray body, Response callback)
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
    const std::uint64_t generation = m_generation;
    QNetworkReply* const reply = body.isNull()
        ? m_network.get(request)
        : m_network.post(request, body);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation, callback = std::move(callback)]() mutable {
        if (generation != m_generation) {
            reply->deleteLater();
            return;
        }
        if (m_reply == reply) m_reply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool successfulTransport = reply->error() == QNetworkReply::NoError
            || (status >= 400 && status < 500);
        const QByteArray payload = reply->readAll();
        reply->deleteLater();
        if (!successfulTransport || payload.size() > 256 * 1024) {
            fail(QStringLiteral("Unable to contact OpenProof securely."));
            return;
        }
        callback(status, payload);
    });
}

void BrowserOpenProofAuth::begin(Observer observer)
{
    reset();
    m_observer = std::move(observer);
    emitState(BrowserOpenProofState{.phase = BrowserOpenProofState::Phase::Starting});

    QUrlQuery form{};
    form.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(clientId));
    form.addQueryItem(QStringLiteral("scope"), QStringLiteral("openid profile account"));
    auto request = secureRequest("/oauth/device_authorization");
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    send(std::move(request), form.toString(QUrl::FullyEncoded).toUtf8(),
         [this](const int status, QByteArray payload) {
        if (status != 200) { fail(safeFailure(status)); return; }
        const auto response = objectFrom(payload);
        const QString deviceCode = response.value(QStringLiteral("device_code")).toString();
        const QString userCode = response.value(QStringLiteral("user_code")).toString();
        if (deviceCode.isEmpty() || userCode.isEmpty() || deviceCode.size() > 4096 ||
            userCode.size() > 256) {
            fail(QStringLiteral("OpenProof returned an incomplete authorization challenge."));
            return;
        }
        m_deviceCode = deviceCode.toUtf8();
        m_intervalSeconds = std::clamp(response.value(QStringLiteral("interval")).toInt(5), 5, 30);
        const int expiry = std::clamp(response.value(QStringLiteral("expires_in")).toInt(600), 60, 600);
        m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(expiry) * 1000;

        // Reuse the existing Genyleap account ceremony; never collect passwords
        // or custody secrets inside Qt/QML.
        const QString continuation = QStringLiteral("/openproof/oauth/device?user_code=") +
            QString::fromLatin1(QUrl::toPercentEncoding(userCode));
        QUrl approval{QStringLiteral("https://genyleap.com/account")};
        QUrlQuery query{};
        query.addQueryItem(QStringLiteral("return_to"), continuation);
        approval.setQuery(query);
        m_approvalUrl = approval.toString(QUrl::FullyEncoded);
        emitState(BrowserOpenProofState{
            .phase = BrowserOpenProofState::Phase::AwaitingApproval,
            .approvalUrl = m_approvalUrl,
        });
        startPolling();
    });
}

void BrowserOpenProofAuth::beginFarcaster(Observer observer)
{
    reset();
    m_directFarcaster = true;
    m_observer = std::move(observer);
    emitState(BrowserOpenProofState{.phase = BrowserOpenProofState::Phase::Starting});

    auto request = QNetworkRequest{QUrl{QStringLiteral(
        "https://storage-farcaster.genyleap.com/v1/browser/farcaster/start")}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15'000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    send(std::move(request), QByteArrayLiteral("{}"),
        [this](const int status, QByteArray payload) {
        if (status != 200) { fail(safeFailure(status)); return; }
        const auto json = objectFrom(payload);
        const QString ticket = json.value(QStringLiteral("ticket")).toString();
        const QUrl approval{json.value(QStringLiteral("approval_url")).toString()};
        if (ticket.size() != 43 || !approval.isValid() ||
            approval.scheme() != QStringLiteral("https") ||
            (approval.host() != QStringLiteral("warpcast.com") &&
             approval.host() != QStringLiteral("farcaster.xyz"))) {
            fail(QStringLiteral("The Farcaster proof challenge was invalid."));
            return;
        }
        m_farcasterTicket = ticket.toUtf8();
        m_intervalSeconds = 3;
        m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + 300'000;
        m_approvalUrl = approval.toString(QUrl::FullyEncoded);
        emitState(BrowserOpenProofState{
            .phase = BrowserOpenProofState::Phase::AwaitingApproval,
            .approvalUrl = m_approvalUrl,
        });
        startPolling();
    });
}

void BrowserOpenProofAuth::pollFarcaster()
{
    if (m_farcasterTicket.isEmpty()) return;
    if (QDateTime::currentMSecsSinceEpoch() >= m_deadlineMs) {
        fail(QStringLiteral("The Farcaster proof challenge expired."));
        return;
    }
    QNetworkRequest request{QUrl{QStringLiteral(
        "https://storage-farcaster.genyleap.com/v1/browser/farcaster/poll")}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15'000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    const QByteArray payload = QByteArrayLiteral("{\"ticket\":\"")
        + m_farcasterTicket + QByteArrayLiteral("\"}");
    send(std::move(request), payload,
        [this](const int status, QByteArray payload) {
        const auto json = objectFrom(payload);
        if (status == 202 && json.value(QStringLiteral("pending")).toBool()) {
            startPolling();
            return;
        }
        if (status != 200 || !json.value(QStringLiteral("authenticated")).toBool()) {
            fail(safeFailure(status));
            return;
        }
        const QString subject = json.value(QStringLiteral("subject")).toString();
        bool valid = false;
        const auto fid = json.value(QStringLiteral("fid")).toString().toULongLong(&valid);
        if (subject.trimmed().isEmpty() || subject.size() > 512 || !valid || fid == 0) {
            fail(QStringLiteral("OpenProof returned an invalid Farcaster identity."));
            return;
        }
        BrowserOpenProofState authenticated{};
        authenticated.phase = BrowserOpenProofState::Phase::Authenticated;
        authenticated.subject = subject;
        authenticated.fid = fid;
        authenticated.username = json.value(QStringLiteral("username")).toString();
        authenticated.displayName = json.value(QStringLiteral("display_name")).toString();
        authenticated.avatarUrl = json.value(QStringLiteral("avatar_url")).toString();
        authenticated.tokenExpiresSeconds = 3600;
        m_farcasterTicket.fill('\0');
        m_farcasterTicket.clear();
        m_sessionExpiryTimer.start(authenticated.tokenExpiresSeconds * 1000);
        emitState(std::move(authenticated));
    });
}

void BrowserOpenProofAuth::startPolling()
{
    if (m_deadlineMs && QDateTime::currentMSecsSinceEpoch() >= m_deadlineMs) {
        fail(QStringLiteral("OpenProof authorization expired. Please try again."));
        return;
    }
    m_pollTimer.start(m_intervalSeconds * 1000);
}

void BrowserOpenProofAuth::pollToken()
{
    if (m_directFarcaster) { pollFarcaster(); return; }
    if (m_deviceCode.isEmpty()) return;
    if (QDateTime::currentMSecsSinceEpoch() >= m_deadlineMs) {
        fail(QStringLiteral("OpenProof authorization expired. Please try again."));
        return;
    }
    QUrlQuery form{};
    form.addQueryItem(QStringLiteral("grant_type"),
                      QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
    form.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(clientId));
    form.addQueryItem(QStringLiteral("device_code"), QString::fromUtf8(m_deviceCode));
    auto request = secureRequest("/oauth/token");
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    send(std::move(request), form.toString(QUrl::FullyEncoded).toUtf8(),
         [this](const int status, QByteArray payload) {
        const auto response = objectFrom(payload);
        if (status != 200) {
            const QString code = response.value(QStringLiteral("error")).toString();
            if (code == QStringLiteral("authorization_pending")) { startPolling(); return; }
            if (code == QStringLiteral("slow_down")) {
                m_intervalSeconds = std::min(m_intervalSeconds + 5, 60);
                startPolling();
                return;
            }
            fail(code == QStringLiteral("access_denied")
                ? QStringLiteral("OpenProof sign-in was denied.")
                : safeFailure(status));
            return;
        }
        m_accessToken = response.value(QStringLiteral("access_token")).toString().toUtf8();
        if (m_accessToken.isEmpty() || m_accessToken.size() > 8192) {
            fail(QStringLiteral("OpenProof returned no valid access token."));
            return;
        }
        m_tokenExpiresSeconds = std::clamp(
            response.value(QStringLiteral("expires_in")).toInt(3600), 60, 3600);
        m_deviceCode.fill('\0');
        m_deviceCode.clear();
        checkUserinfo();
    });
}

void BrowserOpenProofAuth::checkUserinfo()
{
    auto request = secureRequest("/oauth/userinfo");
    request.setRawHeader("Authorization", "Bearer " + m_accessToken);
    send(std::move(request), QByteArray{},
         [this](const int status, QByteArray payload) {
        if (status != 200) { fail(safeFailure(status)); return; }
        const auto object = objectFrom(payload);
        m_subject = object.value(QStringLiteral("sub")).toString().trimmed();
        if (m_subject.isEmpty() || m_subject.size() > 512) {
            fail(QStringLiteral("OpenProof userinfo omitted the verified subject."));
            return;
        }
        checkConnections();
    });
}

void BrowserOpenProofAuth::checkConnections()
{
    auto request = QNetworkRequest{QUrl{QStringLiteral(
        "https://storage-farcaster.genyleap.com/v1/browser/openproof/connections")}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15'000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_accessToken);
    send(std::move(request), QByteArray{},
         [this](const int status, QByteArray payload) {
        if (status != 200) { fail(safeFailure(status)); return; }
        const auto json = objectFrom(payload);
        const auto connections = json.value(QStringLiteral("connections"));
        if (!connections.isArray()) {
            fail(QStringLiteral("OpenProof did not return verified account connections."));
            return;
        }
        std::uint64_t fid = 0;
        QString username{}, displayName{}, avatarUrl{};
        for (const auto& connectionValue : connections.toArray()) {
            if (!connectionValue.isObject()) continue;
            const auto connection = connectionValue.toObject();
            if (connection.value(QStringLiteral("provider")).toString().toLower()
                != QStringLiteral("farcaster")) continue;
            if (fid != 0) {
                fail(QStringLiteral("Multiple Farcaster identities are linked; choose one in Genyleap."));
                return;
            }
            bool valid = false;
            const auto value = connection.value(QStringLiteral("fid"));
            const auto parsed = value.isString()
                ? value.toString().toULongLong(&valid)
                : value.toVariant().toULongLong(&valid);
            if (!valid || parsed == 0) {
                fail(QStringLiteral("OpenProof returned an invalid linked Farcaster FID."));
                return;
            }
            fid = parsed;
            username = connection.value(QStringLiteral("preferred_username")).toString();
            displayName = connection.value(QStringLiteral("display_name")).toString();
            avatarUrl = connection.value(QStringLiteral("picture_url")).toString();
        }
        if (fid == 0) {
            fail(QStringLiteral("Link your Farcaster account in Genyleap, then sign in again."));
            return;
        }
        BrowserOpenProofState authenticated{};
        authenticated.phase = BrowserOpenProofState::Phase::Authenticated;
        authenticated.subject = m_subject;
        authenticated.fid = fid;
        authenticated.username = std::move(username);
        authenticated.displayName = std::move(displayName);
        authenticated.avatarUrl = std::move(avatarUrl);
        authenticated.tokenExpiresSeconds = m_tokenExpiresSeconds;
        // All token material remains ephemeral; no local-storage bearer or
        // refresh token. Protocol signing still requires a separate signer.
        m_accessToken.fill('\0');
        m_accessToken.clear();
        m_sessionExpiryTimer.start(m_tokenExpiresSeconds * 1000);
        emitState(std::move(authenticated));
    });
}

void BrowserOpenProofAuth::cancel()
{
    reset();
    emitState(BrowserOpenProofState{.phase = BrowserOpenProofState::Phase::SignedOut});
}

void BrowserOpenProofAuth::signOut()
{
    cancel();
}
