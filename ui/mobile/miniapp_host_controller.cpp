#include "miniapp_host_controller.hpp"
#include <QHostAddress>
#include <QUrl>
#include <QSettings>
#include <QByteArray>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#if !defined(__EMSCRIPTEN__)
#include <QDesktopServices>
#endif
#include <QtTranslation>
#if defined(__EMSCRIPTEN__)
#include <QHostAddress>
#include <QUrl>
#include <emscripten.h>
#endif

namespace {
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
        const QByteArray encoded = parsed.path().mid(1).toLatin1();
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
#if defined(__EMSCRIPTEN__)
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
QString MiniAppHostController::presentationMode() const { return m_presentationMode; }
void MiniAppHostController::setPresentationMode(const QString& mode) {
    if (!validPresentationMode(mode) || mode == m_presentationMode) { return; }
    m_presentationMode = mode;
    m_settings.setValue(QStringLiteral("miniapps/presentationMode"), mode);
#if defined(__EMSCRIPTEN__)
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
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        const host = window.__genycasterMiniAppOverlay;
        if (host && host.setLimit) host.setLimit($0);
    }, m_sessionLimit);
#endif
    emit sessionLimitChanged();
}
QString MiniAppHostController::errorText() const { return m_error; }
QString MiniAppHostController::launchUrl() const { return m_launchUrl; }
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
#if defined(__EMSCRIPTEN__)
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
#if defined(__EMSCRIPTEN__)
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
        // A supported system browser is safer than pretending Windows/Linux
        // ship an embedded SDK runtime. Never bridge signer/wallet permissions.
        // The Open button remains functional without weakening the native
        // sandbox or silently marking a session active.
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
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
#if defined(__EMSCRIPTEN__)
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
#if defined(__EMSCRIPTEN__)
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
