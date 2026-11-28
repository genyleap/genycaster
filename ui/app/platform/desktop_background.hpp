/*!
 * @file        ui/app/platform/desktop_background.hpp
 * @brief       Native desktop window and background lifecycle.
 *
 * @details
 * Separates window close, task restoration and application shutdown.
 *
 * @author      Genyleap contributors
 * @since       28 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QWindow>
#include <memory>

class QAction;
class QEvent;
class QMenu;
class QSystemTrayIcon;
class QShortcut;
class QApplication;

// One main-window owner. Close is not Quit; normal Qt/OS shutdown remains
// authoritative. The class has no dependency on QML or account credentials.
namespace genycaster::ui {
class DesktopBackgroundController final : public QObject {
public:
    DesktopBackgroundController(QApplication& application, QWindow& window);
    ~DesktopBackgroundController() override;
    void showMainWindow();
    void quitApplication();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void retranslate();
    QAction* m_openAction{};
    QAction* m_quitAction{};
    QApplication& m_application;
    QPointer<QWindow> m_window;
    std::unique_ptr<QMenu> m_menu;
    std::unique_ptr<QSystemTrayIcon> m_tray;
    QPointer<QShortcut> m_quitShortcut;
    bool m_closeQueued{false};
    bool m_quitting{false};
    bool m_shutdownStarted{false};
    QWindow::Visibility m_restoreVisibility{QWindow::Windowed};
};
}
