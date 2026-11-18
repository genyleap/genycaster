#include "host.hpp"
#include <QHostAddress>
#include <QUrl>
#include <QSettings>
#include <QDateTime>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <tuple>
#include <utility>
#if !defined(Q_OS_WASM)
#include <QProcess>
#endif
#include <algorithm>
#include <ranges>
#if !defined(Q_OS_WASM)
#include <QDesktopServices>
#endif
#include <QtTranslation>
#if defined(Q_OS_WASM)
#include <QHostAddress>
#include <QUrl>
#include <emscripten.h>
#endif

namespace {
struct WalletBrowserCandidate final {
    QString id;
    QString label;
    QString executable;
};

[[nodiscard]] QList<WalletBrowserCandidate> installedWalletBrowsers()
{
    QList<WalletBrowserCandidate> available{};
#if defined(Q_OS_MACOS)
    // Installed browsers are only candidates. The user's wallet extension
    // is detected by EIP-1193/EIP-6963 inside the browser, never by probing
    // or reading private browser profiles.
    for (const auto& [id, app] : {
             std::pair{QStringLiteral("brave"), QStringLiteral("Brave Browser")},
             std::pair{QStringLiteral("chrome"), QStringLiteral("Google Chrome")},
             std::pair{QStringLiteral("firefox"), QStringLiteral("Firefox")}}) {
        const QString bundle = app + QStringLiteral(".app");
        for (const QString& folder : {
                 QStringLiteral("/Applications"),
                 QDir::homePath() + QStringLiteral("/Applications"),
                 QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)}) {
            const QString application = QDir(folder).filePath(bundle);
            if (QFileInfo(application).isDir()) {
                // Launch the discovered bundle, not an ambiguous display
                // name. Its existence is not proof of a wallet extension.
                available.append({id, app, application});
                break;
            }
        }
    }
#elif defined(Q_OS_WIN)
    for (const auto& [id, name, executable] : {
             std::tuple{QStringLiteral("brave"), QStringLiteral("Brave Browser"), QStringLiteral("brave.exe")},
             std::tuple{QStringLiteral("chrome"), QStringLiteral("Google Chrome"), QStringLiteral("chrome.exe")},
             std::tuple{QStringLiteral("firefox"), QStringLiteral("Firefox"), QStringLiteral("firefox.exe")}}) {
        const QString onPath = QStandardPaths::findExecutable(executable);
        const QString base = qEnvironmentVariable("ProgramFiles");
        const QString appPath = id == QStringLiteral("brave")
            ? QStringLiteral("/BraveSoftware/Brave-Browser/Application/brave.exe")
            : id == QStringLiteral("chrome") ? QStringLiteral("/Google/Chrome/Application/chrome.exe")
                                            : QStringLiteral("/Mozilla Firefox/firefox.exe");
        const QString located = !onPath.isEmpty() ? onPath : base + appPath;
        if (QFileInfo::exists(located) && QFileInfo(located).isFile())
            available.append({id, name, located});
    }
#elif defined(Q_OS_LINUX)
    for (const auto& [id, name, executable] : {
             std::tuple{QStringLiteral("brave"), QStringLiteral("Brave Browser"), QStringLiteral("brave-browser")},
             std::tuple{QStringLiteral("chrome"), QStringLiteral("Google Chrome"), QStringLiteral("google-chrome")},
             std::tuple{QStringLiteral("firefox"), QStringLiteral("Firefox"), QStringLiteral("firefox")}}) {
        const QString located = QStandardPaths::findExecutable(executable);
        if (!located.isEmpty()) available.append({id, name, located});
    }
#endif
    return available;
}

[[nodiscard]] bool safeWalletSessionUrl(const QString& value)
{
    const QUrl url{value, QUrl::StrictMode};
    static const QRegularExpression sessionPath{
        QStringLiteral("^/session/[0-9a-f]{64}$")};
    return url.isValid() && url.scheme() == QStringLiteral("http")
        && url.host() == QStringLiteral("127.0.0.1") && url.port() > 0
        && url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment()
        && sessionPath.match(url.path()).hasMatch();
}

