#include "native_registration.hpp"
#include "hypersnap_registration_transport.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QList>
#include <QVariantMap>
#include <utility>

#include <cmath>
#include <limits>

NativeRegistrationStatusController::NativeRegistrationStatusController(
    QString baseUrl, QObject* parent)
    : QObject(parent), m_base{baseUrl.trimmed()},
      m_transport{std::make_unique<HyperSnapRegistrationTransport>(m_base, this)}
{}

NativeRegistrationStatusController::~NativeRegistrationStatusController() = default;

bool NativeRegistrationStatusController::setEndpointBeforeRuntime(const QUrl& endpoint)
{
    if (endpoint == m_base) return true;
    auto candidate = std::make_unique<HyperSnapRegistrationTransport>(endpoint, this);
    if (!candidate->endpointIsSafe()) return false;
    // Invalidate lookup and challenge provenance before replacing transport.
    clearExistingLookup();
    m_base = endpoint;
    m_transport = std::move(candidate);
    refresh();
    return true;
}

void NativeRegistrationStatusController::selectBeforeRuntime(
    const QStringList& candidates, QObject* context,
    std::function<void(QString)> completion, const int timeoutMs,
    std::function<void(QVariantList)> observation)
{
    if (candidates.isEmpty()) { completion({}); return; }
    auto* scope = new QObject(context);
    auto* deadline = new QTimer(scope);
    deadline->setSingleShot(true);
    struct State final {
        QStringList origins;
        QList<NativeRegistrationStatusController*> probes;
        std::function<void(QString)> completion;
        std::function<void(QVariantList)> observation;
        bool done{false};
    };
    auto state = std::make_shared<State>();
    state->origins = candidates;
    state->completion = std::move(completion);
    state->observation = std::move(observation);
    for (const auto& origin : candidates)
        state->probes.push_back(new NativeRegistrationStatusController(origin, scope));
    const auto finish = [scope, deadline, state](const bool timedOut) {
        if (state->done) return;
        // Do not abandon the preferred origin just because a peer's DNS was
        // faster. At deadline choose a validated peer or retain the primary.
        const auto eligible = [](const NativeRegistrationStatusController* probe) {
            return probe->syncedForRegistration() && probe->accountLookupChecked()
                && !probe->accountLookupBusy() && !probe->accountLookupFailed();
        };
        if (!timedOut && !eligible(state->probes.front())) return;
        state->done = true;
        deadline->stop();
        QString selected = state->origins.front();
        for (qsizetype i = 0; i < state->probes.size(); ++i) {
            if (eligible(state->probes.at(i))) {
                selected = state->origins.at(i);
                break;
            }
        }
        if (state->observation) {
            QVariantList rows;
            for (qsizetype i = 0; i < state->probes.size(); ++i) {
                const auto* probe = state->probes.at(i);
                const QString status = eligible(probe) ? QStringLiteral("ready")
                    : probe->shardZeroBlockDelay() > 120 ? QStringLiteral("lagging")
                    : probe->checking() ? QStringLiteral("unknown")
                    : QStringLiteral("not_ready");
                rows.push_back(QVariantMap{
                    {QStringLiteral("endpoint"),state->origins.at(i)},
                    {QStringLiteral("registrationPreflight"),status},
                    {QStringLiteral("shardZeroBlockDelay"),probe->shardZeroBlockDelay()}});
            }
            state->observation(std::move(rows));
        }
        auto done = std::move(state->completion);
        scope->deleteLater();
        if (done) done(std::move(selected));
    };
    for (auto* probe : state->probes) {
        QObject::connect(probe, &NativeRegistrationStatusController::changed,
                         scope, [probe, finish, state] {
            if (!state->done && probe->syncedForRegistration()
                && !probe->accountLookupChecked() && !probe->accountLookupBusy())
                probe->findExistingNativeAccount(QStringLiteral("0x0000000000000000000000000000000000000001"));
            finish(false);
        });
        QObject::connect(probe, &NativeRegistrationStatusController::lookupChanged,
                         scope, [finish] { finish(false); });
    }
    QObject::connect(deadline, &QTimer::timeout, scope, [finish] { finish(true); });
    deadline->start(timeoutMs);
    for (auto* probe : state->probes) probe->refresh();
}


