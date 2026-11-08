#include "../../ui/app/social/feed_rows.hpp"

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QTest>
#include <QtQml/qqml.h>

#include <cassert>
#include <memory>

namespace {

bool writeFile(const QString& path, const QByteArray& content)
{
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(content) == content.size();
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app{argc, argv};
    QTemporaryDir temp{};
    assert(temp.isValid());
    const QString directory = temp.path();

    const QByteArray theme = R"(pragma Singleton
import QtQuick
QtObject {
    property real space3: 12
    property real space2: 8
    property real curveContainer: 14
    property color surfaceRaised: "#20242c"
    property int uiDirection: Qt.LeftToRight
    function scaleUi(value) { return value }
})";
    const QByteArray shape = R"(import QtQuick
Rectangle {
    property color fillColor: "#20242c"
    property real borderWidth: 0
    property real curveDepth: 0
    color: fillColor
    radius: curveDepth
})";
    const QByteArray skeleton = R"(import QtQuick
Item {
    property bool mediaPreview: false
    property bool animate: false
    implicitHeight: 120
})";
    const QByteArray scroll = R"(import QtQuick.Controls
ScrollBar {})";
    assert(writeFile(directory + QStringLiteral("/Theme.qml"), theme));
    assert(writeFile(directory + QStringLiteral("/Superellipse.qml"), shape));
    assert(writeFile(directory + QStringLiteral("/CastSkeleton.qml"), skeleton));
    assert(writeFile(directory + QStringLiteral("/GenyScrollBar.qml"), scroll));

    qmlRegisterType<FeedRowsModel>("Farcaster.Client", 1, 0, "FeedRowsModel");
    qmlRegisterType<MasonryWindowModel>("Farcaster.Client", 1, 0, "MasonryWindowModel");
    qmlRegisterSingletonType(QUrl::fromLocalFile(directory + QStringLiteral("/Theme.qml")),
                             "Farcaster.Client", 1, 0, "Theme");
    qmlRegisterType(QUrl::fromLocalFile(directory + QStringLiteral("/Superellipse.qml")),
                    "Farcaster.Client", 1, 0, "Superellipse");
    qmlRegisterType(QUrl::fromLocalFile(directory + QStringLiteral("/CastSkeleton.qml")),
                    "Farcaster.Client", 1, 0, "CastSkeleton");
    qmlRegisterType(QUrl::fromLocalFile(directory + QStringLiteral("/GenyScrollBar.qml")),
                    "Farcaster.Client", 1, 0, "GenyScrollBar");

    const QByteArray wrapper = QByteArray(R"QML(import QtQuick
import Farcaster.Client
import "file:)QML") + QByteArray(GENYCASTER_MASONRY_QML_DIR) + QByteArray(R"QML(" as Feed
Feed.FeedMasonryView {
    id: reader
    width: 960
    height: 740
    targetCardWidth: 420
    maximumColumns: 3
    model: castRows
    FeedRowsModel { id: castRows }
    delegate: Component {
        Item {
            implicitHeight: parent ? 140 + (parent.rowIndex % 4) * 90 : 240
            height: implicitHeight
        }
    }
    Component.onCompleted: {
        for (let i = 0; i < 800; ++i)
            castRows.append({
                stableKey: "cast:" + i, revisionKey: "r1",
                presentationKind: "conversation",
                mediaLayoutKind: "other"
            })
    }
})QML");
    assert(writeFile(directory + QStringLiteral("/tst_Masonry.qml"), wrapper));
    QQmlEngine engine{};
    QQmlComponent component{&engine,
        QUrl::fromLocalFile(directory + QStringLiteral("/tst_Masonry.qml"))};
    if (component.isError())
        qCritical() << component.errors();
    assert(component.isReady());
    std::unique_ptr<QObject> owned{component.create()};
    if (!owned) qCritical() << component.errors();
    assert(owned);
    auto* root = qobject_cast<QQuickItem*>(owned.get());
    assert(root);
    QQuickWindow window{};
    window.setWidth(960);
    window.setHeight(740);
    root->setParentItem(window.contentItem());
    window.show();
    QTest::qWait(150);

    auto* layout = root->findChild<MasonryWindowModel*>(QStringLiteral("masonryWindowModel"));
    auto* source = root->findChild<FeedRowsModel*>();
    assert(layout && source && source->count() == 800);
    assert(layout->count() > 0 && layout->count() < 70);
    const QVariantMap first = layout->geometryAt(0);
    assert(first.value(QStringLiteral("stableKey")).toString() == QStringLiteral("cast:0"));
    // Column geometry must remain non-overlapping after asynchronous,
    // mixed-height delegates replace their initial reservations.
    QHash<int, qreal> previousBottom{};
    for (int i = 0; i < source->count(); ++i) {
        const QVariantMap geometry = layout->geometryAt(i);
        const int laneX = qRound(geometry.value(QStringLiteral("x")).toDouble());
        const qreal y = geometry.value(QStringLiteral("y")).toDouble();
        const qreal height = geometry.value(QStringLiteral("height")).toDouble();
        assert(height > 0);
        if (previousBottom.contains(laneX))
            assert(y + 0.5 >= previousBottom.value(laneX));
        previousBottom[laneX] = y + height + 12.0;
    }
    root->setProperty("contentY", 9000.0);
    QTest::qWait(90);
    assert(layout->count() > 0 && layout->count() < 80);
    root->setProperty("contentY", 0.0);
    QTest::qWait(90);
    assert(layout->windowIndexForSource(0) >= 0);
    window.hide();
    root->setParentItem(nullptr);
    owned.reset();
    return 0;
}
