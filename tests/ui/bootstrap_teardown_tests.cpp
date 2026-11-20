#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QEventLoop>
#include <QTimer>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlIncubator>
#include <QQuickWindow>
#include <QQuickItem>
#include <QUrl>
#include <array>
#include <atomic>
#include <memory>
#include <QDebug>

import genycaster.ui.bootstrap.teardown;

namespace {
struct WindowOrder {
    int popupsExpected = 0;
    int sceneItemsExpected = 0;
    int sceneItemsDestroyed = 0;
    int popupsDestroyed = 0;
    int overlaysDestroyed = 0;
    int windowsDestroyed = 0;
    bool orderValid = true;
};

std::atomic_int nullBindingErrors{0};
QtMessageHandler previousMessageHandler = nullptr;
void observeQmlWarnings(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg && message.contains(QStringLiteral("TypeError: Cannot")))
        nullBindingErrors.fetch_add(1, std::memory_order_relaxed);
    if (previousMessageHandler)
        previousMessageHandler(type, context, message);
}
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    previousMessageHandler = qInstallMessageHandler(observeQmlWarnings);
    std::array<WindowOrder, 2> order{};
    QQmlApplicationEngine engine;
    // Build the same Qt Quick Controls overlay hierarchy implicated by the
    // macOS 27/Qt 6.12 crash, not a bare Window without any Popup.
    engine.loadData(QByteArrayLiteral(
        "import QtQuick\n"
        "import QtQuick.Controls\n"
        "ApplicationWindow { objectName: \"teardownWindow\"; visible: true;"
        " width: 180; height: 120;"
        " Item { id: owner; anchors.fill: parent;"
        " property bool anyPopupOpen: (confirmation && confirmation.opened) || (more && more.opened);"
        " Popup { id: confirmation; width: 80; height: 48; modal: true;"
        " visible: true; contentItem: Text { text: \"Approval\" } }"
        " Popup { id: more; width: 80; height: 48; modal: false;"
        " visible: true; contentItem: Text { text: \"More\" } } } }"),
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

    // The overlay-hosted list survives the initial content cleanup. Its
    // button's hint must be released by the Loader before the overlay dies.
    engine.loadData(QByteArrayLiteral(R"QML(
        import QtQuick
        import QtQuick.Controls

        ApplicationWindow {
            id: secondWindow
            visible: true
            width: 220
            height: 150

            Popup {
                width: 85
                height: 50
                modal: true
                visible: true
                contentItem: Text { text: "More popups" }
            }

            ListView {
                id: hintList
                parent: secondWindow.Overlay.overlay
                width: 180
                height: 100

                Button {
                    parent: hintList.contentItem
                    width: 80
                    height: 40

                    Loader {
                        objectName: "hintLoader"
                        sourceComponent: Component {
                            Popup {
                                objectName: "loadedHint"
                                parent: secondWindow.Overlay.overlay
                                width: 60
                                height: 30
                                visible: true
                            }
                        }
                    }
                }
            }
        }
    )QML"),
        QUrl{QStringLiteral("inmemory:/second-teardown.qml")});
    if (engine.rootObjects().size() != 2) return 4;
    auto* const second = qobject_cast<QQuickWindow*>(engine.rootObjects().constLast());
    if (!second) return 5;
    QPointer<QObject> trackedSecond = second;
    const QPointer<QObject> hint = second->findChild<QObject*>(QStringLiteral("loadedHint"));
    if (!hint || !hint->parent()
        || hint->parent()->objectName() != QStringLiteral("hintLoader")) {
        return 10;
    }

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
            if (child->inherits("QQuickOverlay")) {
                ++overlays;
                QObject::connect(child, &QObject::destroyed, &app,
                    [state] {
                        if (state->popupsDestroyed != state->popupsExpected
                            || state->sceneItemsDestroyed != state->sceneItemsExpected)
                            state->orderValid = false;
                        ++state->overlaysDestroyed;
                    });
            } else {
                ++state->sceneItemsExpected;
                QObject::connect(child, &QObject::destroyed, &app,
                    [state] {
                        if (state->overlaysDestroyed != 0)
                            state->orderValid = false;
                        ++state->sceneItemsDestroyed;
                    });
            }
        }
        QObject::connect(itemWindow, &QObject::destroyed, &app,
            [state] {
                if (state->overlaysDestroyed != 1)
                    state->orderValid = false;
                ++state->windowsDestroyed;
            });
    }
    if (overlays != 2 || order[0].sceneItemsExpected < 1
        || order[0].popupsExpected < 2 || order[1].popupsExpected < 1)
        return 7;

    QTimer::singleShot(0, &app, &QCoreApplication::quit);
    if (app.exec() != 0 || !tracked || !trackedSecond || !hint) {
        return 11;
    }

    // Queue an object after the last frame so the teardown, rather than the
    // window's incubation timer, must advance it while the engine is live.
    QQmlIncubationController controller{};
    engine.setIncubationController(&controller);
    QQmlComponent component{&engine};
    component.setData(QByteArrayLiteral("import QtQuick\nItem {}"), QUrl{});
    QQmlIncubator pending{QQmlIncubator::Asynchronous};
    component.create(pending);
    if (!pending.isLoading() || controller.incubatingObjectCount() != 1) return 12;

    // The native composition root calls this after the GUI event loop returns.
    genycaster::ui::bootstrap::releaseQmlRoots(engine, window);
    const std::unique_ptr<QObject> completed{pending.object()};
    if (!completed || !pending.isReady() || engine.incubationController()) return 13;
    if (tracked || trackedSecond || !engine.rootObjects().isEmpty()) return 3;
    for (const WindowOrder& state : order) {
        if (!state.orderValid
            || state.sceneItemsDestroyed != state.sceneItemsExpected
            || state.popupsDestroyed != state.popupsExpected
            || state.overlaysDestroyed != 1 || state.windowsDestroyed != 1)
            return 8;
    }
    if (nullBindingErrors.load(std::memory_order_relaxed) != 0) return 9;

    // A second idle engine must be a safe no-op.
    QQmlApplicationEngine empty;
    genycaster::ui::bootstrap::releaseQmlRoots(empty, nullptr);
    return 0;
}
