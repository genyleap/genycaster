#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QTimer>
#include <QVariantList>
#include <QtTypes>
#include <cstdint>
#include <functional>

#include "browser_openproof_auth.hpp"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

/** @brief Qt presentation adapter for OpenProof connections. OpenProof remains
 * the only authority for linked identities. Transient account-scoped bearer
 * stays in native C++ memory, never in QML/JS, storage or browser URL. */
class OpenProofConnectionsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList providers READ providers NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool catalogFallback READ catalogFallback NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString identityIssuer READ identityIssuer CONSTANT)
    Q_PROPERTY(bool authorized READ authorized NOTIFY stateChanged)
    Q_PROPERTY(bool authorizing READ authorizing NOTIFY stateChanged)
    Q_PROPERTY(QString approvalUrl READ approvalUrl NOTIFY stateChanged)
    Q_PROPERTY(QVariantList connections READ connections NOTIFY stateChanged)
    Q_PROPERTY(bool actionBusy READ actionBusy NOTIFY stateChanged)
    Q_PROPERTY(QString pendingProvider READ pendingProvider NOTIFY stateChanged)
public:
    explicit OpenProofConnectionsController(QObject* parent = nullptr);

    [[nodiscard]] QStringList providers() const;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] bool catalogFallback() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QString identityIssuer() const;
    /** @brief Configured browser gateway origin for a one-shot recovery proof.
     * Returns empty on invalid/non-HTTPS configuration. No bearer is exposed. */
    [[nodiscard]] QString recoveryGatewayOrigin() const;
    [[nodiscard]] bool authorized() const noexcept;
    [[nodiscard]] bool authorizing() const noexcept;
    [[nodiscard]] QString approvalUrl() const;
    [[nodiscard]] QVariantList connections() const;
    [[nodiscard]] bool actionBusy() const noexcept;
    [[nodiscard]] QString pendingProvider() const;

    /** @brief Changes viewer identity, invalidating any delegated account
     * token and pending link work when FID changes or session signs out. */
    Q_INVOKABLE void setViewerFid(qulonglong fid);
    /** @brief Starts a short-lived OpenProof device approval scoped to a
     * verified matching Farcaster FID, even for local Farcaster sign-ins. */
    Q_INVOKABLE void authorizeAccount();
    Q_INVOKABLE void refreshConnections();
    Q_INVOKABLE void connectProvider(const QString& provider);
    Q_INVOKABLE void disconnectProvider(const QString& provider, const QString& subject);
    Q_INVOKABLE void stopAuthorization();

    /** @brief Accepts the transient account grant from a *verified* OpenProof
     * login, never from QML. The same Farcaster FID must already own the GUI
     * account; other subjects/tokens are erased without affecting the session. */
    void acceptAuthenticatedGrant(QByteArray bearer, QString subject,
                                  std::uint64_t fid, int expiresSeconds);
    /** @brief Adopts the bounded, server-held OpenProof session issued by an
     * independently verified Farcaster custody recovery; never persists it. */
    void acceptRecoveryGrant(QByteArray ticket, QString subject,
                             std::uint64_t fid, int expiresSeconds);

    /** @brief Refreshes public provider capabilities over bounded HTTPS.
     * Network or CORS errors preserve the last known display-only list. */
    Q_INVOKABLE void refresh();
    /** @brief Decodes a public provider catalog, excluding local login factors.
     * Invalid, oversized and incomplete payloads yield an empty list. */
    [[nodiscard]] static QStringList parseProviderCatalog(const QByteArray& json);

signals:
    void stateChanged();

private:
    void clearAccountGrant();
    void sendAccountRequest(const QUrl& url, QByteArray body,
                            std::function<void(int, QByteArray)> callback);
    [[nodiscard]] QUrl gatewayActionUrl(const QString& suffix = {}) const;
    [[nodiscard]] bool isConnected(const QString& provider, const QString& subject = {}) const;
    void finishAction(QString error = {});

    QNetworkAccessManager m_network{this};
    BrowserOpenProofAuth m_authorization{this};
    QPointer<QNetworkReply> m_request{};
    QTimer m_grantExpiry{this};
    QTimer m_connectionPoll{this};
    QUrl m_gatewayUrl{};
    QByteArray m_accountBearer{};
    QString m_accountSubject{};
    QVariantList m_connections{};
    qulonglong m_viewerFid{};
    std::uint64_t m_grantGeneration{};
    bool m_authorizing{};
    bool m_verified{};
    bool m_actionBusy{};
    QString m_approvalUrl{};
    QString m_pendingProvider{};
    int m_remainingPolls{};
    QUrl m_catalogUrl{};
    QUrl m_issuerUrl{};
    QStringList m_providers{};
    QString m_error{};
    bool m_loading{};
    bool m_catalogFallback{};
};
