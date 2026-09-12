// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/** @brief Thin QML adapter for guest-first reads and verified client-session state. */
class QSettings;

class PresentationBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString architectureState READ architectureState CONSTANT)
    Q_PROPERTY(bool isStrictSelfHosted READ isStrictSelfHosted CONSTANT)
    Q_PROPERTY(bool diagnosticsEnabled READ diagnosticsEnabled CONSTANT)
    Q_PROPERTY(bool runtimeReady READ runtimeReady NOTIFY stateChanged)
    Q_PROPERTY(bool authenticationAvailable READ authenticationAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY stateChanged)
    Q_PROPERTY(bool authenticationBusy READ authenticationBusy NOTIFY stateChanged)
    Q_PROPERTY(QString authenticationPhase READ authenticationPhase NOTIFY stateChanged)
    Q_PROPERTY(bool authenticationApprovalAvailable READ authenticationApprovalAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString authenticationErrorText READ authenticationErrorText NOTIFY stateChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY stateChanged)
    Q_PROPERTY(QString accountUsername READ accountUsername NOTIFY stateChanged)
    Q_PROPERTY(QString accountAvatarUrl READ accountAvatarUrl NOTIFY stateChanged)
    Q_PROPERTY(qulonglong accountFid READ accountFid NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap accountProfile READ accountProfile NOTIFY stateChanged)
    Q_PROPERTY(bool writeReady READ writeReady NOTIFY stateChanged)
    Q_PROPERTY(QString signerPhase READ signerPhase NOTIFY stateChanged)
    Q_PROPERTY(bool signerBusy READ signerBusy NOTIFY stateChanged)
    Q_PROPERTY(QString signerStatusText READ signerStatusText NOTIFY stateChanged)
    Q_PROPERTY(QString signerErrorText READ signerErrorText NOTIFY stateChanged)
    Q_PROPERTY(bool writeBusy READ writeBusy NOTIFY stateChanged)
    Q_PROPERTY(QString writePhase READ writePhase NOTIFY stateChanged)
    Q_PROPERTY(QString writeStatusText READ writeStatusText NOTIFY stateChanged)
    Q_PROPERTY(QString writeErrorText READ writeErrorText NOTIFY stateChanged)
    Q_PROPERTY(QString lastPublishedCastId READ lastPublishedCastId NOTIFY stateChanged)
    Q_PROPERTY(QString activeView READ activeView NOTIFY stateChanged)
    Q_PROPERTY(QString activeFeed READ activeFeed NOTIFY stateChanged)
    Q_PROPERTY(QString profileFeed READ profileFeed NOTIFY stateChanged)
    Q_PROPERTY(QString identityName READ identityName NOTIFY stateChanged)
    Q_PROPERTY(qulonglong identityFid READ identityFid NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString diagnosticText READ diagnosticText NOTIFY stateChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY stateChanged)
    Q_PROPERTY(bool isStale READ isStale NOTIFY stateChanged)
    Q_PROPERTY(bool isOffline READ isOffline NOTIFY stateChanged)
    Q_PROPERTY(bool sourceUnavailable READ sourceUnavailable NOTIFY stateChanged)
    Q_PROPERTY(bool networkReachabilityKnown READ networkReachabilityKnown NOTIFY stateChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY stateChanged)
    Q_PROPERTY(bool hasReadActivity READ hasReadActivity NOTIFY stateChanged)
    Q_PROPERTY(bool feedStarted READ feedStarted NOTIFY stateChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList explorerArtRows READ explorerArtRows NOTIFY stateChanged)
    Q_PROPERTY(bool explorerArtBusy READ explorerArtBusy NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY stateChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    Q_PROPERTY(bool searchBusy READ searchBusy NOTIFY searchChanged)
    Q_PROPERTY(QString searchErrorText READ searchErrorText NOTIFY searchChanged)
    Q_PROPERTY(QVariantList notificationRows READ notificationRows NOTIFY notificationsChanged)
    Q_PROPERTY(bool notificationsBusy READ notificationsBusy NOTIFY notificationsChanged)
    Q_PROPERTY(QString notificationsErrorText READ notificationsErrorText NOTIFY notificationsChanged)
    Q_PROPERTY(bool notificationsHasMore READ notificationsHasMore NOTIFY notificationsChanged)
    Q_PROPERTY(int notificationUnreadCount READ notificationUnreadCount NOTIFY notificationsChanged)
    Q_PROPERTY(bool hasUnreadNotifications READ hasUnreadNotifications NOTIFY notificationsChanged)
    Q_PROPERTY(int feedPageSize READ feedPageSize NOTIFY readPreferencesChanged)
    Q_PROPERTY(int contextHydrationLimit READ contextHydrationLimit NOTIFY readPreferencesChanged)
    Q_PROPERTY(bool linkPreviewPrefetchEnabled READ linkPreviewPrefetchEnabled NOTIFY readPreferencesChanged)
    Q_PROPERTY(bool inlineYouTubeEnabled READ inlineYouTubeEnabled NOTIFY readPreferencesChanged)
    Q_PROPERTY(bool bufferNewPosts READ bufferNewPosts NOTIFY readPreferencesChanged)
    Q_PROPERTY(int pendingNewPostCount READ pendingNewPostCount NOTIFY stateChanged)

public:
    /**
     * @brief Creates the presentation adapter with optional Qt ownership and persisted read preferences.
     * @param parent Optional QObject owner.
     * @param settings Optional borrowed settings store; when supplied it must outlive this bridge.
     */
    explicit PresentationBridge(QObject* parent = nullptr, QSettings* settings = nullptr);
    [[nodiscard]] QString architectureState() const;
    [[nodiscard]] bool isStrictSelfHosted() const noexcept;
    [[nodiscard]] bool diagnosticsEnabled() const noexcept;
    [[nodiscard]] bool runtimeReady() const noexcept;
    [[nodiscard]] bool authenticationAvailable() const noexcept;
    [[nodiscard]] bool isAuthenticated() const noexcept;
    [[nodiscard]] bool authenticationBusy() const noexcept;
    [[nodiscard]] QString authenticationPhase() const;
    [[nodiscard]] bool authenticationApprovalAvailable() const noexcept;
    [[nodiscard]] QString authenticationErrorText() const;
    [[nodiscard]] QString accountName() const;
    [[nodiscard]] QString accountUsername() const;
    [[nodiscard]] QString accountAvatarUrl() const;
    [[nodiscard]] qulonglong accountFid() const noexcept;
    /** @brief Returns the cached provider-neutral profile presentation for the authenticated account. */
    [[nodiscard]] QVariantMap accountProfile() const;
    [[nodiscard]] bool writeReady() const noexcept;
    [[nodiscard]] QString signerPhase() const;
    [[nodiscard]] bool signerBusy() const noexcept;
    [[nodiscard]] QString signerStatusText() const;
    [[nodiscard]] QString signerErrorText() const;
    [[nodiscard]] bool writeBusy() const noexcept;
    [[nodiscard]] QString writePhase() const;
    [[nodiscard]] QString writeStatusText() const;
    [[nodiscard]] QString writeErrorText() const;
    [[nodiscard]] QString lastPublishedCastId() const;
    [[nodiscard]] QString activeView() const;
    [[nodiscard]] QString activeFeed() const;
    [[nodiscard]] QString profileFeed() const;
    [[nodiscard]] QString identityName() const;
    [[nodiscard]] qulonglong identityFid() const noexcept;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString diagnosticText() const;
    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] bool isStale() const noexcept;
    [[nodiscard]] bool isOffline() const noexcept;
    [[nodiscard]] bool sourceUnavailable() const noexcept;
    [[nodiscard]] bool networkReachabilityKnown() const noexcept;
    [[nodiscard]] bool hasMore() const noexcept;
    [[nodiscard]] bool hasReadActivity() const noexcept;
    [[nodiscard]] bool feedStarted() const noexcept;
    [[nodiscard]] QVariantList rows() const;
    [[nodiscard]] QVariantList explorerArtRows() const;
    [[nodiscard]] bool explorerArtBusy() const noexcept;
    [[nodiscard]] QVariantMap detail() const;
    [[nodiscard]] QVariantList searchResults() const;
    [[nodiscard]] bool searchBusy() const noexcept;
    [[nodiscard]] QString searchErrorText() const;
    [[nodiscard]] QVariantList notificationRows() const;
    [[nodiscard]] bool notificationsBusy() const noexcept;
    [[nodiscard]] QString notificationsErrorText() const;
    [[nodiscard]] bool notificationsHasMore() const noexcept;
    [[nodiscard]] int notificationUnreadCount() const noexcept;
    [[nodiscard]] bool hasUnreadNotifications() const noexcept;
    [[nodiscard]] int feedPageSize() const noexcept;
    [[nodiscard]] int contextHydrationLimit() const noexcept;
    [[nodiscard]] bool linkPreviewPrefetchEnabled() const noexcept;
    [[nodiscard]] bool inlineYouTubeEnabled() const noexcept;
    [[nodiscard]] bool bufferNewPosts() const noexcept;
    [[nodiscard]] int pendingNewPostCount() const noexcept;

    Q_INVOKABLE void openIdentity(const QString& username);
    Q_INVOKABLE void selectFeed(const QString& feed);
    Q_INVOKABLE void openChannel(const QString& channelId);
    /** @brief Resolves one Explore query to a profile, channel, or canonical cast target. */
    Q_INVOKABLE void openSearchQuery(const QString& query);
    /** @brief Requests debounced provider-neutral suggestions for the Explore palette. */
    Q_INVOKABLE void requestSearchSuggestions(const QString& query);
    /** @brief Clears suggestions and cancels the active interactive search request. */
    Q_INVOKABLE void clearSearchSuggestions();
    /** @brief Opens the signed-in viewer's in-app Farcaster notifications. */
    Q_INVOKABLE void openNotifications();
    /** @brief Loads the next notification page when available. */
    Q_INVOKABLE void loadMoreNotifications();
    /** @brief Refreshes the notification badge without navigating away from the current surface. */
    Q_INVOKABLE void refreshNotificationBadge();
    /** @brief Marks only the notifications already presented to the user as locally seen. */
    Q_INVOKABLE void markNotificationsRead();
    Q_INVOKABLE void openCast(const QString& castId);
    Q_INVOKABLE void openProfile();
    Q_INVOKABLE void openMyProfile();
    /** @brief Resolves one exact Farcaster username from Explore and opens its profile without retargeting Home. */
    Q_INVOKABLE void openProfileUsername(const QString& username);
    Q_INVOKABLE void openProfileFid(qulonglong fid);
    Q_INVOKABLE void selectProfileFeed(const QString& feed);
    Q_INVOKABLE void openFollowers();
    Q_INVOKABLE void openFollowing();
    Q_INVOKABLE void returnFromSocial();
    Q_INVOKABLE void returnFromDetail();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void retry();
    /** @brief Applies pending refresh rows at the top without losing the current viewport history. */
    Q_INVOKABLE void revealPendingPosts();
    /** @brief Sets the bounded number of casts requested per page (1..100). */
    Q_INVOKABLE void setFeedPageSize(int value);
    /** @brief Sets the bounded number of parent/quote contexts hydrated per page (1..100). */
    Q_INVOKABLE void setContextHydrationLimit(int value);
    /** @brief Enables or disables background OpenGraph/link metadata prefetch for visible cards. */
    Q_INVOKABLE void setLinkPreviewPrefetchEnabled(bool enabled);
    /** @brief Enables or disables the isolated in-app YouTube player. */
    Q_INVOKABLE void setInlineYouTubeEnabled(bool enabled);
    /** @brief Enables or disables X-style buffering of newly refreshed feed rows. */
    Q_INVOKABLE void setBufferNewPosts(bool enabled);
    Q_INVOKABLE void showFeed();
    /** @brief Starts one bounded background discovery read across curated Farcaster art channels. */
    Q_INVOKABLE void requestExplorerArt();
    Q_INVOKABLE void openExternalUrl(const QString& url);
    /** @brief Opens an approved rich-media URL in Genycaster's isolated native web-media host. */
    Q_INVOKABLE void openEmbeddedMedia(const QString& url, const QString& title = {});
    Q_INVOKABLE void copyText(const QString& text);
    /** @brief Selects the approved contextual glyph for the existing Like action without changing reaction semantics. */
    [[nodiscard]] Q_INVOKABLE QString contextualReactionIcon(const QString& text) const;
    Q_INVOKABLE void beginSignIn();
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void openAuthenticationApproval();
    Q_INVOKABLE void provisionSigner();
    Q_INVOKABLE void revokeSigner();
    Q_INVOKABLE void publishCast(const QString& text);
    /** @brief Publishes the exact safe-serialized rich document projection plus semantic mentions. */
    Q_INVOKABLE void publishPreparedCast(const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions, const QVariantList& embeds);
    Q_INVOKABLE int utf8ByteCount(const QString& text) const;
    Q_INVOKABLE void beginComposer();
    Q_INVOKABLE bool prepareReply(const QString& parentCastId);
    Q_INVOKABLE void clearReplyTarget();
    Q_INVOKABLE void publishReply(const QString& text);
    /** @brief Publishes the exact safe-serialized reply projection plus semantic mentions. */
    Q_INVOKABLE void publishPreparedReply(const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions, const QVariantList& embeds);
    /** @brief Publishes an ordered list of normal CastAdd messages as a protocol-native reply chain. */
    Q_INVOKABLE void publishPreparedThread(const QVariantList& casts);
    /** @brief Adds or removes a Like/Recast reaction for one canonical cast target. */
    Q_INVOKABLE void setReaction(
        const QString& kind, qulonglong targetAuthorFid, const QString& targetCastId, bool active);
    /** @brief Adds or removes the authenticated account's follow link to targetFid. */
    Q_INVOKABLE void setFollowing(qulonglong targetFid, bool following);
    /** @brief Deletes one cast only when its author is the authenticated account. */
    Q_INVOKABLE void removeCast(qulonglong authorFid, const QString& castId);

    /** @brief Retries the current surface without bypassing an active capability cooldown. */
    void retryAutomatically();
    /** @brief Rebuilds cached transient status/error strings after a runtime locale switch. */
    void retranslateTransientState();

    /** @brief Commits initialization state on the object's Qt thread. */
    void applyRuntimeState(bool ready, QString errorText = {});
    /** @brief Commits OS/network reachability without conflating provider failures with offline state. */
    void applyNetworkReachability(bool known, bool online);
    /** @brief Commits normalized guest/authenticated client-session state. */
    void applyAuthenticationState(
        bool available,
        QString phase,
        qulonglong fid,
        QString username,
        QString displayName,
        QString avatarUrl,
        QString approvalUrl,
        QString errorText);
    /** @brief Reports an authentication worker failure without replacing a valid account session. */
    void applyAuthenticationError(QString errorText);
    /** @brief Commits normalized protocol-signer readiness without exposing provider details. */
    void applySignerState(
        QString phase, bool busy, bool writeReady, QString statusText, QString errorText);
    /** @brief Commits one durable protocol-write lifecycle plus optional presentation mutation. */
    void applyWriteState(
        QString phase, bool busy, QString statusText, QString errorText, QVariantMap optimisticRow = {});
    /** @brief Replaces account-scoped durable Like/Recast state used to overlay refreshed reads. */
    void applyReactionProjections(QVariantList reactions);
    /** @brief Replaces network-observed active Like/Recast state for the signed-in viewer. */
    void applyViewerReactionSnapshot(QVariantList reactions);
    /** @brief Applies authoritative Like/Recast state for a bounded set of visible casts. */
    void applyViewerReactionDelta(QVariantList reactions);
    /** @brief Replaces account-scoped durable Follow/Unfollow state used to overlay refreshed profiles. */
    void applyFollowingProjections(QVariantList following);
    /** @brief Commits a normalized identity presentation snapshot. */
    void applyIdentity(qulonglong fid, QString displayName, QString username, QVariantMap profile,
        QString status, QString error, QString diagnostic, bool busy, bool stale, bool sourceUnavailable);
    /** @brief Commits transient exact-lookup status without mutating the currently viewed Home identity. */
    void applyLookupState(QString status, QString error, QString diagnostic,
        bool busy, bool stale, bool sourceUnavailable);
    /** @brief Commits provider-neutral Explore suggestions without changing the active navigation surface. */
    void applySearchSuggestions(QString query, QVariantList results, QString errorText, bool busy);
    /** @brief Commits one in-app notifications page without disturbing the active feed cache. */
    void applyNotifications(QVariantList rows, QString next, QString errorText, bool busy, bool append);
    /** @brief Commits a lightweight unseen-notification timestamp snapshot for navigation badges. */
    void applyNotificationBadge(QVariantList observedAtMs, bool busy, bool authoritative = true);
    /** @brief Restores the account-scoped local notification seen watermark from durable settings. */
    void restoreNotificationSeenAt(qulonglong accountFid, qlonglong observedAtMs);
    /** @brief Commits normalized rows for a feed or social surface. */
    void applyList(QString view, QString selector, QVariantList rows, QString next,
        QString status, QString error, QString diagnostic, bool busy, bool stale,
        bool sourceUnavailable, bool paginationRequest = false);
    /** @brief Merges one bounded batch of art-channel discovery rows without replacing the active feed. */
    void applyExplorerArtRows(QVariantList rows, bool busy);
    /** @brief Patches a hydrated parent/quoted cast preview into visible rows without exposing raw ids. */
    void applyReferencedCastPreview(QString castId, QVariantMap preview);
    /** @brief Commits normalized profile or cast detail presentation data. */
    void applyDetail(QString view, QVariantMap detail, QString status, QString error, QString diagnostic,
        bool busy, bool stale, bool sourceUnavailable);
    /** @brief Caches and applies whether a viewed profile follows the signed-in account. */
    void applyProfileFollowsMe(qulonglong fid, bool followsMe);
    /** @brief Applies a read-only relationship to matching current search results.
     * Local follow mutations take precedence; absent results are ignored.
     */
    void applySearchFollowing(qulonglong fid, bool following);
    /** @brief Loads deterministic content for smoke and visual verification only. */
    void applyVerificationFixture();

