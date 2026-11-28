/*!
 * @file ui/app/platform/desktop_background.hpp
 * @brief Native desktop window and background lifecycle.
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QWindow>
#include <memory>

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
