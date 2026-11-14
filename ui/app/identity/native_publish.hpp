#pragma once

#include "hypersnap_registration_transport.hpp"
#include <QByteArray>
#include <functional>

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
};

class HyperSnapRegistrationPublisher final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool registrationSupported READ registrationSupported NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool checkingConfirmation READ checkingConfirmation NOTIFY changed)
    Q_PROPERTY(int lastSubmissionHttpStatus READ lastSubmissionHttpStatus NOTIFY changed)
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
    [[nodiscard]] int lastSubmissionHttpStatus() const noexcept { return m_lastSubmissionHttpStatus; }
    [[nodiscard]] bool pending() const noexcept { return m_pending; }
    [[nodiscard]] bool confirmed() const noexcept { return !m_fid.isEmpty(); }
    [[nodiscard]] bool failed() const noexcept { return m_failed; }
    [[nodiscard]] bool outcomeUnknown() const noexcept { return m_unknown; }
    [[nodiscard]] QString registeredFid() const { return m_fid; }
    [[nodiscard]] QUrl nodeEndpoint() const { return m_transport.nodeEndpoint(); }

    /** User-initiated only. Does not automatically repeat an uncertain write. */
    Q_INVOKABLE bool submitPrepared();
    /** Read-only native shard-0 recovery for an interrupted registration. */
    Q_INVOKABLE bool recoverInterruptedRegistration();
    /** Queries the registry; safe even if submit returned timeout/500. */
    Q_INVOKABLE bool checkConfirmation();
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

    HyperSnapRegistrationTransport m_transport;
    NativePublicationPorts m_ports;
    QTimer m_poll{};
    QString m_address{};
    QString m_fid{};
    quint64 m_generation{};
    unsigned m_checks{};
    int m_lastSubmissionHttpStatus{};
    bool m_checkingConfirmation{false};
    bool m_busy{false};
    bool m_pending{false};
    bool m_failed{false};
    bool m_unknown{false};
    bool m_submitted{false};
    bool m_registrationSupported{false};
};
