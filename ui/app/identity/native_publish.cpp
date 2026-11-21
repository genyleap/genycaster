#include "native_publish.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QSettings>
#include <QStringList>

#include <algorithm>

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

QString HyperSnapRegistrationPublisher::retryJournalKey() const
{
    // Public-only journal, scoped to the exact native node and custody
    // address. Hash the scope to avoid exposing the address in settings keys.
    const QByteArray scope = m_transport.nodeEndpoint()
        .toString(QUrl::FullyEncoded).toUtf8()
        + QByteArray(1, char{0}) + m_address.toUtf8();
    return QStringLiteral("nativeRegistration/expiry-v1/")
        + QString::fromLatin1(QCryptographicHash::hash(
            scope, QCryptographicHash::Sha256).toHex());
}

void HyperSnapRegistrationPublisher::loadRetryJournal()
{
    if (m_address.isEmpty()) return;
    const QString entry = QSettings{}.value(retryJournalKey()).toString();
    const QStringList parts = entry.split(QLatin1Char(':'));
    if (parts.size() != 2) return;
    bool headValid = false;
    bool windowValid = false;
    const auto head = parts.at(0).toULongLong(&headValid);
    const auto window = parts.at(1).toUInt(&windowValid);
    if (headValid && head > 0 && windowValid && window > 0
        && window <= 65'536) {
        m_retryBaselineNextBlock = static_cast<std::uint64_t>(head);
        m_retryAnchorWindow = static_cast<std::uint32_t>(window);
    }
}

