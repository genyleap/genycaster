// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/** @brief Thin QML adapter for guest-first reads and verified client-session state. */
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
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY stateChanged)

public:
    /** @brief Creates the presentation adapter with optional Qt ownership. */
    explicit PresentationBridge(QObject* parent = nullptr);
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
    [[nodiscard]] QVariantMap detail() const;

    Q_INVOKABLE void openIdentity(const QString& username);
    Q_INVOKABLE void selectFeed(const QString& feed);
    Q_INVOKABLE void openChannel(const QString& channelId);
    Q_INVOKABLE void openCast(const QString& castId);
    Q_INVOKABLE void openProfile();
    Q_INVOKABLE void openMyProfile();
    Q_INVOKABLE void openProfileFid(qulonglong fid);
    Q_INVOKABLE void selectProfileFeed(const QString& feed);
    Q_INVOKABLE void openFollowers();
    Q_INVOKABLE void openFollowing();
    Q_INVOKABLE void returnFromSocial();
    Q_INVOKABLE void returnFromDetail();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void showFeed();
    Q_INVOKABLE void openExternalUrl(const QString& url);
    Q_INVOKABLE void beginSignIn();
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void openAuthenticationApproval();
    Q_INVOKABLE void provisionSigner();
    Q_INVOKABLE void revokeSigner();
    Q_INVOKABLE void publishCast(const QString& text);
    Q_INVOKABLE void beginComposer();
    Q_INVOKABLE bool prepareReply(const QString& parentCastId);
    Q_INVOKABLE void clearReplyTarget();
    Q_INVOKABLE void publishReply(const QString& text);
    /** @brief Adds or removes a Like/Recast reaction for one canonical cast target. */
    Q_INVOKABLE void setReaction(
        const QString& kind, qulonglong targetAuthorFid, const QString& targetCastId, bool active);
    /** @brief Adds or removes the authenticated account's follow link to targetFid. */
    Q_INVOKABLE void setFollowing(qulonglong targetFid, bool following);
    /** @brief Deletes one cast only when its author is the authenticated account. */
    Q_INVOKABLE void removeCast(qulonglong authorFid, const QString& castId);

    /** @brief Retries the current surface without bypassing an active capability cooldown. */
    void retryAutomatically();

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
    /** @brief Replaces account-scoped durable Follow/Unfollow state used to overlay refreshed profiles. */
    void applyFollowingProjections(QVariantList following);
    /** @brief Commits a normalized identity presentation snapshot. */
    void applyIdentity(qulonglong fid, QString displayName, QString username,
        QString status, QString error, QString diagnostic, bool busy, bool stale, bool sourceUnavailable);
    /** @brief Commits normalized rows for a feed or social surface. */
    void applyList(QString view, QString selector, QVariantList rows, QString next,
        QString status, QString error, QString diagnostic, bool busy, bool stale, bool sourceUnavailable);
    /** @brief Commits normalized profile or cast detail presentation data. */
    void applyDetail(QString view, QVariantMap detail, QString status, QString error, QString diagnostic,
        bool busy, bool stale, bool sourceUnavailable);
    /** @brief Loads deterministic content for smoke and visual verification only. */
    void applyVerificationFixture();

signals:
    void stateChanged();
    /** @brief Emitted only when OS/network reachability knowledge or online state changes. */
    void networkReachabilityChanged(bool known, bool online);
    void castQueued();
    void identityOpened();
    void identityRequested(QString username, bool allowRecoveryProbe);
    void feedRequested(
        QString feed,
        qulonglong fid,
        QString channelId,
        QString continuation,
        bool allowRecoveryProbe);
    void castRequested(QString castId, bool allowRecoveryProbe);
    void profileRequested(qulonglong fid, bool allowRecoveryProbe);
    void signInRequested();
    void signInCancelRequested();
    void signOutRequested();
    void signerProvisionRequested();
    void signerRevokeRequested();
    void publishCastRequested(QString text);
    void publishReplyRequested(QString text, qulonglong parentAuthorFid, QString parentCastId);
    void reactionRequested(QString kind, qulonglong targetAuthorFid, QString targetCastId, bool active);
    void followingMutationRequested(qulonglong targetFid, bool following);
    void removeCastRequested(qulonglong authorFid, QString castId);
    void socialRequested(
        QString kind,
        qulonglong fid,
        QString continuation,
        bool allowRecoveryProbe);

private:
    void beginIntent(QString status);
    void retryCurrent(bool allowRecoveryProbe);
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
    bool m_writeReady{false};
    QString m_signerPhase{QStringLiteral("signed-out")};
    bool m_signerBusy{false};
    QString m_signerStatusText{QStringLiteral("Sign in before enabling interactions.")};
    QString m_signerErrorText{};
    bool m_writeBusy{false};
    QString m_writePhase{QStringLiteral("idle")};
    QString m_writeStatusText{};
    QString m_writeErrorText{};
    QString m_lastPublishedCastId{};
    qulonglong m_replyParentAuthorFid{};
    QString m_replyParentCastId{};
    QVariantList m_writeRows{};
    QVariantMap m_reactionOverrides{};
    QVariantMap m_followOverrides{};
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
    QString m_statusText{QStringLiteral("Starting local read runtime…")};
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
    bool m_shouldOpenProfileFeedAfterProfile{false};
    bool m_allowRecoveryProbeForNextIntent{false};
    QString m_next{};
    QVariantList m_rows{};
    QVariantMap m_detail{};
};
