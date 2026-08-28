#include "presentation_bridge.hpp"

#include <QByteArray>
#include <QDesktopServices>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <utility>

import farcaster.application.configuration;

namespace {

void updateReactionPresentation(
    QVariantMap& map,
    const QString& castId,
    const QString& kind,
    const bool active)
{
    if (map.value(QStringLiteral("castId")).toString() != castId) {
        return;
    }
    const QString stateKey = kind == QStringLiteral("like")
        ? QStringLiteral("likedByMe")
        : QStringLiteral("recastedByMe");
    const QString countKey = kind == QStringLiteral("like")
        ? QStringLiteral("likeCount")
        : QStringLiteral("recastCount");
    const bool previous = map.value(stateKey).toBool();
    if (previous == active) {
        return;
    }
    qulonglong count = map.value(countKey).toULongLong();
    if (active) {
        ++count;
    } else if (count > 0U) {
        --count;
    }
    map.insert(stateKey, active);
    map.insert(countKey, count);
}

void updateReactionPresentation(
    QVariantList& rows,
    const QString& castId,
    const QString& kind,
    const bool active)
{
    for (qsizetype index = 0; index < rows.size(); ++index) {
        auto map = rows[index].toMap();
        if (map.value(QStringLiteral("castId")).toString() != castId) {
            continue;
        }
        updateReactionPresentation(map, castId, kind, active);
        rows[index] = std::move(map);
    }
}

void removeCastPresentation(QVariantList& rows, const QString& castId)
{
    for (qsizetype index = rows.size(); index > 0; --index) {
        if (rows[index - 1].toMap().value(QStringLiteral("castId")).toString() == castId) {
            rows.removeAt(index - 1);
        }
    }
}

}

PresentationBridge::PresentationBridge(QObject* const parent) : QObject(parent) {}

