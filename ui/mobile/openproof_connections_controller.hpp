#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QTimer>
#include <QSet>
#include <QVariantList>
#include <QtTypes>
#include <cstdint>
#include <functional>

#include "browser_openproof_auth.hpp"
#include <QObject>
class QSettings;
class QNetworkRequest;
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
    Q_PROPERTY(bool cachedSnapshot READ cachedSnapshot NOTIFY stateChanged)
    Q_PROPERTY(QString pendingProvider READ pendingProvider NOTIFY stateChanged)
    Q_PROPERTY(QString manualLinkedInUrl READ manualLinkedInUrl NOTIFY stateChanged)
    Q_PROPERTY(bool cloudBusy READ cloudBusy NOTIFY cloudStateChanged)
    Q_PROPERTY(bool cloudAuthorizationReady READ cloudAuthorizationReady NOTIFY stateChanged)
    Q_PROPERTY(bool cloudServiceAvailable READ cloudServiceAvailable NOTIFY cloudStateChanged)
    Q_PROPERTY(QString cloudError READ cloudError NOTIFY cloudStateChanged)
    Q_PROPERTY(int cloudRevision READ cloudRevision NOTIFY cloudStateChanged)
    Q_PROPERTY(QVariantMap cloudProfileExtras READ cloudProfileExtras NOTIFY cloudStateChanged)
    Q_PROPERTY(bool bookmarkRefreshBusy READ bookmarkRefreshBusy NOTIFY cloudStateChanged)
    Q_PROPERTY(QString bookmarkError READ bookmarkError NOTIFY cloudStateChanged)
    Q_PROPERTY(int bookmarkPendingCount READ bookmarkPendingCount NOTIFY cloudStateChanged)
    Q_PROPERTY(QVariantList topicCatalog READ topicCatalog NOTIFY topicsChanged)
    Q_PROPERTY(bool topicsBusy READ topicsBusy NOTIFY topicsChanged)
    Q_PROPERTY(QString topicsError READ topicsError NOTIFY topicsChanged)
public:
    explicit OpenProofConnectionsController(QObject* parent = nullptr,
                                            QSettings* settings = nullptr);

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
    [[nodiscard]] bool cachedSnapshot() const noexcept;
    [[nodiscard]] QString pendingProvider() const;
    [[nodiscard]] QString manualLinkedInUrl() const;
    [[nodiscard]] bool cloudBusy() const noexcept { return m_cloudRequest != nullptr; }
    [[nodiscard]] bool cloudAuthorizationReady() const noexcept {
        return (m_verified && !m_accountBearer.isEmpty()) || m_cloudSessionVerified;
    }
    [[nodiscard]] bool cloudServiceAvailable() const noexcept { return m_cloudServiceAvailable; }
    Q_INVOKABLE void checkCloudService();
    [[nodiscard]] QString cloudError() const { return m_cloudError; }
    [[nodiscard]] int cloudRevision() const noexcept { return m_cloudRevision; }
    [[nodiscard]] QVariantMap cloudProfileExtras() const { return m_cloudProfileExtras; }
    [[nodiscard]] bool bookmarkPreviewSupported() const noexcept {
        return m_bookmarkPreviewsReady;
    }
    void publishBookmarkPreviewIfMissing(const QString& castId,
                                         const QVariantMap& snapshot);
    [[nodiscard]] bool bookmarkRefreshBusy() const noexcept {
        return m_bookmarkSnapshotRequest!=nullptr;
    }
    [[nodiscard]] QString bookmarkError() const { return m_bookmarkError; }
    [[nodiscard]] int bookmarkPendingCount() const noexcept {
        return static_cast<int>(m_bookmarkOutbox.size());
    }
    [[nodiscard]] QVariantList topicCatalog() const { return m_topicCatalog; }
    [[nodiscard]] bool topicsBusy() const noexcept { return m_topicCatalogRequest != nullptr; }
    [[nodiscard]] QString topicsError() const { return m_topicsError; }
    Q_INVOKABLE void fetchTopics();
    Q_INVOKABLE void saveCastTopics(const QString& castHash, const QStringList& slugs);
    /** Automatic account-owned profile storage; bearer never leaves native Qt. */
    Q_INVOKABLE void fetchCloudProfileExtras();
    /** Read the last server-confirmed OpenProof link display snapshot. */
    Q_INVOKABLE void refreshAccountConnectionsSnapshot();
    /** Re-read the bookmark change history on demand, without local data loss. */
    Q_INVOKABLE void refreshAccountBookmarks();
    Q_INVOKABLE void pushCloudProfileExtras(const QVariantMap& fields);
    /** Manual vanity URLs are self-asserted; OpenProof remains link authority. */
    Q_INVOKABLE void uploadManualLinkedInCloud();
    Q_INVOKABLE void restoreManualLinkedInCloud();
    /** Automatically record a durable local reading-list mutation for replay. */
    void recordBookmarkMutation(const QString& castId, bool deleted,
                                const QVariantMap& snapshot = {});
    /** Migrate existing local bookmarks once per verified account. */
    void migrateLocalBookmarks(const QStringList& ids);
    /** Persist the per-account notification seen watermark automatically. */
    void recordNotificationSeenAt(qulonglong fid, qlonglong observedAtMs);
    /** @brief Queue local composer edits and deletions for the verified FID. */
    void recordDraftMutation(qulonglong fid, bool deleted);
    /** @brief Acknowledge a server snapshot only after the editor applied it. */
    void confirmDraftSnapshot(qulonglong fid, int revision);
    /** Refresh the current account draft immediately on normal composer entry. */
    Q_INVOKABLE void refreshAccountDraft();
    /** Store a user-asserted public vanity URL in device-local state only.
     * Does not upgrade OpenProof verification or silently enable cloud sync. */
    Q_INVOKABLE void setManualLinkedInUrl(const QString& url);

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
    /** An already registered Farcaster signer proved ownership to the
     * same-origin Genycaster API. Accept only the active account's CSRF
     * capability; the HttpOnly cookie itself never crosses into Qt. */
    void acceptBrowserAccountSession(qulonglong fid, QByteArray csrf);

    /** @brief Refreshes public provider capabilities over bounded HTTPS.
     * Network or CORS errors preserve the last known display-only list. */
    Q_INVOKABLE void refresh();
    /** @brief Decodes a public provider catalog, excluding local login factors.
     * Invalid, oversized and incomplete payloads yield an empty list. */
    [[nodiscard]] static QStringList parseProviderCatalog(const QByteArray& json);