signals:
    void stateChanged();
    void searchChanged();
    void notificationsChanged();
    void readPreferencesChanged();
    /** @brief Emitted only when OS/network reachability knowledge or online state changes. */
    void networkReachabilityChanged(bool known, bool online);
    void castQueued();
    void identityOpened();
    void identityRequested(QString username, bool allowRecoveryProbe);
    void profileUsernameRequested(QString username, bool allowRecoveryProbe);
    void searchSuggestionsRequested(QString query);
    void searchSuggestionsCancelled();
    void notificationsRequested(QString continuation, bool allowRecoveryProbe);
    void notificationBadgeRequested(bool allowRecoveryProbe);
    void notificationSeenAtChanged(qulonglong accountFid, qlonglong observedAtMs);
    void feedRequested(
        QString feed,
        qulonglong fid,
        QString channelId,
        QString continuation,
        bool allowRecoveryProbe);
    void explorerArtRequested();
    void castRequested(QString castId, bool allowRecoveryProbe);
    void profileRequested(qulonglong fid, bool allowRecoveryProbe);
    void signInRequested();
    void signInCancelRequested();
    void signOutRequested();
    void signerProvisionRequested();
    void signerRevokeRequested();
    void publishCastRequested(QString text, QVariantList mentionFids, QVariantList mentionPositions, QVariantList embeds);
    void publishReplyRequested(QString text, QVariantList mentionFids, QVariantList mentionPositions, QVariantList embeds, qulonglong parentAuthorFid, QString parentCastId);
    void publishThreadRequested(QVariantList casts, qulonglong parentAuthorFid, QString parentCastId);
    void reactionRequested(QString kind, qulonglong targetAuthorFid, QString targetCastId, bool active);
    void followingMutationRequested(qulonglong targetFid, bool following);
    void removeCastRequested(qulonglong authorFid, QString castId);
    void embeddedMediaRequested(QString url, QString title);
    void socialRequested(
        QString kind,
        qulonglong fid,
        QString continuation,
        bool allowRecoveryProbe);

