/*!
 * @file        ui/app/identity/neynar_registration_controller.hpp
 * @brief       Farcaster registration state coordinator for Qt onboarding.
 *
 * @details
 * Coordinates FID reservation, custody transfer and verified confirmation.
 * Never treats relay acceptance alone as completed registration.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>
#include <optional>

class QNetworkReply;

/**
 * Farcaster/Neynar-only registration coordinator.
 *
 * There is no mnemonic, raw private key, Neynar API key, or funded app-wallet
 * ID in this class. A self-custody C++ callback signs an exact reserved FID.
 * An invoice-bound sponsorship voucher is short-lived session state.
 *
 * A submitted transaction is never assumed final from HTTP status alone.
 */
struct FarcasterRegistrationPorts final {
    std::function<bool()> backupVerified;
    std::function<QString()> custodyAddress;
    std::function<std::optional<QString>(
        std::uint64_t fid, std::uint64_t nonce, std::uint64_t deadline)> signTransfer;
};

class NeynarRegistrationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool registrationSupported READ registrationSupported NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasVoucher READ hasVoucher NOTIFY changed)
    Q_PROPERTY(bool canSubmit READ canSubmit NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(bool confirmed READ confirmed NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString reservedFid READ reservedFid NOTIFY changed)

public:
    explicit NeynarRegistrationController(
        QUrl origin, FarcasterRegistrationPorts ports,
        QObject* parent = nullptr, bool allowLocalTesting = false);
    ~NeynarRegistrationController() override;

    [[nodiscard]] bool available() const noexcept { return m_available; }
    [[nodiscard]] bool registrationSupported() const noexcept {
        return m_registrationSupported;
    }
    void setIdentityRuntimeReady(bool ready);
    [[nodiscard]] bool busy() const noexcept { return m_busy; }
    [[nodiscard]] bool hasVoucher() const noexcept { return !m_voucher.isEmpty(); }
    [[nodiscard]] bool canSubmit() const;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] bool confirmed() const noexcept;
    [[nodiscard]] QString phase() const { return m_phase; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] QString reservedFid() const { return m_fid; }

    /** A voucher is provided after a separate, verified paid sponsorship. */
    Q_INVOKABLE bool useEnrollmentVoucher(QString voucher);

    /** Cost-bearing, requires a real user click; never automatically retried. */
    Q_INVOKABLE bool reserveFid();

    /** Native EIP-712 transfer, then one irrevocable Neynar network request. */
    Q_INVOKABLE bool submit(QString requestedUsername);

    /** Read-only registry confirmation, safe after an ambiguous HTTP timeout. */
    Q_INVOKABLE bool checkConfirmation();

    /** Stops UI polling, but does not claim a chain write was revoked. */
    Q_INVOKABLE void stopPolling();

    /** Never discard a submitted or unknown-outcome registration. */
    Q_INVOKABLE bool reset();

signals:
    void changed();
    void farcasterFidConfirmed(QString fid, QString custodyAddress);

private:
    enum class Operation : std::uint8_t { Reserve, Submit, Status };
    void request(Operation operation, QByteArray body);
    void applyResponse(Operation operation, QByteArray payload);
    [[nodiscard]] bool walletMatches() const;
    void fail(QString reason);
    void setPhase(QString value);
    void scheduleStatus();

    QNetworkAccessManager m_network{};
    QPointer<QNetworkReply> m_reply{};
    QUrl m_origin{};
    FarcasterRegistrationPorts m_ports{};
    QByteArray m_voucher{};
    QString m_wallet{};
    QString m_reservationId{};
    QString m_fid{};
    QString m_nonce{};
    QString m_phase{QStringLiteral("idle")};
    QString m_error{};
    qint64 m_expiresAtMs{};
    bool m_available{false};
    bool m_registrationSupported{false};
    bool m_busy{false};
    int m_checks{};
    quint64 m_generation{};
};
