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

#include <cmath>
#include <memory>
#include <source_location>

namespace {

void require(const bool condition,
             const std::source_location location = std::source_location::current())
{
    if (!condition)
        qFatal("Masonry runtime test failed at %s:%u", location.file_name(), location.line());
}

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
    require(temp.isValid());
    const QString directory = temp.path();

    const QByteArray theme = R"(pragma Singleton
import QtQuick
QtObject {
    property real space3: 12
    property real space2: 8
    property bool neonEnabled: false
    property bool reduceMotion: false
    property int motionStandard: 150
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
    require(writeFile(directory + QStringLiteral("/Theme.qml"), theme));
    require(writeFile(directory + QStringLiteral("/Superellipse.qml"), shape));
    require(writeFile(directory + QStringLiteral("/CastSkeleton.qml"), skeleton));
    require(writeFile(directory + QStringLiteral("/GenyScrollBar.qml"), scroll));

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
        // Exercise the same nested Loader contract as FeedRow. Its reserved
        // 850px height must never become an exact measurement while the real
        // card is only 140px tall.
        Loader {
            id: nestedCast
            readonly property bool incubating: status === Loader.Loading || item === null
            readonly property real resolvedHeight: item ? item.implicitHeight : 0
            readonly property int sourceRow: parent ? parent.rowIndex : -1
            readonly property string sourceRevision: parent ? parent.revisionKey : ""
            height: 850
            sourceComponent: Component {
                Item {
                    implicitHeight: nestedCast.sourceRevision === "expanded"
                        ? 790 : 140 + (nestedCast.sourceRow % 4) * 90
                }
            }
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
    require(writeFile(directory + QStringLiteral("/tst_Masonry.qml"), wrapper));
    QQmlEngine engine{};
    QQmlComponent component{&engine,
        QUrl::fromLocalFile(directory + QStringLiteral("/tst_Masonry.qml"))};
    if (component.isError())
        qCritical() << component.errors();
    require(component.isReady());
    std::unique_ptr<QObject> owned{component.create()};
    if (!owned) qCritical() << component.errors();
    require(owned != nullptr);
    auto* root = qobject_cast<QQuickItem*>(owned.get());
    require(root);
    QQuickWindow window{};
    window.setWidth(960);
    window.setHeight(740);
    root->setParentItem(window.contentItem());
    window.show();
    auto* layout = root->findChild<MasonryWindowModel*>(QStringLiteral("masonryWindowModel"));
    auto* source = root->findChild<FeedRowsModel*>();
    require(layout && source && source->count() == 800);
    require(QTest::qWaitFor([&] {
        return layout->count() > 0 && layout->count() < 70
            && std::abs(layout->geometryAt(0).value(QStringLiteral("height")).toDouble() - 140.0) < 2.0;
    }, 2000));
    const QVariantMap first = layout->geometryAt(0);
    require(first.value(QStringLiteral("stableKey")).toString() == QStringLiteral("cast:0"));
    require(std::abs(first.value(QStringLiteral("height")).toDouble() - 140.0) < 2.0);
    // A painted card may expand and contract after a preview loads. The next
    // card in its column must move with it, not retain the old 790px slot.
    source->setProperty(0, QStringLiteral("revisionKey"), QStringLiteral("expanded"));
    QTest::qWait(85);
    const QVariantMap grown = layout->geometryAt(0);
    require(std::abs(grown.value(QStringLiteral("height")).toDouble() - 790.0) < 2.0);
    QTest::qWait(390);
    // Hydration must never move a previously admitted card to another lane.
    const QVariantMap packed = layout->geometryAt(2);
    require(packed.value(QStringLiteral("x")) == first.value(QStringLiteral("x")));
    source->setProperty(0, QStringLiteral("revisionKey"), QStringLiteral("settled"));
    QTest::qWait(390);
    const QVariantMap settled = layout->geometryAt(0);
    require(std::abs(settled.value(QStringLiteral("height")).toDouble() - 140.0) < 2.0);
    // As the same row contracts, row 2 remains in its committed lane;
    // the prior height reservation is gone.
    require(layout->geometryAt(2).value(QStringLiteral("x"))
            == settled.value(QStringLiteral("x")));
    // Column geometry must have no oversized holes or overlapping children,
    // including after the delegate contracts from a larger reservation.
    QHash<int, qreal> previousBottom{};
    for (int i = 0; i < source->count(); ++i) {
        const QVariantMap geometry = layout->geometryAt(i);
        const int laneX = qRound(geometry.value(QStringLiteral("x")).toDouble());
        const qreal y = geometry.value(QStringLiteral("y")).toDouble();
        const qreal height = geometry.value(QStringLiteral("height")).toDouble();
        require(height > 0);
        if (previousBottom.contains(laneX))
            require(std::abs(y - previousBottom.value(laneX)) <= 0.75);
        previousBottom[laneX] = y + height + 12.0;
    }
    // Repeated 2 -> 3 -> 2 column changes retain the mounted card identity.
    auto itemForFirst = [&]() -> QObject* {
        QVariant item{};
        const QVariant index{0};
        require(QMetaObject::invokeMethod(root, "itemAtIndex",
                    Q_RETURN_ARG(QVariant, item), Q_ARG(QVariant, index)));
        return item.value<QObject*>();
    };
    auto* firstItem = itemForFirst();
    require(firstItem != nullptr);
    for (const auto width : {1050, 1250, 980, 1320, 960}) {
        root->setWidth(width);
        QTest::qWait(35);
        require(itemForFirst() == firstItem);
    }
    QTest::qWait(420);
    require(itemForFirst() == firstItem);
    require(layout->count() < 80);
    root->setProperty("contentY", 9000.0);
    QTest::qWait(90);
    require(layout->count() > 0 && layout->count() < 80);
    root->setProperty("contentY", 0.0);
    QTest::qWait(90);
    require(layout->windowIndexForSource(0) >= 0);
    window.hide();
    root->setParentItem(nullptr);
    owned.reset();
    return 0;
}
