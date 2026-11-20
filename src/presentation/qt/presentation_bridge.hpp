/*!
 * @file        src/presentation/qt/presentation_bridge.hpp
 * @brief       Qt application presentation facade.
 *
 * @details
 * Connects verified session state, feed operations and social actions to
 * QML without making the interface own protocol adapters.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

/** @brief QML presentation facade for verified-session state and feature-scoped models. */
class QSettings;

class PresentationBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString architectureState READ architectureState CONSTANT)
    Q_PROPERTY(bool isStrictSelfHosted READ isStrictSelfHosted CONSTANT)
    Q_PROPERTY(bool diagnosticsEnabled READ diagnosticsEnabled CONSTANT)
    Q_PROPERTY(bool runtimeReady READ runtimeReady NOTIFY runtimeStateChanged)
    Q_PROPERTY(bool authenticationAvailable READ authenticationAvailable NOTIFY authenticationStateChanged)
    Q_PROPERTY(bool initialSetupRequired READ initialSetupRequired NOTIFY setupStateChanged)
    Q_PROPERTY(bool authenticationStateObserved READ authenticationStateObserved NOTIFY authenticationStateChanged)
    Q_PROPERTY(bool initialSetupCompleted READ initialSetupCompleted NOTIFY setupStateChanged)
    Q_PROPERTY(bool legalConsentAccepted READ legalConsentAccepted NOTIFY setupStateChanged)
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY authenticationStateChanged)
    Q_PROPERTY(bool authenticationBusy READ authenticationBusy NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString authenticationPhase READ authenticationPhase NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString authenticationRoute READ authenticationRoute NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString preferredNetwork READ preferredNetwork NOTIFY preferredNetworkChanged)
    Q_PROPERTY(QString independentNetworkKey READ independentNetworkKey CONSTANT)
    Q_PROPERTY(QString hostedNetworkKey READ hostedNetworkKey CONSTANT)
    Q_PROPERTY(bool farcasterReadConfigured READ farcasterReadConfigured NOTIFY farcasterReadConfigurationChanged)
    Q_PROPERTY(QString farcasterReadEndpoint READ farcasterReadEndpoint NOTIFY farcasterReadConfigurationChanged)
    Q_PROPERTY(bool authenticationApprovalAvailable READ authenticationApprovalAvailable NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString authenticationErrorText READ authenticationErrorText NOTIFY authenticationStateChanged)
    Q_PROPERTY(QVariantMap setupConnections READ setupConnections NOTIFY setupConnectionsChanged)
    Q_PROPERTY(QString accountIdentityIssuer READ accountIdentityIssuer NOTIFY authenticationStateChanged)
    Q_PROPERTY(bool farcasterCustodyAccount READ farcasterCustodyAccount NOTIFY authenticationStateChanged)
    Q_PROPERTY(bool nativeCustodyAccount READ nativeCustodyAccount NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString accountIdentityBadge READ accountIdentityBadge NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString accountUsername READ accountUsername NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString accountAvatarUrl READ accountAvatarUrl NOTIFY authenticationStateChanged)
    Q_PROPERTY(qulonglong accountFid READ accountFid NOTIFY authenticationStateChanged)
    Q_PROPERTY(QString accountFidText READ accountFidText NOTIFY authenticationStateChanged)
    Q_PROPERTY(bool ownProfileNavigation READ ownProfileNavigation NOTIFY stateChanged)
    Q_PROPERTY(bool ownProfileReadReady READ ownProfileReadReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap accountProfile READ accountProfile NOTIFY authenticationStateChanged)
    Q_PROPERTY(bool writeReady READ writeReady NOTIFY stateChanged)
    Q_PROPERTY(QString signerPhase READ signerPhase NOTIFY stateChanged)
    Q_PROPERTY(bool signerBusy READ signerBusy NOTIFY stateChanged)
    Q_PROPERTY(bool nativeCustodySignerAvailable READ nativeCustodySignerAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString signerStatusText READ signerStatusText NOTIFY stateChanged)
    Q_PROPERTY(QString signerErrorText READ signerErrorText NOTIFY stateChanged)
    Q_PROPERTY(bool writeBusy READ writeBusy NOTIFY stateChanged)
    Q_PROPERTY(QString writePhase READ writePhase NOTIFY stateChanged)
    Q_PROPERTY(QString writeStatusText READ writeStatusText NOTIFY stateChanged)
    Q_PROPERTY(QString writeErrorText READ writeErrorText NOTIFY stateChanged)
    Q_PROPERTY(QString lastPublishedCastId READ lastPublishedCastId NOTIFY stateChanged)
    Q_PROPERTY(QString lastRemovedCastId READ lastRemovedCastId NOTIFY stateChanged)
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
    Q_PROPERTY(bool cachedFeedHasMore READ cachedFeedHasMore NOTIFY stateChanged)
    Q_PROPERTY(bool hasReadActivity READ hasReadActivity NOTIFY stateChanged)
    Q_PROPERTY(bool feedStarted READ feedStarted NOTIFY stateChanged)
    Q_PROPERTY(bool communityFallback READ communityFallback NOTIFY stateChanged)
    Q_PROPERTY(bool localDerivedRecent READ localDerivedRecent NOTIFY stateChanged)
    Q_PROPERTY(bool localDerivedFollowing READ localDerivedFollowing NOTIFY stateChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList explorerArtRows READ explorerArtRows NOTIFY stateChanged)
    Q_PROPERTY(bool explorerArtBusy READ explorerArtBusy NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY stateChanged)
    Q_PROPERTY(QStringList bookmarkedCastIds READ bookmarkedCastIds NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList bookmarkedCasts READ bookmarkedCasts NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList topicPreviewCasts READ topicPreviewCasts NOTIFY topicPreviewCastsChanged)
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
    Q_PROPERTY(QVariantList followingStoryProfiles READ followingStoryProfiles NOTIFY followingStoriesChanged)
    Q_PROPERTY(int pendingNewPostCount READ pendingNewPostCount NOTIFY stateChanged)

