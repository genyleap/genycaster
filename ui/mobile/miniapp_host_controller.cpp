#include "miniapp_host_controller.hpp"
#include <QHostAddress>
#include <QUrl>
#include <QSettings>
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
    emit presentationModeChanged();
}
void MiniAppHostController::setReduceMotion(bool reduceMotion) noexcept { m_reduceMotion = reduceMotion; }
bool MiniAppHostController::reduceMotion() const noexcept { return m_reduceMotion; }
bool MiniAppHostController::available() const noexcept { return m_available; }
bool MiniAppHostController::active() const noexcept { return m_active; }
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
    if (!m_user.isEmpty() && m_user.value(QStringLiteral("fid")) != user.value(QStringLiteral("fid"))) {
        closeAll();
    }
    m_user = user;
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
    m_launchUrl = url;
    m_error.clear();
    emit stateChanged();
#if defined(__EMSCRIPTEN__)
    // WebAssembly cannot create the native macOS WKWebView. Host public,
    // embeddable Mini Apps in a sandboxed in-page iframe instead of launching
    // an unrelated browser tab. Do not claim that the unprivileged iframe
    // has the desktop Mini App SDK or wallet signer permissions.
    const QUrl destination{url, QUrl::StrictMode};
    QHostAddress numericHost{};
    const QString host = destination.host().toLower();
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
    const QByteArray launchUtf8 = destination.toString(QUrl::FullyEncoded).toUtf8();
    const QByteArray titleUtf8 = title.left(160).toUtf8();
    const QByteArray closeUtf8 = qtTrId("genycaster.locale.close").toUtf8();
    const int opened = EM_ASM_INT({
        const host = window.__genycasterMiniAppOverlay;
        if (!host) return 0;
        return host.open(UTF8ToString($0, $1), UTF8ToString($2, $3),
            UTF8ToString($4, $5)) ? 1 : 0;
    }, launchUtf8.constData(), launchUtf8.size(),
       titleUtf8.constData(), titleUtf8.size(),
       closeUtf8.constData(), closeUtf8.size());
    if (opened == 0) {
        setState(false, qtTrId("genycaster.miniapp.unavailable"));
    } else {
        setState(true);
        m_browserVisibilityTimer.start();
    }
    return;
#endif
    if (!m_available) {
        setState(false, qtTrId("genycaster.miniapp.unavailable"));
        return;
    }
    if (m_user.value(QStringLiteral("fid")).toULongLong() == 0) {
        setState(false, qtTrId("genycaster.miniapp.sign_in_first"));
        return;
    }
    // A catalog URL is still untrusted. Android's WebView must never receive
    // file/content/local-network entry points or credentials in its origin.
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
