module;

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

module genycaster.ui.bootstrap.teardown;

namespace genycaster::ui::bootstrap {
void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow)
{
    // QML Loaders and effects must release their scene-graph resources while
    // the GUI dispatcher and every borrowed controller are still alive.
    if (mainWindow) {
        mainWindow->setPersistentGraphics(false);
        mainWindow->setPersistentSceneGraph(false);
        mainWindow->hide();
        mainWindow->releaseResources();
    }

    // Do not defer root deletion until the QQmlApplicationEngine destructor:
    // its referenced runtime may already have been released by then.
    // deleteLater() is invalid here because the main event loop has stopped.
    for (QObject* const root : engine.rootObjects())
        delete root;

    // Drain deferred child cleanup while Qt still has a GUI dispatcher;
    // never restart the event loop or admit new network requests.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
} // namespace genycaster::ui::bootstrap
