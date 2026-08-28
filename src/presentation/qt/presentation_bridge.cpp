#include "presentation_bridge.hpp"

#include <QDesktopServices>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <utility>

import farcaster.application.configuration;

PresentationBridge::PresentationBridge(QObject* const parent) : QObject(parent) {}

QString PresentationBridge::architectureState() const
{
    return QStringLiteral("Phase 5A · guest read → verified session → signer-gated interaction");
}

bool PresentationBridge::isStrictSelfHosted() const noexcept
{
    return farcaster::application::configuration::configuredDeploymentProfile()
        == farcaster::application::configuration::DeploymentProfile::StrictSelfHosted;
}

bool PresentationBridge::diagnosticsEnabled() const noexcept
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

bool PresentationBridge::runtimeReady() const noexcept { return m_runtimeReady; }
bool PresentationBridge::authenticationAvailable() const noexcept { return m_authenticationAvailable; }
bool PresentationBridge::isAuthenticated() const noexcept { return m_isAuthenticated; }
bool PresentationBridge::authenticationBusy() const noexcept
{
    return m_authenticationPhase == QStringLiteral("starting") ||
        m_authenticationPhase == QStringLiteral("awaiting-approval");
}
QString PresentationBridge::authenticationPhase() const { return m_authenticationPhase; }
bool PresentationBridge::authenticationApprovalAvailable() const noexcept
{
    return !m_authenticationApprovalUrl.isEmpty();
}
QString PresentationBridge::authenticationErrorText() const { return m_authenticationErrorText; }
QString PresentationBridge::accountName() const { return m_accountName; }
QString PresentationBridge::accountUsername() const { return m_accountUsername; }
QString PresentationBridge::accountAvatarUrl() const { return m_accountAvatarUrl; }
qulonglong PresentationBridge::accountFid() const noexcept { return m_accountFid; }
bool PresentationBridge::writeReady() const noexcept { return m_writeReady; }
QString PresentationBridge::signerPhase() const { return m_signerPhase; }
bool PresentationBridge::signerBusy() const noexcept { return m_signerBusy; }
QString PresentationBridge::signerStatusText() const { return m_signerStatusText; }
QString PresentationBridge::signerErrorText() const { return m_signerErrorText; }
QString PresentationBridge::activeView() const { return m_activeView; }
QString PresentationBridge::activeFeed() const { return m_activeFeed; }
QString PresentationBridge::identityName() const { return m_identityName; }
qulonglong PresentationBridge::identityFid() const noexcept { return m_identityFid; }
QString PresentationBridge::statusText() const { return m_statusText; }
QString PresentationBridge::errorText() const { return m_errorText; }
QString PresentationBridge::diagnosticText() const { return m_diagnosticText; }
bool PresentationBridge::isBusy() const noexcept { return m_isBusy; }
bool PresentationBridge::isStale() const noexcept { return m_isStale; }
bool PresentationBridge::isOffline() const noexcept { return m_isOffline; }
bool PresentationBridge::sourceUnavailable() const noexcept { return m_sourceUnavailable; }
bool PresentationBridge::networkReachabilityKnown() const noexcept { return m_networkReachabilityKnown; }
bool PresentationBridge::canLoadMore() const noexcept { return !m_next.isEmpty() && !m_isBusy; }
bool PresentationBridge::hasReadActivity() const noexcept { return m_hasReadActivity; }
bool PresentationBridge::feedStarted() const noexcept { return m_feedStarted; }
QVariantList PresentationBridge::rows() const { return m_rows; }
QVariantMap PresentationBridge::detail() const { return m_detail; }

void PresentationBridge::beginIntent(QString status)
{
    m_hasReadActivity = true;
    m_isBusy = true;
    m_errorText.clear();
    m_diagnosticText.clear();
    m_sourceUnavailable = false;
    m_statusText = std::move(status);
    emit stateChanged();
}

bool PresentationBridge::consumeRecoveryProbe() noexcept
{
    return std::exchange(m_allowRecoveryProbeForNextIntent, false);
}

void PresentationBridge::openIdentity(const QString& username)
{
    const QString canonical = username.trimmed();
    if (canonical.isEmpty()) {
        m_errorText = QStringLiteral("Enter a Farcaster username.");
        emit stateChanged();
        return;
    }
    m_shouldOpenFollowingAfterIdentity = true;
    beginIntent(QStringLiteral("Checking local identity…"));
    emit identityRequested(canonical, consumeRecoveryProbe());
}