public:
    /**
     * @brief Creates the presentation adapter with optional Qt ownership and persisted read preferences.
     * @param parent Optional QObject owner.
     * @param settings Optional borrowed settings store; when supplied it must outlive this bridge.
     */
    explicit PresentationBridge(QObject* parent = nullptr, QSettings* settings = nullptr,
        QString independentNetworkKey = QStringLiteral("independent"),
        QString hostedNetworkKey = QStringLiteral("hosted"));
    [[nodiscard]] QString architectureState() const;
    [[nodiscard]] bool isStrictSelfHosted() const noexcept;
    [[nodiscard]] bool diagnosticsEnabled() const noexcept;
    [[nodiscard]] bool runtimeReady() const noexcept;
    [[nodiscard]] bool authenticationAvailable() const noexcept;
    [[nodiscard]] bool initialSetupRequired() const noexcept;
    [[nodiscard]] bool initialSetupCompleted() const noexcept;
    [[nodiscard]] bool legalConsentAccepted() const noexcept;
    [[nodiscard]] bool isAuthenticated() const noexcept;
    [[nodiscard]] bool authenticationBusy() const noexcept;
    [[nodiscard]] QString authenticationPhase() const;
    [[nodiscard]] bool authenticationStateObserved() const noexcept;

    /** Resolve initial browser identity restoration before exposing onboarding. */
    void finishBrowserAccountRestore() noexcept;
    [[nodiscard]] QString authenticationRoute() const;

    /** @brief Preferred connection route, independent of account identity and signing authority. */
    [[nodiscard]] QString preferredNetwork() const;

    /** @brief Read-only route keys supplied by executable composition; no provider DTOs. */
    [[nodiscard]] QString independentNetworkKey() const;
    [[nodiscard]] QString hostedNetworkKey() const;
    [[nodiscard]] bool farcasterReadConfigured() const noexcept;
    [[nodiscard]] QString farcasterReadEndpoint() const;
    void setFarcasterReadConfigured(bool configured);
    void setFarcasterReadEndpoint(QString endpoint);

    /** @brief Persist one of the two composition-configured provider-neutral route keys. */
    Q_INVOKABLE bool setPreferredNetwork(const QString& network);
    [[nodiscard]] bool authenticationApprovalAvailable() const noexcept;
    [[nodiscard]] QString authenticationErrorText() const;

    /** @brief Advisory states and response times of the account setup services. */
    [[nodiscard]] QVariantMap setupConnections() const;
    [[nodiscard]] QString accountIdentityIssuer() const;

    /** @brief Trusted issuer classification, not a QML comparison of provider IDs. */
    [[nodiscard]] bool nativeCustodyAccount() const noexcept;

    /** @brief Whether the active identity was recovered through the Farcaster custody registry. */
    [[nodiscard]] bool farcasterCustodyAccount() const noexcept;

    /** @brief Localized identity badge for the authenticated provider. */
    [[nodiscard]] QString accountIdentityBadge() const;
    [[nodiscard]] QString accountName() const;
    [[nodiscard]] QString accountUsername() const;
    [[nodiscard]] QString accountAvatarUrl() const;
    [[nodiscard]] qulonglong accountFid() const noexcept;
    [[nodiscard]] QString accountFidText() const { return QString::number(accountFid()); }
    [[nodiscard]] bool ownProfileReadReady() const noexcept { return m_ownProfileReadReady; }
    [[nodiscard]] bool ownProfileNavigation() const noexcept {
        return m_isAuthenticated && m_accountFid != 0 && m_profileIsOwnAccount
            && m_activeView == QStringLiteral("profile") && m_profileFid == m_accountFid;
    }

    /** @brief Returns the cached provider-neutral profile presentation for the authenticated account. */
    [[nodiscard]] QVariantMap accountProfile() const;
    [[nodiscard]] bool writeReady() const noexcept;
    [[nodiscard]] QString signerPhase() const;
    [[nodiscard]] bool signerBusy() const noexcept;
    [[nodiscard]] bool nativeCustodySignerAvailable() const noexcept { return m_nativeCustodySignerAvailable; }
    void setNativeCustodySignerAvailable(bool available);
    [[nodiscard]] QString signerStatusText() const;
    [[nodiscard]] QString signerErrorText() const;
    [[nodiscard]] bool writeBusy() const noexcept;
    [[nodiscard]] QString writePhase() const;
    [[nodiscard]] QString writeStatusText() const;
    [[nodiscard]] QString writeErrorText() const;
    [[nodiscard]] QString lastPublishedCastId() const;
    [[nodiscard]] QString lastRemovedCastId() const;
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
    [[nodiscard]] bool cachedFeedHasMore() const noexcept;
    [[nodiscard]] bool hasReadActivity() const noexcept;
    [[nodiscard]] bool feedStarted() const noexcept;
    [[nodiscard]] QVariantList rows() const;
    [[nodiscard]] QVariantList explorerArtRows() const;

    /** @brief Actual followed identities, never inferred from public cast authors. */
    [[nodiscard]] QVariantList followingStoryProfiles() const;
    [[nodiscard]] bool explorerArtBusy() const noexcept;
    [[nodiscard]] QVariantMap detail() const;
    [[nodiscard]] QStringList bookmarkedCastIds() const;
    [[nodiscard]] QVariantList bookmarkedCasts() const;

    /** Local-first reading list; owner-scoped mutations are replicated by the
     * Genycaster service (not by the Farcaster protocol). */
    Q_INVOKABLE void toggleBookmark(const QVariantMap& cast);
    Q_INVOKABLE void removeBookmark(const QString& castId);
    Q_INVOKABLE void setBookmarkPriority(const QString& castId, int priority);

    /** Background, navigation-free restoration of rich saved cast models. */
    Q_INVOKABLE void hydrateBookmarkedCasts(const QStringList& castIds);
    void applyHydratedBookmark(QString castId, QVariantMap cast);

    /** @brief Bounded, navigation-free previews of server-indexed topic casts; never modifies bookmarks. */
    [[nodiscard]] QVariantList topicPreviewCasts() const;
    Q_INVOKABLE void hydrateTopicCasts(const QStringList& castIds);
    void applyHydratedTopicCast(QString castId, QVariantMap cast);
    [[nodiscard]] QVariantMap bookmarkEntry(const QString& castId) const;
    void applyCloudBookmarkChanges(const QVariantList& changes);
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

    /** @brief Emits bounded protocol-native UserDataAdd changes; never writes Genycaster-only extras. */
    Q_INVOKABLE bool updateProtocolProfile(const QVariantMap& fields);
    Q_INVOKABLE void openMyProfile();

    /** @brief Resolves one exact Farcaster username from Explore and opens its profile without retargeting Home. */
    Q_INVOKABLE void openProfileUsername(const QString& username);
    Q_INVOKABLE void openProfileFid(qulonglong fid);
    Q_INVOKABLE void selectProfileFeed(const QString& feed);
    Q_INVOKABLE void openFollowers();
    Q_INVOKABLE void openFollowing();
    Q_INVOKABLE void returnFromSocial();
    Q_INVOKABLE void returnFromDetail();

    /** Explicit primary navigation discards nested cast/profile breadcrumbs. */
    Q_INVOKABLE void clearDetailHistory();
    Q_INVOKABLE void loadMore();

    /** @brief Update independent local pagination after a cached/fresh read. */
    void applyLocalFeedWindowProgress(bool hasMore, qulonglong nextOffset);

    /** @brief Finish an unsuccessful local window without removing cached rows. */
    void applyLocalFeedWindowFailure(QString error);
    Q_INVOKABLE void retry();

    /** @brief Explicitly refreshes the first feed page without buffering newer casts.
     * Preserves the user's new-post buffering preference for ordinary background reads.
     */
    Q_INVOKABLE void refreshLatestFeed();

    /** @brief Retains one account-scoped visible cast reaction interest; deduplicated in composition. */
    Q_INVOKABLE void retainViewerReactionInterest(QString castId, qulonglong authorFid, qulonglong viewerFid);

    /** @brief Releases one consumer on pooling, navigation or account change. */
    Q_INVOKABLE void releaseViewerReactionInterest(QString castId, qulonglong viewerFid);

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

    /** @brief Refreshes only the signed-in account's Follow graph for StoryStrip. */
    Q_INVOKABLE void refreshFollowingStories();

    /** @brief Applies an isolated Following graph read without navigating away from Explorer. */
    void applyFollowingStoryProfiles(qulonglong viewerFid, QVariantList profiles, bool finished, bool successful);

    /** @brief Recent, clickable casts from the same viewer's Following timeline. */
    void applyFollowingStoryActivities(qulonglong viewerFid, QVariantList activities, bool finished, bool successful);
    Q_INVOKABLE void openExternalUrl(const QString& url);

    /** @brief Open the website's verified native release selector on WASM.
     * Native builds do not open a browser download overlay.
     */
    Q_INVOKABLE void openBrowserNativeDownloads();

    /** @brief Opens an approved rich-media URL in Genycaster's isolated native web-media host. */
    Q_INVOKABLE void openEmbeddedMedia(const QString& url, const QString& title = {});

    /** @brief Dispatch a clipboard copy; native platforms verify read-back. */
    Q_INVOKABLE bool copyText(const QString& text);

    /** @brief Active HTTPS base for deployment-independent public permalinks. */
    [[nodiscard]] Q_INVOKABLE QString publicShareBaseUrl() const;

    /** @brief Returns an HTTPS permalink to the canonical cast hash, or empty on invalid input. */
    [[nodiscard]] Q_INVOKABLE QString castShareUrl(const QString& castId) const;

    /** @brief Returns an HTTPS permalink to a profile FID, or empty for FID zero. */
    [[nodiscard]] Q_INVOKABLE QString profileShareUrl(qulonglong fid) const;

    /** @brief Returns a client-independent experimental Farcaster URI for a cast. */
    [[nodiscard]] Q_INVOKABLE QString farcasterCastUri(const QString& castId, qulonglong fid) const;

    /** @brief Returns a client-independent experimental Farcaster URI for a profile. */
    [[nodiscard]] Q_INVOKABLE QString farcasterProfileUri(qulonglong fid) const;

    /** @brief Returns a Farcaster web client URL when the username and hash are valid. */
    [[nodiscard]] Q_INVOKABLE QString farcasterWebCastUrl(const QString& castId, const QString& username) const;

    /** @brief Returns a Farcaster web client profile URL when the username is valid. */
    [[nodiscard]] Q_INVOKABLE QString farcasterWebProfileUrl(const QString& username) const;

    /** @brief Quorum Expo /feed cast route; usable only when Quorum is installed. */
    [[nodiscard]] Q_INVOKABLE QString quorumCastUri(const QString& castId) const;

    /** @brief Open a Farcaster cast using its HTTPS universal/app link with web fallback. */
    Q_INVOKABLE void openFarcasterCast(const QString& castId, const QString& username);

    /** @brief Open a Farcaster profile using its HTTPS universal/app link with web fallback. */
    Q_INVOKABLE void openFarcasterProfile(const QString& username);

    /** @brief Open the verified Quorum app cast URI, falling back to its web client. */
    Q_INVOKABLE void openQuorumCast(const QString& castId);

    /** @brief Shares an HTTPS URL with the browser's OS share sheet, falling back to copy. */
    Q_INVOKABLE void shareUrl(const QString& url);

    /** @brief Validates and routes an incoming Genycaster/Farcaster cast/profile link. */
    [[nodiscard]] Q_INVOKABLE bool openSharedLink(const QString& url);

    /** @brief Selects the approved contextual glyph for the existing Like action without changing reaction semantics. */
    [[nodiscard]] Q_INVOKABLE QString contextualReactionIcon(const QString& text) const;

    /** @brief Records explicit acceptance of the current legal bundle after the review gate. */
    Q_INVOKABLE bool acceptLegalConsent(
        bool termsAccepted,
        bool privacyAcknowledged,
        bool reviewCompleted);

    /** @brief Completes first-run setup after legal consent and a verified account session exist. */
    Q_INVOKABLE void completeInitialSetup();

    /** @brief Requests a read-only account connection check without recovery material. */
    Q_INVOKABLE void checkSetupConnections();

    /** @brief Starts canonical Genyleap/OpenProof sign-in. */
    Q_INVOKABLE void beginSignIn();

    /** @brief Starts the direct Farcaster onboarding route through OpenProof. */
    Q_INVOKABLE void beginFarcasterSignIn();

    /** @brief Browser direct Farcaster recovery; no Sign In ceremony. */
    Q_INVOKABLE void beginRecoverySetup();

    /** Browser-only protocol-scoped onboarding. No recovery material enters Qt. */
    Q_INVOKABLE void beginBrowserProtocolSetup(QString protocol, bool createAccount);

    /** @brief Native-only explicit custody recovery, with a transient input.
     * Never retain the phrase in the presentation model or settings. */
    Q_INVOKABLE void beginNativeRecoverySetup(QString recoveryWords);

    /** Recover HyperSnap-native FID through its own registry, not IdRegistry. */
    Q_INVOKABLE void beginNativeHyperSnapRecoverySetup(QString recoveryWords, bool authorizeSigner);

    /** Recover an onchain Farcaster FID without an OpenProof account grant. */
    Q_INVOKABLE void beginNativeFarcasterRecoverySetup(QString recoveryWords, bool authorizeSigner);
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void openAuthenticationApproval();

    /** @brief Rechecks the saved native signer without requesting custody approval. */
    Q_INVOKABLE void refreshSigner();
    Q_INVOKABLE void provisionSigner();

    /** Authorize a HyperSnap-native signer from an explicit, transient local
     * recovery phrase. Native runtime verifies custody against shard 0 first. */
    Q_INVOKABLE void authorizeNativeHyperSnapSigner(QString recoveryWords);

    /** @brief Requests one local signer permit after verifying the current Farcaster FID on Optimism. */
    Q_INVOKABLE void authorizeNativeFarcasterSigner(QString recoveryWords);
    Q_INVOKABLE void revokeSigner();
    Q_INVOKABLE void publishCast(const QString& text);

    /** @brief Publishes the exact safe-serialized rich document projection plus semantic mentions. */
    Q_INVOKABLE void publishPreparedCast(const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions, const QVariantList& embeds, const QString& parentUrl = {});
    Q_INVOKABLE int utf8ByteCount(const QString& text) const;
    Q_INVOKABLE void beginComposer();
    Q_INVOKABLE bool prepareReply(const QString& parentCastId);

    /** @brief Prepares a reply to a canonical public Cast with a provider-supplied author FID.
     * Uses the selected reply's identity when the target was not part of the
     * current top-level feed, including nested conversation replies.
     * Invalid hashes/FIDs are rejected without modifying the active target.
     */
    Q_INVOKABLE bool prepareReplyToCast(const QString& parentCastId, double parentAuthorFid);
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

    /** @brief Publishes a canonical Block/Unblock against the selected protocol node. */
    Q_INVOKABLE void setBlocked(qulonglong targetFid, bool blocked);

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

    /** @brief Commits normalized signed-out/authenticated client-session state. */
    void applyAuthenticationState(
        bool available,
        QString phase,
        qulonglong fid,
        QString username,
        QString displayName,
        QString avatarUrl,
        QString identityIssuer,
        QString approvalUrl,
        QString errorText);

    /** @brief Replaces account presentation with the Farcaster profile resolved from the verified session FID. */
    void applyAccountProfile(qulonglong fid, QString displayName, QString username, QString avatarUrl, QVariantMap profile);

    /** @brief Reports an authentication worker failure without replacing a valid account session. */
    void applyAuthenticationError(QString errorText);

    /** @brief Publishes one advisory result, with latency in milliseconds when available. */
    void applySetupConnection(const QString& service, QString state, int elapsedMs = -1);

    /** @brief Commits normalized protocol-signer readiness without exposing provider details. */
    void applySignerState(
        QString phase, bool busy, bool writeReady, QString statusText, QString errorText);

    /** @brief Commits one durable protocol-write lifecycle plus optional presentation mutation. */
    void applyWriteState(
        QString phase, bool busy, QString statusText, QString errorText, QVariantMap optimisticRow = {});

    /** @brief Records confirmed browser writes without forging optimistic feed rows. */
    void applyVerifiedBrowserWrite(QString hash, bool publishedCast);

    /** @brief Rehydrates locally saved CastAdds after restart without altering write lifecycle. */
    void applyCastProjections(qulonglong accountFid, QVariantList casts);

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
    [[nodiscard]] bool localDerivedRecent() const noexcept { return m_localDerivedRecent; }
    void setLocalDerivedRecent(const bool active) {
        if (m_localDerivedRecent == active) return;
        m_localDerivedRecent = active;
        emit stateChanged();
    }
    [[nodiscard]] bool localDerivedFollowing() const noexcept { return m_localDerivedFollowing; }
    void setLocalDerivedFollowing(const bool active) {
        if (m_localDerivedFollowing == active) return;
        m_localDerivedFollowing = active;
        emit stateChanged();
    }
    [[nodiscard]] bool communityFallback() const noexcept { return m_communityFallback; }
    void setCommunityFallback(bool active) {
        if (m_communityFallback == active) return;
        m_communityFallback = active;
        emit stateChanged();
    }
    void applyList(QString view, QString selector, QVariantList rows, QString next,
        QString status, QString error, QString diagnostic, bool busy, bool stale,
        bool sourceUnavailable, bool paginationRequest = false,
        bool boundedLocalSnapshot = false);

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

    /** @brief Records confirmed protocol Link(type=block) state by direction. */
    void applyProfileBlockRelation(qulonglong fid, bool outgoing, bool blocked);

    /** @brief Loads deterministic content for smoke and visual verification only. */
    void applyVerificationFixture();