signals:
    void stateChanged();
    void cloudStateChanged();
    void topicsChanged();
    void cloudProfileExtrasReceived(const QVariantMap& item);
    void cloudProfileExtrasSaved(const QVariantMap& item);
    void cloudBookmarkChangesReceived(const QVariantList& changes);
    void cloudNotificationSeenAtReceived(qulonglong fid, qlonglong observedAtMs);
    void cloudDraftSnapshotReceived(qulonglong fid, QByteArray document,
                                    bool deleted, int revision);

private:
    [[nodiscard]] QString manualLinkedInSettingKey() const;
    void clearAccountGrant();
    void restoreAccountSession();
    void bootstrapAccountSession();
    void revokeAccountSession();
    void authorizeCloudRequest(QNetworkRequest& request) const;
    void restoreCachedConnections();
    void publishVerifiedConnectionsSnapshot();
    void publishPortableConnectionLabels();
    void saveCachedConnections() const;
    void sendAccountRequest(const QUrl& url, QByteArray body,
                            std::function<void(int, QByteArray)> callback);
    [[nodiscard]] QUrl gatewayActionUrl(const QString& suffix = {}) const;
    [[nodiscard]] bool isConnected(const QString& provider, const QString& subject = {}) const;
    void finishAction(QString error = {});
    void sendCloudProfileRequest(bool upload, const QVariantMap& fields = {});
    void sendCloudLinkedInRequest(bool upload);
    void pumpBookmarkChanges();
    void pumpNotificationSeenAt(bool upload = false);
    void pumpDraftChanges();
    void uploadDraftChange();
    [[nodiscard]] QString draftStorageKey() const;
    void reconcileLinkedIn();
    [[nodiscard]] QString bookmarkStorageKey() const;

    QNetworkAccessManager m_network{this};
    QSettings* m_settings{};
    BrowserOpenProofAuth m_authorization{this};
    QPointer<QNetworkReply> m_request{};
    QPointer<QNetworkReply> m_cloudRequest{};
    QPointer<QNetworkReply> m_accountConnectionsRequest{};
    QPointer<QNetworkReply> m_portableConnectionsRequest{};
    bool m_portableConnectionsPublished{};
    QPointer<QNetworkReply> m_bookmarkSnapshotRequest{};
    bool m_bookmarkPreviewsReady{};
    QSet<QString> m_bookmarkPreviewSent{};
    QPointer<QNetworkReply> m_draftRequest{};
    QPointer<QNetworkReply> m_cloudProbe{};
    QPointer<QNetworkReply> m_topicCatalogRequest{};
    QPointer<QNetworkReply> m_topicSaveRequest{};
    QVariantList m_topicCatalog{};
    QVariantList m_topicOutbox{};
    QString m_topicsError{};
    void pumpTopicWrites();
    [[nodiscard]] QString topicOutboxKey() const;
    QPointer<QNetworkReply> m_sessionRequest{};
    bool m_cloudServiceAvailable{};
    bool m_cloudSessionVerified{};
    bool m_sessionRevoking{};
    QByteArray m_cloudCsrf{};
    bool m_applyingCloudLinkedIn{};
    bool m_linkedInReconciled{};
    QUrl m_cloudUrl{};
    QString m_cloudError{};
    int m_cloudRevision{};
    QVariantMap m_cloudProfileExtras{};
    QTimer m_grantExpiry{this};
    QTimer m_connectionPoll{this};
    QTimer m_bookmarkPoll{this};
    QVariantList m_bookmarkOutbox{};
    QString m_bookmarkError{};
    QString m_bookmarkCursor{QStringLiteral("0")};
    bool m_legacyBookmarksPending{};
    QStringList m_legacyBookmarkCandidates{};
    QSet<QString> m_bookmarkHistoryKeys{};
    qlonglong m_notificationSeenAtMs{};
    qlonglong m_notificationRemoteAtMs{};
    int m_draftRevision{};
    int m_draftRemoteRevision{};
    qulonglong m_draftMutationSerial{};
    bool m_draftRemoteInitialized{};
    bool m_draftConflict{};
    QUrl m_gatewayUrl{};
    QUrl m_browserConnectionBaseUrl{};
    QByteArray m_accountBearer{};
    QString m_accountSubject{};
    QVariantList m_connections{};
    qulonglong m_viewerFid{};
    std::uint64_t m_grantGeneration{};
    bool m_authorizing{};
    bool m_verified{};
    bool m_cachedSnapshot{};
    bool m_actionBusy{};
    QString m_approvalUrl{};
    QString m_pendingProvider{};
    bool m_reconnectingLinkedIn{};
    int m_remainingPolls{};
    QUrl m_catalogUrl{};
    QUrl m_issuerUrl{};
    QStringList m_providers{};
    QString m_error{};
    bool m_loading{};
    bool m_catalogFallback{};
};
