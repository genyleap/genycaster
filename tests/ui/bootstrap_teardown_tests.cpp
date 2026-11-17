#include <QByteArray>
#include <QGuiApplication>
#include <QEventLoop>
#include <QTimer>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QUrl>
#include <array>

import genycaster.ui.bootstrap.teardown;

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    // Build the same Qt Quick Controls overlay hierarchy implicated by the
    // macOS 27/Qt 6.12 crash, not a bare Window without any Popup.
    engine.loadData(QByteArrayLiteral(
        "import QtQuick\n"
        "import QtQuick.Controls\n"
        "ApplicationWindow { objectName: \"teardownWindow\"; visible: true;"
        " width: 180; height: 120;"
        " Popup { id: confirmation; width: 80; height: 48; modal: true;"
        " visible: true; contentItem: Text { text: \"Approval\" } }"
        " Popup { id: more; width: 80; height: 48; modal: false;"
        " visible: true; contentItem: Text { text: \"More\" } } }"),
        QUrl{QStringLiteral("inmemory:/teardown.qml")});
    // Some Qt backends finish QML loading on the next event-loop turn.
    // Wait for construction instead of treating deferred incubation as failure.
    if (engine.rootObjects().isEmpty()) {
        QEventLoop loadLoop;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                         &loadLoop, &QEventLoop::quit);
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                         &loadLoop, &QEventLoop::quit);
        QObject::connect(&watchdog, &QTimer::timeout,
                         &loadLoop, &QEventLoop::quit);
        watchdog.start(3'000);
        loadLoop.exec();
    }
    if (engine.rootObjects().size() != 1) return 1;
    auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!window) return 2;
    QPointer<QObject> tracked = window;

    // Reproduce multi-root Qt Quick Controls teardown, too. The original
    // implementation detached the primary window's overlay only.
    engine.loadData(QByteArrayLiteral(
        "import QtQuick\n"
        "import QtQuick.Controls\n"
        "ApplicationWindow { objectName: \"secondWindow\"; visible: true;"
        " width: 220; height: 150;"
        " Popup { width: 85; height: 50; modal: true; visible: true;"
        " contentItem: Text { text: \"More popups\" } } }"),
        QUrl{QStringLiteral("inmemory:/second-teardown.qml")});
    if (engine.rootObjects().size() != 2) return 4;
    auto* const second = qobject_cast<QQuickWindow*>(engine.rootObjects().constLast());
    if (!second) return 5;
    QPointer<QObject> trackedSecond = second;

    struct WindowOrder {
        int popupsExpected = 0;
        int popupsDestroyed = 0;
        int overlaysDestroyed = 0;
        int windowsDestroyed = 0;
        bool orderValid = true;
    };
    std::array<WindowOrder, 2> order;
    int attachedVisualRoots = 0;
    int detachedVisualRoots = 0;
    int overlays = 0;
    int index = 0;
    for (QQuickWindow* const itemWindow : {window, second}) {
        WindowOrder* const state = &order.at(index++);
        QQuickItem* const content = itemWindow->contentItem();
        if (!content) return 6;

        for (QObject* const child : itemWindow->findChildren<QObject*>()) {
            if (!child || !child->inherits("QQuickPopup")) continue;
            ++state->popupsExpected;
            QObject::connect(child, &QObject::destroyed, &app,
                [state] {
                    if (state->overlaysDestroyed != 0)
                        state->orderValid = false;
                    ++state->popupsDestroyed;
                });
        }
        for (QQuickItem* const child : content->childItems()) {
            if (!child || child->window() != itemWindow) continue;
            ++attachedVisualRoots;
            QObject::connect(child, &QQuickItem::windowChanged, &app,
                [&detachedVisualRoots](QQuickWindow* const changedWindow) {
                    if (!changedWindow) ++detachedVisualRoots;
                });
            if (!child->inherits("QQuickOverlay")) continue;
            ++overlays;
            QObject::connect(child, &QObject::destroyed, &app,
                [state] {
                    if (state->popupsDestroyed != state->popupsExpected)
                        state->orderValid = false;
                    ++state->overlaysDestroyed;
                });
        }
        QObject::connect(itemWindow, &QObject::destroyed, &app,
            [state] {
                if (state->overlaysDestroyed != 1)
                    state->orderValid = false;
                ++state->windowsDestroyed;
            });
    }
    if (overlays != 2 || attachedVisualRoots < 4
        || order[0].popupsExpected < 2 || order[1].popupsExpected < 1)
        return 7;

    // Called after all worker threads and network producers have stopped.
    genycaster::ui::bootstrap::releaseQmlRoots(engine, window);
    if (tracked || trackedSecond || !engine.rootObjects().isEmpty()) return 3;
    for (const WindowOrder& state : order) {
        if (!state.orderValid
            || state.popupsDestroyed != state.popupsExpected
            || state.overlaysDestroyed != 1 || state.windowsDestroyed != 1)
            return 8;
    }
    if (detachedVisualRoots + overlays < attachedVisualRoots) return 9;

    // A second idle engine must be a safe no-op.
    QQmlApplicationEngine empty;
    genycaster::ui::bootstrap::releaseQmlRoots(empty, nullptr);
    return 0;
}