bool validPresentationMode(const QString& mode) {
    return mode == QStringLiteral("window") || mode == QStringLiteral("bottom")
        || mode == QStringLiteral("left") || mode == QStringLiteral("right");
}

// Native images may be shown through Qt's image://remote provider. Farcaster
// Mini Apps receive an ordinary HTTPS URL in sdk.context.user.pfpUrl, not a
// process-local QQuickImageProvider address that WKWebView cannot resolve.
[[nodiscard]] QString sdkProfileImageUrl(const QString& value)
{
    QString source = value.trimmed();
    if (source.isEmpty() || source.size() > 4096) return {};

    QUrl parsed{source, QUrl::StrictMode};
    if (parsed.scheme() == QStringLiteral("image")
        && parsed.host() == QStringLiteral("remote")
        && parsed.path().startsWith(QLatin1Char('/'))) {
        QString encodedText = parsed.path().mid(1);
        // Storage-policy sources have a version/category/manual-or-auto
        // envelope. Decode the actual final URL exactly as the project's
        // RemoteImageProvider does; never pass image://remote/v22/... to WKWebView.
        if (encodedText.startsWith(QStringLiteral("v22/"))) {
            const QStringList parts = encodedText.split(QLatin1Char('/'));
            if (parts.size() < 4) return {};
            encodedText = parts.mid(3).join(QLatin1Char('/'));
        }
        const QByteArray encoded = encodedText.toLatin1();
        if (encoded.isEmpty() || encoded.size() > 8192) return {};
        const QByteArray decoded = QByteArray::fromBase64(
            encoded, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
        source = QString::fromUtf8(decoded);
        parsed = QUrl{source, QUrl::StrictMode};
    }
    // Never expose local files, data blobs, credentials or internal service
    // URLs to an untrusted third-party Mini App.
    if (!parsed.isValid() || parsed.scheme() != QStringLiteral("https")
        || !parsed.userInfo().isEmpty() || parsed.port(443) != 443
        || parsed.host().size() > 253 || source.size() > 4096) return {};
    const QString host = parsed.host().toLower();
    if (!host.contains(QLatin1Char('.')) || host == QStringLiteral("localhost")
        || host.endsWith(QStringLiteral(".localhost"))
        || host.endsWith(QStringLiteral(".local"))
        || host.endsWith(QStringLiteral(".internal"))
        || host.endsWith(QStringLiteral(".test"))
        || host.endsWith(QStringLiteral(".invalid"))) return {};
    QHostAddress address{};
    if (address.setAddress(host)) return {};
    parsed.setFragment(QString{});
    return parsed.toString(QUrl::FullyEncoded);
}

[[nodiscard]] QVariantMap sanitizedMiniAppUser(const QVariantMap& input)
{
    const qulonglong fid = input.value(QStringLiteral("fid")).toULongLong();
    if (!fid || fid > 9007199254740991ULL) return {};
    QVariantMap result{
        {QStringLiteral("fid"), QVariant::fromValue(fid)},
        {QStringLiteral("username"), input.value(QStringLiteral("username")).toString().left(100)},
        {QStringLiteral("displayName"), input.value(QStringLiteral("displayName")).toString().left(140)}
    };
    const QString avatar = sdkProfileImageUrl(input.value(QStringLiteral("pfpUrl")).toString());
    if (!avatar.isEmpty()) result.insert(QStringLiteral("pfpUrl"), avatar);
    return result;
}
}
MiniAppHostController::MiniAppHostController(bool runtimeAvailable, QSettings& settings, QObject* parent)
    : QObject(parent), m_settings(settings), m_available(runtimeAvailable) {
    const QString stored = m_settings.value(QStringLiteral("miniapps/presentationMode")).toString();
    if (validPresentationMode(stored)) { m_presentationMode = stored; }
#if defined(Q_OS_WASM)
    m_browserVisibilityTimer.setInterval(300);
    connect(&m_browserVisibilityTimer, &QTimer::timeout, this, [this] {
        const bool visible = EM_ASM_INT({
            return window.__genycasterMiniAppOverlay
                && window.__genycasterMiniAppOverlay.active() ? 1 : 0;
        }) != 0;
        if (!visible) {
            m_browserVisibilityTimer.stop();
            setState(false);
        }
    });
#endif
}
bool MiniAppHostController::walletBrowserPickerActive() const noexcept
{
    return !m_walletBrowserSessionUrl.isEmpty() && !m_walletBrowserWaiting;
}

