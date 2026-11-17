/*!
 * @file        ui/app/identity/pro_checkout.hpp
 * @brief       Trusted Pro ERC-20 wallet checkout adapter.
 *
 * @details
 * Owns only a transient external wallet approval workflow. The account API,
 * not this adapter or the wallet response, owns payment verification and Pro.
 *
 * @author      Kambiz Asadzadeh
 * @since       09 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <functional>

class OpenProofConnectionsController;

/** @brief Independent first-party EIP-1193 payment session, never a Mini App grant. */
class ProCheckoutController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString txHash READ txHash NOTIFY changed)
    Q_PROPERTY(QString quoteId READ quoteId NOTIFY changed)
    Q_PROPERTY(bool walletAvailable READ walletAvailable NOTIFY changed)
    Q_PROPERTY(bool walletConnected READ walletConnected NOTIFY changed)
public:
    using RpcComplete = std::function<void(QJsonValue, QString)>;
    using RpcRequest = std::function<void(QString, QJsonArray, RpcComplete)>;

    explicit ProCheckoutController(OpenProofConnectionsController& account,
                                    QObject* parent = nullptr);
    ~ProCheckoutController() override;
    void setWalletTransport(std::function<bool()> begin, RpcRequest request,
                             std::function<void()> end);
    [[nodiscard]] bool busy() const noexcept { return m_busy; }
    [[nodiscard]] QString phase() const { return m_phase; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] QString txHash() const { return m_txHash; }
    [[nodiscard]] QString quoteId() const { return m_quoteId; }
    [[nodiscard]] bool walletAvailable() const noexcept { return static_cast<bool>(m_begin); }
    [[nodiscard]] bool walletConnected() const noexcept {
        return m_session && !m_busy && m_phase == QLatin1String("connected")
               && !m_payer.isEmpty();
    }

    /** @brief Must be invoked by a user button; never triggered by new quote binding. */
    /** Connect an explicitly selected external wallet before requesting a quote.
     * No transfer or chain switch is permitted during this preflight. */
    Q_INVOKABLE void connectWallet();
    Q_INVOKABLE void pay();
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void walletConnectedSuccessfully();
    void transactionSubmitted(const QString& quoteId, const QString& txHash);
private:
    void request(const QString& method, QJsonArray parameters, RpcComplete callback);
    void finishError(const QString& error);
    void finishSubmitted(const QString& hash);
    void requestChain();
    void sendTransfer();
    [[nodiscard]] QString checkedTransferData() const;
    [[nodiscard]] bool validQuote(const QVariantMap& quote) const;

    QPointer<OpenProofConnectionsController> m_account;
    std::function<bool()> m_begin{};
    RpcRequest m_request{};
    std::function<void()> m_end{};
    QVariantMap m_quote{};
    qulonglong m_fid{};
    int m_generation{};
    bool m_busy{};
    bool m_session{};
    QString m_phase{};
    QString m_error{};
    QString m_txHash{};
    QString m_quoteId{};
    QString m_payer{};
};
