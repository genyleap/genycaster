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
    // macOS / Qt 6.12: QQuickWindow::~QQuickWindow() clears its internal
    // contentItem pointer *before* deleting the QQuickRootItem. Its QObject
    // descendants (notably QQuickOverlay and QQuickPopupItem) must therefore
    // lose their visual window association while QQuickWindow is still alive.
    // Detaching only the overlay was insufficient: the remaining QML scene
    // could reparent popup items during teardown, leaving a QQuickOverlay
    // attached when its destructor called QQuickItemPrivate::derefWindow().
    // Keep QObject ownership unchanged; the engine still deletes each root.
    const auto detachScene = [](QQuickWindow* const window) {
        if (!window) return;
        window->setPersistentGraphics(true);
        window->setPersistentSceneGraph(true);
        QQuickItem* const content = window->contentItem();
        if (!content) return;

        // Detach application content before overlays. Content and popup
        // callbacks can mutate childItems(), so take a fresh snapshot for
        // the overlays rather than holding iterators across reparenting.
        for (QQuickItem* const child : content->childItems()) {
            if (child && child->parentItem() == content
                && !child->inherits("QQuickOverlay"))
                child->setParentItem(nullptr);
        }
        // This includes every overlay still on the root visual scene.
        // Never delete the overlay directly: active QML Popup objects own
        // references to it and must be destroyed through QObject ownership.
        for (QQuickItem* const child : content->childItems()) {
            if (child && child->parentItem() == content)
                child->setParentItem(nullptr);
        }
    };

    // More than one QML root/window can be loaded into the same engine.
    // The old code detached only mainWindow, leaving other overlays attached.
    // Iterate the actual roots while each native window is fully valid.
    bool handledMainWindow = false;
    for (QObject* const root : engine.rootObjects()) {
        if (auto* const window = qobject_cast<QQuickWindow*>(root)) {
            detachScene(window);
            handledMainWindow = handledMainWindow || window == mainWindow;
        }
    }
    if (mainWindow && !handledMainWindow)
        detachScene(mainWindow);

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