void HyperSnapRegistrationPublisher::saveRetryJournal() const
{
    if (m_address.isEmpty() || m_retryBaselineNextBlock == 0
        || m_retryAnchorWindow == 0 || m_retryAnchorWindow > 65'536) return;
    QSettings settings{};
    settings.setValue(retryJournalKey(),
        QString::number(static_cast<qulonglong>(m_retryBaselineNextBlock))
        + QLatin1Char(':') + QString::number(m_retryAnchorWindow));
    settings.sync();
}

void HyperSnapRegistrationPublisher::eraseRetryJournal() const
{
    if (m_address.isEmpty()) return;
    QSettings settings{};
    settings.remove(retryJournalKey());
    settings.sync();
}

void HyperSnapRegistrationPublisher::setIdentityRuntimeReady(const bool ready)
{
    if (m_registrationSupported == ready) return;
    m_registrationSupported = ready;
    emit changed();
}

void HyperSnapRegistrationPublisher::verifyCustodyForSession(
    const QString& custody,
    std::function<void(std::optional<std::uint64_t>)> onResult)
{
    m_transport.lookupByCustody(custody,
        [callback = std::move(onResult)](
            HyperSnapRegistrationTransport::LookupResult result) mutable {
            callback(result.state == HyperSnapRegistrationTransport::LookupState::Confirmed
                && result.fid >= (1ULL << 32) && result.blockNumber > 0
                    ? std::optional<std::uint64_t>{result.fid} : std::nullopt);
        });
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
    m_verifyingExisting = false;
    m_existingVerificationFailed = false;
    m_lastSubmissionHttpStatus = 0;
    m_confirmationLookupHttpStatus = 0;
    m_confirmationLookupState = QStringLiteral("not_checked");
    m_retryBaselineNextBlock = 0;
    m_retryAnchorWindow = 0;
    m_retryRemainingBlocks = -1;
    m_retryReady = false;
    m_checks = 0;
    const auto generation = ++m_generation;
    emit changed();

    // The authoritative registry is checked a second time IMMEDIATELY before
    // any irreversible write; a previously unregistered wallet could have
    // registered from another device while the PoW was being prepared.
    lookup(true, generation);
    return true;
}

bool HyperSnapRegistrationPublisher::confirmExistingAccount()
{
    if (!m_registrationSupported || m_busy || m_pending || m_submitted
        || confirmed() || !m_ports.backupVerified || !m_ports.backupVerified()
        || !m_ports.custodyAddress || !m_ports.existingFidForCustody
        || !m_transport.endpointIsSafe())
        return false;
    const QString custody = m_ports.custodyAddress().toLower();
    if (custody.size() != 42 || !custody.startsWith(QLatin1String("0x")))
        return false;
    const QString expectedFid = m_ports.existingFidForCustody(custody);
    bool validFid = false;
    const auto expected = expectedFid.toULongLong(&validFid);
    if (!validFid || expected < (std::uint64_t{1} << 32))
        return false;

    // The first lookup is only a UI hint. Before provisioning a signer,
    // independently confirm the same FID and custody on the same native node.
    // No POST, proof, signature or retry journal is involved in this path.
    m_address = custody;
    m_busy = true;
    m_checkingConfirmation = true;
    m_verifyingExisting = true;
    m_existingVerificationFailed = false;
    m_failed = false;
    m_unknown = false;
    m_confirmationLookupHttpStatus = 0;
    m_confirmationLookupState = QStringLiteral("not_checked");
    const auto generation = ++m_generation;
    emit changed();

    m_transport.lookupByCustody(custody, [this, generation, expected](
        const HyperSnapRegistrationTransport::LookupResult result) {
        if (generation != m_generation) return;
        using State = HyperSnapRegistrationTransport::LookupState;
        m_confirmationLookupHttpStatus = result.httpStatus;
        if (result.state == State::Confirmed && result.fid == expected) {
            m_confirmationLookupState = QStringLiteral("confirmed");
            m_verifyingExisting = false;
            onConfirmed(result.fid, generation);
            return;
        }
        // A changed FID, a missing account or an unavailable node must never
        // produce a session or authorize a device signer.
        m_confirmationLookupState = result.state == State::Confirmed
            ? QStringLiteral("fid_changed")
            : result.state == State::NotFound
                ? QStringLiteral("not_found")
                : result.state == State::InvalidResponse
                    ? QStringLiteral("invalid_response")
                    : QStringLiteral("unavailable");
        m_verifyingExisting = false;
        m_existingVerificationFailed = true;
        m_checkingConfirmation = false;
        m_busy = false;
        emit changed();
    });
    return true;
}

bool HyperSnapRegistrationPublisher::resumeConfirmedAccount()
{
    if (!m_registrationSupported || m_busy || m_pending || !confirmed()
        || !m_ports.backupVerified || !m_ports.backupVerified()
        || !m_ports.custodyAddress || !m_transport.endpointIsSafe()
        || m_address.isEmpty() ||
        m_ports.custodyAddress().toLower() != m_address)
        return false;

    bool validFid = false;
    const auto expected = m_fid.toULongLong(&validFid);
    if (!validFid || expected < (std::uint64_t{1} << 32))
        return false;

    // Revalidate on the HyperSnap authoritative shard-0 node before
    // re-emitting the identity event. Never POST/redo PoW/overwrite custody.
    m_busy = true;
    m_checkingConfirmation = true;
    m_verifyingExisting = true;
    m_existingVerificationFailed = false;
    m_confirmationLookupHttpStatus = 0;
    m_confirmationLookupState = QStringLiteral("not_checked");
    const auto generation = ++m_generation;
    emit changed();

    m_transport.lookupByCustody(m_address, [this, expected, generation](
        const HyperSnapRegistrationTransport::LookupResult result) {
        if (generation != m_generation) return;
        using State = HyperSnapRegistrationTransport::LookupState;
        m_confirmationLookupHttpStatus = result.httpStatus;
        if (result.state == State::Confirmed && result.fid == expected) {
            m_confirmationLookupState = QStringLiteral("confirmed");
            m_verifyingExisting = false;
            onConfirmed(result.fid, generation);
            return;
        }
        m_confirmationLookupState = result.state == State::Confirmed
            ? QStringLiteral("fid_changed")
            : result.state == State::NotFound
                ? QStringLiteral("not_found")
                : result.state == State::InvalidResponse
                    ? QStringLiteral("invalid_response")
                    : QStringLiteral("unavailable");
        m_busy = false;
        m_checkingConfirmation = false;
        m_verifyingExisting = false;
        m_existingVerificationFailed = true;
        emit changed();
    });
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
    m_verifyingExisting = false;
    m_existingVerificationFailed = false;
    m_submitted = true;
    m_pending = true;
    m_unknown = true;
    m_failed = false;
    m_checkingConfirmation = true;
    m_busy = true;
    m_confirmationLookupHttpStatus = 0;
    m_confirmationLookupState = QStringLiteral("not_checked");
    m_retryBaselineNextBlock = 0;
    m_retryAnchorWindow = 0;
    m_retryRemainingBlocks = -1;
    m_retryReady = false;
    m_checks = 0;
    loadRetryJournal(); // Survives Back and process restart.
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
        using State = HyperSnapRegistrationTransport::LookupState;
        // Read-only confirmation is independently observable. A NotFound
        // lookup does NOT prove a previously sent write was never accepted.
        if (!beforeSubmit) {
            m_checkingConfirmation = false;
            m_confirmationLookupHttpStatus = result.httpStatus;
            switch (result.state) {
            case State::Confirmed:
                m_confirmationLookupState = QStringLiteral("confirmed");
                break;
            case State::NotFound:
                m_confirmationLookupState = QStringLiteral("not_found");
                break;
            case State::Unavailable:
                m_confirmationLookupState = QStringLiteral("unavailable");
                break;
            case State::InvalidResponse:
                m_confirmationLookupState = QStringLiteral("invalid_response");
                break;
            }
        }
        if (result.state == State::Confirmed) {
            onConfirmed(result.fid, generation);
            return;
        }
        if (!beforeSubmit) updateRetryEligibility();
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
            if (m_ports.registrationWindow) {
                if (const auto info = m_ports.registrationWindow()) {
                    if (info->first > 0 && info->second > 0 && info->second <= 65'536) {
                        m_retryBaselineNextBlock = info->first;
                        m_retryAnchorWindow = info->second;
                        saveRetryJournal();
                    }
                }
            }
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
                    eraseRetryJournal();
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
            // Inclusion may exceed the first two minutes. Resume bounded,
            // read-only polling at a lower rate; never repeat the POST.
            if (m_checks >= 60) m_unknown = true;
            if (m_pending) scheduleCheck(generation);
        } else {
            m_unknown = true;
            ++m_checks;
            if (m_checks >= 60) m_unknown = true;
            if (m_pending) scheduleCheck(generation);
        }
        emit changed();
    });
}

