#include "native_registration.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

NativeRegistrationStatusController::NativeRegistrationStatusController(
    QString baseUrl, QObject* parent)
    : QObject(parent), m_base{baseUrl.trimmed()}
{}

void NativeRegistrationStatusController::refresh()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_active = false;
    m_checking = false;
    m_error.clear();
    m_bits = 0;
    m_firstFid.clear();
    // Avoid SSRF, insecure transport, redirects or disguised HTTPS HTTP.
    const bool localHost = m_base.host() == QStringLiteral("127.0.0.1")
        || m_base.host() == QStringLiteral("localhost");
    if (!m_base.isValid() || m_base.host().isEmpty()
        || m_base.hasQuery() || m_base.hasFragment()
        || (m_base.scheme() != QStringLiteral("https")
            && !(localHost && m_base.scheme() == QStringLiteral("http")))) {
        m_error = QStringLiteral("Native registration requires a valid HTTPS node.");
        emit changed();
        return;
    }
    auto endpoint = m_base.resolved(QUrl{QStringLiteral("/v1/nativeRegistration")});
    QNetworkRequest request{endpoint};
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    m_reply = m_network.get(request);
    m_checking = true;
    emit changed();
    const auto reply = m_reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply != m_reply) return;
        m_reply = nullptr;
        m_checking = false;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto fault = reply->error();
        const auto bytes = reply->bytesAvailable() <= 8192 ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        const auto json = QJsonDocument::fromJson(bytes);
        if (fault != QNetworkReply::NoError || status != 200
            || !json.isObject()
            || !json.object().value(QStringLiteral("active")).isBool()) {
            m_active = false;
            m_error = QStringLiteral("Unable to verify native registration at the selected node.");
            emit changed();
            return;
        }
        const auto fields = json.object();
        const auto bits = fields.value(QStringLiteral("minDifficultyBits")).toInt();
        const auto first = fields.value(QStringLiteral("firstNativeFid")).toString();
        const auto anchor = fields.value(QStringLiteral("anchorBlockHash")).toString();
        const auto height = fields.value(QStringLiteral("anchorBlockHeight")).toDouble();
        const auto window = fields.value(QStringLiteral("anchorWindow")).toInt();
        if (bits < 1 || bits > 256 || anchor.size() != 66
            || height < 1 || window < 1 || first.isEmpty()) {
            m_active = false;
            m_error = QStringLiteral("Native registration node returned invalid parameters.");
            emit changed();
            return;
        }
        m_bits = bits;
        m_firstFid = first; // JSON string avoids unsigned-64 precision loss.
        m_active = fields.value(QStringLiteral("active")).toBool();
        m_error = m_active ? QString{} :
            QStringLiteral("The selected HyperSnap node has not enabled native registration.");
        emit changed();
    });
}
