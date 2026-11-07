#include <QByteArray>
#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QUrl>

import genycaster.ui.bootstrap.teardown;

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.loadData(QByteArrayLiteral(
        "import QtQuick\n"
        "Window { objectName: \"teardownWindow\"; visible: false; width: 32; height: 32 }"),
        QUrl{QStringLiteral("inmemory:/teardown.qml")});
    if (engine.rootObjects().size() != 1) return 1;
    auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!window) return 2;
    QPointer<QObject> tracked = window;

    // Called after all worker threads and network producers have stopped.
    genycaster::ui::bootstrap::releaseQmlRoots(engine, window);
    if (tracked || !engine.rootObjects().isEmpty()) return 3;

    // A second idle engine must be a safe no-op.
    QQmlApplicationEngine empty;
    genycaster::ui::bootstrap::releaseQmlRoots(empty, nullptr);
    return 0;
}
