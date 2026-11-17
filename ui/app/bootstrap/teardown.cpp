module;

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>

module genycaster.ui.bootstrap.teardown;

namespace genycaster::ui::bootstrap {
void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow)
{
    // Crash 2026-11-17 (macOS/Qt 6.12): deleting the ApplicationWindow
    // destroyed QQuickOverlay after the QQuickWindow's graphics/window state
    // had already been invalidated, faulting in QQuickItemPrivate::derefWindow.
    // Do not force releaseResources() or disable scene persistence before the
    // QML object tree has been destroyed. Qt releases the graph itself when
    // the last QQuickWindow dies.
    if (mainWindow) {
        mainWindow->setPersistentGraphics(true);
        mainWindow->setPersistentSceneGraph(true);

        // QQuickOverlay is an item owned by the content item. Detach its
        // *visual* parent while QQuickWindow is still fully valid; keep the
        // QObject parent intact so Qt still destroys it exactly once.
        // Never construct a new overlay during shutdown.
        if (QQuickItem* const content = mainWindow->contentItem()) {
            for (QQuickItem* const child : content->childItems()) {
                if (child && child->inherits("QQuickOverlay"))
                    child->setParentItem(nullptr);
            }
        }
        mainWindow->hide();
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
