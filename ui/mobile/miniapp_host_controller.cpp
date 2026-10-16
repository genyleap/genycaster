#include "miniapp_host_controller.hpp"
#include <QSettings>
#include <QtTranslation>
#if defined(__EMSCRIPTEN__)
#include <QDesktopServices>
#include <QUrl>
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
    // WebAssembly has no macOS WebKit host. Launch as an isolated external
    // browser tab, without claiming Mini App SDK, signer or wallet support.
    const QUrl destination{url, QUrl::StrictMode};
    if (!destination.isValid() || destination.scheme() != QStringLiteral("https")
        || destination.host().isEmpty() || !destination.userName().isEmpty()
        || !destination.password().isEmpty()) {
        setState(false, QStringLiteral("Mini App URL must be a public HTTPS address."));
        return;
    }
    if (!QDesktopServices::openUrl(destination)) {
        setState(false, QStringLiteral("The browser could not open this Mini App."));
    } else {
        setState(false);
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
    emit launchRequested(url, title, iconSource.left(4096));
}
void MiniAppHostController::close() {
    emit closeRequested();
    setState(false);
}

void MiniAppHostController::closeAll() {
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
