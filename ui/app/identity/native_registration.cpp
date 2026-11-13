#include "native_registration.hpp"
#include "hypersnap_registration_transport.hpp"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <cmath>

NativeRegistrationStatusController::NativeRegistrationStatusController(
    QString baseUrl, QObject* parent)
    : QObject(parent), m_base{baseUrl.trimmed()},
      m_transport{std::make_unique<HyperSnapRegistrationTransport>(m_base, this)}
{}

NativeRegistrationStatusController::~NativeRegistrationStatusController() = default;

bool NativeRegistrationStatusController::verifiedAbsentForCustody(
    const QString& address) const
{
    return m_lookupChecked && !m_lookupBusy && !m_lookupFailed
        && m_existingFid.isEmpty() && !m_lookupAddress.isEmpty()
        && m_lookupAddress == address.toLower();
}

void NativeRegistrationStatusController::clearExistingLookup()
{
    ++m_lookupGeneration;
    m_lookupBusy = false;
    m_lookupChecked = false;
    m_lookupFailed = false;
    m_existingFid.clear();
    m_lookupAddress.clear();
    emit lookupChanged();
}

void NativeRegistrationStatusController::findExistingNativeAccount(
    const QString& address)
{
    clearExistingLookup();
    m_lookupBusy = true;
    m_lookupAddress = address.toLower();
    emit lookupChanged();
    const auto generation = m_lookupGeneration;
    m_transport->lookupByCustody(address,
        [this, generation](HyperSnapRegistrationTransport::LookupResult result) {
            if (generation != m_lookupGeneration) return;
            m_lookupBusy = false;
            m_lookupChecked = true;
            using State = HyperSnapRegistrationTransport::LookupState;
            m_lookupFailed = result.state != State::Confirmed
                             && result.state != State::NotFound;
            if (result.state == State::Confirmed)
                m_existingFid = QString::number(static_cast<qulonglong>(result.fid));
            emit lookupChanged();
        });
}

std::optional<NativeRegistrationStatusController::Challenge>
NativeRegistrationStatusController::snapshot() const
{
    const auto ageMs = QDateTime::currentMSecsSinceEpoch() - m_checkedAtMs;
    if (!m_active || !m_challenge || ageMs < 0 || ageMs > 60'000)
        return std::nullopt;
    return m_challenge;
}

void NativeRegistrationStatusController::refresh()
{
    // QNetworkReply::abort() can emit finished() synchronously. That callback
    // clears m_reply, so detach the guarded pointer *before* aborting it.
    // Keep a QPointer copy in case another slot destroys the reply inline.
    auto previousReply = m_reply;
    m_reply = nullptr;
    if (previousReply) {
        previousReply->abort();
        if (previousReply)
            previousReply->deleteLater();
    }
    m_active = false;
    m_checking = false;
    m_error.clear();
    m_bits = 0;
    m_firstFid.clear();
    m_challenge.reset();
    m_checkedAtMs = 0;
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
        const auto next = fields.value(QStringLiteral("nextBlock")).toDouble();
        auto validHex = [](QStringView hex) {
            if (hex.size() != 66 || hex.left(2) != QStringView{u"0x"})
                return false;
            for (const auto ch : hex.sliced(2)) {
                const auto v = ch.unicode();
                if (!((v >= '0' && v <= '9') || (v >= 'a' && v <= 'f')
                      || (v >= 'A' && v <= 'F'))) return false;
            }
            return true;
        };
        if (bits < 1 || bits > 256 || !validHex(anchor)
            || !std::isfinite(height) || height < 1 || std::floor(height) != height
            || !std::isfinite(next) || next < height || std::floor(next) != next
            || next - height > window || window < 1
            || height > 9'007'199'254'740'991.0 || first.isEmpty()) {
            m_active = false;
            m_error = QStringLiteral("Native registration node returned invalid parameters.");
            emit changed();
            return;
        }
        m_bits = bits;
        m_firstFid = first;
        m_challenge = Challenge{
            .anchorHeight = static_cast<std::uint64_t>(height),
            .nextBlock = static_cast<std::uint64_t>(next),
            .anchorHash = anchor.toLower(),
            .window = static_cast<std::uint32_t>(window),
            .difficultyBits = static_cast<std::uint32_t>(bits)
        };
        m_checkedAtMs = QDateTime::currentMSecsSinceEpoch(); // JSON string avoids unsigned-64 precision loss.
        m_active = fields.value(QStringLiteral("active")).toBool();
        m_error = m_active ? QString{} :
            QStringLiteral("The selected HyperSnap node has not enabled native registration.");
        emit changed();
    });
}
