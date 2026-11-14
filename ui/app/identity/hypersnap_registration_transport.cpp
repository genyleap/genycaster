#include "hypersnap_registration_transport.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <utility>

namespace {
constexpr qint64 MaxResponseBytes = 32768;
constexpr qsizetype MaxPayloadBytes = 8192;
constexpr quint64 FirstNativeFid = (1ULL << 32);

bool validCustodyAddress(const QString& address)
{
    if (address.size() != 42 || !address.startsWith(QLatin1String("0x")))
        return false;
    for (qsizetype i = 2; i < address.size(); ++i) {
        const ushort c = address.at(i).unicode();
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
              || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

std::pair<bool, std::uint64_t> exactUnsigned(const QJsonValue& value)
{
    // FID is a JSON decimal STRING in the HyperSnap API, not an IEEE 754
    // double (which silently loses precision for u64 values above 2^53).
    if (!value.isString()) return {false, 0};
    const QString text = value.toString();
    if (text.isEmpty() || text.size() > 20) return {false, 0};
    for (QChar c : text)
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return {false, 0};
    bool ok = false;
    const qulonglong result = text.toULongLong(&ok, 10);
    return {ok, static_cast<std::uint64_t>(result)};
}

QNetworkRequest requestFor(const QUrl& url)
{
    QNetworkRequest request{url};
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    return request;
}
} // namespace

HyperSnapRegistrationTransport::HyperSnapRegistrationTransport(
    QUrl base, QObject* parent)
    : QObject{parent}, m_base{std::move(base)}
{}

bool HyperSnapRegistrationTransport::endpointIsSafe() const noexcept
{
    if (!m_base.isValid() || m_base.host().isEmpty()
        || !m_base.userName().isEmpty() || !m_base.password().isEmpty()
        || m_base.hasQuery() || m_base.hasFragment()
        || (m_base.path() != QStringLiteral("/")
            && !m_base.path().isEmpty()))
        return false;
    const auto host = m_base.host().toLower();
    const bool local = host == QStringLiteral("localhost")
        || host == QStringLiteral("127.0.0.1")
        || host == QStringLiteral("::1");
    return m_base.scheme() == QStringLiteral("https")
        || (local && m_base.scheme() == QStringLiteral("http"));
}

void HyperSnapRegistrationTransport::lookupByCustody(
    const QString& address, LookupCallback onResult)
{
    if (!endpointIsSafe() || !validCustodyAddress(address)) {
        onResult({LookupState::InvalidResponse});
        return;
    }
    QUrl target = m_base.resolved(QUrl{QStringLiteral("/v1/nativeAccount")});
    QUrlQuery query{};
    query.addQueryItem(QStringLiteral("custody"), address.toLower());
    target.setQuery(query);
    QNetworkReply* const reply = m_network.get(requestFor(target));
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [reply, expected = address.toLower(),
                      cb = std::move(onResult)]() mutable {
        const int code = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto err = reply->error();
        const QByteArray payload = reply->bytesAvailable() <= MaxResponseBytes
            ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        // The public HyperSnap hub currently returns HTTP 400 for a missing
        // custody account, with a precise gRPC NotFound detail. Do NOT turn
        // arbitrary HTTP 400/404 responses into "wallet is unregistered":
        // the URL might be unsupported, misrouted or temporarily broken.
        if (code == 400) {
            const auto diagnostic = QJsonDocument::fromJson(payload);
            if (diagnostic.isObject()) {
                const auto body = diagnostic.object();
                const auto detail =
                    body.value(QStringLiteral("error_detail")).toString();
                if (body.value(QStringLiteral("error")).toString()
                        == QStringLiteral("Native registration lookup failed")
                    && detail.startsWith(QStringLiteral(
                        "status: NotFound, message: \"native account not found\""))) {
                    cb({LookupState::NotFound});
                    return;
                }
            }
        }
        if (err != QNetworkReply::NoError || code != 200) {
            cb({LookupState::Unavailable});
            return;
        }
        const auto json = QJsonDocument::fromJson(payload);
        if (!json.isObject()) {
            cb({LookupState::InvalidResponse});
            return;
        }
        const QJsonObject data = json.object();
        const auto [fidValid, fid] = exactUnsigned(
            data.value(QStringLiteral("fid")));
        const auto custody = data.value(
            QStringLiteral("custodyAddress")).toString().toLower();
        const auto block = data.value(QStringLiteral("blockNumber"));
        // Require authoritative shard-0 inclusion and a matching on-chain
        // custody key. No application session can be established from a
        // response lacking the message that the node included.
        if (!fidValid || fid < FirstNativeFid || custody != expected
            || !block.isDouble() || block.toDouble() < 1
            || !data.value(QStringLiteral("message")).isObject()) {
            cb({LookupState::InvalidResponse});
            return;
        }
        cb({LookupState::Confirmed, fid,
            static_cast<std::uint64_t>(block.toDouble())});
    });
}

void HyperSnapRegistrationTransport::submit(
    const QByteArray& protobufEnvelope, SubmitCallback onResult)
{
    if (!endpointIsSafe() || protobufEnvelope.isEmpty()
        || protobufEnvelope.size() > MaxPayloadBytes) {
        onResult({SubmitState::Rejected});
        return;
    }
    auto request = requestFor(m_base.resolved(
        QUrl{QStringLiteral("/v1/submitMessage")}));
    // HyperSnap's HTTP server accepts "application/octet-stream" EXACTLY;
    // application/x-protobuf and application/json are rejected.
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/octet-stream"));
    QNetworkReply* const reply = m_network.post(request, protobufEnvelope);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [reply, cb = std::move(onResult)]() mutable {
        const int code = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto err = reply->error();
        reply->deleteLater();
        // A 200 only means the hub accepted the submission. The outcome
        // stays pending until a separate nativeAccount confirmation.
        if (err == QNetworkReply::NoError && code == 200) {
            cb({SubmitState::AcceptedPending});
        } else if (code == 400 || code == 403 || code == 422) {
            cb({SubmitState::Rejected});
        } else {
            // A timeout/5xx may have happened AFTER the node accepted the
            // message. A blind replay could double-submit. Lookup first.
            cb({SubmitState::UnknownOutcome});
        }
    });
}
