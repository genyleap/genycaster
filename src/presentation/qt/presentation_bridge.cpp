#include "presentation_bridge.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtTranslation>
#include <QVariantList>
#include <QVariantMap>

#include <utility>

import farcaster.application.configuration;

namespace {

struct ContextualReactionRule final {
    const char* icon;
    QStringList phrases;
    QStringList tokens;
};

[[nodiscard]] bool isTokenCharacter(const QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

[[nodiscard]] bool containsToken(const QString& text, const QString& token)
{
    qsizetype offset = 0;
    while ((offset = text.indexOf(token, offset, Qt::CaseInsensitive)) >= 0) {
        const qsizetype end = offset + token.size();
        const bool hasLeftBoundary = offset == 0 || !isTokenCharacter(text.at(offset - 1));
        const bool hasRightBoundary = end == text.size() || !isTokenCharacter(text.at(end));
        if (hasLeftBoundary && hasRightBoundary) {
            return true;
        }
        offset = end;
    }
    return false;
}

[[nodiscard]] QString resolveContextualReaction(const QString& text)
{
    static const QList<ContextualReactionRule> rules{
        {"brands/genyleap", {QStringLiteral("genyleap")}, {}},
        {"brands/geny", {}, {QStringLiteral("geny")}},
        {"quilibrium/quilibrium", {QStringLiteral("quilibrium")}, {}},
        {"quilibrium/qstore", {}, {QStringLiteral("qstore")}},
        {"quilibrium/qkms", {}, {QStringLiteral("qkms")}},
        {"quilibrium/qping", {}, {QStringLiteral("qping")}},
        {"quilibrium/quark", {}, {QStringLiteral("quark")}},
        {"brands/farcaster", {}, {QStringLiteral("farcaster"), QStringLiteral("fc"), QStringLiteral("warpcast")}},
        {"brands/degen", {}, {QStringLiteral("degen")}},
        {"languages/cplusplus", {QStringLiteral("c++"), QStringLiteral("cplusplus")}, {QStringLiteral("cpp"), QStringLiteral("cxx")}},
        {"languages/qml", {QStringLiteral("qt quick")}, {QStringLiteral("qml")}},
        {"languages/typescript", {}, {QStringLiteral("typescript")}},
        {"languages/javascript", {}, {QStringLiteral("javascript")}},
        {"languages/php", {}, {QStringLiteral("php")}},
        {"platforms/linux", {QStringLiteral("gnu/linux")}, {QStringLiteral("linux")}},
        {"platforms/windows", {}, {QStringLiteral("windows"), QStringLiteral("win32"), QStringLiteral("win64")}},
        {"platforms/macos", {QStringLiteral("os x"), QStringLiteral("mac os")}, {QStringLiteral("macos")}},
        {"platforms/ios", {}, {QStringLiteral("ios"), QStringLiteral("iphone"), QStringLiteral("ipados")}},
        {"platforms/android", {}, {QStringLiteral("android")}},
        {"brands/github", {}, {QStringLiteral("github")}},
        {"brands/youtube", {}, {QStringLiteral("youtube")}},
        {"brands/telegram", {}, {QStringLiteral("telegram")}},
        {"crypto/base", {QStringLiteral("eth base"), QStringLiteral("base chain"), QStringLiteral("base network")}, {QStringLiteral("base")}},
        {"crypto/bitcoin", {}, {QStringLiteral("bitcoin"), QStringLiteral("btc")}},
        {"crypto/ethereum", {}, {QStringLiteral("ethereum"), QStringLiteral("ether"), QStringLiteral("eth")}},
        {"crypto/solana", {}, {QStringLiteral("solana"), QStringLiteral("sol")}},
        {"reactions/gem", {QStringLiteral("super rare")}, {QStringLiteral("superrare"), QStringLiteral("gem")}},
        {"crypto/nft", {}, {QStringLiteral("nft"), QStringLiteral("nfts")}},
        {"reactions/wow", {QStringLiteral("mind blown")}, {QStringLiteral("wow")}},
        {"reactions/art", {}, {QStringLiteral("art"), QStringLiteral("artist"), QStringLiteral("artwork")}},
        {"reactions/ai", {QStringLiteral("artificial intelligence")}, {QStringLiteral("ai"), QStringLiteral("llm")}},
        {"reactions/security", {}, {QStringLiteral("security"), QStringLiteral("privacy"), QStringLiteral("secure")}},
        {"reactions/code", {}, {QStringLiteral("code"), QStringLiteral("coding"), QStringLiteral("programming"), QStringLiteral("developer")}},
    };

    for (const auto& rule : rules) {
        for (const auto& phrase : rule.phrases) {
            if (text.contains(phrase, Qt::CaseInsensitive)) {
                return QString::fromLatin1(rule.icon);
            }
        }
        for (const auto& token : rule.tokens) {
            if (containsToken(text, token)) {
                return QString::fromLatin1(rule.icon);
            }
        }
    }
    return QStringLiteral("actions/heart");
}

[[nodiscard]] QString canonicalCastKey(QString castId)
{
    castId = castId.trimmed().toLower();
    if (castId.startsWith(QStringLiteral("0x"))) {
        castId.remove(0, 2);
    }
    return castId;
}

void updateReactionPresentation(
    QVariantMap& map,
    const QString& castId,
    const QString& kind,
    const bool active)
{
    if (canonicalCastKey(map.value(QStringLiteral("castId")).toString()) != canonicalCastKey(castId)) {
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
        if (canonicalCastKey(map.value(QStringLiteral("castId")).toString()) != canonicalCastKey(castId)) {
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

[[nodiscard]] QString reactionOverrideKey(const QString& castId, const QString& kind)
{
    return kind + QChar{0x1f} + canonicalCastKey(castId);
}

void rememberReactionOverride(
    QVariantMap& overrides,
    const QString& castId,
    const QString& kind,
    const bool active)
{
    if (castId.isEmpty() || (kind != QStringLiteral("like") && kind != QStringLiteral("recast"))) {
        return;
    }
    overrides.insert(reactionOverrideKey(castId, kind), active);
}

void applyReactionOverrides(QVariantMap& map, const QVariantMap& overrides)
{
    const QString castId = canonicalCastKey(map.value(QStringLiteral("castId")).toString());
    if (castId.isEmpty()) {
        return;
    }
    const QString likeKey = reactionOverrideKey(castId, QStringLiteral("like"));
    if (overrides.contains(likeKey)) {
        const bool likedByMe = overrides.value(likeKey).toBool();
        map.insert(QStringLiteral("likedByMe"), likedByMe);
        if (likedByMe && map.value(QStringLiteral("likeCount")).toULongLong() == 0U) {
            map.insert(QStringLiteral("likeCount"), qulonglong{1});
        }
    }
    const QString recastKey = reactionOverrideKey(castId, QStringLiteral("recast"));
    if (overrides.contains(recastKey)) {
        const bool recastedByMe = overrides.value(recastKey).toBool();
        map.insert(QStringLiteral("recastedByMe"), recastedByMe);
        if (recastedByMe && map.value(QStringLiteral("recastCount")).toULongLong() == 0U) {
            map.insert(QStringLiteral("recastCount"), qulonglong{1});
        }
    }
}

void applyReactionOverrides(QVariantList& rows, const QVariantMap& overrides)
{
    for (qsizetype index = 0; index < rows.size(); ++index) {
        auto map = rows[index].toMap();
        applyReactionOverrides(map, overrides);
        rows[index] = std::move(map);
    }
}

void applyViewerReactionSnapshotTo(QVariantMap& map, const QVariantMap& snapshot)
{
    const QString castId = canonicalCastKey(map.value(QStringLiteral("castId")).toString());
    if (castId.isEmpty()) { return; }
    if (snapshot.value(reactionOverrideKey(castId, QStringLiteral("like"))).toBool()) {
        map.insert(QStringLiteral("likedByMe"), true);
    }
    if (snapshot.value(reactionOverrideKey(castId, QStringLiteral("recast"))).toBool()) {
        map.insert(QStringLiteral("recastedByMe"), true);
    }
}

void applyViewerReactionSnapshotTo(QVariantList& rows, const QVariantMap& snapshot)
{
    for (qsizetype index = 0; index < rows.size(); ++index) {
        auto map = rows[index].toMap();
        applyViewerReactionSnapshotTo(map, snapshot);
        rows[index] = std::move(map);
    }
}

void applyViewerReactionDeltaTo(
    QVariantMap& map,
    const QString& targetCastId,
    const bool liked,
    const bool recasted)
{
    const QString castId = canonicalCastKey(map.value(QStringLiteral("castId")).toString());
    if (castId.isEmpty() || castId != canonicalCastKey(targetCastId)) {
        return;
    }
    map.insert(QStringLiteral("likedByMe"), liked);
    map.insert(QStringLiteral("recastedByMe"), recasted);
}

void applyViewerReactionDeltaTo(
    QVariantList& rows,
    const QString& castId,
    const bool liked,
    const bool recasted)
{
    for (qsizetype index = 0; index < rows.size(); ++index) {
        auto map = rows[index].toMap();
        applyViewerReactionDeltaTo(map, castId, liked, recasted);
        rows[index] = std::move(map);
    }
}

[[nodiscard]] QString profileCacheKey(const qulonglong fid, const QString& feed)
{
    return QString::number(fid) + QChar{0x1f} + feed;
}

[[nodiscard]] QString followingOverrideKey(const qulonglong targetFid)
{
    return QString::number(targetFid);
}

void rememberFollowingOverride(
    QVariantMap& overrides,
    const qulonglong targetFid,
    const bool isFollowing)
{
    if (targetFid == 0U) {
        return;
    }
    overrides.insert(followingOverrideKey(targetFid), isFollowing);
}

void applyFollowingOverride(QVariantMap& map, const QVariantMap& overrides)
{
    const qulonglong targetFid = map.value(QStringLiteral("fid")).toULongLong();
    if (targetFid == 0U) {
        return;
    }
    const QString key = followingOverrideKey(targetFid);
    if (overrides.contains(key)) {
        map.insert(QStringLiteral("followingByMe"), overrides.value(key).toBool());
        map.insert(QStringLiteral("followingKnown"), true);
    }
}

void applyFollowingOverrides(QVariantList& rows, const QVariantMap& overrides)
{
    for (qsizetype index = 0; index < rows.size(); ++index) {
        auto map = rows[index].toMap();
        applyFollowingOverride(map, overrides);
        rows[index] = std::move(map);
    }
}

void applyFollowsMeSnapshot(QVariantMap& map, const QVariantMap& snapshot)
{
    const qulonglong fid = map.value(QStringLiteral("fid")).toULongLong();
    if (fid == 0U) {
        return;
    }
    const QString key = QString::number(fid);
    if (!snapshot.contains(key)) {
        return;
    }
    map.insert(QStringLiteral("followsMe"), snapshot.value(key).toBool());
    map.insert(QStringLiteral("followsMeKnown"), true);
}

}

PresentationBridge::PresentationBridge(QObject* const parent)
    : QObject(parent),
      m_signerStatusText{qtTrId("genycaster.bridge.sign_in_before_enabling_interactions")},
      m_statusText{qtTrId("genycaster.bridge.starting_local_read_runtime")}
{
}

QString PresentationBridge::architectureState() const
{
    return QStringLiteral("Phase 5F · network-aware durable writes with restart-safe social state");
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
QVariantMap PresentationBridge::accountProfile() const { return m_accountProfile; }
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
QString PresentationBridge::profileFeed() const { return m_profileFeed; }
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
bool PresentationBridge::hasMore() const noexcept { return !m_next.isEmpty(); }
bool PresentationBridge::hasReadActivity() const noexcept { return m_hasReadActivity; }
bool PresentationBridge::feedStarted() const noexcept { return m_feedStarted; }
QVariantList PresentationBridge::rows() const { return m_rows; }
QVariantMap PresentationBridge::detail() const { return m_detail; }
QVariantList PresentationBridge::searchResults() const { return m_searchResults; }
bool PresentationBridge::searchBusy() const noexcept { return m_searchBusy; }
QString PresentationBridge::searchErrorText() const { return m_searchErrorText; }

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
        m_errorText = qtTrId("genycaster.bridge.enter_a_farcaster_username");
        emit stateChanged();
        return;
    }
    m_shouldOpenFollowingAfterIdentity = true;
    beginIntent(qtTrId("genycaster.bridge.checking_local_identity"));
    emit identityRequested(canonical, consumeRecoveryProbe());
}

void PresentationBridge::selectFeed(const QString& feed)
{
    if (!m_runtimeReady) {
        m_errorText = qtTrId("genycaster.bridge.the_local_read_runtime_is_still_starting");
        emit stateChanged();
        return;
    }
    if (feed == QStringLiteral("following") && m_identityFid == 0) {
        m_errorText = qtTrId("genycaster.bridge.open_an_identity_before_reading_following");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("feed");
    m_feedStarted = true;
    m_activeFeed = feed == QStringLiteral("following")
        ? QStringLiteral("following") : QStringLiteral("trending");
    m_channelId.clear();
    m_next.clear();
    beginIntent(qtTrId("genycaster.bridge.reading_cached_feed"));
    emit feedRequested(m_activeFeed, m_identityFid, {}, {}, consumeRecoveryProbe());
}

void PresentationBridge::openChannel(const QString& channelId)
{
    const QString canonical = channelId.trimmed();
    if (canonical.isEmpty()) {
        m_errorText = qtTrId("genycaster.bridge.enter_a_channel_id");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("feed");
    m_feedStarted = true;
    m_activeFeed = QStringLiteral("channel");
    m_channelId = canonical;
    m_next.clear();
    beginIntent(qtTrId("genycaster.bridge.reading_cached_channel"));
    emit feedRequested(
        m_activeFeed, m_identityFid, m_channelId, {}, consumeRecoveryProbe());
}


void PresentationBridge::openSearchQuery(const QString& query)
{
    QString canonical = query.trimmed();
    if (canonical.isEmpty()) {
        m_errorText = qtTrId("genycaster.bridge.enter_a_search_query");
        emit stateChanged();
        return;
    }

    if (canonical.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        canonical.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        const QUrl url{canonical};
        const QString host = url.host().toLower();
        const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if ((host == QStringLiteral("farcaster.xyz") || host.endsWith(QStringLiteral(".farcaster.xyz"))) &&
            segments.size() >= 3 && segments[0] == QStringLiteral("~") &&
            segments[1] == QStringLiteral("channel")) {
            openChannel(segments[2]);
            return;
        }
        if ((host == QStringLiteral("farcaster.xyz") || host.endsWith(QStringLiteral(".farcaster.xyz"))) &&
            !segments.isEmpty() && !segments[0].startsWith(QLatin1Char('~'))) {
            openProfileUsername(segments[0]);
            return;
        }
    }

    if (canonical.startsWith(QLatin1Char('@'))) {
        canonical.remove(0, 1);
        openProfileUsername(canonical);
        return;
    }
    if (canonical.startsWith(QStringLiteral("channel:"), Qt::CaseInsensitive)) {
        openChannel(canonical.sliced(8).trimmed());
        return;
    }
    if (canonical.startsWith(QLatin1Char('/')) || canonical.startsWith(QLatin1Char('#'))) {
        canonical.remove(0, 1);
        openChannel(canonical);
        return;
    }
    if (canonical.startsWith(QStringLiteral("cast:"), Qt::CaseInsensitive)) {
        openCast(canonical.sliced(5).trimmed());
        return;
    }
    if (canonical.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) && canonical.size() >= 18) {
        openCast(canonical);
        return;
    }

    openProfileUsername(canonical);
}

void PresentationBridge::requestSearchSuggestions(const QString& query)
{
    QString canonical = query.trimmed();
    if (canonical.startsWith(QStringLiteral("cast:"), Qt::CaseInsensitive) ||
        (canonical.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) && canonical.size() >= 18)) {
        clearSearchSuggestions();
        return;
    }
    if (canonical.startsWith(QStringLiteral("channel:"), Qt::CaseInsensitive)) {
        canonical = canonical.sliced(8).trimmed();
    } else if (canonical.startsWith(QLatin1Char('@')) || canonical.startsWith(QLatin1Char('/')) ||
               canonical.startsWith(QLatin1Char('#'))) {
        canonical.remove(0, 1);
        canonical = canonical.trimmed();
    }
    if (canonical.size() < 2) {
        clearSearchSuggestions();
        return;
    }
    m_searchResults.clear();
    m_searchQuery = canonical;
    m_searchErrorText.clear();
    m_searchBusy = true;
    emit searchChanged();
    emit searchSuggestionsRequested(canonical);
}

void PresentationBridge::clearSearchSuggestions()
{
    const bool changed = !m_searchResults.isEmpty() || !m_searchErrorText.isEmpty() || m_searchBusy;
    m_searchResults.clear();
    m_searchQuery.clear();
    m_searchErrorText.clear();
    m_searchBusy = false;
    emit searchSuggestionsCancelled();
    if (changed) {
        emit searchChanged();
    }
}

void PresentationBridge::openCast(const QString& castId)
{
    if (castId.trimmed().isEmpty()) {
        m_errorText = qtTrId("genycaster.bridge.that_cast_identifier_is_empty");
        emit stateChanged();
        return;
    }
    m_detailReturnView = m_activeView == QStringLiteral("profile")
        ? QStringLiteral("profile")
        : QStringLiteral("feed");
    beginIntent(qtTrId("genycaster.bridge.reading_cached_cast"));
    emit castRequested(castId, consumeRecoveryProbe());
}

void PresentationBridge::openProfile()
{
    if (m_identityFid == 0) {
        m_errorText = qtTrId("genycaster.bridge.open_an_identity_first");
        emit stateChanged();
        return;
    }
    openProfileFid(m_identityFid);
}

void PresentationBridge::openMyProfile()
{
    if (!m_isAuthenticated || m_accountFid == 0) {
        m_errorText = qtTrId("genycaster.bridge.sign_in_before_opening_your_account_profile");
        emit stateChanged();
        return;
    }
    openProfileFid(m_accountFid);
}

void PresentationBridge::openProfileUsername(const QString& username)
{
    const QString canonical = username.trimmed();
    if (canonical.isEmpty()) {
        m_errorText = qtTrId("genycaster.bridge.enter_a_farcaster_username");
        emit stateChanged();
        return;
    }
    beginIntent(qtTrId("genycaster.bridge.checking_local_identity"));
    emit profileUsernameRequested(canonical, consumeRecoveryProbe());
}

void PresentationBridge::openProfileFid(const qulonglong fid)
{
    if (fid == 0) {
        m_errorText = qtTrId("genycaster.bridge.that_farcaster_profile_identifier_is_invalid");
        emit stateChanged();
        return;
    }
    m_profileFid = fid;
    m_activeView = QStringLiteral("profile");
    if (const auto detail = m_profileDetailCache.constFind(fid); detail != m_profileDetailCache.cend()) {
        m_detail = *detail;
    } else {
        m_detail.clear();
    }
    const QString cachedListKey = profileCacheKey(m_profileFid, m_profileFeed);
    if (const auto rows = m_profileListCache.constFind(cachedListKey); rows != m_profileListCache.cend()) {
        m_rows = *rows;
        m_next = m_profileNextCache.value(cachedListKey);
    } else {
        m_rows.clear();
        m_next.clear();
    }
    beginIntent(qtTrId("genycaster.bridge.reading_cached_profile"));
    const bool recoveryProbe = consumeRecoveryProbe();
    emit profileRequested(m_profileFid, recoveryProbe);
    // Profile metadata and its selected timeline are independent cache-first
    // surfaces. Start both immediately: SQLite/memory can paint the screen in
    // the same turn while their network refreshes reconcile in parallel.
    emit feedRequested(m_profileFeed, m_profileFid, {}, {}, recoveryProbe);
}

void PresentationBridge::selectProfileFeed(const QString& feed)
{
    if (m_profileFid == 0) {
        m_errorText = qtTrId("genycaster.bridge.open_a_profile_before_loading_profile_posts");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("profile");
    const QString requestedFeed =
        feed == QStringLiteral("user-quotes") ? QStringLiteral("user-quotes") :
        feed == QStringLiteral("user-replies") ? QStringLiteral("user-replies") :
        feed == QStringLiteral("user-likes") ? QStringLiteral("user-likes") :
        feed == QStringLiteral("user-popular") ? QStringLiteral("user-popular") :
        QStringLiteral("user-posts");
    m_profileFeed = requestedFeed;
    const QString cachedListKey = profileCacheKey(m_profileFid, m_profileFeed);
    if (const auto rows = m_profileListCache.constFind(cachedListKey); rows != m_profileListCache.cend()) {
        m_rows = *rows;
        m_next = m_profileNextCache.value(cachedListKey);
    } else {
        m_rows.clear();
        m_next.clear();
    }
    QString message = qtTrId("genycaster.bridge.reading_cached_posts");
    if (m_profileFeed == QStringLiteral("user-quotes")) { message = qtTrId("genycaster.bridge.reading_cached_quotes"); }
    else if (m_profileFeed == QStringLiteral("user-replies")) { message = qtTrId("genycaster.bridge.reading_cached_replies"); }
    else if (m_profileFeed == QStringLiteral("user-likes")) { message = qtTrId("genycaster.bridge.reading_cached_likes"); }
    else if (m_profileFeed == QStringLiteral("user-popular")) { message = qtTrId("genycaster.bridge.reading_cached_popular_casts"); }
    beginIntent(std::move(message));
    emit feedRequested(m_profileFeed, m_profileFid, {}, {}, consumeRecoveryProbe());
}

void PresentationBridge::openFollowers()
{
    m_socialReturnView = m_activeView == QStringLiteral("profile")
        ? QStringLiteral("profile")
        : QStringLiteral("feed");
    m_socialFid = m_socialReturnView == QStringLiteral("profile") && m_profileFid != 0
        ? m_profileFid
        : m_identityFid;
    if (m_socialFid == 0) {
        m_errorText = qtTrId("genycaster.bridge.open_an_identity_first");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("followers");
    m_next.clear();
    beginIntent(qtTrId("genycaster.bridge.reading_cached_followers"));
    emit socialRequested(
        QStringLiteral("followers"), m_socialFid, {}, consumeRecoveryProbe());
}

void PresentationBridge::openFollowing()
{
    m_socialReturnView = m_activeView == QStringLiteral("profile")
        ? QStringLiteral("profile")
        : QStringLiteral("feed");
    m_socialFid = m_socialReturnView == QStringLiteral("profile") && m_profileFid != 0
        ? m_profileFid
        : m_identityFid;
    if (m_socialFid == 0) {
        m_errorText = qtTrId("genycaster.bridge.open_an_identity_first");
        emit stateChanged();
        return;
    }
    m_activeView = QStringLiteral("following");
    m_next.clear();
    beginIntent(qtTrId("genycaster.bridge.reading_cached_following"));
    emit socialRequested(
        QStringLiteral("following"), m_socialFid, {}, consumeRecoveryProbe());
}

void PresentationBridge::returnFromSocial()
{
    if (m_socialReturnView == QStringLiteral("profile") && m_profileFid != 0) {
        openProfileFid(m_profileFid);
        return;
    }
    showFeed();
}

void PresentationBridge::returnFromDetail()
{
    if (m_detailReturnView == QStringLiteral("profile") && m_profileFid != 0) {
        openProfileFid(m_profileFid);
        return;
    }
    showFeed();
}

void PresentationBridge::loadMore()
{
    if (m_next.isEmpty() || m_isBusy) {
        return;
    }
    beginIntent(qtTrId("genycaster.bridge.loading_the_next_page"));
    if (m_activeView == QStringLiteral("feed")) {
        emit feedRequested(m_activeFeed, m_identityFid, m_channelId, m_next, false);
    } else if (m_activeView == QStringLiteral("profile")) {
        emit feedRequested(m_profileFeed, m_profileFid, {}, m_next, false);
    } else {
        emit socialRequested(m_activeView, m_socialFid, m_next, false);
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

void PresentationBridge::retranslateTransientState()
{
    if (m_signerPhase == QStringLiteral("signed-out")) {
        m_signerStatusText = qtTrId("genycaster.bridge.sign_in_before_enabling_interactions");
    } else if (m_signerPhase == QStringLiteral("checking")) {
        m_signerStatusText = qtTrId("genycaster.runtime.checking_the_protocol_signer_against_the_network");
    } else if (m_signerPhase == QStringLiteral("missing")) {
        m_signerStatusText = qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet");
    } else if (m_signerPhase == QStringLiteral("authorization-required")) {
        m_signerStatusText = qtTrId("genycaster.runtime.custody_wallet_approval_is_required_to_enable_interactions");
    } else if (m_signerPhase == QStringLiteral("provisioning")) {
        m_signerStatusText = qtTrId("genycaster.runtime.waiting_for_custody_approval_and_network_confirmation");
    } else if (m_signerPhase == QStringLiteral("ready")) {
        m_signerStatusText = qtTrId("genycaster.runtime.interaction_signing_is_ready");
    } else if (m_signerPhase == QStringLiteral("revoked")) {
        m_signerStatusText = qtTrId("genycaster.runtime.the_protocol_signer_has_been_revoked");
    } else if (m_signerPhase == QStringLiteral("expired")) {
        m_signerStatusText = qtTrId("genycaster.runtime.the_protocol_signer_expired_enable_interactions_again_to_con");
    } else if (m_signerPhase == QStringLiteral("unavailable")) {
        m_signerStatusText = qtTrId("genycaster.runtime.interaction_signing_is_temporarily_unavailable_reading_is_un");
    } else if (m_signerPhase == QStringLiteral("failed")) {
        m_signerStatusText = qtTrId("genycaster.runtime.interaction_signing_could_not_be_verified_safely_reading_is_");
    }

    // Provider errors are ephemeral presentation data. Clearing them on a locale
    // switch prevents a translated message from surviving in the wrong language.
    m_authenticationErrorText.clear();
    m_signerErrorText.clear();
    m_writeErrorText.clear();
    emit stateChanged();
}

void PresentationBridge::retryCurrent(const bool allowRecoveryProbe)
{
    m_allowRecoveryProbeForNextIntent = allowRecoveryProbe;
    if (m_activeView == QStringLiteral("feed") && m_activeFeed == QStringLiteral("channel")) {
        openChannel(m_channelId);
    } else if (m_activeView == QStringLiteral("feed")) {
        selectFeed(m_activeFeed);
    } else if (m_activeView == QStringLiteral("profile")) {
        selectProfileFeed(m_profileFeed);
    } else if (m_activeView == QStringLiteral("cast")) {
        const QString castId = m_detail.value(QStringLiteral("castId")).toString().trimmed();
        if (castId.isEmpty()) {
            m_errorText = qtTrId("genycaster.bridge.that_cast_identifier_is_empty");
            emit stateChanged();
            return;
        }
        beginIntent(qtTrId("genycaster.bridge.reading_cached_cast"));
        emit castRequested(castId, consumeRecoveryProbe());
    } else if (m_activeView == QStringLiteral("followers")) {
        beginIntent(qtTrId("genycaster.bridge.reading_cached_followers"));
        emit socialRequested(
            QStringLiteral("followers"), m_socialFid, {}, consumeRecoveryProbe());
    } else {
        beginIntent(qtTrId("genycaster.bridge.reading_cached_following"));
        emit socialRequested(
            QStringLiteral("following"), m_socialFid, {}, consumeRecoveryProbe());
    }
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
        m_errorText = qtTrId("genycaster.bridge.only_valid_http_s_links_can_be_opened_externally");
        emit stateChanged();
        return;
    }
    if (!QDesktopServices::openUrl(parsed)) {
        m_errorText = qtTrId("genycaster.bridge.the_system_could_not_open_that_link");
        emit stateChanged();
    }
}

void PresentationBridge::copyText(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    if (QClipboard* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
        clipboard->setText(text);
    }
}

QString PresentationBridge::contextualReactionIcon(const QString& text) const
{
    return resolveContextualReaction(text);
}

void PresentationBridge::beginSignIn()
{
    if (!m_runtimeReady) {
        m_authenticationErrorText = qtTrId("genycaster.bridge.the_local_runtime_is_still_starting");
        emit stateChanged();
        return;
    }
    if (!m_authenticationAvailable) {
        m_authenticationErrorText = qtTrId(
            "genycaster.bridge.sign_in_is_not_configured_guest_reading_remains_available");
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

int PresentationBridge::utf8ByteCount(const QString& text) const
{
    return static_cast<int>(text.toUtf8().size());
}

void PresentationBridge::publishCast(const QString& text)
{
    publishPreparedCast(text, {}, {}, {});
}

void PresentationBridge::publishPreparedCast(
    const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions,
    const QVariantList& embeds)
{
    if (!m_runtimeReady) {
        m_writeErrorText = qtTrId("genycaster.bridge.the_local_runtime_is_still_starting");
        emit stateChanged();
        return;
    }
    if (!m_isAuthenticated) {
        m_writeErrorText = qtTrId("genycaster.bridge.sign_in_before_publishing_a_cast");
        emit stateChanged();
        return;
    }
    if (!m_writeReady) {
        m_writeErrorText = qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_publishing");
        emit stateChanged();
        return;
    }
    if (m_writeBusy) { return; }
    if (text.trimmed().isEmpty() && embeds.isEmpty()) {
        m_writeErrorText = qtTrId("genycaster.bridge.write_something_before_publishing");
        emit stateChanged();
        return;
    }
    if (text.toUtf8().size() > 1024) {
        m_writeErrorText = qtTrId("genycaster.bridge.farcaster_casts_are_limited_to_1024_utf_8_bytes");
        emit stateChanged();
        return;
    }
    if (mentionFids.size() != mentionPositions.size() || mentionFids.size() > 10) {
        m_writeErrorText = qtTrId("genycaster.bridge.invalid_semantic_mentions");
        emit stateChanged();
        return;
    }
    if (embeds.size() > 2) {
        m_writeErrorText = qtTrId("genycaster.bridge.too_many_cast_embeds");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = qtTrId("genycaster.bridge.preparing_a_canonical_farcaster_cast");
    m_writeErrorText.clear();
    emit stateChanged();
    emit publishCastRequested(text, mentionFids, mentionPositions, embeds);
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
        m_writeErrorText = qtTrId("genycaster.bridge.that_reply_target_has_an_empty_cast_identifier");
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

    m_writeErrorText = qtTrId(
        "genycaster.bridge.reply_target_not_available_as_canonical_cast");
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
    publishPreparedReply(text, {}, {}, {});
}

void PresentationBridge::publishPreparedReply(
    const QString& text, const QVariantList& mentionFids, const QVariantList& mentionPositions,
    const QVariantList& embeds)
{
    if (!m_runtimeReady) {
        m_writeErrorText = qtTrId("genycaster.bridge.the_local_runtime_is_still_starting");
        emit stateChanged();
        return;
    }
    if (!m_isAuthenticated) {
        m_writeErrorText = qtTrId("genycaster.bridge.sign_in_before_replying_to_a_cast");
        emit stateChanged();
        return;
    }
    if (!m_writeReady) {
        m_writeErrorText = qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_replying");
        emit stateChanged();
        return;
    }
    if (m_writeBusy) { return; }
    if (text.trimmed().isEmpty() && embeds.isEmpty()) {
        m_writeErrorText = qtTrId("genycaster.bridge.write_something_before_replying");
        emit stateChanged();
        return;
    }
    if (text.toUtf8().size() > 1024) {
        m_writeErrorText = qtTrId("genycaster.bridge.farcaster_casts_are_limited_to_1024_utf_8_bytes");
        emit stateChanged();
        return;
    }
    if (mentionFids.size() != mentionPositions.size() || mentionFids.size() > 10) {
        m_writeErrorText = qtTrId("genycaster.bridge.invalid_semantic_mentions");
        emit stateChanged();
        return;
    }
    if (embeds.size() > 2) {
        m_writeErrorText = qtTrId("genycaster.bridge.too_many_cast_embeds");
        emit stateChanged();
        return;
    }
    if (m_replyParentAuthorFid == 0 || m_replyParentCastId.isEmpty()) {
        m_writeErrorText = qtTrId("genycaster.bridge.choose_a_canonical_cast_before_publishing_a_reply");
        emit stateChanged();
        return;
    }

    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = qtTrId("genycaster.bridge.preparing_a_canonical_farcaster_reply");
    m_writeErrorText.clear();
    emit stateChanged();
    emit publishReplyRequested(text, mentionFids, mentionPositions, embeds, m_replyParentAuthorFid, m_replyParentCastId);
}



void PresentationBridge::publishPreparedThread(const QVariantList& casts)
{
    if (!m_runtimeReady) {
        m_writeErrorText = qtTrId("genycaster.bridge.the_local_runtime_is_still_starting");
        emit stateChanged();
        return;
    }
    if (!m_isAuthenticated) {
        m_writeErrorText = qtTrId("genycaster.bridge.sign_in_before_publishing_a_cast");
        emit stateChanged();
        return;
    }
    if (!m_writeReady) {
        m_writeErrorText = qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_publishing");
        emit stateChanged();
        return;
    }
    if (m_writeBusy) {
        return;
    }
    if (casts.size() < 2 || casts.size() > 12) {
        m_writeErrorText = qtTrId("genycaster.bridge.thread_cast_count_invalid");
        emit stateChanged();
        return;
    }
    for (const auto& value : casts) {
        const QVariantMap cast = value.toMap();
        const QString text = cast.value(QStringLiteral("text")).toString();
        const QVariantList mentionFids = cast.value(QStringLiteral("mentionFids")).toList();
        const QVariantList mentionPositions = cast.value(QStringLiteral("mentionPositions")).toList();
        const QVariantList embeds = cast.value(QStringLiteral("embeds")).toList();
        if (text.trimmed().isEmpty() && embeds.isEmpty()) {
            m_writeErrorText = qtTrId("genycaster.bridge.thread_cast_empty");
            emit stateChanged();
            return;
        }
        if (text.toUtf8().size() > 1024) {
            m_writeErrorText = qtTrId("genycaster.bridge.farcaster_casts_are_limited_to_1024_utf_8_bytes");
            emit stateChanged();
            return;
        }
        if (mentionFids.size() != mentionPositions.size() || mentionFids.size() > 10) {
            m_writeErrorText = qtTrId("genycaster.bridge.invalid_semantic_mentions");
            emit stateChanged();
            return;
        }
        if (embeds.size() > 2) {
            m_writeErrorText = qtTrId("genycaster.bridge.too_many_cast_embeds");
            emit stateChanged();
            return;
        }
    }
    if (!m_replyParentCastId.isEmpty() && m_replyParentAuthorFid == 0) {
        m_writeErrorText = qtTrId("genycaster.bridge.choose_a_canonical_cast_before_publishing_a_reply");
        emit stateChanged();
        return;
    }

    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = qtTrId("genycaster.bridge.preparing_a_canonical_farcaster_thread");
    m_writeErrorText.clear();
    emit stateChanged();
    emit publishThreadRequested(casts, m_replyParentAuthorFid, m_replyParentCastId);
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
            ? qtTrId("genycaster.bridge.sign_in_before_interacting_with_a_cast")
            : qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_interacting");
        emit stateChanged();
        return;
    }
    if ((canonicalKind != QStringLiteral("like") && canonicalKind != QStringLiteral("recast")) ||
        targetAuthorFid == 0U || canonicalCastId.isEmpty()) {
        m_writeErrorText = qtTrId("genycaster.bridge.that_farcaster_reaction_target_is_invalid");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = active
        ? qtTrId("genycaster.bridge.preparing_the_farcaster_interaction")
        : qtTrId("genycaster.bridge.preparing_to_remove_the_farcaster_interaction");
    m_writeErrorText.clear();
    emit stateChanged();
    emit reactionRequested(canonicalKind, targetAuthorFid, canonicalCastId, active);
}

void PresentationBridge::setFollowing(const qulonglong targetFid, const bool following)
{
    if (!m_runtimeReady || !m_isAuthenticated || !m_writeReady || m_writeBusy) {
        m_writeErrorText = !m_isAuthenticated
            ? qtTrId("genycaster.bridge.sign_in_before_changing_follows")
            : qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_changing_follows");
        emit stateChanged();
        return;
    }
    if (targetFid == 0U || targetFid == m_accountFid) {
        m_writeErrorText = qtTrId("genycaster.bridge.that_farcaster_follow_target_is_invalid");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = following
        ? qtTrId("genycaster.bridge.preparing_the_farcaster_follow")
        : qtTrId("genycaster.bridge.preparing_the_farcaster_unfollow");
    m_writeErrorText.clear();
    emit stateChanged();
    emit followingMutationRequested(targetFid, following);
}

void PresentationBridge::removeCast(const qulonglong authorFid, const QString& castId)
{
    const QString canonicalCastId = castId.trimmed();
    if (!m_runtimeReady || !m_isAuthenticated || !m_writeReady || m_writeBusy) {
        m_writeErrorText = !m_isAuthenticated
            ? qtTrId("genycaster.bridge.sign_in_before_deleting_a_cast")
            : qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_deleting_a_cast");
        emit stateChanged();
        return;
    }
    if (authorFid == 0U || authorFid != m_accountFid || canonicalCastId.isEmpty()) {
        m_writeErrorText = qtTrId("genycaster.bridge.only_your_own_canonical_farcaster_cast_can_be_deleted");
        emit stateChanged();
        return;
    }
    m_writeBusy = true;
    m_writePhase = QStringLiteral("preparing");
    m_writeStatusText = qtTrId("genycaster.bridge.preparing_the_farcaster_cast_deletion");
    m_writeErrorText.clear();
    emit stateChanged();
    emit removeCastRequested(authorFid, canonicalCastId);
}

void PresentationBridge::openAuthenticationApproval()
{
    const QUrl parsed{m_authenticationApprovalUrl};
    if (!parsed.isValid() ||
        (parsed.scheme() != QStringLiteral("https") && parsed.scheme() != QStringLiteral("http"))) {
        m_authenticationErrorText = qtTrId("genycaster.bridge.the_authentication_approval_link_is_invalid");
        emit stateChanged();
        return;
    }
    if (!QDesktopServices::openUrl(parsed)) {
        m_authenticationErrorText = qtTrId("genycaster.bridge.the_system_could_not_open_the_farcaster_approval_link");
        emit stateChanged();
    }
}

void PresentationBridge::applyRuntimeState(const bool ready, QString errorText)
{
    m_runtimeReady = ready;
    m_isBusy = false;
    m_errorText = errorText;
    m_diagnosticText = ready ? QString{} : std::move(errorText);
    m_statusText = ready ? qtTrId("genycaster.bridge.ready_to_read")
                         : qtTrId("genycaster.bridge.local_runtime_unavailable");
    emit stateChanged();
}

void PresentationBridge::applyNetworkReachability(const bool known, const bool online)
{
    const bool changed = m_networkReachabilityKnown != known || m_isOffline != (known && !online);
    m_networkReachabilityKnown = known;
    m_isOffline = known && !online;
    emit stateChanged();
    if (changed) {
        emit networkReachabilityChanged(known, online);
    }
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
        m_viewerReactionSnapshot.clear();
        m_reactionOverrides.clear();
        m_followOverrides.clear();
        m_lastPublishedCastId.clear();
        m_accountProfile.clear();
        clearReplyTarget();
    }
    // Authentication proves account ownership only. Sign-out clears write authority
    // immediately; an authenticated session keeps the separately observed signer state.
    if (!m_isAuthenticated) {
        m_writeReady = false;
        m_signerPhase = QStringLiteral("signed-out");
        m_signerBusy = false;
        m_signerStatusText = qtTrId("genycaster.bridge.sign_in_before_enabling_interactions");
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
        if (m_writePhase == QStringLiteral("confirmed")) {
            m_lastPublishedCastId = castId;
        }
        bool replaced = false;
        for (qsizetype index = 0; index < m_writeRows.size(); ++index) {
            if (m_writeRows[index].toMap().value(QStringLiteral("castId")).toString() == castId) {
                m_writeRows[index] = presentationMutation;
                replaced = true;
                break;
            }
        }
        if (!replaced) { m_writeRows.prepend(presentationMutation); }

        if (m_activeView == QStringLiteral("feed") ||
            (m_activeView == QStringLiteral("profile") &&
             m_profileFeed == QStringLiteral("user-posts") &&
             m_profileFid == m_accountFid)) {
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
        rememberReactionOverride(m_reactionOverrides, targetCastId, reaction, active);
        updateReactionPresentation(m_rows, targetCastId, reaction, active);
        updateReactionPresentation(m_writeRows, targetCastId, reaction, active);
        updateReactionPresentation(m_detail, targetCastId, reaction, active);
    } else if (operationKind == QStringLiteral("follow") || operationKind == QStringLiteral("unfollow")) {
        const qulonglong targetFid = presentationMutation.value(QStringLiteral("targetFid")).toULongLong();
        const bool isFollowing = operationKind == QStringLiteral("follow");
        rememberFollowingOverride(m_followOverrides, targetFid, isFollowing);
        applyFollowingOverrides(m_rows, m_followOverrides);
        applyFollowingOverride(m_detail, m_followOverrides);
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

void PresentationBridge::applyReactionProjections(QVariantList reactions)
{
    m_reactionOverrides.clear();
    for (const auto& value : reactions) {
        const QVariantMap projection = value.toMap();
        rememberReactionOverride(
            m_reactionOverrides,
            projection.value(QStringLiteral("castId")).toString(),
            projection.value(QStringLiteral("kind")).toString(),
            projection.value(QStringLiteral("active")).toBool());
    }
    applyReactionOverrides(m_rows, m_reactionOverrides);
    applyReactionOverrides(m_writeRows, m_reactionOverrides);
    applyReactionOverrides(m_detail, m_reactionOverrides);
    emit stateChanged();
}

void PresentationBridge::applyViewerReactionSnapshot(QVariantList reactions)
{
    m_viewerReactionSnapshot.clear();
    for (const auto& value : reactions) {
        const QVariantMap snapshot = value.toMap();
        const QString castId = snapshot.value(QStringLiteral("castId")).toString();
        if (snapshot.value(QStringLiteral("liked")).toBool()) {
            rememberReactionOverride(m_viewerReactionSnapshot, castId, QStringLiteral("like"), true);
        }
        if (snapshot.value(QStringLiteral("recasted")).toBool()) {
            rememberReactionOverride(m_viewerReactionSnapshot, castId, QStringLiteral("recast"), true);
        }
    }
    applyViewerReactionSnapshotTo(m_rows, m_viewerReactionSnapshot);
    applyViewerReactionSnapshotTo(m_writeRows, m_viewerReactionSnapshot);
    applyViewerReactionSnapshotTo(m_detail, m_viewerReactionSnapshot);
    applyReactionOverrides(m_rows, m_reactionOverrides);
    applyReactionOverrides(m_writeRows, m_reactionOverrides);
    applyReactionOverrides(m_detail, m_reactionOverrides);
    emit stateChanged();
}

void PresentationBridge::applyViewerReactionDelta(QVariantList reactions)
{
    for (const auto& value : reactions) {
        const QVariantMap reaction = value.toMap();
        const QString castId = reaction.value(QStringLiteral("castId")).toString();
        const QString canonical = canonicalCastKey(castId);
        if (canonical.isEmpty()) {
            continue;
        }

        const bool liked = reaction.value(QStringLiteral("liked")).toBool();
        const bool recasted = reaction.value(QStringLiteral("recasted")).toBool();
        m_viewerReactionSnapshot.remove(reactionOverrideKey(canonical, QStringLiteral("like")));
        m_viewerReactionSnapshot.remove(reactionOverrideKey(canonical, QStringLiteral("recast")));
        if (liked) {
            rememberReactionOverride(m_viewerReactionSnapshot, canonical, QStringLiteral("like"), true);
        }
        if (recasted) {
            rememberReactionOverride(m_viewerReactionSnapshot, canonical, QStringLiteral("recast"), true);
        }

        applyViewerReactionDeltaTo(m_rows, canonical, liked, recasted);
        applyViewerReactionDeltaTo(m_writeRows, canonical, liked, recasted);
        applyViewerReactionDeltaTo(m_detail, canonical, liked, recasted);
    }

    // Local optimistic/durable write projections are newer than an eventually
    // consistent network read and therefore remain the final authority.
    applyReactionOverrides(m_rows, m_reactionOverrides);
    applyReactionOverrides(m_writeRows, m_reactionOverrides);
    applyReactionOverrides(m_detail, m_reactionOverrides);
    emit stateChanged();
}

void PresentationBridge::applyFollowingProjections(QVariantList following)
{
    m_followOverrides.clear();
    for (const auto& value : following) {
        const QVariantMap projection = value.toMap();
        rememberFollowingOverride(
            m_followOverrides,
            projection.value(QStringLiteral("targetFid")).toULongLong(),
            projection.value(QStringLiteral("following")).toBool());
    }
    applyFollowingOverrides(m_rows, m_followOverrides);
    applyFollowingOverride(m_detail, m_followOverrides);
    emit stateChanged();
}

void PresentationBridge::applySearchSuggestions(
    QString query, QVariantList results, QString errorText, const bool busy)
{
    if (query.trimmed() != m_searchQuery) {
        return;
    }
    m_searchResults = std::move(results);
    m_searchErrorText = std::move(errorText);
    m_searchBusy = busy;
    emit searchChanged();
}

void PresentationBridge::applyLookupState(QString status, QString error, QString diagnostic,
    const bool busy, const bool stale, const bool sourceUnavailable)
{
    m_statusText = std::move(status);
    m_errorText = std::move(error);
    m_diagnosticText = std::move(diagnostic);
    m_isBusy = busy;
    m_isStale = stale;
    m_sourceUnavailable = sourceUnavailable;
    emit stateChanged();
}

void PresentationBridge::applyIdentity(const qulonglong fid, QString displayName,
    QString username, QVariantMap profile, QString status, QString error, QString diagnostic, const bool busy,
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
    if (fid != 0 && fid == m_accountFid && !profile.isEmpty()) {
        m_accountProfile = std::move(profile);
        m_profileDetailCache.insert(fid, m_accountProfile);
    }
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
    } else if (m_activeView == QStringLiteral("profile")) {
        m_profileFeed = std::move(selector);
    }
    const bool mergeLocalWrites = !m_writeRows.isEmpty() &&
        (m_activeView == QStringLiteral("feed") ||
         (m_activeView == QStringLiteral("profile") &&
          m_profileFeed == QStringLiteral("user-posts") &&
          m_profileFid == m_accountFid));
    if (mergeLocalWrites) {
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
    applyViewerReactionSnapshotTo(m_rows, m_viewerReactionSnapshot);
    applyReactionOverrides(m_rows, m_reactionOverrides);
    applyFollowingOverrides(m_rows, m_followOverrides);
    m_next = std::move(next);
    if (m_activeView == QStringLiteral("profile") && m_profileFid != 0) {
        const QString key = profileCacheKey(m_profileFid, m_profileFeed);
        m_profileListCache.insert(key, m_rows);
        m_profileNextCache.insert(key, m_next);
    }
    m_statusText = std::move(status);
    m_errorText = std::move(error);
    m_diagnosticText = std::move(diagnostic);
    m_isBusy = busy;
    m_isStale = stale;
    m_sourceUnavailable = sourceUnavailable;
    emit stateChanged();
}


void PresentationBridge::applyReferencedCastPreview(QString castId, QVariantMap preview)
{
    const QString targetKey = canonicalCastKey(std::move(castId));
    if (targetKey.isEmpty() || preview.isEmpty()) {
        return;
    }

    auto patchEmbeds = [&targetKey, &preview](QVariantMap& row) {
        QVariantList embeds = row.value(QStringLiteral("embeds")).toList();
        bool changed = false;
        for (qsizetype index = 0; index < embeds.size(); ++index) {
            QVariantMap embed = embeds[index].toMap();
            if (embed.value(QStringLiteral("kind")).toString() != QStringLiteral("cast-reference") ||
                canonicalCastKey(embed.value(QStringLiteral("castId")).toString()) != targetKey) {
                continue;
            }
            for (auto it = preview.cbegin(); it != preview.cend(); ++it) {
                if (it.key() != QStringLiteral("kind")) {
                    embed.insert(it.key(), it.value());
                }
            }
            embed.insert(QStringLiteral("kind"), QStringLiteral("cast-reference"));
            embeds[index] = std::move(embed);
            changed = true;
        }
        if (changed) {
            row.insert(QStringLiteral("embeds"), std::move(embeds));
        }
    };

    bool changed = false;
    for (qsizetype index = 0; index < m_rows.size(); ++index) {
        QVariantMap row = m_rows[index].toMap();
        if (canonicalCastKey(row.value(QStringLiteral("parentCastId")).toString()) == targetKey) {
            row.insert(QStringLiteral("parentPreview"), preview);
            changed = true;
        }
        const QVariantList before = row.value(QStringLiteral("embeds")).toList();
        patchEmbeds(row);
        if (before != row.value(QStringLiteral("embeds")).toList()) {
            changed = true;
        }
        m_rows[index] = std::move(row);
    }

    const QVariantList detailBefore = m_detail.value(QStringLiteral("embeds")).toList();
    patchEmbeds(m_detail);
    if (detailBefore != m_detail.value(QStringLiteral("embeds")).toList()) {
        changed = true;
    }

    if (changed) {
        emit stateChanged();
    }
}

void PresentationBridge::applyDetail(QString view, QVariantMap detail, QString status,
    QString error, QString diagnostic, const bool busy, const bool stale, const bool sourceUnavailable)
{
    m_activeView = std::move(view);
    m_detail = std::move(detail);
    if (m_activeView == QStringLiteral("profile")) {
        const qulonglong resolvedProfileFid = m_detail.value(QStringLiteral("fid")).toULongLong();
        if (resolvedProfileFid != 0) {
            m_profileFid = resolvedProfileFid;
        }
    }
    applyViewerReactionSnapshotTo(m_detail, m_viewerReactionSnapshot);
    applyReactionOverrides(m_detail, m_reactionOverrides);
    applyFollowingOverride(m_detail, m_followOverrides);
    if (m_activeView == QStringLiteral("profile")) {
        applyFollowsMeSnapshot(m_detail, m_followsMeSnapshot);
    }
    if (m_activeView == QStringLiteral("profile") && m_profileFid != 0) {
        m_profileDetailCache.insert(m_profileFid, m_detail);
        if (m_profileFid == m_accountFid) {
            m_accountProfile = m_detail;
        }
    }
    m_statusText = std::move(status);
    m_errorText = std::move(error);
    m_diagnosticText = std::move(diagnostic);
    m_isBusy = busy;
    m_isStale = stale;
    m_sourceUnavailable = sourceUnavailable;
    emit stateChanged();

}


void PresentationBridge::applyProfileFollowsMe(const qulonglong fid, const bool followsMe)
{
    if (fid == 0U) {
        return;
    }

    const QString key = QString::number(fid);
    m_followsMeSnapshot.insert(key, followsMe);

    bool changed = false;
    if (m_activeView == QStringLiteral("profile")
        && m_detail.value(QStringLiteral("fid")).toULongLong() == fid) {
        m_detail.insert(QStringLiteral("followsMe"), followsMe);
        m_detail.insert(QStringLiteral("followsMeKnown"), true);
        changed = true;
    }

    if (m_profileDetailCache.contains(fid)) {
        QVariantMap cached = m_profileDetailCache.value(fid);
        cached.insert(QStringLiteral("followsMe"), followsMe);
        cached.insert(QStringLiteral("followsMeKnown"), true);
        m_profileDetailCache.insert(fid, std::move(cached));
    }

    if (changed) {
        emit stateChanged();
    }
}

void PresentationBridge::applyVerificationFixture()
{
    m_runtimeReady = true;
    m_hasReadActivity = true;
    m_feedStarted = true;
    m_identityFid = 6841;
    m_identityName = QStringLiteral("Protocol Field Notes");
    m_statusText = qtTrId("genycaster.bridge.cached_refreshing_in_background");
    m_isBusy = true;
    m_isStale = true;
    m_networkReachabilityKnown = true;
    m_isOffline = false;
    m_sourceUnavailable = false;
    m_diagnosticText.clear();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_rows = {
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0x01")}, {QStringLiteral("author"), QStringLiteral("alice")}, {QStringLiteral("displayName"), QStringLiteral("Alice Lin")}, {QStringLiteral("text"), QStringLiteral("Native clients feel different when the cache is the product, not a fallback.")}, {QStringLiteral("createdAtMs"), nowMs - 120'000}, {QStringLiteral("replyCount"), 12}, {QStringLiteral("recastCount"), 4}, {QStringLiteral("likeCount"), 86}, {QStringLiteral("embeds"), QVariantList{QVariantMap{{QStringLiteral("kind"), QStringLiteral("url")}, {QStringLiteral("url"), QStringLiteral("https://example.com/native-client")}}}}},
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0x02")}, {QStringLiteral("author"), QStringLiteral("builders")}, {QStringLiteral("displayName"), QStringLiteral("Farcaster Builders")}, {QStringLiteral("text"), QStringLiteral("Channel telemetry is flowing through canonical local state.")}, {QStringLiteral("createdAtMs"), nowMs - 480'000}},
        QVariantMap{{QStringLiteral("castId"), QStringLiteral("0x03")}, {QStringLiteral("author"), QStringLiteral("violet")}, {QStringLiteral("displayName"), QStringLiteral("Violet")}, {QStringLiteral("text"), QStringLiteral("The network can disappear. The reading surface should not.")}, {QStringLiteral("createdAtMs"), nowMs - 840'000}}
    };
    m_next = QStringLiteral("fixture-next");
    emit stateChanged();
}