QString PresentationBridge::architectureState() const
{
    return QStringLiteral("Phase 5E · guest read → verified session → scoped durable Farcaster writes");
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
bool PresentationBridge::writeBusy() const noexcept { return m_writeBusy; }
QString PresentationBridge::writePhase() const { return m_writePhase; }
QString PresentationBridge::writeStatusText() const { return m_writeStatusText; }
QString PresentationBridge::writeErrorText() const { return m_writeErrorText; }
QString PresentationBridge::lastPublishedCastId() const { return m_lastPublishedCastId; }
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

void PresentationBridge::publishCast(const QString& text)
{
    if (!m_runtimeReady) {
        m_writeErrorText = QStringLiteral("The local runtime is still starting.");
        emit stateChanged();
        return;
    }
    if (!m_isAuthenticated) {
        m_writeErrorText = QStringLiteral("Sign in before publishing a cast.");
        emit stateChanged();
        return;
    }
    if (!m_writeReady) {
        m_writeErrorText = QStringLiteral("Enable a verified Farcaster signer before publishing.");
        emit stateChanged();
        return;
    }
    if (m_writeBusy) { return; }
    if (text.trimmed().isEmpty()) {
        m_writeErrorText = QStringLiteral("Write something before publishing.");
        emit stateChanged();
        return;
    }
    if (text.toUtf8().size() > 320) {
        m_writeErrorText = QStringLiteral("Standard Farcaster casts are limited to 320 UTF-8 bytes.");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = QStringLiteral("Preparing a canonical Farcaster cast…");
    m_writeErrorText.clear();
    emit stateChanged();
    emit publishCastRequested(text);
}

void PresentationBridge::beginComposer()
{
    if (m_writeBusy) { return; }
    m_writePhase = QStringLiteral("idle");
    m_writeStatusText.clear();
    m_writeErrorText.clear();
    clearReplyTarget();
    emit stateChanged();
}

bool PresentationBridge::prepareReply(const QString& parentCastId)
{
    const QString canonicalParentId = parentCastId.trimmed();
    if (canonicalParentId.isEmpty()) {
        m_writeErrorText = QStringLiteral("That reply target has an empty cast identifier.");
        emit stateChanged();
        return false;
    }

    auto capture = [this, &canonicalParentId](const QVariantMap& map) {
        if (map.value(QStringLiteral("castId")).toString() != canonicalParentId) { return false; }
        bool ok = false;
        const auto fid = map.value(QStringLiteral("authorFid")).toULongLong(&ok);
        if (!ok || fid == 0) { return false; }
        m_replyParentAuthorFid = fid;
        m_replyParentCastId = canonicalParentId;
        return true;
    };

    if (capture(m_detail)) {
        m_writeErrorText.clear();
        emit stateChanged();
        return true;
    }
    for (const auto& row : m_rows) {
        if (capture(row.toMap())) {
            m_writeErrorText.clear();
            emit stateChanged();
            return true;
        }
    }
    for (const auto& row : m_writeRows) {
        if (capture(row.toMap())) {
            m_writeErrorText.clear();
            emit stateChanged();
            return true;
        }
    }

    m_writeErrorText = QStringLiteral(
        "The reply target is not available as a canonical cast in the current presentation state.");
    emit stateChanged();
    return false;
}

void PresentationBridge::clearReplyTarget()
{
    m_replyParentAuthorFid = 0;
    m_replyParentCastId.clear();
}

void PresentationBridge::publishReply(const QString& text)
{
    if (!m_runtimeReady) {
        m_writeErrorText = QStringLiteral("The local runtime is still starting.");
        emit stateChanged();
        return;
    }
    if (!m_isAuthenticated) {
        m_writeErrorText = QStringLiteral("Sign in before replying to a cast.");
        emit stateChanged();
        return;
    }
    if (!m_writeReady) {
        m_writeErrorText = QStringLiteral("Enable a verified Farcaster signer before replying.");
        emit stateChanged();
        return;
    }
    if (m_writeBusy) { return; }
    if (text.trimmed().isEmpty()) {
        m_writeErrorText = QStringLiteral("Write something before replying.");
        emit stateChanged();
        return;
    }
    if (text.toUtf8().size() > 320) {
        m_writeErrorText = QStringLiteral("Standard Farcaster casts are limited to 320 UTF-8 bytes.");
        emit stateChanged();
        return;
    }
    if (m_replyParentAuthorFid == 0 || m_replyParentCastId.isEmpty()) {
        m_writeErrorText = QStringLiteral("Choose a canonical cast before publishing a reply.");
        emit stateChanged();
        return;
    }

    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = QStringLiteral("Preparing a canonical Farcaster reply…");
    m_writeErrorText.clear();
    emit stateChanged();
    emit publishReplyRequested(text, m_replyParentAuthorFid, m_replyParentCastId);
}


void PresentationBridge::setReaction(
    const QString& kind,
    const qulonglong targetAuthorFid,
    const QString& targetCastId,
    const bool active)
{
    const QString canonicalKind = kind.trimmed().toLower();
    const QString canonicalCastId = targetCastId.trimmed();
    if (!m_runtimeReady || !m_isAuthenticated || !m_writeReady || m_writeBusy) {
        m_writeErrorText = !m_isAuthenticated
            ? QStringLiteral("Sign in before interacting with a cast.")
            : QStringLiteral("Enable a verified Farcaster signer before interacting.");
        emit stateChanged();
        return;
    }
    if ((canonicalKind != QStringLiteral("like") && canonicalKind != QStringLiteral("recast")) ||
        targetAuthorFid == 0U || canonicalCastId.isEmpty()) {
        m_writeErrorText = QStringLiteral("That Farcaster reaction target is invalid.");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = active
        ? QStringLiteral("Preparing the Farcaster interaction…")
        : QStringLiteral("Preparing to remove the Farcaster interaction…");
    m_writeErrorText.clear();
    emit stateChanged();
    emit reactionRequested(canonicalKind, targetAuthorFid, canonicalCastId, active);
}

void PresentationBridge::setFollowing(const qulonglong targetFid, const bool following)
{
    if (!m_runtimeReady || !m_isAuthenticated || !m_writeReady || m_writeBusy) {
        m_writeErrorText = !m_isAuthenticated
            ? QStringLiteral("Sign in before changing follows.")
            : QStringLiteral("Enable a verified Farcaster signer before changing follows.");
        emit stateChanged();
        return;
    }
    if (targetFid == 0U || targetFid == m_accountFid) {
        m_writeErrorText = QStringLiteral("That Farcaster follow target is invalid.");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = following
        ? QStringLiteral("Preparing the Farcaster follow…")
        : QStringLiteral("Preparing the Farcaster unfollow…");
    m_writeErrorText.clear();
    emit stateChanged();
    emit followingMutationRequested(targetFid, following);
}

void PresentationBridge::removeCast(const qulonglong authorFid, const QString& castId)
{
    const QString canonicalCastId = castId.trimmed();
    if (!m_runtimeReady || !m_isAuthenticated || !m_writeReady || m_writeBusy) {
        m_writeErrorText = !m_isAuthenticated
            ? QStringLiteral("Sign in before deleting a cast.")
            : QStringLiteral("Enable a verified Farcaster signer before deleting a cast.");
        emit stateChanged();
        return;
    }
    if (authorFid == 0U || authorFid != m_accountFid || canonicalCastId.isEmpty()) {
        m_writeErrorText = QStringLiteral("Only your own canonical Farcaster cast can be deleted.");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = QStringLiteral("Preparing the Farcaster cast deletion…");
    m_writeErrorText.clear();
    emit stateChanged();
    emit removeCastRequested(authorFid, canonicalCastId);
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
    const qulonglong previousAccountFid = m_accountFid;
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
    if (!m_isAuthenticated || (previousAccountFid != 0 && previousAccountFid != m_accountFid)) {
        if (!m_writeRows.isEmpty() && !m_rows.isEmpty()) {
            QVariantList retained{};
            retained.reserve(m_rows.size());
            for (const auto& row : m_rows) {
                const QString castId = row.toMap().value(QStringLiteral("castId")).toString();
                bool wasOptimistic = false;
                for (const auto& writeRow : m_writeRows) {
                    if (!castId.isEmpty() &&
                        writeRow.toMap().value(QStringLiteral("castId")).toString() == castId) {
                        wasOptimistic = true;
                        break;
                    }
                }
                if (!wasOptimistic) { retained.push_back(row); }
            }
            m_rows = std::move(retained);
        }
        m_writeRows.clear();
        m_lastPublishedCastId.clear();
        clearReplyTarget();
    }
    // Authentication proves account ownership only. Sign-out clears write authority
    // immediately; an authenticated session keeps the separately observed signer state.
    if (!m_isAuthenticated) {
        m_writeReady = false;
        m_signerPhase = QStringLiteral("signed-out");
        m_signerBusy = false;
        m_signerStatusText = QStringLiteral("Sign in before enabling interactions.");
        m_signerErrorText.clear();
        m_writeBusy = false;
        m_writePhase = QStringLiteral("idle");
        m_writeStatusText.clear();
        m_writeErrorText.clear();
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

void PresentationBridge::applyWriteState(
    QString phase,
    const bool busy,
    QString statusText,
    QString errorText,
    QVariantMap presentationMutation)
{
    m_writePhase = std::move(phase);
    m_writeBusy = busy;
    m_writeStatusText = std::move(statusText);
    m_writeErrorText = std::move(errorText);

    const QString operationKind = presentationMutation.value(QStringLiteral("operationKind")).toString();
    const QString castId = presentationMutation.value(QStringLiteral("castId")).toString();
    if (operationKind == QStringLiteral("publish-cast") && !castId.isEmpty()) {
        m_lastPublishedCastId = castId;
        bool replaced = false;
        for (qsizetype index = 0; index < m_writeRows.size(); ++index) {
            if (m_writeRows[index].toMap().value(QStringLiteral("castId")).toString() == castId) {
                m_writeRows[index] = presentationMutation;
                replaced = true;
                break;
            }
        }
        if (!replaced) { m_writeRows.prepend(presentationMutation); }

        if (m_activeView == QStringLiteral("feed")) {
            QVariantList merged{};
            merged.reserve(m_writeRows.size() + m_rows.size());
            for (const auto& row : m_writeRows) { merged.push_back(row); }
            for (const auto& row : m_rows) {
                const QString existingId = row.toMap().value(QStringLiteral("castId")).toString();
                bool duplicate = false;
                for (const auto& writeRow : m_writeRows) {
                    if (!existingId.isEmpty() &&
                        writeRow.toMap().value(QStringLiteral("castId")).toString() == existingId) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) { merged.push_back(row); }
            }
            m_rows = std::move(merged);
        }
    }

    const QString targetCastId = presentationMutation.value(QStringLiteral("targetCastId")).toString();
    if ((operationKind == QStringLiteral("like") || operationKind == QStringLiteral("unlike") ||
         operationKind == QStringLiteral("recast") || operationKind == QStringLiteral("undo-recast")) &&
        !targetCastId.isEmpty()) {
        const bool active = operationKind == QStringLiteral("like") || operationKind == QStringLiteral("recast");
        const QString reaction = (operationKind == QStringLiteral("like") || operationKind == QStringLiteral("unlike"))
            ? QStringLiteral("like") : QStringLiteral("recast");
        updateReactionPresentation(m_rows, targetCastId, reaction, active);
        updateReactionPresentation(m_writeRows, targetCastId, reaction, active);
        updateReactionPresentation(m_detail, targetCastId, reaction, active);
    } else if (operationKind == QStringLiteral("follow") || operationKind == QStringLiteral("unfollow")) {
        const qulonglong targetFid = presentationMutation.value(QStringLiteral("targetFid")).toULongLong();
        if (m_activeView == QStringLiteral("profile") &&
            m_detail.value(QStringLiteral("fid")).toULongLong() == targetFid) {
            m_detail.insert(QStringLiteral("followingByMe"), operationKind == QStringLiteral("follow"));
        }
    } else if (operationKind == QStringLiteral("remove-cast") && !targetCastId.isEmpty()) {
        removeCastPresentation(m_rows, targetCastId);
        removeCastPresentation(m_writeRows, targetCastId);
        if (m_activeView == QStringLiteral("cast") &&
            m_detail.value(QStringLiteral("castId")).toString() == targetCastId) {
            m_activeView = QStringLiteral("feed");
            m_detail.clear();
        }
    }

    if (operationKind == QStringLiteral("publish-cast") &&
        m_writePhase == QStringLiteral("queued")) {
        emit castQueued();
    }
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
    if (m_activeView == QStringLiteral("feed") && !m_writeRows.isEmpty()) {
        QVariantList retainedWriteRows{};
        QVariantList merged{};
        retainedWriteRows.reserve(m_writeRows.size());
        merged.reserve(m_writeRows.size() + rows.size());
        for (const auto& writeRow : m_writeRows) {
            const auto map = writeRow.toMap();
            const QString castId = map.value(QStringLiteral("castId")).toString();
            const bool confirmed = map.value(QStringLiteral("writeState")).toString()
                == QStringLiteral("confirmed");
            bool authoritative = false;
            if (confirmed && !castId.isEmpty()) {
                for (const auto& row : rows) {
                    if (row.toMap().value(QStringLiteral("castId")).toString() == castId) {
                        authoritative = true;
                        break;
                    }
                }
            }
            if (!authoritative) {
                retainedWriteRows.push_back(writeRow);
                merged.push_back(writeRow);
            }
        }
        m_writeRows = std::move(retainedWriteRows);
        for (const auto& row : rows) {
            const QString castId = row.toMap().value(QStringLiteral("castId")).toString();
            bool duplicate = false;
            for (const auto& writeRow : m_writeRows) {
                if (!castId.isEmpty() &&
                    writeRow.toMap().value(QStringLiteral("castId")).toString() == castId) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) { merged.push_back(row); }
        }
        m_rows = std::move(merged);
    } else {
        m_rows = std::move(rows);
    }
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