bool MiniAppHostController::walletBrowserWaiting() const noexcept
{
    return !m_walletBrowserSessionUrl.isEmpty() && m_walletBrowserWaiting;
}

QString MiniAppHostController::walletBrowserAppName() const { return m_walletBrowserAppName; }
QString MiniAppHostController::walletBrowserOrigin() const { return m_walletBrowserOrigin; }
QString MiniAppHostController::walletBrowserSelectedName() const { return m_walletBrowserSelectedName; }

QVariantList MiniAppHostController::walletBrowserOptions() const
{
    return m_walletBrowserOptions;
}

QString MiniAppHostController::walletBrowserError() const
{
    return m_walletBrowserError;
}

void MiniAppHostController::setWalletBrowserContext(
    const QString& appName, const QString& origin)
{
    m_walletBrowserAppName = appName.left(80);
    m_walletBrowserOrigin = origin.left(512);
    emit walletBrowserPickerChanged();
}

void MiniAppHostController::walletBrowserSessionEnded()
{
    const bool changed = !m_walletBrowserSessionUrl.isEmpty()
        || m_walletBrowserWaiting || !m_walletBrowserError.isEmpty();
    m_walletBrowserSessionUrl.clear();
    m_walletBrowserSelectedId.clear();
    m_walletBrowserSelectedName.clear();
    m_walletBrowserWaiting = false;
    m_walletBrowserError.clear();
    m_walletBrowserAppName.clear();
    m_walletBrowserOrigin.clear();
    if (changed) emit walletBrowserPickerChanged();
}

void MiniAppHostController::walletBrowserRequestFinished(const bool successful)
{
    if (m_walletBrowserSessionUrl.isEmpty()) return;
    // Opening an executable is not proof that its wallet was usable:
    // remember the preference only after the real wallet replied successfully.
    if (successful && !m_walletBrowserSelectedId.isEmpty()
        && m_walletBrowserSelectedId != QStringLiteral("system")) {
        m_settings.setValue(QStringLiteral("miniapps/walletBrowserChoice"),
                            m_walletBrowserSelectedId);
    }
    m_walletBrowserSessionUrl.clear();
    m_walletBrowserWaiting = false;
    m_walletBrowserError.clear();
    m_walletBrowserSelectedId.clear();
    m_walletBrowserSelectedName.clear();
    emit walletBrowserPickerChanged();
}

void MiniAppHostController::requestWalletBrowser(const QString& localSessionUrl)
{
    // A launch callback can arrive after a Mini App was dismissed or
    // switched; its session URL must not resurrect a stale wallet picker.
    if (m_walletBrowserAppName.isEmpty() || !safeWalletSessionUrl(localSessionUrl))
        return;
    if (m_walletBrowserSessionUrl == localSessionUrl)
        return;
    m_walletBrowserSessionUrl = localSessionUrl;
    m_walletBrowserWaiting = false;
    m_walletBrowserError.clear();
    m_walletBrowserSelectedId.clear();
    m_walletBrowserSelectedName.clear();
    m_walletBrowserOptions.clear();
    const auto candidates = installedWalletBrowsers();
    for (const auto& candidate : candidates) {
        m_walletBrowserOptions.append(QVariantMap{
            {QStringLiteral("id"), candidate.id},
            {QStringLiteral("label"), candidate.label},
            {QStringLiteral("icon"), candidate.id},
        });
    }
    m_walletBrowserOptions.append(QVariantMap{
        {QStringLiteral("id"), QStringLiteral("system")},
        {QStringLiteral("label"), QStringLiteral("Default browser")},
        {QStringLiteral("icon"), QStringLiteral("system")},
    });

    const QString previous = m_settings.value(
        QStringLiteral("miniapps/walletBrowserChoice")).toString();
    if (!previous.isEmpty() && previous != QStringLiteral("system")) {
        for (qsizetype index = 0; index < m_walletBrowserOptions.size(); ++index) {
            const auto option = m_walletBrowserOptions.at(index).toMap();
            if (option.value(QStringLiteral("id")).toString() == previous) {
                m_walletBrowserOptions.removeAt(index);
                auto labeled = option;
                labeled.insert(QStringLiteral("lastUsed"), true);
                m_walletBrowserOptions.prepend(labeled);
                break;
            }
        }
    }
    emit walletBrowserPickerChanged();
}

