#pragma once

#include "native_registration.hpp"

#include <QByteArray>
#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

/**
 * Explicit, read-only preparation of a HyperSnap native registration.
 * PoW uses only the PUBLIC custody address and anchor on a Qt worker.
 * The registered custody key never crosses worker threads or enters QML.
 * The resulting envelope is private C++ state and is NOT broadcast here.
 */
struct NativePreparationPorts final {
    std::function<bool()> backupVerified;
    std::function<QString()> custodyAddress;
    /** Must prove a NotFound lookup for this exact address on this node. */
    std::function<bool(const QString&)> walletIsUnregistered;
    std::function<std::optional<NativeRegistrationStatusController::Challenge>()> challenge;
    std::function<QByteArray(
        const NativeRegistrationStatusController::Challenge&, const QString& nonce)>
        seal;
};

class NativeRegistrationPreparationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool prepared READ prepared NOTIFY changed)
    Q_PROPERTY(bool failed READ failed NOTIFY changed)
public:
    explicit NativeRegistrationPreparationController(
        NativePreparationPorts ports, QObject* parent = nullptr);
    ~NativeRegistrationPreparationController() override;

    [[nodiscard]] bool busy() const noexcept { return m_busy; }
    [[nodiscard]] bool prepared() const noexcept { return !m_envelope.isEmpty(); }
    [[nodiscard]] bool failed() const noexcept { return m_failed; }
    /** The envelope is public protocol data, but is NEVER a QML property. */
    [[nodiscard]] QByteArray preparedEnvelope() const { return m_envelope; }

    Q_INVOKABLE bool prepare();
    Q_INVOKABLE void cancel();
signals:
    void changed();

private:
    void finish();
    void erasePrepared() noexcept;

    NativePreparationPorts m_ports{};
    QFutureWatcher<QString> m_watcher{};
    std::shared_ptr<std::atomic_bool> m_cancel{};
    std::optional<NativeRegistrationStatusController::Challenge> m_challenge{};
    QByteArray m_envelope{};
    QString m_walletAddress{};
    bool m_busy{false};
    bool m_failed{false};
    quint64 m_proofRevision{};
};
