/*!
 * @file        ui/app/identity/native_publish.hpp
 * @brief       Single-use HyperSnap native registration publication controller.
 *
 * @details
 * Submits consented custody proofs and independently verifies registry state.
 * Never interprets an HTTP acknowledgement as confirmed FID ownership.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include "hypersnap_registration_transport.hpp"
#include <QByteArray>
#include <functional>
#include <optional>
#include <utility>
#include <cstdint>

#include <QObject>
#include <QTimer>
#include <QString>
#include <QUrl>

/**
 * HyperSnap-only transaction coordinator. A write is explicit and single-use.
 * A 200 response from submitMessage is not a registered account. Confirmation
 * uses a fresh custody-bound shard-0 nativeAccount lookup.
 *
 * This object never sees a mnemonic or the user's custody private key.
 */
struct NativePublicationPorts final {
    std::function<bool()> backupVerified;
    std::function<QString()> custodyAddress;
    std::function<bool()> anchorIsFresh;
    std::function<bool(const QString&)> walletIsUnregistered;
    std::function<bool()> proofPrepared;
    std::function<QByteArray(const QString&)> takeProof;
    std::function<void()> cancelProof;
    // Last fresh shard-0 head and maximum onboard anchor validity window.
    // Must come from the SAME node used by the publisher's custody lookup.
    std::function<std::optional<std::pair<std::uint64_t, std::uint32_t>>()> registrationWindow;

    /** Prior read-only lookup must identify this exact custody wallet. */
    std::function<QString(const QString&)> existingFidForCustody;
};