void MiniAppHostController::chooseWalletBrowser(const QString& browserId)
{
#if defined(Q_OS_WASM)
    Q_UNUSED(browserId);
    return;
#else
    if (!walletBrowserPickerActive() || !safeWalletSessionUrl(m_walletBrowserSessionUrl))
        return;
    const auto candidates = installedWalletBrowsers();
    const QUrl url{m_walletBrowserSessionUrl, QUrl::StrictMode};
    bool launched = false;
    QString selectedName{};
    if (browserId == QStringLiteral("system")) {
        selectedName = QStringLiteral("default browser");
        launched = QDesktopServices::openUrl(url);
    } else {
        const auto selected = std::ranges::find_if(candidates, [&browserId](const auto& item) {
            return item.id == browserId;
        });
        if (selected == candidates.end()) return;
        selectedName = selected->label;
#if defined(Q_OS_MACOS)
        launched = QProcess::startDetached(QStringLiteral("/usr/bin/open"),
            {QStringLiteral("-a"), selected->executable, url.toString(QUrl::FullyEncoded)});
#else
        launched = QProcess::startDetached(selected->executable,
            {url.toString(QUrl::FullyEncoded)});
#endif
    }
    if (!launched) {
        m_walletBrowserError = qtTrId("genycaster.wallet.browser_open_failed");
        emit walletBrowserPickerChanged();
        return;
    }
    // Do not discard the session token or dismiss the pending RPC here.
    // The same Mini App stays alive until wallet response/cancel/timeout.
    m_walletBrowserSelectedId = browserId;
    m_walletBrowserSelectedName = selectedName;
    m_walletBrowserWaiting = true;
    m_walletBrowserError.clear();
    emit walletBrowserPickerChanged();
#endif
}

void MiniAppHostController::cancelWalletBrowser()
{
    if (m_walletBrowserSessionUrl.isEmpty()) return;
    m_walletBrowserSessionUrl.clear();
    m_walletBrowserWaiting = false;
    m_walletBrowserError.clear();
    m_walletBrowserSelectedId.clear();
    m_walletBrowserSelectedName.clear();
    emit walletBrowserPickerChanged();
    // User cancellation rejects the current wallet request but must NEVER
    // close the entire native Mini App or its unrelated browser window.
    emit walletBrowserCancelRequested();
}

QString MiniAppHostController::presentationMode() const { return m_presentationMode; }
void MiniAppHostController::setPresentationMode(const QString& mode) {
    if (!validPresentationMode(mode) || mode == m_presentationMode) { return; }
    m_presentationMode = mode;
    m_settings.setValue(QStringLiteral("miniapps/presentationMode"), mode);
#if defined(Q_OS_WASM)
    const QByteArray modeUtf8 = mode.toUtf8();
    EM_ASM({
        const host=window.__genycasterMiniAppOverlay;
        if(host && host.setMode) host.setMode(UTF8ToString($0,$1));
    }, modeUtf8.constData(), modeUtf8.size());
#endif
    emit presentationModeChanged();
}
void MiniAppHostController::setReduceMotion(bool reduceMotion) noexcept { m_reduceMotion = reduceMotion; }
bool MiniAppHostController::reduceMotion() const noexcept { return m_reduceMotion; }
bool MiniAppHostController::available() const noexcept { return m_available; }
bool MiniAppHostController::active() const noexcept { return m_active; }
int MiniAppHostController::sessionLimit() const noexcept { return m_sessionLimit; }
void MiniAppHostController::setSessionLimit(const int limit)
{
    const int normalized = std::clamp(limit,1,4);
    if (m_sessionLimit == normalized) return;
    m_sessionLimit = normalized;
#if defined(Q_OS_WASM)
    EM_ASM({
        const host = window.__genycasterMiniAppOverlay;
        if (host && host.setLimit) host.setLimit($0);
    }, m_sessionLimit);
#endif
    emit sessionLimitChanged();
}
QString MiniAppHostController::errorText() const { return m_error; }
QString MiniAppHostController::launchUrl() const { return m_launchUrl; }