// The upstream /v1/info contract reports blockDelay in seconds. A positive
// nativeRegistration.active flag only tests protocol activation, NOT sync.
bool NativeRegistrationStatusController::syncedForRegistration() const noexcept
{
    const auto ageMs = QDateTime::currentMSecsSinceEpoch() - m_healthCheckedAtMs;
    constexpr int MaxAcceptableShardZeroLagSeconds = 120;
    return m_active && m_shardZeroDelay >= 0
        && m_shardZeroDelay <= MaxAcceptableShardZeroLagSeconds
        && m_healthCheckedAtMs > 0 && ageMs >= 0 && ageMs <= 60'000;
}

bool NativeRegistrationStatusController::verifiedAbsentForCustody(
    const QString& address) const
{
    return m_lookupChecked && !m_lookupBusy && !m_lookupFailed
        && m_existingFid.isEmpty() && !m_lookupAddress.isEmpty()
        && m_lookupAddress == address.toLower();
}

QString NativeRegistrationStatusController::verifiedExistingFidForCustody(
    const QString& address) const
{
    return m_lookupChecked && !m_lookupBusy && !m_lookupFailed
        && !m_existingFid.isEmpty() && !m_lookupAddress.isEmpty()
        && m_lookupAddress == address.toLower()
        ? m_existingFid : QString{};
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
    // Separate read-only health probe. Detach before abort to avoid a
    // synchronous QNetworkReply::finished callback corrupting newer state.
    auto previousHealthReply = m_healthReply;
    m_healthReply = nullptr;
    if (previousHealthReply) {
        previousHealthReply->abort();
        if (previousHealthReply)
            previousHealthReply->deleteLater();
    }
    m_shardZeroDelay = -1;
    m_healthCheckedAtMs = 0;
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
    const auto reply = m_reply;
    // Install the completion handler before announcing the new state: QML
    // observers may synchronously request another refresh in changed().
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
        if (m_active) {
            // HTTP 200 from /v1/nativeRegistration can coexist with 30+
            // minutes of shard-0 lag. Never mine/sign/send a new irreversible
            // NATIVE_ONBOARD against an unqualified chain head. Reads of an
            // already pending custody registration remain permitted.
            QNetworkRequest health{m_base.resolved(QUrl{QStringLiteral("/v1/info")})};
            health.setTransferTimeout(10000);
            health.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                QNetworkRequest::ManualRedirectPolicy);
            health.setRawHeader("Accept", "application/json");
            m_healthReply = m_network.get(health);
            const auto healthReply = m_healthReply;
            QObject::connect(healthReply, &QNetworkReply::finished, this,
                             [this, healthReply] {
                if (healthReply != m_healthReply) return;
                m_healthReply = nullptr;
                const int http = healthReply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const auto error = healthReply->error();
                const auto bytes = healthReply->bytesAvailable() <= 16'384
                    ? healthReply->readAll() : QByteArray{};
                healthReply->deleteLater();
                const auto info = QJsonDocument::fromJson(bytes);
                if (error == QNetworkReply::NoError && http == 200
                    && info.isObject()) {
                    const auto shards = info.object().value(
                        QStringLiteral("shardInfos")).toArray();
                    for (const auto& shard : shards) {
                        if (!shard.isObject()) continue;
                        const auto fields = shard.toObject();
                        if (fields.value(QStringLiteral("shardId")).toInt(-1) != 0)
                            continue;
                        const auto seconds = fields.value(
                            QStringLiteral("blockDelay")).toDouble(-1);
                        if (std::isfinite(seconds) && seconds >= 0
                            && seconds <= std::numeric_limits<int>::max()
                            && std::floor(seconds) == seconds) {
                            m_shardZeroDelay = static_cast<int>(seconds);
                            m_healthCheckedAtMs = QDateTime::currentMSecsSinceEpoch();
                        }
                        break;
                    }
                }
                // Unknown/invalid block delay fails CLOSED for new
                // registration, but does not suppress custody/FID lookups.
                emit changed();
            });
        }
        emit changed();
    });
    emit changed();
}