signals:
    /** @brief Runtime bootstrap readiness changed; isolated from ordinary feed invalidation. */
    void runtimeStateChanged();

    /** @brief Authentication/account presentation changed; isolated from feed/write invalidation. */
    void authenticationStateChanged();
    void preferredNetworkChanged(QString network);
    void farcasterReadConfigurationChanged();

    /** @brief First-run setup completion or authentication gate visibility changed. */
    void setupStateChanged();
    void stateChanged();

    /** @brief Feed/profile row payload, order or identity changed (not just read status). */
    void rowsChanged();
    void searchChanged();
    void notificationsChanged();
    void bookmarksChanged();
    void bookmarkMutated(const QString& castId, bool isDeleted);
    void bookmarkHydrationRequested(QStringList castIds);
    void topicHydrationRequested(QStringList castIds);
    void topicPreviewCastsChanged();
    void followingStoriesChanged();
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
    void localFeedWindowRequested(QString feed, qulonglong fid, QString channelId,
                                  qulonglong offset, qulonglong limit);
    void explorerArtRequested();
    void followingStoriesRequested(qulonglong viewerFid);
    void followingStoryActivitiesRequested(qulonglong viewerFid);
    void castRequested(QString castId, bool allowRecoveryProbe);
    void viewerReactionInterestRetained(QString castId, qulonglong authorFid, qulonglong viewerFid);
    void viewerReactionInterestReleased(QString castId, qulonglong viewerFid);
    void profileRequested(qulonglong fid, bool allowRecoveryProbe);

    /** Profile-only routing affinity for the verified account, never user settings. */
    void ownProfileReadAffinityRequested(bool ownProfile, bool nativeCustody);
    void signInRequested(QString route);

    /** @brief Requests the runtime to start the advisory setup checks. */
    void setupConnectionCheckRequested();

    /** @brief Indicates that an advisory setup connection state changed. */
    void setupConnectionsChanged();
    void browserProtocolSetupRequested(QString protocol, bool createAccount);

    /** A one-shot phrase delivered to the native recovery executor, never to a provider. */
    void nativeRecoveryRequested(QString recoveryWords);
    void nativeHyperSnapRecoveryRequested(QString recoveryWords, bool authorizeSigner);
    void nativeFarcasterRecoveryRequested(QString recoveryWords, bool authorizeSigner);
    void signInCancelRequested();
    void signOutRequested();
    void signerRefreshRequested();
    void signerProvisionRequested();

    /** One-use recovery words; never exposed through a Q_PROPERTY or stored. */
    void nativeHyperSnapSignerAuthorizationRequested(QString recoveryWords);
    void nativeFarcasterSignerAuthorizationRequested(QString recoveryWords);
    void signerRevokeRequested();
    void publishCastRequested(QString text, QVariantList mentionFids, QVariantList mentionPositions, QVariantList embeds, QString parentUrl);
    void publishReplyRequested(QString text, QVariantList mentionFids, QVariantList mentionPositions, QVariantList embeds, qulonglong parentAuthorFid, QString parentCastId);
    void publishThreadRequested(QVariantList casts, qulonglong parentAuthorFid, QString parentCastId);
    void reactionRequested(QString kind, qulonglong targetAuthorFid, QString targetCastId, bool active);
    void followingMutationRequested(qulonglong targetFid, bool following);
    void blockingMutationRequested(qulonglong targetFid, bool blocked);
    void protocolProfileUpdateRequested(QVariantMap fields);

    /** @brief Terminal confirmation for the user's protocol profile operation only. */
    void protocolProfileWriteCompleted(bool confirmed, QString message);
    void removeCastRequested(qulonglong authorFid, QString castId);
    void embeddedMediaRequested(QString url, QString title);
    void socialRequested(
        QString kind,
        qulonglong fid,
        QString continuation,
        bool allowRecoveryProbe);

