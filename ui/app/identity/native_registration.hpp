/*!
 * @file        ui/app/identity/native_registration.hpp
 * @brief       Read-only HyperSnap native account discovery controller.
 *
 * @details
 * Resolves custody-to-FID relationships against an authoritative registry.
 * Exposes bounded verification state to onboarding without signing.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <optional>
#include <memory>

class HyperSnapRegistrationTransport;

/**
 * Read-only HyperSnap native registration readiness.
 * Never interprets an HTTP 200 as permission to create a wallet or assign an
 * FID. Those actions require a separate audited custody/signer onboarding.
 */
class NativeRegistrationStatusController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool syncedForRegistration READ syncedForRegistration NOTIFY changed)
    Q_PROPERTY(int shardZeroBlockDelay READ shardZeroBlockDelay NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int difficultyBits READ difficultyBits NOTIFY changed)
    Q_PROPERTY(QString firstNativeFid READ firstNativeFid NOTIFY changed)
    Q_PROPERTY(bool accountLookupBusy READ accountLookupBusy NOTIFY lookupChanged)
    Q_PROPERTY(bool accountLookupChecked READ accountLookupChecked NOTIFY lookupChanged)
    Q_PROPERTY(bool accountLookupFailed READ accountLookupFailed NOTIFY lookupChanged)
    Q_PROPERTY(QString existingNativeFid READ existingNativeFid NOTIFY lookupChanged)
public:
    struct Challenge final {
        std::uint64_t anchorHeight{};
        std::uint64_t nextBlock{};
        QString anchorHash{};
        std::uint32_t window{};
        std::uint32_t difficultyBits{};
    };
    explicit NativeRegistrationStatusController(QString baseUrl, QObject* parent = nullptr);
    ~NativeRegistrationStatusController() override;
    [[nodiscard]] bool active() const noexcept { return m_active; }
    [[nodiscard]] bool checking() const noexcept { return m_checking; }
    // A reachable HTTP endpoint may still be hours behind shard-0. Do not
    // authorize new irreversible writes against an unsynchronized node.
    [[nodiscard]] bool syncedForRegistration() const noexcept;
    [[nodiscard]] int shardZeroBlockDelay() const noexcept { return m_shardZeroDelay; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] int difficultyBits() const noexcept { return m_bits; }
    [[nodiscard]] QString firstNativeFid() const { return m_firstFid; }

    /** Unexpired, bound-to-this-node anchor information for C++ only. */
    [[nodiscard]] std::optional<Challenge> snapshot() const;
    Q_INVOKABLE void refresh();
    [[nodiscard]] bool accountLookupBusy() const noexcept { return m_lookupBusy; }
    [[nodiscard]] bool accountLookupChecked() const noexcept { return m_lookupChecked; }
    [[nodiscard]] bool accountLookupFailed() const noexcept { return m_lookupFailed; }
    [[nodiscard]] QString existingNativeFid() const { return m_existingFid; }

    /** C++ gate bound to the *exact* wallet previously checked with this node. */
    [[nodiscard]] bool verifiedAbsentForCustody(const QString& address) const;

    /** Returns an FID only after a successful, custody-bound registry lookup. */
    [[nodiscard]] QString verifiedExistingFidForCustody(const QString& address) const;
    Q_INVOKABLE void findExistingNativeAccount(const QString& address);
    Q_INVOKABLE void clearExistingLookup();
signals:
    void lookupChanged();
    void changed();
private:
    QNetworkAccessManager m_network{};
    QUrl m_base{};
    std::unique_ptr<HyperSnapRegistrationTransport> m_transport{};
    QPointer<QNetworkReply> m_reply{};
    QPointer<QNetworkReply> m_healthReply{};
    bool m_active{false};
    bool m_checking{false};
    int m_bits{0};
    QString m_firstFid{};
    QString m_error{};
    std::optional<Challenge> m_challenge{};
    qint64 m_checkedAtMs{};
    qint64 m_healthCheckedAtMs{};
    int m_shardZeroDelay{-1};
    quint64 m_lookupGeneration{};
    bool m_lookupBusy{false};
    bool m_lookupChecked{false};
    bool m_lookupFailed{false};
    QString m_existingFid{};
    QString m_lookupAddress{};
};
