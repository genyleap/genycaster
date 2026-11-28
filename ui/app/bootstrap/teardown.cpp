module;

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QList>
#include <QLoggingCategory>
#include <QString>
#include <QVariant>
#include <QQmlApplicationEngine>
#include <QQmlIncubationController>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>

module genycaster.ui.bootstrap.teardown;

namespace genycaster::ui::bootstrap {
namespace {
Q_LOGGING_CATEGORY(qmlTeardownLog, "genycaster.qml.teardown", QtWarningMsg)

[[nodiscard]] QList<QPointer<QObject>> guardedRoots(const QQmlApplicationEngine& engine)
{
    QList<QPointer<QObject>> roots;
    for (QObject* const root : engine.rootObjects())
        roots.append(QPointer<QObject>{root});
    return roots;
}
} // namespace

void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow)
{
    // The window normally advances incubators on frame/timer callbacks. At
    // shutdown finish already admitted delegates while their QML contexts are live,
    // then disable further deferred creation before destroying their owners.
    // The last QQuickWindow can already have detached its controller during
    // close. Pending incubators belong to the engine, not the controller.
    // Attach a local, manually driven controller even in that case; never
    // depend on a window or timer after the application event loop stops.
    QQmlIncubationController finalController;
    engine.setIncubationController(&finalController);
    if (auto* const controller = engine.incubationController()) {
        const auto initialCount = controller->incubatingObjectCount();
        const QDeadlineTimer deadline{500};
        qCDebug(qmlTeardownLog) << "beforeDrain" << initialCount
                              << "roots" << engine.rootObjects().size();
        while (controller->incubatingObjectCount() > 0 && !deadline.hasExpired())
            controller->incubateFor(10);
        const auto afterDrain = controller->incubatingObjectCount();
        qCDebug(qmlTeardownLog) << "afterDrain" << afterDrain;
        if (afterDrain > 0) {
            // A nested Loader can still be waiting for its parent or a URL.
            // Cancel it while its context exists. Destroying a parent loader
            // can invalidate other entries in this snapshot.
            QList<QPointer<QObject>> pending;
            for (auto* root : engine.rootObjects()) {
                for (auto* object : root->findChildren<QObject*>()) {
                    if (object->inherits("QQuickLoader") && object->property("active").toBool()
                        && !object->property("item").value<QObject*>())
                        pending.append(object);
                }
            }
            for (const auto& loader : pending) {
                if (loader) {
                    qCDebug(qmlTeardownLog) << "cancelLoaderType"
                                           << loader->metaObject()->className();
                    QQmlProperty::write(loader.data(), QStringLiteral("active"), false);
                }
            }
        }
        qCDebug(qmlTeardownLog) << "beforeDetach" << controller->incubatingObjectCount();
        // With no controller, new creation during teardown is synchronous.
        // Keep the existing popup/overlay lifetime boundary intact.
        engine.setIncubationController(nullptr);
    }

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

        // Destroy application content while the native QQuickWindow and its
        // overlay are valid. Its QML-owned Popups and their attached bindings
        // then die with their owners instead of being invalidated individually.
        // Do not call setParentItem(nullptr): on macOS/Qt 6.12 it dispatches
        // QQuickOverlayAttached::setWindowAndParent() into stale connections.
        QList<QPointer<QQuickItem>> sceneItems;
        for (QQuickItem* const child : content->childItems()) {
            if (child && !child->inherits("QQuickOverlay"))
                sceneItems.append(QPointer<QQuickItem>{child});
        }
        for (const QPointer<QQuickItem>& item : sceneItems) {
            if (item && item->parentItem() == content)
                delete item.data();
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        // Some Popups may be declared directly on ApplicationWindow and not
        // in the visual scene. They must be destroyed before the overlay.
        // Use QObject's window tree rather than contentItem's QObject tree:
        // QML ownership does not have to mirror visual parentItem ownership.
        QList<QPointer<QObject>> popups;
        for (QObject* const obj : window->findChildren<QObject*>(
                 QString{}, Qt::FindChildrenRecursively)) {
            if (obj && obj->inherits("QQuickPopup"))
                popups.append(QPointer<QObject>{obj});
        }
        for (qsizetype i = popups.size(); i > 0; --i) {
            QObject* const popup = popups.at(i - 1).data();
            if (!popup) {
                continue;
            }

            QObject* const owner = popup->parent();
            if (owner && owner->inherits("QQuickLoader")
                && owner->property("item").value<QObject*>() == popup) {
                // Loader keeps an internal pointer to its loaded Popup.
                // Deactivation clears that pointer and queues the Popup's
                // deletion while the window and overlay are still alive.
                QQmlProperty::write(owner, QStringLiteral("active"), false);
            } else {
                delete popup;
            }
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        // Delete overlays before QQuickWindow clears its contentItem pointer
        // in the window destructor. Never detach surviving scene items:
        // visual reparenting invokes unsafe QQuickOverlayAttached callbacks.
        QList<QPointer<QObject>> overlays;
        for (QObject* const obj : content->children()) {
            if (obj && obj->inherits("QQuickOverlay"))
                overlays.append(QPointer<QObject>{obj});
        }
        for (const QPointer<QObject>& overlay : overlays) {
            if (overlay)
                delete overlay.data();
        }
    };

    // Multiple QML roots/windows can coexist. Each needs its own Popup and
    // Overlay teardown while its QQuickWindow is still fully valid.
    // engine.rootObjects() is authoritative. Do not dereference a raw
    // mainWindow pointer if its QML root was already destroyed on close.
    Q_UNUSED(mainWindow);
    for (const auto& root : guardedRoots(engine)) {
        if (auto* const window = qobject_cast<QQuickWindow*>(root.data()))
            releaseWindowScene(window);
    }
    qCDebug(qmlTeardownLog) << "afterSceneCleanup" << "roots" << engine.rootObjects().size();

    // Do not defer root deletion until the QQmlApplicationEngine destructor:
    // its referenced runtime may already have been released by then.
    // deleteLater() is invalid here because the main event loop has stopped.
    // A root destruction handler can release another root. The engine's
    // snapshot contains raw pointers, so guard every entry before deletion.
    for (const auto& root : guardedRoots(engine)) {
        if (root)
            delete root.data();
    }

    // Drain deferred child cleanup while Qt still has a GUI dispatcher;
    // never restart the event loop or admit new network requests.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // Reattach only to read the public count, not to finalize objects whose
    // creation contexts have already been destroyed. Qt's engine warning
    // tracks a wider creation counter, including deferred control properties.
    if (qmlTeardownLog().isDebugEnabled()) {
        engine.setIncubationController(&finalController);
        qCDebug(qmlTeardownLog) << "afterRootCleanup"
                              << finalController.incubatingObjectCount()
                              << "roots" << engine.rootObjects().size();
        engine.setIncubationController(nullptr);
    }
}
} // namespace genycaster::ui::bootstrap
