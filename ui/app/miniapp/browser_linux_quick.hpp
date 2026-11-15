/*!
 * @file        ui/app/miniapp/browser_linux_quick.hpp
 * @brief       Linux Qt WebEngine Quick host bridge for Mini Apps.
 *
 * @details
 * Binds the native WebEngine browser session to its QML host window.
 * Supplies reduced-motion and presentation metadata without exposing secrets.
 *
 * @author      Genyleap Engineering
 * @since       15 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
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
