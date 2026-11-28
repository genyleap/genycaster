#include "desktop_background.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMetaObject>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTimer>

namespace genycaster::ui {
DesktopBackgroundController::DesktopBackgroundController(QApplication& application,
                                                         QWindow& window)
    : m_application(application), m_window(&window),
      m_menu(std::make_unique<QMenu>()),
      m_tray(std::make_unique<QSystemTrayIcon>(QGuiApplication::windowIcon())),
      m_quitShortcut(new QShortcut(QKeySequence::Quit, &window))
{
    QApplication::setQuitOnLastWindowClosed(false);
    // Application filter also sees native Quit and application activation.
    // Only the main window's Close event is intercepted.
    m_application.installEventFilter(this);
    auto* open = m_menu->addAction(QCoreApplication::translate("DesktopBackground", "Open Genycaster"));
    connect(open, &QAction::triggered, this, &DesktopBackgroundController::showMainWindow);
    m_menu->addSeparator();
    auto* quit = m_menu->addAction(QCoreApplication::translate("DesktopBackground", "Quit Genycaster"));
    connect(quit, &QAction::triggered, this, &DesktopBackgroundController::quitApplication);
    m_tray->setToolTip(QStringLiteral("Genycaster"));
    m_tray->setContextMenu(m_menu.get());
    connect(m_tray.get(), &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showMainWindow();
    });
    connect(m_tray.get(), &QSystemTrayIcon::messageClicked,
            this, &DesktopBackgroundController::showMainWindow);
    // show() also allows Qt to register if a tray becomes available later.
    m_tray->show();
    if (m_quitShortcut->keys().isEmpty())
        m_quitShortcut->setKey(QKeySequence{Qt::CTRL | Qt::Key_Q});
    m_quitShortcut->setContext(Qt::ApplicationShortcut);
    connect(m_quitShortcut.data(), &QShortcut::activated,
            this, &DesktopBackgroundController::quitApplication);
    connect(&application, &QCoreApplication::aboutToQuit, this, [this] {
        m_quitting = true;
        m_shutdownStarted = true;
        m_tray->hide();
    });
#if defined(Q_OS_MACOS)
    connect(&application, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && m_window && !m_window->isVisible())
            showMainWindow();
    });
#endif
}

DesktopBackgroundController::~DesktopBackgroundController()
{
    m_application.removeEventFilter(this);
    delete m_quitShortcut.data();
    m_tray->hide();
    m_tray->setContextMenu(nullptr);
}

void DesktopBackgroundController::showMainWindow()
{
    if (!m_window || m_quitting) return;
    m_closeQueued = false;
    if (m_window->visibility() == QWindow::Hidden ||
        m_window->visibility() == QWindow::Minimized)
        m_window->setVisibility(m_restoreVisibility);
    m_window->raise();
    m_window->requestActivate();
}

void DesktopBackgroundController::quitApplication()
{
    // Qt owns the final shutdown/worker teardown, including update restarts.
    QCoreApplication::quit();
}

bool DesktopBackgroundController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == &m_application && event->type() == QEvent::Quit) {
        m_quitting = true;
        // If another window rejects a quit request, do not permanently
        // disable close-to-background for subsequent user actions.
        QTimer::singleShot(0, this, [this] { if (!m_shutdownStarted) m_quitting = false; });
        return false;
    }
    if (watched != m_window || event->type() != QEvent::Close || m_quitting)
        return false;
#ifndef QT_NO_SESSIONMANAGER
    if (m_application.isSavingSession()) return false;
#endif
    event->ignore();
    if (!m_closeQueued) {
        m_closeQueued = true;
        if (m_window->visibility() != QWindow::Minimized &&
            m_window->visibility() != QWindow::Hidden)
            m_restoreVisibility = m_window->visibility();
        // Do not hide synchronously from a native Cocoa close event (GenyDL
        // established this reentrancy boundary). No tray: retain a reachable
        // taskbar/window-manager entry instead of stranding a hidden process.
        QMetaObject::invokeMethod(this, [this] {
            if (!m_closeQueued || m_quitting || !m_window) return;
            m_closeQueued = false;
            if (QSystemTrayIcon::isSystemTrayAvailable())
                m_window->hide();
            else
                m_window->showMinimized();
        }, Qt::QueuedConnection);
    }
    return true;
}
}
