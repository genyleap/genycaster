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
    Q_PROPERTY(QVariantMap profileStatus READ profileStatus NOTIFY profileStatusChanged)
    Q_PROPERTY(QVariantMap streak READ streak NOTIFY streakChanged)
    Q_PROPERTY(QVariantMap proEntitlements READ proEntitlements NOTIFY proEntitlementsChanged)
    Q_PROPERTY(bool proEntitlementsBusy READ proEntitlementsBusy NOTIFY proEntitlementsChanged)
    Q_PROPERTY(QVariantMap genyAssets READ genyAssets NOTIFY genyAssetsChanged)
    Q_PROPERTY(bool streakBusy READ streakBusy NOTIFY streakChanged)
    Q_PROPERTY(QString streakError READ streakError NOTIFY streakChanged)
    Q_PROPERTY(QVariantMap streakPreferences READ streakPreferences NOTIFY streakChanged)
    Q_PROPERTY(QStringList mutedProfileFids READ mutedProfileFids NOTIFY mutedProfilesChanged)
    Q_PROPERTY(bool mutedProfilesReady READ mutedProfilesReady NOTIFY mutedProfilesChanged)
    Q_PROPERTY(bool mutedProfilesBusy READ mutedProfilesBusy NOTIFY mutedProfilesChanged)
    Q_PROPERTY(QString mutedProfilesError READ mutedProfilesError NOTIFY mutedProfilesChanged)
    Q_PROPERTY(QVariantList streakLeaderboard READ streakLeaderboard NOTIFY streakChanged)
    Q_PROPERTY(QString publicActivitySource READ publicActivitySource NOTIFY streakChanged)
    Q_PROPERTY(QString publicActivityGithubLogin READ publicActivityGithubLogin NOTIFY streakChanged)
    Q_PROPERTY(qulonglong publicActivityFid READ publicActivityFid NOTIFY streakChanged)
    Q_PROPERTY(bool streakLeaderboardBusy READ streakLeaderboardBusy NOTIFY streakChanged)
    Q_PROPERTY(bool profileStatusBusy READ profileStatusBusy NOTIFY profileStatusChanged)
    Q_PROPERTY(QString profileStatusError READ profileStatusError NOTIFY profileStatusChanged)
    Q_PROPERTY(bool bookmarkRefreshBusy READ bookmarkRefreshBusy NOTIFY cloudStateChanged)
    Q_PROPERTY(QString bookmarkError READ bookmarkError NOTIFY cloudStateChanged)
    Q_PROPERTY(int bookmarkPendingCount READ bookmarkPendingCount NOTIFY cloudStateChanged)
    Q_PROPERTY(QVariantList topicCatalog READ topicCatalog NOTIFY topicsChanged)
    Q_PROPERTY(bool topicsBusy READ topicsBusy NOTIFY topicsChanged)
    Q_PROPERTY(QString topicsError READ topicsError NOTIFY topicsChanged)
    Q_PROPERTY(QVariantList topicCastItems READ topicCastItems NOTIFY topicCastsChanged)
    Q_PROPERTY(bool topicCastsBusy READ topicCastsBusy NOTIFY topicCastsChanged)
    Q_PROPERTY(bool topicCastsHasMore READ topicCastsHasMore NOTIFY topicCastsChanged)
    Q_PROPERTY(QString topicCastsError READ topicCastsError NOTIFY topicCastsChanged)
    Q_PROPERTY(QString activeCastTopic READ activeCastTopic NOTIFY topicCastsChanged)
    Q_PROPERTY(QVariantList bookmarkCollections READ bookmarkCollections NOTIFY bookmarkCollectionsChanged)
    Q_PROPERTY(QVariantList bookmarkCollectionItems READ bookmarkCollectionItems NOTIFY bookmarkCollectionsChanged)
    Q_PROPERTY(QVariantList bookmarkMembershipItems READ bookmarkMembershipItems NOTIFY bookmarkCollectionsChanged)
    Q_PROPERTY(QString activeBookmarkCollection READ activeBookmarkCollection NOTIFY bookmarkCollectionsChanged)
    Q_PROPERTY(bool bookmarkCollectionsBusy READ bookmarkCollectionsBusy NOTIFY bookmarkCollectionsChanged)
    Q_PROPERTY(QString bookmarkCollectionsError READ bookmarkCollectionsError NOTIFY bookmarkCollectionsChanged)
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
    [[nodiscard]] QVariantMap profileStatus() const { return m_profileStatus; }
    [[nodiscard]] QVariantMap streak() const { return m_streak; }
    [[nodiscard]] bool streakBusy() const noexcept { return m_streakRead || m_streakWrite; }
    [[nodiscard]] QString streakError() const { return m_streakError; }
    Q_INVOKABLE void fetchStreak();
    [[nodiscard]] QVariantMap proEntitlements() const {
        return m_proEntitlementsFid == m_viewerFid && m_viewerFid != 0
            ? m_proEntitlements : QVariantMap{};
    }
    [[nodiscard]] bool proEntitlementsBusy() const noexcept {
        return m_proEntitlementsRead != nullptr;
    }
    Q_INVOKABLE void fetchProEntitlements(bool force = false);
    [[nodiscard]] QVariantMap genyAssets() const {
        return m_genyAssetsFid == m_viewerFid && m_viewerFid != 0
            ? m_genyAssets : QVariantMap{};
    }
    Q_INVOKABLE void fetchGenyAssets(bool force = false);
    Q_INVOKABLE void recordStreakCast(const QString& hash);
    [[nodiscard]] QVariantMap streakPreferences() const { return m_streakPreferences; }
    [[nodiscard]] QStringList mutedProfileFids() const { return m_mutedProfileFids; }
    [[nodiscard]] bool mutedProfilesReady() const noexcept { return m_mutedProfilesLoaded; }
    [[nodiscard]] bool mutedProfilesBusy() const noexcept { return m_mutedProfilesWrite != nullptr; }
    [[nodiscard]] QString mutedProfilesError() const { return m_mutedProfilesError; }
    Q_INVOKABLE void fetchMutedProfiles();
    Q_INVOKABLE void setProfileMuted(qulonglong targetFid, bool muted);
    [[nodiscard]] QVariantList streakLeaderboard() const { return m_streakLeaderboard; }
    [[nodiscard]] bool streakLeaderboardBusy() const noexcept { return m_streakLeaderboardRead != nullptr; }
    [[nodiscard]] QString publicActivitySource() const { return m_publicActivitySource; }
    [[nodiscard]] QString publicActivityGithubLogin() const { return m_publicActivityGithubLogin; }
    [[nodiscard]] qulonglong publicActivityFid() const noexcept { return m_publicActivityFid; }
    Q_INVOKABLE void fetchPublicActivitySource(qulonglong fid);
    Q_INVOKABLE void fetchStreakPreferences();
    Q_INVOKABLE void setStreakPreferences(const QString& activitySource,
                                        bool leaderboardVisible);
    Q_INVOKABLE void fetchStreakLeaderboard(const QString& track);
    [[nodiscard]] bool profileStatusBusy() const noexcept {
        return m_profileStatusRead || m_profileStatusWrite;
    }
    [[nodiscard]] QString profileStatusError() const { return m_profileStatusError; }
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
    [[nodiscard]] QVariantList topicCastItems() const { return m_topicCastItems; }
    [[nodiscard]] bool topicCastsBusy() const noexcept { return m_topicCastsRequest != nullptr; }
    [[nodiscard]] bool topicCastsHasMore() const noexcept { return m_topicCastsHasMore; }
    [[nodiscard]] QString topicCastsError() const { return m_topicCastsError; }
    [[nodiscard]] QString activeCastTopic() const { return m_activeCastTopic; }
    Q_INVOKABLE void selectCastTopic(const QString& slug, const QString& authorFid = {});
    Q_INVOKABLE void loadMoreTopicCasts();
    Q_INVOKABLE void clearCastTopic();
    Q_INVOKABLE void saveCastTopics(const QString& castHash, const QStringList& slugs);
    [[nodiscard]] QVariantList bookmarkCollections() const { return m_bookmarkCollections; }
    [[nodiscard]] QVariantList bookmarkCollectionItems() const { return m_bookmarkCollectionItems; }
    [[nodiscard]] QVariantList bookmarkMembershipItems() const { return m_bookmarkMembershipItems; }
    [[nodiscard]] QString activeBookmarkCollection() const { return m_activeBookmarkCollection; }
    [[nodiscard]] bool bookmarkCollectionsBusy() const noexcept { return m_bookmarkCollectionRequest != nullptr; }
    [[nodiscard]] QString bookmarkCollectionsError() const { return m_bookmarkCollectionsError; }
    Q_INVOKABLE void requestBookmarkCollectionChoice(const QString& castHash);
    Q_INVOKABLE void refreshBookmarkCollections();
    Q_INVOKABLE void selectBookmarkCollection(const QString& id);
    Q_INVOKABLE void createBookmarkCollection(const QString& name);
    Q_INVOKABLE void renameBookmarkCollection(const QString& id, const QString& name,
                                              const QString& revision);
    Q_INVOKABLE void deleteBookmarkCollection(const QString& id, const QString& revision);
    Q_INVOKABLE void setBookmarkCollectionMembership(const QString& castHash,
                                                      const QString& collectionId, bool remove);
    /** Automatic account-owned profile storage; bearer never leaves native Qt. */
    Q_INVOKABLE void fetchCloudProfileExtras();
    Q_INVOKABLE void fetchProfileStatus(qulonglong profileFid);
    Q_INVOKABLE void saveProfileStatus(const QString& mood,const QString& note,bool visible);
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
    void mutedProfilesChanged();
    void proEntitlementsChanged();
    void genyAssetsChanged();
    void stateChanged();
    void cloudStateChanged();
    void profileStatusChanged();
    void streakChanged();
    /** Emitted exactly once for each status save attempt (including rejected requests). */
    void profileStatusSaveFinished(bool saved, const QString& message);
    void topicsChanged();
    void topicCastsChanged();
    void bookmarkCollectionsChanged();
    void bookmarkCollectionChoiceRequested(const QString& castHash);
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
    QPointer<QNetworkReply> m_profileStatusRead{};
    QPointer<QNetworkReply> m_streakRead{};
    QPointer<QNetworkReply> m_proEntitlementsRead{};
    QVariantMap m_proEntitlements{};
    qulonglong m_proEntitlementsFid{};
    qint64 m_proEntitlementsFetchedAt{};
    QPointer<QNetworkReply> m_genyAssetsRead{};
    QVariantMap m_genyAssets{};
    qulonglong m_genyAssetsFid{};
    qint64 m_genyAssetsFetchedAt{};
    QPointer<QNetworkReply> m_streakWrite{};
    QStringList m_pendingStreakCasts{};
    int m_streakRetryDelayMs{10'000};
    void pumpStreakWrites();
    QPointer<QNetworkReply> m_publicActivityRead{};
    qulonglong m_publicActivityFid{};
    QString m_publicActivitySource{QStringLiteral("casts")};
    QString m_publicActivityGithubLogin{};
    QPointer<QNetworkReply> m_streakPreferencesRead{};
    QPointer<QNetworkReply> m_mutedProfilesRead{};
    QPointer<QNetworkReply> m_mutedProfilesWrite{};
    QStringList m_mutedProfileFids{};
    QString m_mutedProfilesError{};
    qlonglong m_mutedProfilesRevision{};
    qint64 m_mutedProfilesFetchedAt{};
    bool m_mutedProfilesLoaded{};
    QPointer<QNetworkReply> m_streakPreferencesWrite{};
    QPointer<QNetworkReply> m_streakLeaderboardRead{};
    QVariantMap m_streakPreferences{};
    QVariantList m_streakLeaderboard{};
    QString m_streakLeaderboardTrack{QStringLiteral("activity")};
    QVariantMap m_streak{};
    QString m_streakError{};
    QPointer<QNetworkReply> m_profileStatusWrite{};
    qulonglong m_profileStatusFid{};
    qlonglong m_profileStatusRevision{};
    QVariantMap m_profileStatus{};
    QString m_profileStatusError{};
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
    QPointer<QNetworkReply> m_topicCastsRequest{};
    QVariantList m_topicCastItems{};
    qsizetype m_topicCastOffset{}; // Raw server offset, independent of deduped display count.
    QString m_activeCastTopic{};
    QString m_topicAuthorFid{};
    QString m_topicCastsError{};
    bool m_topicCastsHasMore{};
    std::uint64_t m_topicCastsGeneration{};
    void fetchTopicCastPage();
    QVariantList m_topicCatalog{};
    QVariantList m_topicOutbox{};
    int m_topicRetryDelayMs{10'000};
    QString m_topicsError{};
    QVariantList m_bookmarkCollections{};
    QVariantList m_bookmarkCollectionItems{};
    QVariantList m_bookmarkMembershipItems{};
    QString m_activeBookmarkCollection{};
    QString m_bookmarkCollectionsError{};
    QPointer<QNetworkReply> m_bookmarkCollectionRequest{};
    void pumpTopicWrites();
    [[nodiscard]] QString topicOutboxKey() const;
    void sendBookmarkCollectionRequest(const QString& resource, const QVariantMap& body,
                                       bool upload, const QString& queryId = {});
    // Cancel safely even when QNetworkReply::abort() emits finished() inline.
    void cancelAccountSessionRequest();
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
