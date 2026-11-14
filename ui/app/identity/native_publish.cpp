#include "native_publish.hpp"

#include <QByteArray>

#include <utility>

HyperSnapRegistrationPublisher::HyperSnapRegistrationPublisher(
    QUrl nativeNode, NativePublicationPorts ports, QObject* parent)
    : QObject{parent}, m_transport{std::move(nativeNode), this},
      m_ports{std::move(ports)}
{
    m_poll.setSingleShot(true);
    QObject::connect(&m_poll, &QTimer::timeout, this,
                     [this] { (void)checkConfirmation(); });
}

void HyperSnapRegistrationPublisher::setIdentityRuntimeReady(const bool ready)
{
    if (m_registrationSupported == ready) return;
    m_registrationSupported = ready;
    emit changed();
}

bool HyperSnapRegistrationPublisher::submitPrepared()
{
    if (!m_registrationSupported || m_busy || m_pending || m_submitted || confirmed()
        || !m_ports.backupVerified || !m_ports.backupVerified()
        || !m_ports.proofPrepared || !m_ports.proofPrepared()
        || !m_ports.anchorIsFresh || !m_ports.anchorIsFresh()
        || !m_transport.endpointIsSafe())
        return false;
    if (!m_ports.custodyAddress || !m_ports.walletIsUnregistered
        || !m_ports.takeProof) return false;
    m_address = m_ports.custodyAddress().toLower();
    if (m_address.isEmpty() || !m_ports.walletIsUnregistered(m_address))
        return false;
    m_busy = true;
    m_failed = false;
    m_unknown = false;
    m_checkingConfirmation = false;
    m_lastSubmissionHttpStatus = 0;
    m_checks = 0;
    const auto generation = ++m_generation;
    emit changed();

    // The authoritative registry is checked a second time IMMEDIATELY before
    // any irreversible write; a previously unregistered wallet could have
    // registered from another device while the PoW was being prepared.
    lookup(true, generation);
    return true;
}

bool HyperSnapRegistrationPublisher::recoverInterruptedRegistration()
{
    if (!m_registrationSupported || m_busy || m_pending || confirmed()
        || !m_ports.backupVerified || !m_ports.backupVerified()
        || !m_ports.custodyAddress || !m_transport.endpointIsSafe())
        return false;
    const QString custody = m_ports.custodyAddress().toLower();
    if (custody.size() != 42 || !custody.startsWith(QLatin1String("0x")))
        return false;
    // A chain write may already be in flight after restart: never replay it.
    // Only a custody-bound read of shard-0 can confirm ownership.
    m_address = custody;
    m_fid.clear();
    m_submitted = true;
    m_pending = true;
    m_unknown = true;
    m_failed = false;
    m_checkingConfirmation = true;
    m_busy = true;
    m_checks = 0;
    const auto generation = ++m_generation;
    if (m_ports.cancelProof) m_ports.cancelProof();
    emit changed();
    lookup(false, generation);
    return true;
}

void HyperSnapRegistrationPublisher::lookup(bool beforeSubmit, quint64 generation)
{
    m_transport.lookupByCustody(m_address, [this, generation, beforeSubmit](
        const HyperSnapRegistrationTransport::LookupResult result) {
        if (generation != m_generation) return;
        // Read-only confirmation checks must not animate the Submit button.
        if (!beforeSubmit) m_checkingConfirmation = false;
        using State = HyperSnapRegistrationTransport::LookupState;
        if (result.state == State::Confirmed) {
            onConfirmed(result.fid, generation);
            return;
        }
        if (beforeSubmit) {
            if (result.state != State::NotFound
                || !m_ports.backupVerified() || !m_ports.backupVerified()
                || m_ports.custodyAddress().toLower() != m_address
                || !m_ports.walletIsUnregistered(m_address)
                || !m_ports.anchorIsFresh()) {
                m_busy = false;
                m_failed = true;
                emit changed();
                return;
            }
            auto envelope = m_ports.takeProof(m_address);
            if (envelope.isEmpty()) {
                m_busy = false;
                m_failed = true;
                emit changed();
                return;
            }
            // Mark this attempt consumed BEFORE starting network I/O. Whether
            // a timeout occurred before or after submission is unknowable.
            m_submitted = true;
            m_pending = true;
            emit changed();
            m_transport.submit(envelope, [this, generation](
                HyperSnapRegistrationTransport::SubmitResult result) {
                if (generation != m_generation) return;
                m_busy = false;
                m_lastSubmissionHttpStatus = result.httpStatus;
                if (result.state == HyperSnapRegistrationTransport::SubmitState::Rejected) {
                    m_pending = false;
                    m_submitted = false;
                    m_failed = true;
                    emit changed();
                    return;
                }
                m_unknown = result.state
                    == HyperSnapRegistrationTransport::SubmitState::UnknownOutcome;
                m_pending = true;
                emit changed();
                scheduleCheck(generation);
            });
            // Public signature material is still erased after the POST copies
            // the request into QNetworkAccessManager.
            envelope.fill('\0');
            return;
        }
        m_busy = false;
        if (result.state == State::NotFound) {
            // A genuine pending write must not be declared failed after one
            // registry lookup; shard-0 inclusion is asynchronous.
            ++m_checks;
            if (m_checks < 60 && m_pending) scheduleCheck(generation);
            else m_unknown = true; // Manual "check again" is always possible.
        } else {
            m_unknown = true;
            ++m_checks;
            if (m_checks < 60 && m_pending) scheduleCheck(generation);
        }
        emit changed();
    });
}

void HyperSnapRegistrationPublisher::scheduleCheck(quint64 generation)
{
    if (generation != m_generation || !m_pending || confirmed()) return;
    m_poll.start(2000);
}

bool HyperSnapRegistrationPublisher::checkConfirmation()
{
    if (m_busy || m_address.isEmpty() || confirmed()) return false;
    m_poll.stop();
    m_checkingConfirmation = true;
    m_busy = true;
    emit changed();
    lookup(false, m_generation);
    return true;
}

void HyperSnapRegistrationPublisher::onConfirmed(quint64 fid, quint64 generation)
{
    if (generation != m_generation || !m_ports.custodyAddress
        || m_ports.custodyAddress().toLower() != m_address
        || !m_ports.backupVerified || !m_ports.backupVerified()) {
        m_busy = false;
        m_unknown = true;
        emit changed();
        return;
    }
    m_poll.stop();
    m_checkingConfirmation = false;
    m_fid = QString::number(static_cast<qulonglong>(fid));
    m_pending = false;
    m_busy = false;
    m_unknown = false;
    m_failed = false;
    if (m_ports.cancelProof) m_ports.cancelProof();
    emit changed();
    emit nativeAccountConfirmed(m_fid, m_address);
}

void HyperSnapRegistrationPublisher::stopPolling()
{
    ++m_generation;
    m_poll.stop();
    m_checkingConfirmation = false;
    m_busy = false;
    // Never clear m_pending / m_submitted: the write may already be on-chain.
    emit changed();
}

void HyperSnapRegistrationPublisher::clear()
{
    // A submitted write may still commit after the UI is dismissed. Preserve
    // its custody address and single-use guard so returning to registration
    // cannot blindly submit a second identity transaction.
    if (m_submitted && !confirmed()) {
        stopPolling();
        m_unknown = true;
        emit changed();
        return;
    }
    stopPolling();
    m_pending = false;
    m_failed = false;
    m_unknown = false;
    m_fid.clear();
    m_address.clear();
    m_checks = 0;
    m_lastSubmissionHttpStatus = 0;
    m_submitted = false;
    emit changed();
}
