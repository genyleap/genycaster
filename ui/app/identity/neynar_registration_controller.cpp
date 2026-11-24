#include "neynar_registration_controller.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace {
const QRegularExpression VoucherPattern{
    QStringLiteral("^[A-Za-z0-9_-]{40,1024}\\.[A-Za-z0-9_-]{43}$")};
const QRegularExpression NamePattern{QStringLiteral("^[a-z0-9][a-z0-9-]{0,15}$")};
const QRegularExpression FidPattern{QStringLiteral("^[1-9][0-9]{0,15}$")};
const QRegularExpression UintPattern{QStringLiteral("^(0|[1-9][0-9]{0,19})$")};
const QRegularExpression UuidPattern{
    QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$")};
[[nodiscard]] bool canonicalEndpoint(const QUrl& origin, bool allowLoopback)
{
    if (!origin.isValid() || !origin.userInfo().isEmpty()
        || !origin.query().isEmpty() || !origin.fragment().isEmpty()
        || (!origin.path().isEmpty() && origin.path() != QStringLiteral("/"))
        || origin.scheme() != QStringLiteral("https")) {
        return allowLoopback && origin.isValid()
            && origin.scheme() == QStringLiteral("http")
            && origin.host() == QStringLiteral("127.0.0.1")
            && origin.userInfo().isEmpty() && origin.query().isEmpty()
            && origin.fragment().isEmpty()
            && (origin.path().isEmpty() || origin.path() == QStringLiteral("/"));
    }
    return !origin.host().isEmpty() && origin.port(-1) == -1;
}
[[nodiscard]] bool decimalU64(const QString& value, std::uint64_t* target)
{
    if (!UintPattern.match(value).hasMatch()) return false;
    bool ok = false;
    const auto parsed = value.toULongLong(&ok);
    if (ok) *target = static_cast<std::uint64_t>(parsed);
    return ok;
}
[[nodiscard]] QString deterministicUuid(const QByteArray& voucher, const QString& wallet)
{
    QByteArray material = voucher;
    material.append('\0');
    material.append(wallet.toUtf8());
    auto hash = QCryptographicHash::hash(material, QCryptographicHash::Sha256);
    material.fill('\0');
    auto bytes = hash.left(16);
    bytes[6] = char((static_cast<unsigned char>(bytes[6]) & 0x0fU) | 0x40U);
    bytes[8] = char((static_cast<unsigned char>(bytes[8]) & 0x3fU) | 0x80U);
    return QUuid::fromRfc4122(bytes).toString(QUuid::WithoutBraces).toLower();
}
} // namespace

NeynarRegistrationController::NeynarRegistrationController(
    QUrl origin, FarcasterRegistrationPorts ports, QObject* parent, bool allowLocalTesting)
    : QObject{parent}, m_origin{std::move(origin)}, m_ports{std::move(ports)},
      m_endpointValid{canonicalEndpoint(m_origin, allowLocalTesting)}
{
    // Paid Farcaster enrollment is optional and may not be deployed. Never
    // issue a registration POST merely because the app started (including
    // signed-in users opening Notifications). The setup route probes only
    // when a user explicitly chooses the Farcaster registration provider.
}

NeynarRegistrationController::~NeynarRegistrationController()
{
    ++m_generation;
    if (m_reply) m_reply->abort();
    if (m_probeReply) m_probeReply->abort();
    m_voucher.fill('\0');
}

void NeynarRegistrationController::refreshAvailability()
{
    if (!m_endpointValid || m_probeBusy || pending() || confirmed()) return;
    auto url = m_origin.resolved(QUrl{QStringLiteral(
        "/api/v1/registration/farcaster/reserve")});
    QNetworkRequest request{url};
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    // An empty, unauthenticated request MUST be refused with 401, before
    // reading a wallet, leasing an FID, or spending sponsorship funds.
    // This distinguishes an installed protected relay from Nginx 404.
    m_probeBusy = true;
    m_probeReply = m_network.post(request, QByteArrayLiteral("{}"));
    auto* reply = m_probeReply.data();
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply != m_probeReply) return;
        m_probeReply = nullptr;
        m_probeBusy = false;
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto fault = reply->error();
        const QByteArray body = reply->bytesAvailable() <= 2048
            ? reply->readAll() : QByteArray{};
        reply->deleteLater();
        // Qt reports 401 as an HTTP error; the expected status + exact
        // error payload is nevertheless a successful guarded preflight.
        const auto response = QJsonDocument::fromJson(body);
        const bool installed = status == 401 && response.isObject()
            && response.object().value(QStringLiteral("error")).toString()
               == QStringLiteral("paid_enrollment_required");
        Q_UNUSED(fault);
        if (m_available != installed) m_available = installed;
        emit changed();
    });
    emit changed();
}