void PresentationBridge::selectFeed(const QString& feed)
{
    if (!m_runtimeReady) {
        m_errorText = QStringLiteral("The local read runtime is still starting.");
        emit stateChanged();
        return;
    }
    if (feed == QStringLiteral("following") && m_identityFid == 0) {
        m_errorText = QStringLiteral("Open an identity before reading Following.");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("feed");
    m_feedStarted = true;
    m_activeFeed = feed == QStringLiteral("following")
        ? QStringLiteral("following") : QStringLiteral("trending");
    m_channelId.clear();
    m_next.clear();
    beginIntent(QStringLiteral("Reading cached feed…"));
    emit feedRequested(m_activeFeed, m_identityFid, {}, {}, consumeRecoveryProbe());
}

void PresentationBridge::openChannel(const QString& channelId)
{
    const QString canonical = channelId.trimmed();
    if (canonical.isEmpty()) {
        m_errorText = QStringLiteral("Enter a channel id.");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("feed");
    m_feedStarted = true;
    m_activeFeed = QStringLiteral("channel");
    m_channelId = canonical;
    m_next.clear();
    beginIntent(QStringLiteral("Reading cached channel…"));
    emit feedRequested(
        m_activeFeed, m_identityFid, m_channelId, {}, consumeRecoveryProbe());
}

void PresentationBridge::openCast(const QString& castId)
{
    if (castId.trimmed().isEmpty()) {
        m_errorText = QStringLiteral("That cast identifier is empty.");
        emit stateChanged();
        return;
    }
    beginIntent(QStringLiteral("Reading cached cast…"));
    emit castRequested(castId, consumeRecoveryProbe());
}

void PresentationBridge::openProfile()
{
    if (m_identityFid == 0) {
        m_errorText = QStringLiteral("Open an identity first.");
        emit stateChanged();
        return;
    }
    beginIntent(QStringLiteral("Reading cached profile…"));
    emit profileRequested(m_identityFid, consumeRecoveryProbe());
}

void PresentationBridge::openFollowers()
{
    if (m_identityFid == 0) {
        m_errorText = QStringLiteral("Open an identity first.");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("followers");
    m_next.clear();
    beginIntent(QStringLiteral("Reading cached followers…"));
    emit socialRequested(
        QStringLiteral("followers"), m_identityFid, {}, consumeRecoveryProbe());
}

void PresentationBridge::openFollowing()
{
    if (m_identityFid == 0) {
        m_errorText = QStringLiteral("Open an identity first.");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("following");
    m_next.clear();
    beginIntent(QStringLiteral("Reading cached following…"));
    emit socialRequested(
        QStringLiteral("following"), m_identityFid, {}, consumeRecoveryProbe());
}

void PresentationBridge::loadMore()
{
    if (m_next.isEmpty() || m_isBusy) {
        return;
    }
    beginIntent(QStringLiteral("Loading the next page…"));
    if (m_activeView == QStringLiteral("feed")) {
        emit feedRequested(m_activeFeed, m_identityFid, m_channelId, m_next, false);
    } else {
        emit socialRequested(m_activeView, m_identityFid, m_next, false);
    }
}

void PresentationBridge::retry()
{
    retryCurrent(true);
}

void PresentationBridge::retryAutomatically()
{
    retryCurrent(false);
}

void PresentationBridge::retryCurrent(const bool allowRecoveryProbe)
{
    m_allowRecoveryProbeForNextIntent = allowRecoveryProbe;
    if (m_activeView == QStringLiteral("feed") && m_activeFeed == QStringLiteral("channel")) {
        openChannel(m_channelId);
    } else if (m_activeView == QStringLiteral("feed")) { selectFeed(m_activeFeed); }
    else if (m_activeView == QStringLiteral("profile")) { openProfile(); }
    else if (m_activeView == QStringLiteral("cast")) {
        openCast(m_detail.value(QStringLiteral("castId")).toString());
    } else if (m_activeView == QStringLiteral("followers")) { openFollowers(); }
    else { openFollowing(); }
}

void PresentationBridge::showFeed()
{
    if (m_activeFeed == QStringLiteral("channel")) { openChannel(m_channelId); }
    else { selectFeed(m_activeFeed); }
}

void PresentationBridge::openExternalUrl(const QString& url)
{
    const QUrl parsed{url};
    if (!parsed.isValid() || (parsed.scheme() != QStringLiteral("https") && parsed.scheme() != QStringLiteral("http"))) {
        m_errorText = QStringLiteral("Only valid HTTP(S) links can be opened externally.");
        emit stateChanged();
        return;
    }
    if (!QDesktopServices::openUrl(parsed)) {
        m_errorText = QStringLiteral("The system could not open that link.");
        emit stateChanged();
    }
}

void PresentationBridge::beginSignIn()
{
    if (!m_runtimeReady) {
        m_authenticationErrorText = QStringLiteral("The local runtime is still starting.");
        emit stateChanged();
        return;
    }
    if (!m_authenticationAvailable) {
        m_authenticationErrorText = QStringLiteral(
            "Sign in is not configured in this build. Guest reading remains available.");
        emit stateChanged();
        return;
    }
    m_authenticationErrorText.clear();
    emit signInRequested();
}

void PresentationBridge::cancelSignIn()
{
    emit signInCancelRequested();
}

void PresentationBridge::signOut()
{
    emit signOutRequested();
}

void PresentationBridge::provisionSigner()
{
    if (!m_isAuthenticated || m_signerBusy) { return; }
    m_signerErrorText.clear();
    emit signerProvisionRequested();
}

void PresentationBridge::revokeSigner()
{
    if (!m_isAuthenticated || m_signerBusy || !m_writeReady) { return; }
    m_signerErrorText.clear();
    emit signerRevokeRequested();
}

void PresentationBridge::openAuthenticationApproval()
{
    const QUrl parsed{m_authenticationApprovalUrl};
    if (!parsed.isValid() ||
        (parsed.scheme() != QStringLiteral("https") && parsed.scheme() != QStringLiteral("http"))) {
        m_authenticationErrorText = QStringLiteral("The authentication approval link is invalid.");
        emit stateChanged();
        return;
    }
    if (!QDesktopServices::openUrl(parsed)) {
        m_authenticationErrorText = QStringLiteral("The system could not open the Farcaster approval link.");
        emit stateChanged();
    }
}

void PresentationBridge::applyRuntimeState(const bool ready, QString errorText)
{
    m_runtimeReady = ready;
    m_isBusy = false;
    m_errorText = errorText;
    m_diagnosticText = ready ? QString{} : std::move(errorText);
    m_statusText = ready ? QStringLiteral("Ready to read")
                         : QStringLiteral("Local runtime unavailable");
    emit stateChanged();
}

void PresentationBridge::applyNetworkReachability(const bool known, const bool online)
{
    m_networkReachabilityKnown = known;
    m_isOffline = known && !online;
    emit stateChanged();
}

void PresentationBridge::applyAuthenticationState(
    const bool available,
    QString phase,
    const qulonglong fid,
    QString username,
    QString displayName,
    QString avatarUrl,
    QString approvalUrl,
    QString errorText)
{
    m_authenticationAvailable = available;
    m_authenticationPhase = std::move(phase);
    // The attempt phase is independent from the already established account
    // session. Starting a replacement/re-authentication flow must not make a
    // verified current account appear signed out until a new proof succeeds.
    m_isAuthenticated = fid > 0;
    m_accountFid = m_isAuthenticated ? fid : 0;
    m_accountUsername = m_isAuthenticated ? std::move(username) : QString{};
    m_accountName = m_isAuthenticated
        ? (displayName.isEmpty() ? m_accountUsername : std::move(displayName))
        : QString{};
    m_accountAvatarUrl = m_isAuthenticated ? std::move(avatarUrl) : QString{};
    m_authenticationApprovalUrl = std::move(approvalUrl);
    m_authenticationErrorText = std::move(errorText);
    // Authentication proves account ownership only. Sign-out clears write authority
    // immediately; an authenticated session keeps the separately observed signer state.
    if (!m_isAuthenticated) {
        m_writeReady = false;
        m_signerPhase = QStringLiteral("signed-out");
        m_signerBusy = false;
        m_signerStatusText = QStringLiteral("Sign in before enabling interactions.");
        m_signerErrorText.clear();
    } else {
        m_writeReady = m_signerPhase == QStringLiteral("ready");
    }
    emit stateChanged();
}

void PresentationBridge::applyAuthenticationError(QString errorText)
{
    m_authenticationErrorText = std::move(errorText);
    emit stateChanged();
}

void PresentationBridge::applySignerState(
    QString phase,
    const bool busy,
    const bool writeReady,
    QString statusText,
    QString errorText)
{
    m_signerPhase = std::move(phase);
    m_signerBusy = busy;
    m_writeReady = m_isAuthenticated && writeReady;
    m_signerStatusText = std::move(statusText);
    m_signerErrorText = std::move(errorText);
    emit stateChanged();
}

void PresentationBridge::applyIdentity(const qulonglong fid, QString displayName,
    QString username, QString status, QString error, QString diagnostic, const bool busy,
    const bool stale, const bool sourceUnavailable)
{
    m_identityFid = fid;
    m_identityName = displayName.isEmpty() ? std::move(username) : std::move(displayName);
    m_statusText = std::move(status);
    m_errorText = std::move(error);
    m_diagnosticText = std::move(diagnostic);
    m_isBusy = busy;
    m_isStale = stale;
    m_sourceUnavailable = sourceUnavailable;
    emit stateChanged();

    if (m_shouldOpenFollowingAfterIdentity && m_identityFid != 0 && !m_isBusy) {
        m_shouldOpenFollowingAfterIdentity = false;
        emit identityOpened();
        QMetaObject::invokeMethod(this,
            [this] { selectFeed(QStringLiteral("following")); },
            Qt::QueuedConnection);
    }
}

void PresentationBridge::applyList(QString view, QString selector, QVariantList rows,
    QString next, QString status, QString error, QString diagnostic, const bool busy,
    const bool stale, const bool sourceUnavailable)
{
    m_activeView = std::move(view);
    if (m_activeView == QStringLiteral("feed")) {
        m_feedStarted = true;
        m_activeFeed = std::move(selector);
    }
    m_rows = std::move(rows);
    m_next = std::move(next);
    m_statusText = std::move(status);
    m_errorText = std::move(error);
    m_diagnosticText = std::move(diagnostic);
    m_isBusy = busy;
    m_isStale = stale;
    m_sourceUnavailable = sourceUnavailable;
    emit stateChanged();
}

void PresentationBridge::applyDetail(QString view, QVariantMap detail, QString status,
    QString error, QString diagnostic, const bool busy, const bool stale, const bool sourceUnavailable)
{
    m_activeView = std::move(view);
    m_detail = std::move(detail);
    m_statusText = std::move(status);
    m_errorText = std::move(error);
    m_diagnosticText = std::move(diagnostic);
    m_isBusy = busy;
    m_isStale = stale;
    m_sourceUnavailable = sourceUnavailable;
    emit stateChanged();
}

void PresentationBridge::applyVerificationFixture()
{
    m_runtimeReady = true;
    m_hasReadActivity = true;
    m_feedStarted = true;
    m_identityFid = 6841;
    m_identityName = QStringLiteral("Protocol Field Notes");
    m_statusText = QStringLiteral("Cached · refreshing in background");
    m_isBusy = true;
    m_isStale = true;
    m_networkReachabilityKnown = true;
    m_isOffline = false;
    m_sourceUnavailable = false;
    m_diagnosticText.clear();
    m_rows = {
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0x01")}, {QStringLiteral("author"), QStringLiteral("alice")}, {QStringLiteral("displayName"), QStringLiteral("Alice Lin")}, {QStringLiteral("text"), QStringLiteral("Native clients feel different when the cache is the product, not a fallback.")}, {QStringLiteral("time"), QStringLiteral("2m")}, {QStringLiteral("replyCount"), 12}, {QStringLiteral("recastCount"), 4}, {QStringLiteral("likeCount"), 86}, {QStringLiteral("embeds"), QVariantList{QVariantMap{{QStringLiteral("kind"), QStringLiteral("url")}, {QStringLiteral("url"), QStringLiteral("https://example.com/native-client")}}}}},
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0x02")}, {QStringLiteral("author"), QStringLiteral("builders")}, {QStringLiteral("displayName"), QStringLiteral("Farcaster Builders")}, {QStringLiteral("text"), QStringLiteral("Channel telemetry is flowing through canonical local state.")}, {QStringLiteral("time"), QStringLiteral("8m")}},
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0x03")}, {QStringLiteral("author"), QStringLiteral("violet")}, {QStringLiteral("displayName"), QStringLiteral("Violet")}, {QStringLiteral("text"), QStringLiteral("The network can disappear. The reading surface should not.")}, {QStringLiteral("time"), QStringLiteral("14m")}}
    };
    m_next = QStringLiteral("fixture-next");
    emit stateChanged();
}