private:
    void beginIntent(QString status);
    void retryCurrent(bool allowRecoveryProbe);
    void persistReadPreference(const QString& key, const QVariant& value);
    [[nodiscard]] bool consumeRecoveryProbe() noexcept;
    bool m_runtimeReady{false};
    bool m_authenticationAvailable{false};
    bool m_isAuthenticated{false};
    QString m_authenticationPhase{QStringLiteral("guest")};
    QString m_authenticationApprovalUrl{};
    QString m_authenticationErrorText{};
    QString m_accountName{};
    QString m_accountUsername{};
    QString m_accountAvatarUrl{};
    qulonglong m_accountFid{};
    QVariantMap m_accountProfile{};
    bool m_writeReady{false};
    QString m_signerPhase{QStringLiteral("signed-out")};
    bool m_signerBusy{false};
    QString m_signerStatusText{};
    QString m_signerErrorText{};
    bool m_writeBusy{false};
    QString m_writePhase{QStringLiteral("idle")};
    QString m_writeStatusText{};
    QString m_writeErrorText{};
    QString m_lastPublishedCastId{};
    qulonglong m_replyParentAuthorFid{};
    QString m_replyParentCastId{};
    QVariantList m_writeRows{};
    QVariantMap m_viewerReactionSnapshot{};
    QVariantMap m_reactionOverrides{};
    QVariantMap m_followOverrides{};
    QVariantMap m_followsMeSnapshot{};
    QHash<qulonglong, QVariantMap> m_profileDetailCache{};
    QHash<QString, QVariantList> m_profileListCache{};
    QHash<QString, QString> m_profileNextCache{};
    QString m_activeView{QStringLiteral("feed")};
    QString m_activeFeed{QStringLiteral("following")};
    QString m_profileFeed{QStringLiteral("user-posts")};
    qulonglong m_profileFid{};
    qulonglong m_socialFid{};
    QString m_socialReturnView{QStringLiteral("feed")};
    QString m_detailReturnView{QStringLiteral("feed")};
    QString m_channelId{};
    QString m_identityName{};
    qulonglong m_identityFid{};
    QString m_statusText{};
    QString m_errorText{};
    QString m_diagnosticText{};
    bool m_isBusy{true};
    bool m_isStale{false};
    bool m_isOffline{false};
    bool m_sourceUnavailable{false};
    bool m_networkReachabilityKnown{false};
    bool m_hasReadActivity{false};
    bool m_feedStarted{false};
    bool m_shouldOpenFollowingAfterIdentity{false};
    bool m_allowRecoveryProbeForNextIntent{false};
    QSettings* m_settings{nullptr};
    int m_feedPageSize{20};
    int m_contextHydrationLimit{40};
    bool m_linkPreviewPrefetchEnabled{true};
    bool m_inlineYouTubeEnabled{true};
    bool m_bufferNewPosts{true};
    bool m_bufferNextFeedRefresh{false};
    QVariantList m_pendingFeedRows{};
    QString m_next{};
    QVariantList m_rows{};
    QVariantList m_explorerArtRows{};
    bool m_explorerArtBusy{false};
    QVariantMap m_detail{};
    QVariantList m_searchResults{};
    QString m_searchQuery{};
    QString m_searchErrorText{};
    bool m_searchBusy{false};
    QVariantList m_notificationRows{};
    QString m_notificationNext{};
    QString m_notificationsErrorText{};
    bool m_notificationsBusy{false};
    qlonglong m_notificationSeenAtMs{0};
    int m_notificationUnreadCount{0};
    bool m_notificationBadgeBusy{false};
};
