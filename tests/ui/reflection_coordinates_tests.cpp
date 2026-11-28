#include "../../ui/render/reflection_coordinates.hpp"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTest>
#include <memory>
#include <source_location>

namespace {
void require(bool condition, const std::source_location location = std::source_location::current())
{
    if (!condition)
        qFatal("Reflection coordinate contract failed at %s:%u",
               location.file_name(), location.line());
}
}

int main(int argc, char** argv)
{
    QGuiApplication app{argc, argv};
    qmlRegisterUncreatableType<ReflectionCoordinates>("Farcaster.Client", 1, 0,
        "ReflectionCoordinates", "Attach ReflectionCoordinates to a visual item");
    QQmlEngine engine;
    QQmlComponent component{&engine};
    component.setData(R"QML(
import QtQuick
import Farcaster.Client 1.0
Item {
    id: root
    width: 600; height: 400
    property bool captureEnabled: true
    Item {
        id: canvas
        objectName: "canvas"
        x: 70; y: 40
        Item {
            id: source
            objectName: "source"
            x: 20; y: 30; width: 200; height: 100
        }
        Item {
            id: receiver
            objectName: "receiver"
            x: 250; y: 50; width: 40; height: 60
            ReflectionCoordinates.source: root.captureEnabled ? source : null
            property rect mapped: ReflectionCoordinates.bounds
        }
    }
}
)QML", QUrl{});
    require(QTest::qWaitFor([&] { return component.isReady() || component.isError(); }, 3000));
    if (component.isError()) qWarning().noquote() << component.errorString();
    require(component.isReady());
    const std::unique_ptr<QObject> root{component.create()};
    require(root != nullptr);
    auto* canvas = root->findChild<QQuickItem*>("canvas");
    auto* source = root->findChild<QQuickItem*>("source");
    auto* receiver = root->findChild<QQuickItem*>("receiver");
    require(canvas && source && receiver);
    const auto mapped = [receiver] { return receiver->property("mapped").toRectF(); };
    require(mapped() == QRectF(230, 20, 40, 60));
    auto* coordinates = qobject_cast<ReflectionCoordinates*>(
        qmlAttachedPropertiesObject<ReflectionCoordinates>(receiver, false));
    require(coordinates != nullptr);
    QSignalSpy notifications{coordinates, &ReflectionCoordinates::boundsChanged};

    // Movement of the common ancestor cancels in relative coordinates.
    canvas->setX(170);
    canvas->setY(80);
    require(notifications.isEmpty());
    require(mapped() == QRectF(230, 20, 40, 60));

    // Source and receiver movement below the common ancestor are reactive.
    source->setX(30);
    require(notifications.size() == 1);
    require(mapped() == QRectF(220, 20, 40, 60));
    receiver->setY(70);
    require(mapped() == QRectF(220, 40, 40, 60));
    receiver->setScale(2);
    require(mapped() == receiver->mapRectToItem(source, QRectF(0, 0, 40, 60)));
    receiver->setScale(1);

    // Disabled capture must not observe any source or ancestor movement.
    require(root->setProperty("captureEnabled", false));
    require(coordinates->source() == nullptr);
    require(mapped() == QRectF(0, 0, 40, 60));
    notifications.clear();
    source->setY(10);
    canvas->setX(30);
    receiver->setX(260);
    require(notifications.isEmpty());
    receiver->setWidth(50);
    require(notifications.size() == 1);
    require(mapped() == QRectF(0, 0, 50, 60));
    require(root->setProperty("captureEnabled", true));
    require(mapped() == QRectF(230, 60, 50, 60));

    // A reparent reconnects the new chain and disconnects the old one.
    QQuickItem newParent{canvas};
    newParent.setPosition(QPointF(12, 18));
    receiver->setParentItem(&newParent);
    require(mapped() == receiver->mapRectToItem(source, QRectF(0, 0, 50, 60)));
    notifications.clear();
    newParent.setX(20);
    require(notifications.size() == 1);
    require(mapped() == receiver->mapRectToItem(source, QRectF(0, 0, 50, 60)));
    receiver->setParentItem(canvas);

    // Source lifetime is non-owning and an explicit override can leave the
    // default mapping unread without installing geometry observers.
    ReflectionCoordinates unused{receiver};
    QSignalSpy unusedNotifications{&unused, &ReflectionCoordinates::boundsChanged};
    auto transientSource = std::make_unique<QQuickItem>(canvas);
    unused.setSource(transientSource.get());
    unusedNotifications.clear();
    transientSource->setX(15);
    require(unusedNotifications.isEmpty());
    require(unused.bounds() == receiver->mapRectToItem(transientSource.get(), QRectF(0, 0, 50, 60)));
    transientSource->setX(25);
    require(unusedNotifications.size() == 1);
    transientSource.reset();
    require(unused.source() == nullptr);
    require(unused.bounds() == QRectF(0, 0, 50, 60));
    return 0;
}
