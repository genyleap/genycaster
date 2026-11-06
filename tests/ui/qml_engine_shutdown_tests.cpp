#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>

int main(int argc, char* argv[])
{
    QGuiApplication application{argc, argv};
    QQmlApplicationEngine engine{};
    engine.loadData(R"QML(
        import QtQuick
        Item {
            Loader {
                asynchronous: true
                sourceComponent: Component {
                    Item {
                        width: 40
                        height: 40
                    }
                }
            }
        }
    )QML");
    if (engine.rootObjects().size() != 1) {
        return 2;
    }

    // Quit while a deferred Loader may still be constructing its object.
    QTimer::singleShot(0, &application, &QCoreApplication::quit);
    const int exitCode = application.exec();

    // Match the GenyCaster native composition root's important lifecycle
    // order: destroy QML while the GUI dispatcher is still installed.
    for (QObject* const root : engine.rootObjects()) {
        delete root;
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (!engine.rootObjects().isEmpty()) {
        return 3;
    }
    // QT_FATAL_WARNINGS=1 turns any incomplete-QML or timer warning
    // emitted during engine destruction into a failing test.
    return exitCode;
}
