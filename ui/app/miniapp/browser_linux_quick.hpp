/*!
 * @file ui/app/miniapp/browser_linux_quick.hpp
 * @brief Connect the Linux Qt WebEngine Quick Mini App drawer to the stable QML host window.
 */
#pragma once
#include <memory>
#include <QString>
#include <QUrl>

class QQmlEngine;
class QQuickWindow;

namespace farcaster::platform::miniapp {
class Browser;
struct BrowserCallbacks;
enum class PresentationMode;
enum class BrowserRole;

void setLinuxMiniAppQuickHost(QQuickWindow* window, QQmlEngine* engine);
[[nodiscard]] std::unique_ptr<Browser> createLinuxQuickBrowser(
    const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
    PresentationMode mode, bool reduceMotion, bool initiallyVisible);
}