void HyperSnapRegistrationPublisher::updateRetryEligibility()
{
    m_retryReady = false;
    m_retryRemainingBlocks = -1;
    if (!m_pending || !m_submitted
        || m_confirmationLookupState != QStringLiteral("not_found")
        || m_confirmationLookupHttpStatus != 400 || !m_ports.registrationWindow
        || !m_ports.backupVerified || !m_ports.backupVerified()
        || !m_ports.custodyAddress
        || m_ports.custodyAddress().toLower() != m_address) return;

    const auto info = m_ports.registrationWindow();
    if (!info || info->first == 0 || info->second == 0
        || info->second > 65'536) return;
    const auto [nextBlock, window] = *info;
    if (m_retryBaselineNextBlock == 0) {
        // Earlier builds did not persist the original anchor. Starting from
        // the first observed chain head is deliberately conservative.
        m_retryBaselineNextBlock = nextBlock;
        m_retryAnchorWindow = window;
        saveRetryJournal();
    }
    if (window > m_retryAnchorWindow) {
        m_retryAnchorWindow = window;
        // The node's safety window may increase after restart. Persist the
        // larger value; never shorten it on a later process launch.
        saveRetryJournal();
    }
    if (nextBlock < m_retryBaselineNextBlock) return;
    constexpr std::uint64_t FinalityMarginBlocks = 16;
    const auto needed = std::uint64_t{m_retryAnchorWindow} + FinalityMarginBlocks;
    const auto passed = nextBlock - m_retryBaselineNextBlock;
    m_retryRemainingBlocks = passed >= needed
        ? 0 : static_cast<int>(needed - passed);
    m_retryReady = passed >= needed;
}

bool HyperSnapRegistrationPublisher::releaseExpiredAttempt()
{
    if (!m_retryReady || m_busy || !m_pending || !m_submitted
        || confirmed()
        || m_confirmationLookupState != QStringLiteral("not_found")
        || m_confirmationLookupHttpStatus != 400) return false;
    // This is a local state reset, NEVER an automatic second submission.
    if (!m_ports.registrationWindow || !m_ports.backupVerified
        || !m_ports.backupVerified() || !m_ports.custodyAddress
        || m_ports.custodyAddress().toLower() != m_address) return false;
    const auto window = m_ports.registrationWindow();
    if (!window || m_retryBaselineNextBlock == 0
        || window->second == 0 || window->second > 65'536
        || window->first < m_retryBaselineNextBlock
        || window->first - m_retryBaselineNextBlock
             < std::uint64_t{std::max(window->second, m_retryAnchorWindow)} + 16)
        return false;

    stopPolling();
    eraseRetryJournal();
    m_pending = false;
    m_submitted = false;
    m_unknown = false;
    m_failed = false;
    m_fid.clear();
    m_address.clear();
    m_retryReady = false;
    m_retryRemainingBlocks = -1;
    m_retryBaselineNextBlock = 0;
    m_retryAnchorWindow = 0;
    m_checks = 0;
    m_lastSubmissionHttpStatus = 0;
    m_confirmationLookupHttpStatus = 0;
    m_confirmationLookupState = QStringLiteral("not_checked");
    if (m_ports.cancelProof) m_ports.cancelProof();
    emit changed();
    return true;
}

bool HyperSnapRegistrationPublisher::detachWalletForNewRegistration()
{
    // This is an explicit local wallet switch, not a cancellation of any
    // network transaction. The former wallet remains recoverable with its
    // own 24 words. The UI must confirm this before discarding that wallet.
    if (m_busy || confirmed()
        || !m_ports.backupVerified || !m_ports.backupVerified())
        return false;
    if (m_submitted && !m_address.isEmpty())
        saveRetryJournal(); // Keep old wallet's chain-height guard on disk.

    stopPolling(); // Invalidate callbacks belonging to the old wallet.
    if (m_ports.cancelProof) m_ports.cancelProof();
    m_pending = false;
    m_submitted = false;
    m_unknown = false;
    m_failed = false;
    m_address.clear();
    m_fid.clear();
    m_retryReady = false;
    m_retryRemainingBlocks = -1;
    m_retryBaselineNextBlock = 0;
    m_retryAnchorWindow = 0;
    m_lastSubmissionHttpStatus = 0;
    m_confirmationLookupHttpStatus = 0;
    m_confirmationLookupState = QStringLiteral("not_checked");
    m_checks = 0;
    emit changed();
    return true;
}

void HyperSnapRegistrationPublisher::scheduleCheck(quint64 generation)
{
    if (generation != m_generation || !m_pending || confirmed()) return;
    // This timer performs ONLY read-only custody/FID lookups. A shard-0 node
    // can lag tens of minutes or longer; arbitrarily stopping after 240
    // requests made the UI promise automatic confirmation but cease polling.
    // Slow down after the first attempts, and keep checking until the view is
    // closed or the chain independently confirms the registration.
    m_poll.start(m_checks < 30 ? 2000 : m_checks < 90 ? 15000 : 60000);
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
    eraseRetryJournal();
    m_checkingConfirmation = false;
    m_fid = QString::number(static_cast<qulonglong>(fid));
    m_pending = false;
    m_busy = false;
    m_retryReady = false;
    m_retryRemainingBlocks = -1;
    m_unknown = false;
    m_failed = false;
    m_verifyingExisting = false;
    m_existingVerificationFailed = false;
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
    m_verifyingExisting = false;
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
    m_verifyingExisting = false;
    m_existingVerificationFailed = false;
    m_fid.clear();
    m_address.clear();
    m_checks = 0;
    m_retryReady = false;
    m_retryRemainingBlocks = -1;
    m_retryBaselineNextBlock = 0;
    m_retryAnchorWindow = 0;
    m_lastSubmissionHttpStatus = 0;
    m_confirmationLookupHttpStatus = 0;
    m_confirmationLookupState = QStringLiteral("not_checked");
    m_submitted = false;
    emit changed();
}
