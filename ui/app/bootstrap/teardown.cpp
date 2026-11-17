module;

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QList>
#include <QString>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>

module genycaster.ui.bootstrap.teardown;

namespace genycaster::ui::bootstrap {
void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow)
{
    // macOS / Qt 6.12: deleting QQuickWindow clears its contentItem
    // pointer before the QQuickRootItem (and its QQuickOverlay) is destroyed.
    // At that point QQuickItemPrivate::derefWindow() can access an invalid
    // window state. Detaching visual parents alone does not fix QObject
    // destruction order: the overlay still belongs to QQuickRootItem.
    const auto releaseWindowScene = [](QQuickWindow* const window) {
        if (!window) return;
        QQuickItem* const content = window->contentItem();
        if (!content) return;

        window->setPersistentGraphics(true);
        window->setPersistentSceneGraph(true);

        // The QQuickPopup objects can still have active connections to the
        // overlay. Destroy them *first*, while both the overlay and native
        // window are fully alive. Deleting the overlay with live popups causes
        // a use-after-free through QQuickPopupPrivate::setWindow.
        // Collect QPointers before deletion: destroying an enclosing Popup
        // may also destroy nested Popup objects.
        QList<QPointer<QObject>> popups;
        for (QObject* const obj : window->findChildren<QObject*>(
                 QString{}, Qt::FindChildrenRecursively)) {
            if (obj && obj->inherits("QQuickPopup"))
                popups.append(QPointer<QObject>{obj});
        }
        for (qsizetype i = popups.size(); i > 0; --i) {
            if (QObject* const popup = popups.at(i - 1).data())
                delete popup;
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        // QObject ownership of overlays normally belongs to QQuickRootItem.
        // Release them now rather than waiting until QQuickWindow has already
        // entered its destructor and cleared the contentItem pointer.
        // Query after Popup destruction as closing Popups can remove overlays.
        QList<QPointer<QObject>> overlays;
        for (QObject* const obj : content->children()) {
            if (obj && obj->inherits("QQuickOverlay"))
                overlays.append(QPointer<QObject>{obj});
        }
        for (const QPointer<QObject>& overlay : overlays) {
            if (overlay)
                delete overlay.data();
        }

        // Detach the remaining scene items while QQuickWindow remains valid.
        // Never change QObject ownership or call releaseResources()/hide().
        for (QQuickItem* const child : content->childItems()) {
            if (child && child->parentItem() == content)
                child->setParentItem(nullptr);
        }
    };

    // Multiple QML roots/windows can coexist. Each needs its own Popup and
    // Overlay teardown while its QQuickWindow is still fully valid.
    // engine.rootObjects() is authoritative. Do not dereference a raw
    // mainWindow pointer if its QML root was already destroyed on close.
    Q_UNUSED(mainWindow);
    for (QObject* const root : engine.rootObjects()) {
        if (auto* const window = qobject_cast<QQuickWindow*>(root))
            releaseWindowScene(window);
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