void NeynarRegistrationController::setIdentityRuntimeReady(const bool ready)
{
    if (m_registrationSupported == ready) return;
    m_registrationSupported = ready;
    // Readiness alone is not user intent to start sponsored enrollment.
    // Setup probes the endpoint on explicit provider selection.
    emit changed();
}

bool NeynarRegistrationController::walletMatches() const
{
    return m_ports.backupVerified && m_ports.backupVerified()
        && m_ports.custodyAddress && m_ports.custodyAddress().toLower() == m_wallet;
}

bool NeynarRegistrationController::canSubmit() const
{
    return m_registrationSupported && !m_busy
        && m_phase == QStringLiteral("reserved")
        && !m_nonce.isEmpty() && m_expiresAtMs > QDateTime::currentMSecsSinceEpoch()+40'000
        && walletMatches() && static_cast<bool>(m_ports.signTransfer);
}

bool NeynarRegistrationController::pending() const noexcept
{
    return m_phase == QStringLiteral("reserving")
        || m_phase == QStringLiteral("submitted")
        || m_phase == QStringLiteral("submission_unknown")
        || m_phase == QStringLiteral("submitting")
        || m_phase == QStringLiteral("reservation_unknown");
}

bool NeynarRegistrationController::confirmed() const noexcept
{
    return m_phase == QStringLiteral("confirmed");
}

void NeynarRegistrationController::setPhase(QString value)
{
    m_phase = std::move(value);
    emit changed();
}

void NeynarRegistrationController::fail(QString reason)
{
    m_error = std::move(reason);
    emit changed();
}

bool NeynarRegistrationController::useEnrollmentVoucher(QString voucher)
{
    if (!m_available || !m_registrationSupported || m_busy || pending() || confirmed()
        || m_phase == QStringLiteral("reserved")
        || !m_ports.backupVerified || !m_ports.backupVerified()
        || !m_ports.custodyAddress) return false;
    const auto valid = VoucherPattern.match(voucher).hasMatch();
    if (!valid) {
        voucher.fill(QChar{});
        fail(QStringLiteral("invalid_enrollment_voucher"));
        return false;
    }
    const QString address = m_ports.custodyAddress().toLower();
    if (!QRegularExpression{QStringLiteral("^0x[0-9a-f]{40}$")}.match(address).hasMatch())
        return false;
    m_voucher.fill('\0');
    m_voucher = voucher.toUtf8();
    voucher.fill(QChar{});
    m_wallet = address;
    m_reservationId = deterministicUuid(m_voucher, m_wallet);
    m_fid.clear();
    m_nonce.clear();
    m_expiresAtMs = 0;
    m_error.clear();
    setPhase(QStringLiteral("ready"));
    return true;
}