bool MiniAppHostController::swapRequestActive() const noexcept
{
    return !m_swapRequestId.isEmpty();
}

QString MiniAppHostController::swapRequestId() const { return m_swapRequestId; }
QVariantMap MiniAppHostController::swapRequest() const { return m_swapRequest; }

void MiniAppHostController::presentSwapRequest(
    const QString& requestId, const QVariantMap& details)
{
    if (requestId.isEmpty() || !m_swapRequestId.isEmpty()) return;
    m_swapRequestId = requestId;
    m_swapRequest = details;
    m_swapQuote.clear();
    m_swapQuoteError.clear();
    m_swapQuoteBusy = false;
    emit swapRequestChanged();
}

void MiniAppHostController::clearSwapRequest(const QString& requestId)
{
    if (requestId.isEmpty() || m_swapRequestId != requestId) return;
    m_swapRequestId.clear();
    m_swapRequest.clear();
    m_swapQuote.clear();
    m_swapQuoteError.clear();
    m_swapQuoteBusy = false;
    emit swapRequestChanged();
}

bool MiniAppHostController::swapQuoteBusy() const noexcept { return m_swapQuoteBusy; }
QVariantMap MiniAppHostController::swapQuote() const { return m_swapQuote; }
QString MiniAppHostController::swapQuoteError() const { return m_swapQuoteError; }

void MiniAppHostController::beginSwapQuote(
    const QString& requestId, const QVariantMap& editedIntent)
{
    if (requestId != m_swapRequestId || requestId.isEmpty() || m_swapQuoteBusy) return;
    m_swapQuoteBusy = true;
    m_swapQuote.clear();
    m_swapQuoteError.clear();
    emit swapRequestChanged();
    emit swapQuotePreparationRequested(requestId, editedIntent);
}

void MiniAppHostController::dispatchSwapQuote(
    const QString& requestId, const QVariantMap& parameters)
{
    if (requestId == m_swapRequestId && m_swapQuoteBusy)
        emit swapQuoteNetworkRequested(requestId, parameters);
}

void MiniAppHostController::receiveSwapQuoteResult(
    const QString& requestId, const QVariantMap& quote, const QString& error)
{
    if (requestId == m_swapRequestId && m_swapQuoteBusy)
        emit swapQuoteResultReceived(requestId, quote, error);
}

void MiniAppHostController::showSwapQuote(
    const QString& requestId, const QVariantMap& verifiedQuote)
{
    if (requestId != m_swapRequestId || !m_swapQuoteBusy) return;
    m_swapQuoteBusy = false;
    m_swapQuoteError.clear();
    m_swapQuote = verifiedQuote;
    emit swapRequestChanged();
    // Quotes are transient, never cache them as durable execution authority.
    const qint64 expiry = verifiedQuote.value(QStringLiteral("expiresAt")).toLongLong();
    const qint64 remaining = expiry - QDateTime::currentMSecsSinceEpoch();
    if (remaining <= 0 || remaining > 25'000) {
        invalidateSwapQuote(requestId);
        return;
    }
    QTimer::singleShot(static_cast<int>(remaining), this, [this, requestId, expiry] {
        if (m_swapRequestId == requestId
            && m_swapQuote.value(QStringLiteral("expiresAt")).toLongLong() == expiry)
            invalidateSwapQuote(requestId);
    });
}

