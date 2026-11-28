#include "platform/background.hpp"

#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QWindow>

int main(int argc, char** argv)
{
    QApplication app{argc, argv};
    QWindow window;
    window.resize(320, 240);
    auto* background = new genycaster::ui::DesktopBackground{window};
    int phase = 0;
    bool quitObserved = false;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
        [&] { quitObserved = true; });
    window.show();
    QTimer::singleShot(0, &app, [&] {
        if (window.close() || QGuiApplication::quitOnLastWindowClosed()) {
            app.exit(1);
            return;
        }
        phase = 1;
        // Close is deliberately deferred to avoid re-entering Cocoa.
        QTimer::singleShot(50, &app, [&] {
            if (window.isVisible() && window.visibility() != QWindow::Minimized) {
                app.exit(2);
                return;
            }
            background->restore();
            if (!window.isVisible() || window.visibility() == QWindow::Minimized) {
                app.exit(3);
                return;
            }
            phase = 2;
            // Updater/native Quit must bypass close-to-background even while
            // the main window is visible.
            QCoreApplication::quit();
        });
    });
    QTimer::singleShot(3000, &app, [&] { app.exit(4); });
    const int code = app.exec();
    if (code != 0) return code;
    if (phase != 2 || !quitObserved) return 5;
    qInfo() << "Desktop close, deferred background, restore and real quit passed";
    return 0;
}