void NeynarRegistrationController::request(Operation operation, QByteArray body)
{
    const char* path = operation == Operation::Reserve
        ? "/api/v1/registration/farcaster/reserve"
        : operation == Operation::Submit
          ? "/api/v1/registration/farcaster/submit"
          : "/api/v1/registration/farcaster/status";
    if (m_busy || !m_available || !walletMatches()) return;
    m_busy = true;
    m_error.clear();
    const auto generation = ++m_generation;
    emit changed();
    QNetworkRequest req{m_origin.resolved(QUrl{QString::fromLatin1(path)})};
    req.setRawHeader("Accept", "application/json");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("X-Genycaster-Paid-Enrollment", m_voucher);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(35'000);
    QNetworkReply* reply = m_network.post(req, body);
    body.fill('\0'); // The request holds its own copy.
    m_reply = reply;
    QObject::connect(reply, &QNetworkReply::finished, this,
        [this, generation, operation, reply] {
            const auto status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto payload = reply->read(16'385);
            const auto tooLarge = reply->bytesAvailable() > 0;
            const auto ok = reply->error() == QNetworkReply::NoError;
            reply->deleteLater();
            if (m_reply == reply) m_reply = nullptr;
            if (generation != m_generation) return;
            m_busy = false;
            if (!walletMatches()) {
                fail(QStringLiteral("custody_wallet_changed"));
                return;
            }
            if (!ok || status != 200 || payload.size() > 16'384 || tooLarge) {
                const auto details = QJsonDocument::fromJson(payload).object();
                const QString reason = details.value(
                    QStringLiteral("error")).toString();
                // These two errors are explicitly issued BEFORE the backend
                // touches the paid FID allocator. They cannot leave a funded
                // transaction in flight and are safe to retry after fixing.
                const bool safeReject = operation == Operation::Reserve
                    && !tooLarge && payload.size() <= 16'384
                    && ((status == 503
                            && reason == QStringLiteral("registration_not_configured"))
                        || (status == 403
                            && reason == QStringLiteral("registration_not_authorized")));
                if (safeReject) {
                    if (status == 403) {
                        m_voucher.fill('\0');
                        m_voucher.clear();
                        m_reservationId.clear();
                        setPhase(QStringLiteral("idle"));
                        fail(QStringLiteral("invalid_enrollment_voucher"));
                    } else {
                        setPhase(QStringLiteral("ready"));
                        fail(QStringLiteral("registration_not_configured"));
                    }
                    return;
                }
                // Timeouts or errors after an irreversible allocation/transfer
                // are ambiguous. Never replay them automatically.
                if (operation == Operation::Submit)
                    setPhase(QStringLiteral("submission_unknown"));
                else if (operation == Operation::Reserve)
                    setPhase(QStringLiteral("reservation_unknown"));
                fail(QStringLiteral("registration_request_unavailable"));
                return;
            }
            applyResponse(operation, payload);
        });
}

bool NeynarRegistrationController::reserveFid()
{
    if (!m_registrationSupported || !hasVoucher() || m_busy || pending()
        || confirmed() || !walletMatches())
        return false;
    if (m_phase != QStringLiteral("ready")) return false;
    // Store one deterministic idempotency key for this voucher and wallet.
    // A process restart with the same voucher derives the same UUID.
    const QJsonObject payload{
        {QStringLiteral("custody_address"), m_wallet},
        {QStringLiteral("idempotency_key"), m_reservationId}
    };
    setPhase(QStringLiteral("reserving"));
    request(Operation::Reserve, QJsonDocument{payload}.toJson(QJsonDocument::Compact));
    return true;
}

bool NeynarRegistrationController::submit(QString requestedUsername)
{
    requestedUsername = requestedUsername.trimmed().toLower();
    if (!canSubmit() || !NamePattern.match(requestedUsername).hasMatch())
        return false;
    std::uint64_t fid{}, nonce{};
    if (!decimalU64(m_fid, &fid) || !decimalU64(m_nonce, &nonce)
        || fid > 9'007'199'254'740'991ULL) return false;
    const auto deadline = static_cast<std::uint64_t>(
        QDateTime::currentSecsSinceEpoch() + 300);
    auto signature = m_ports.signTransfer(fid, nonce, deadline);
    if (!signature || !QRegularExpression{
        QStringLiteral("^0x[0-9a-fA-F]{130}$")}.match(*signature).hasMatch()) {
        fail(QStringLiteral("custody_signing_unavailable"));
        return false;
    }
    const QJsonObject payload{
        {QStringLiteral("custody_address"), m_wallet},
        {QStringLiteral("reservation_id"), m_reservationId},
        {QStringLiteral("deadline"), QString::number(deadline)},
        {QStringLiteral("signature"), *signature},
        {QStringLiteral("fname"), requestedUsername}
    };
    signature->fill(QChar{});
    setPhase(QStringLiteral("submitting"));
    request(Operation::Submit, QJsonDocument{payload}.toJson(QJsonDocument::Compact));
    return true;
}

bool NeynarRegistrationController::checkConfirmation()
{
    if (!m_registrationSupported || !hasVoucher() || m_busy || !walletMatches()
        || m_reservationId.isEmpty() || confirmed()) return false;
    const QJsonObject payload{
        {QStringLiteral("custody_address"), m_wallet},
        {QStringLiteral("reservation_id"), m_reservationId}
    };
    request(Operation::Status, QJsonDocument{payload}.toJson(QJsonDocument::Compact));
    return true;
}

void NeynarRegistrationController::applyResponse(
    Operation operation, QByteArray payload)
{
    QJsonParseError parseError{};
    const auto json = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()
        || !json.object().value(QStringLiteral("registration")).isObject()) {
        if (operation == Operation::Submit)
            setPhase(QStringLiteral("submission_unknown"));
        fail(QStringLiteral("invalid_registration_response"));
        return;
    }
    const auto response = json.object().value(QStringLiteral("registration")).toObject();
    const QString custody = response.value(QStringLiteral("custody_address")).toString();
    const QString id = response.value(QStringLiteral("reservation_id")).toString();
    const QString phase = response.value(QStringLiteral("status")).toString();
    const QString fid = response.value(QStringLiteral("fid")).toString();
    const QString nonce = response.value(QStringLiteral("id_registry_nonce")).toString();
    const qint64 expiry = static_cast<qint64>(
        response.value(QStringLiteral("expires_at")).toDouble());
    static const QStringList phases{
        QStringLiteral("reserving"), QStringLiteral("reserved"),
        QStringLiteral("reservation_unknown"), QStringLiteral("submitting"),
        QStringLiteral("submitted"), QStringLiteral("submission_unknown"),
        QStringLiteral("rejected"), QStringLiteral("confirmed"),
        QStringLiteral("already_registered")
    };
    if (custody != m_wallet || id != m_reservationId
        || (phase == QStringLiteral("confirmed") && fid.isEmpty())
        || !UuidPattern.match(id).hasMatch() || !phases.contains(phase)
        || (!fid.isEmpty() && !FidPattern.match(fid).hasMatch())
        || (!nonce.isEmpty() && !UintPattern.match(nonce).hasMatch())) {
        if (operation == Operation::Submit)
            setPhase(QStringLiteral("submission_unknown"));
        fail(QStringLiteral("invalid_registration_response"));
        return;
    }
    if (!fid.isEmpty()) m_fid = fid;
    m_nonce = nonce;
    m_expiresAtMs = expiry;
    setPhase(phase);
    if (phase == QStringLiteral("confirmed")) {
        // A public chain read has verified both IdRegistry mappings.
        m_voucher.fill('\0');
        m_voucher.clear();
        emit farcasterFidConfirmed(m_fid, m_wallet);
    } else if (phase == QStringLiteral("submitted")
               || phase == QStringLiteral("submission_unknown")) {
        scheduleStatus();
    }
}

void NeynarRegistrationController::scheduleStatus()
{
    if (++m_checks >= 50) return;
    QTimer::singleShot(3500, this, [this] {
        if (!m_busy && pending()) (void)checkConfirmation();
    });
}

void NeynarRegistrationController::stopPolling()
{
    ++m_generation;
    if (m_reply) m_reply->abort();
    m_reply = nullptr;
    m_busy = false;
    // Retain the voucher/lease even after dismissing the UI; an outstanding
    // Neynar submission remains independently confirmable on chain.
    emit changed();
}

bool NeynarRegistrationController::reset()
{
    if (m_busy || pending() || confirmed()
        || m_phase == QStringLiteral("reserved"))
        return false;
    stopPolling();
    m_voucher.fill('\0');
    m_voucher.clear();
    m_wallet.clear();
    m_reservationId.clear();
    m_fid.clear();
    m_nonce.clear();
    m_expiresAtMs = 0;
    m_error.clear();
    m_checks = 0;
    setPhase(QStringLiteral("idle"));
    return true;
}
