#pragma once

// Native Qt window-lifecycle adapter. Never instantiated in mobile/WASM
// builds or in the diagnostic/smoke-test process.
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWindow>

namespace genycaster::ui {

class DesktopBackground final : public QObject {
public:
    explicit DesktopBackground(QWindow& window)
        : QObject{&window}, m_window{window},
          m_tray{QIcon{QStringLiteral(":/genycaster/app-icon.png")}}
    {
        QGuiApplication::setQuitOnLastWindowClosed(false);
        auto* show = m_menu.addAction(tr("Show Genycaster"));
        auto* quit = m_menu.addAction(tr("Quit Genycaster"));
        QObject::connect(show, &QAction::triggered, this, [this] { restore(); });
        QObject::connect(quit, &QAction::triggered, this, [this] { requestQuit(); });
        m_tray.setToolTip(QStringLiteral("Genycaster"));
        m_tray.setContextMenu(&m_menu);
        QObject::connect(&m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) restore();
            });
        // Qt registers the icon if a tray becomes available later.
        m_tray.show();

        // A desktop without a tray must still have an explicit exit path.
        auto* shortcut = new QShortcut(QKeySequence{Qt::CTRL | Qt::Key_Q}, &window);
        shortcut->setContext(Qt::ApplicationShortcut);
        QObject::connect(shortcut, &QShortcut::activated, this,
            [this] { requestQuit(); });

        qApp->installEventFilter(this);
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
            m_quitting = true;
            m_tray.hide();
        });
#if defined(Q_OS_MACOS)
        // The Dock activates the existing windowless app. Use the same window
        // rather than restarting authentication and runtime composition.
        QObject::connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive && !m_quitting &&
                    m_hiddenByClose && !m_window.isVisible()) restore();
            });
#endif
    }

    ~DesktopBackground() override
    {
        qApp->removeEventFilter(this);
        m_tray.hide();
        m_tray.setContextMenu(nullptr);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // Native Quit, updater restart and OS shutdown must not be turned into
        // hide-to-tray. Quit is delivered before Qt closes its windows.
        if (watched == qApp && event->type() == QEvent::Quit) {
            m_quitting = true;
            return false;
        }
        if (watched != &m_window || event->type() != QEvent::Close ||
            m_quitting || qApp->isSavingSession()) return false;

        event->ignore();
        if (!m_closePending) {
            m_closePending = true;
            // As in GenyDL: never hide from inside Cocoa's native close callback.
            QTimer::singleShot(0, this, [this] {
                if (!m_closePending) return;
                m_closePending = false;
                if (m_quitting) return;
#if !defined(Q_OS_MACOS)
                if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                    // E.g. GNOME without a status notifier: remain reachable
                    // from the taskbar rather than creating a headless process.
                    m_window.showMinimized();
                    return;
                }
#endif
                m_hiddenByClose = true;
                m_window.hide();
            });
        }
        return true;
    }

private:
    void restore()
    {
        if (m_quitting) return;
        m_closePending = false;
        m_hiddenByClose = false;
        if (m_window.visibility() == QWindow::Minimized)
            m_window.showNormal();
        else if (!m_window.isVisible())
            m_window.setVisible(true);
        m_window.raise();
        m_window.requestActivate();
    }

    void requestQuit()
    {
        m_quitting = true;
        m_closePending = false;
        QCoreApplication::quit();
    }

    QWindow& m_window;
    QMenu m_menu;
    QSystemTrayIcon m_tray;
    bool m_closePending{false};
    bool m_hiddenByClose{false};
    bool m_quitting{false};
};

} // namespace genycaster::ui