void MiniAppHostController::invalidateSwapQuote(const QString& requestId)
{
    if (requestId != m_swapRequestId || requestId.isEmpty()) return;
    m_swapQuote.clear();
    m_swapQuoteError.clear();
    emit swapRequestChanged();
}

void MiniAppHostController::setSwapQuoteError(
    const QString& requestId, const QString& error)
{
    if (requestId != m_swapRequestId) return;
    m_swapQuoteBusy = false;
    m_swapQuote.clear();
    m_swapQuoteError = error.left(128);
    emit swapRequestChanged();
}

void MiniAppHostController::cancelSwapToken(const QString& requestId)
{
    if (requestId.isEmpty() || requestId != m_swapRequestId) return;
    clearSwapRequest(requestId);
    emit swapTokenCancelled(requestId);
}
QVariantMap MiniAppHostController::user() const { return m_user; }
void MiniAppHostController::setState(bool active, const QString& error) {
    if (m_active == active && m_error == error) { return; }
    m_active = active;
    m_error = error;
    emit stateChanged();
}
void MiniAppHostController::setUser(const QVariantMap& user) {
    const QVariantMap publicUser = sanitizedMiniAppUser(user);
    if (m_user == publicUser) return;
    const auto previousFid = m_user.value(QStringLiteral("fid")).toULongLong();
    const auto nextFid = publicUser.value(QStringLiteral("fid")).toULongLong();
    // An anonymous-to-authenticated switch is also an identity change.
    // Do not leak the previous iframe's session/context across accounts.
    // Background/minimized Mini Apps remain live even when m_active is
    // false. Revoke every isolated document on an account identity change.
    if (previousFid != nextFid) closeAll();
    m_user = publicUser;
    emit userChanged();
#if defined(Q_OS_WASM)
    const auto json = QJsonDocument(QJsonObject::fromVariantMap(publicUser))
                          .toJson(QJsonDocument::Compact);
    EM_ASM({
        const host = window.__genycasterMiniAppOverlay;
        if (host && host.setUser) {
            try { host.setUser(JSON.parse(UTF8ToString($0,$1))); } catch {}
        }
    }, json.constData(), json.size());
#endif
}
void MiniAppHostController::open(const QString& url, const QString& title)
{
    openWithMetadata(url, title, QString{});
}