private:
    bool beginSignerRecovery(QString& recoveryWords);
    void beginIntent(QString status);
    void retryCurrent(bool allowRecoveryProbe);
    void persistReadPreference(const QString& key, const QVariant& value);
    void resetInitialSetupProgress();
    [[nodiscard]] bool consumeRecoveryProbe() noexcept;
    bool m_runtimeReady{false};
    bool m_authenticationAvailable{false};
    bool m_initialSetupCompleted{false};
    QVariantMap m_setupConnections{};
    bool m_showSetupAfterSignOut{false};
    bool m_signOutRequestedByUser{false};
    bool m_legalConsentAccepted{false};
    bool m_setupBypassedForVerification{false};
    bool m_isAuthenticated{false};
    QString m_authenticationPhase{QStringLiteral("signed-out")};
    bool m_authenticationStateObserved{false};
#if defined(Q_OS_WASM)
    bool m_browserAccountRestorePending{true};
#endif
    QString m_authenticationRoute{QStringLiteral("openproof")};
    bool m_hyperSnapRecoveryRequest{false};
    bool m_hyperSnapRecoverySignerConsent{false};
    bool m_farcasterRecoveryRequest{false};
    bool m_farcasterRecoverySignerConsent{false};
    QString m_independentNetworkKey{QStringLiteral("independent")};
    QString m_hostedNetworkKey{QStringLiteral("hosted")};
    QString m_preferredNetwork{};
    bool m_farcasterReadConfigured{false};
    QString m_farcasterReadEndpoint{};
    QString m_authenticationApprovalUrl{};
    QString m_authenticationErrorText{};
    QString m_accountIdentityIssuer{};
    QString m_accountName{};
    QString m_accountUsername{};
    QString m_accountAvatarUrl{};
    qulonglong m_accountFid{};
    QVariantMap m_accountProfile{};
    bool m_writeReady{false};
    QString m_signerPhase{QStringLiteral("signed-out")};
    bool m_signerBusy{false};
    bool m_nativeCustodySignerAvailable{false};
    QString m_signerStatusText{};
    QString m_signerErrorText{};
    bool m_writeBusy{false};
    QString m_writePhase{QStringLiteral("idle")};
    QString m_writeStatusText{};
    QString m_writeErrorText{};
    QString m_lastPublishedCastId{};
    QString m_lastRemovedCastId{};
    qulonglong m_replyParentAuthorFid{};
    QString m_replyParentCastId{};
    QVariantList m_writeRows{};
    QVariantMap m_viewerReactionSnapshot{};
    // Only complete per-target reads establish a known negative reaction.
    // Distinguish first negative evidence from an unchanged negative result.
    QHash<QString, quint8> m_viewerReactionKnownStates{};
    QVariantMap m_reactionOverrides{};
    QVariantMap m_followOverrides{};
    // Account-scoped verified block link evidence, never inferred from 404s
    // of an unrelated endpoint or the absence of a profile.
    QVariantMap m_blockByMeSnapshot{};
    QVariantMap m_blocksMeSnapshot{};
    QVariantMap m_blockOverrides{};
    QVariantList m_followingStoryProfiles{};
    QHash<qulonglong, QVariantMap> m_followingStoryActivity{};
    qulonglong m_followingStoriesViewerFid{};
    bool m_followingStoriesLoaded{false};
    bool m_followingStoriesLoading{false};
    bool m_followingActivityLoaded{false};
    bool m_followingActivityLoading{false};
    qint64 m_followingActivityCheckedAtMs{};
    qint64 m_followingStoriesCheckedAtMs{};
    QVariantMap m_followsMeSnapshot{};
    QHash<qulonglong, QVariantMap> m_profileDetailCache{};
    QHash<QString, QVariantList> m_profileListCache{};
    QHash<QString, QString> m_profileNextCache{};
    QString m_activeView{QStringLiteral("feed")};
    QString m_activeFeed{QStringLiteral("following")};
    QString m_profileFeed{QStringLiteral("user-posts")};
    qulonglong m_profileFid{};
    bool m_profileIsOwnAccount{false};
    bool m_ownProfileReadReady{false};
    bool m_socialReturnOwnProfile{false};
    qulonglong m_socialFid{};
    QString m_socialReturnView{QStringLiteral("feed")};
    struct DetailLocation {
        QString view{};
        QString castId{};
        qulonglong profileFid{};
        QString profileUsername{};
        QString profileFeed{};
    };
    void rememberDetailLocation();
    std::vector<DetailLocation> m_detailHistory{};
    QString m_currentCastId{};
    QString m_currentProfileUsername{};
    bool m_restoringDetailHistory{false};
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
    bool m_communityFallback{false};
    bool m_localDerivedRecent{false};
    bool m_localDerivedFollowing{false};
    bool m_shouldOpenFollowingAfterIdentity{false};
    bool m_allowRecoveryProbeForNextIntent{false};
    QSettings* m_settings{nullptr};
    int m_feedPageSize{20};
    int m_contextHydrationLimit{40};
    bool m_linkPreviewPrefetchEnabled{true};
    bool m_inlineYouTubeEnabled{true};
    bool m_bufferNewPosts{true};
    bool m_bufferNextFeedRefresh{false};
    bool m_feedSourceSwitchPending{false};
    QVariantList m_pendingFeedRows{};
    QString m_next{};
    bool m_cachedFeedHasMore{false};
    bool m_localWindowPending{false};
    qulonglong m_localNextOffset{0};
    QVariantList m_rows{};
    QVariantList m_explorerArtRows{};
    bool m_explorerArtBusy{false};
    QVariantMap m_detail{};
    QVariantList m_bookmarkedCasts{};
    QVariantList m_topicPreviewCasts{};
    // Short-lived context hydrations are shared by Feed and Cast detail.
    // A late cache-first snapshot must never erase an already resolved quote.
    QHash<QString, QVariantMap> m_referencedPreviewCache{};
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