class HyperSnapRegistrationPublisher final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool registrationSupported READ registrationSupported NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool checkingConfirmation READ checkingConfirmation NOTIFY changed)
    Q_PROPERTY(bool verifyingExisting READ verifyingExisting NOTIFY changed)
    Q_PROPERTY(bool existingVerificationFailed READ existingVerificationFailed NOTIFY changed)
    Q_PROPERTY(int lastSubmissionHttpStatus READ lastSubmissionHttpStatus NOTIFY changed)
    Q_PROPERTY(QString confirmationLookupState READ confirmationLookupState NOTIFY changed)
    Q_PROPERTY(int confirmationLookupHttpStatus READ confirmationLookupHttpStatus NOTIFY changed)
    Q_PROPERTY(int confirmationLookupChecks READ confirmationLookupChecks NOTIFY changed)
    Q_PROPERTY(bool retryReady READ retryReady NOTIFY changed)
    Q_PROPERTY(int retryRemainingBlocks READ retryRemainingBlocks NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(bool confirmed READ confirmed NOTIFY changed)
    Q_PROPERTY(bool failed READ failed NOTIFY changed)
    Q_PROPERTY(bool outcomeUnknown READ outcomeUnknown NOTIFY changed)
    Q_PROPERTY(QString registeredFid READ registeredFid NOTIFY changed)
public:
    HyperSnapRegistrationPublisher(
        QUrl nativeNode, NativePublicationPorts ports, QObject* parent = nullptr);
    ~HyperSnapRegistrationPublisher() override = default;

    [[nodiscard]] bool registrationSupported() const noexcept { return m_registrationSupported; }
    void setIdentityRuntimeReady(bool ready);
    [[nodiscard]] bool busy() const noexcept { return m_busy; }
    [[nodiscard]] bool checkingConfirmation() const noexcept { return m_checkingConfirmation; }
    [[nodiscard]] bool verifyingExisting() const noexcept { return m_verifyingExisting; }
    [[nodiscard]] bool existingVerificationFailed() const noexcept { return m_existingVerificationFailed; }
    [[nodiscard]] int lastSubmissionHttpStatus() const noexcept { return m_lastSubmissionHttpStatus; }
    [[nodiscard]] QString confirmationLookupState() const { return m_confirmationLookupState; }
    [[nodiscard]] int confirmationLookupHttpStatus() const noexcept { return m_confirmationLookupHttpStatus; }
    [[nodiscard]] int confirmationLookupChecks() const noexcept { return static_cast<int>(m_checks); }
    [[nodiscard]] bool retryReady() const noexcept { return m_retryReady; }
    [[nodiscard]] int retryRemainingBlocks() const noexcept { return m_retryRemainingBlocks; }
    [[nodiscard]] bool pending() const noexcept { return m_pending; }
    [[nodiscard]] bool confirmed() const noexcept { return !m_fid.isEmpty(); }
    [[nodiscard]] bool failed() const noexcept { return m_failed; }
    [[nodiscard]] bool outcomeUnknown() const noexcept { return m_unknown; }
    [[nodiscard]] QString registeredFid() const { return m_fid; }
    [[nodiscard]] QUrl nodeEndpoint() const { return m_transport.nodeEndpoint(); }
    bool setEndpointBeforeRuntime(const QUrl& endpoint);

    /** Read-only shard-0 custody ownership confirmation used for renewing a
     * device's short-lived native session; never publishes or signs. */
    void verifyCustodyForSession(
        const QString& custody,
        std::function<void(std::optional<std::uint64_t>)> onResult);

    /** User-initiated only. Does not automatically repeat an uncertain write. */
    Q_INVOKABLE bool submitPrepared();

    /** Read-only native shard-0 recovery for an interrupted registration. */
    Q_INVOKABLE bool recoverInterruptedRegistration();

    /** Verify an already registered FID by a fresh custody-bound shard-0
     * lookup. Never submits a registration or reuses a pending-write journal. */
    Q_INVOKABLE bool confirmExistingAccount();

    /** Explicitly retry sign-in after an FID was already confirmed. Fresh
     * registry lookup only: no registration submission or proof generation. */
    Q_INVOKABLE bool resumeConfirmedAccount();

    /** Queries the registry; safe even if submit returned timeout/500. */
    Q_INVOKABLE bool checkConfirmation();

    /** Explicitly release a timed-out attempt only after its anchor can no
     * longer enter shard-0 and a fresh custody lookup still shows NotFound.
     * Does NOT submit a new message or sign anything. */
    Q_INVOKABLE bool releaseExpiredAttempt();

    /** Explicit wallet switch. The old in-flight write is NOT cancelled and
     * its public recovery journal remains intact for later phrase recovery. */
    Q_INVOKABLE bool detachWalletForNewRegistration();

    /** Cancel local polling, never claim a network transaction was cancelled. */
    Q_INVOKABLE void stopPolling();

    /** Erase the transient UI status; does not revoke a blockchain identity. */
    Q_INVOKABLE void clear();

signals:
    void changed();
    void nativeAccountConfirmed(QString fid, QString custody);

private:
    void lookup(bool beforeSubmit, quint64 generation);
    void scheduleCheck(quint64 generation);
    void onConfirmed(quint64 fid, quint64 generation);
    void updateRetryEligibility();
    // Only public node/wallet fingerprint and shard-0 height/window are kept
    // in QSettings; never a mnemonic, private key, signature or proof.
    [[nodiscard]] QString retryJournalKey() const;
    void loadRetryJournal();
    void saveRetryJournal() const;
    void eraseRetryJournal() const;

    HyperSnapRegistrationTransport m_transport;
    NativePublicationPorts m_ports;
    QTimer m_poll{};
    QString m_address{};
    QString m_fid{};
    quint64 m_generation{};
    unsigned m_checks{};
    std::uint64_t m_retryBaselineNextBlock{};
    std::uint32_t m_retryAnchorWindow{};
    int m_retryRemainingBlocks{-1};
    bool m_retryReady{false};
    int m_lastSubmissionHttpStatus{};
    int m_confirmationLookupHttpStatus{};
    QString m_confirmationLookupState{QStringLiteral("not_checked")};
    bool m_checkingConfirmation{false};
    bool m_verifyingExisting{false};
    bool m_existingVerificationFailed{false};
    bool m_busy{false};
    bool m_pending{false};
    bool m_failed{false};
    bool m_unknown{false};
    bool m_submitted{false};
    bool m_registrationSupported{false};
};