void MiniAppHostController::openWithMetadata(const QString& url,
                                             const QString& title,
                                             const QString& iconSource)
{
    // A nested QML TapHandler may observe the same button gesture. Runtime-level
    // session deduplication handles repeated launches without destroying other apps.
    m_launchUrl.clear();
    m_error.clear();
    emit stateChanged();
    // Only public HTTPS origins may open in unprivileged web content.
    // Check both web and native before dispatching to a platform host.
    const QUrl destination{url.trimmed(), QUrl::StrictMode};
    const QString host = destination.host().toLower();
    QHostAddress numericHost{};
    if (!destination.isValid() || destination.scheme() != QStringLiteral("https")
        || destination.port(443) != 443 || !host.contains(QLatin1Char('.'))
        || host.endsWith(QLatin1Char('.'))
        || host.endsWith(QStringLiteral(".localhost"))
        || host.endsWith(QStringLiteral(".local"))
        || host.endsWith(QStringLiteral(".internal"))
        || host.endsWith(QStringLiteral(".test"))
        || host.endsWith(QStringLiteral(".invalid"))
        || numericHost.setAddress(host)
        || !destination.userName().isEmpty()
        || !destination.password().isEmpty()) {
        setState(false, qtTrId("genycaster.miniapp.unavailable"));
        return;
    }
    m_launchUrl = destination.toString(QUrl::FullyEncoded);
    emit stateChanged();
#if defined(Q_OS_WASM)
    // WebAssembly cannot create the native macOS WKWebView. Host public,
    // embeddable Mini Apps in a sandboxed in-page iframe instead of launching
    // an unrelated browser tab. Do not claim that the unprivileged iframe
    // has the desktop Mini App SDK or wallet signer permissions.
    const QByteArray launchUtf8 = destination.toString(QUrl::FullyEncoded).toUtf8();
    const QByteArray titleUtf8 = title.left(160).toUtf8();
    const QByteArray closeUtf8 = qtTrId("genycaster.locale.close").toUtf8();
    const QByteArray modeUtf8 = m_presentationMode.toUtf8();
    const int opened = EM_ASM_INT({
        const host = window.__genycasterMiniAppOverlay;
        if (!host) return 0;
        return host.open(UTF8ToString($0, $1), UTF8ToString($2, $3),
            UTF8ToString($4, $5), UTF8ToString($6,$7)) ? 1 : 0;
    }, launchUtf8.constData(), launchUtf8.size(),
       titleUtf8.constData(), titleUtf8.size(),
       closeUtf8.constData(), closeUtf8.size(),
       modeUtf8.constData(), modeUtf8.size());
    if (opened == 0) {
        setState(false, qtTrId("genycaster.miniapp.unavailable"));
    } else {
        setState(true);
        m_browserVisibilityTimer.start();
    }
    return;
#endif
    if (m_user.value(QStringLiteral("fid")).toULongLong() == 0) {
        setState(false, qtTrId("genycaster.miniapp.sign_in_first"));
        return;
    }
    if (!m_available) {
        // The Mini App "Open" control is for the in-app SDK host. Linux
        // must not silently delegate an untrusted app to Firefox and bypass
        // the origin-bound host, wallet-consent and session lifecycle.
        // A separate, explicit "Open in browser" action may use the OS
        // browser, but must never be conflated with opening a Mini App.
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        setState(false, qtTrId("genycaster.miniapp.unavailable"));
#elif !defined(Q_OS_WASM) && !defined(Q_OS_ANDROID)
        if (QDesktopServices::openUrl(destination)) {
            emit externalLaunchSucceeded();
            setState(false);
        } else {
            setState(false, qtTrId("genycaster.miniapp.unavailable"));
        }
#else
        setState(false, qtTrId("genycaster.miniapp.unavailable"));
#endif
        return;
    }
    emit launchRequested(destination.toString(QUrl::FullyEncoded), title, iconSource.left(4096));
}
void MiniAppHostController::close() {
#if defined(Q_OS_WASM)
    m_browserVisibilityTimer.stop();
    EM_ASM({
        if (window.__genycasterMiniAppOverlay)
            window.__genycasterMiniAppOverlay.close();
    });
#endif
    emit closeRequested();
    setState(false);
}

void MiniAppHostController::closeAll() {
#if defined(Q_OS_WASM)
    m_browserVisibilityTimer.stop();
    EM_ASM({
        if (window.__genycasterMiniAppOverlay)
            window.__genycasterMiniAppOverlay.close();
    });
#endif
    emit closeAllRequested();
    setState(false);
}

void MiniAppHostController::requestShare(const QString& url, const QString& title)
{
    if (!url.trimmed().isEmpty()) {
        emit shareRequested(url, title, QString{});
    }
}

void MiniAppHostController::requestSessionShare(const QString& url,
                                                const QString& title,
                                                const QString& taskId)
{
    if (!url.trimmed().isEmpty() && !taskId.isEmpty()) {
        emit shareRequested(url, title, taskId);
    }
}

void MiniAppHostController::requestComposeCast(const QString& requestId, const QVariantMap& options)
{
    if (!requestId.isEmpty()) {
        emit composeCastRequested(requestId, options);
    }
}

void MiniAppHostController::completeComposeCast(const QString& requestId,
                                                const QString& castId,
                                                const QString& text,
                                                const QVariantList& embeds)
{
    if (requestId.isEmpty()) {
        return;
    }
    emit composeCastCompleted(requestId, QVariantMap{
        {QStringLiteral("castId"), castId},
        {QStringLiteral("text"), text},
        {QStringLiteral("embeds"), embeds},
    });
}

void MiniAppHostController::cancelComposeCast(const QString& requestId)
{
    if (!requestId.isEmpty()) {
        emit composeCastCancelled(requestId);
    }
}
