#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <cmath>
#include <memory>
#include <source_location>

namespace {

void require(const bool condition,
             const std::source_location location = std::source_location::current())
{
    if (!condition)
        qFatal("Mobile gallery Qt runtime invariant failed at %s:%u",
               location.file_name(), location.line());
}

QQuickItem* findVisual(QQuickItem* parent, const QString& name)
{
    for (QQuickItem* child : parent->childItems()) {
        if (child->objectName() == name)
            return child;
        if (auto* nested = findVisual(child, name))
            return nested;
    }
    return nullptr;
}

bool saveText(const QString& filename, const QByteArray& source)
{
    QFile file{filename};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(source) == source.size();
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication application{argc, argv};
    QTemporaryDir temporary{};
    require(temporary.isValid());

    // Mount the real responsive GalleryLayout with only its theme and remote
    // image delegate stubbed. A QML timer simulates an asynchronous provider
    // response without depending on the production HTTP/media services.
    constexpr auto theme = R"QML(pragma Singleton
import QtQuick
QtObject {
    readonly property real space2: 8
    readonly property real space5: 20
    readonly property int breakpointMobile: 600
    readonly property color surfaceRaised: "#333333"
    readonly property color background: "#eeeeee"
})QML";
    constexpr auto tile = R"QML(import QtQuick
Item {
    id: tile
    property var embed: ({})
    property bool preserveAspect: false
    // Mirror properties added to the production GalleryTile contract. These
    // flags only gate presentation; the stub still supplies the delayed ratio.
    property bool awaitFinalAspect: false
    property bool fullBleedAspect: false
    property bool useLayeredMask: false
    property bool ambientEnabled: false
    property color cornerCoverColor: "transparent"
    property int loadDelayMs: 0
    property string continuationText: ""
    signal openRequested(string url)
    signal aspectRatioResolved(real ratio)
    Timer {
        interval: 35
        running: true
        repeat: false
        onTriggered: tile.aspectRatioResolved(Number(tile.embed.fixtureRatio || 0))
    }
})QML";
    const QString themeFile = temporary.filePath(QStringLiteral("Theme.qml"));
    const QString tileFile = temporary.filePath(QStringLiteral("GalleryTile.qml"));
    require(saveText(themeFile, theme));
    require(saveText(tileFile, tile));
    qmlRegisterSingletonType(QUrl::fromLocalFile(themeFile),
                             "Farcaster.Client", 1, 0, "Theme");
    qmlRegisterType(QUrl::fromLocalFile(tileFile),
                    "Farcaster.Client", 1, 0, "GalleryTile");

    QQmlEngine engine{};
    QQmlComponent production{&engine, QUrl::fromLocalFile(
        QStringLiteral(GENYCASTER_MEDIA_QML_DIR "/GalleryLayout.qml"))};
    if (production.isError()) qCritical() << production.errors();
    require(production.isReady());

    QQuickWindow window{};
    window.resize(390, 760);
    window.show();

    const auto makeGallery = [&](const QVariantMap& embed, const bool discover,
                                 const qreal reservedHeight = 180.0) {
        const QVariantMap properties{
            {QStringLiteral("width"), 360},
            {QStringLiteral("compact"), true},
            {QStringLiteral("mobilePresentation"), true},
            {QStringLiteral("embeds"), QVariantList{embed}},
            {QStringLiteral("dynamicAspectDiscovery"), discover},
            {QStringLiteral("singleImageHeight"), reservedHeight}
        };
        std::unique_ptr<QObject> owned{production.createWithInitialProperties(properties)};
        require(owned != nullptr);
        auto* item = qobject_cast<QQuickItem*>(owned.get());
        require(item);
        item->setParentItem(window.contentItem());
        QCoreApplication::processEvents();
        return owned;
    };

    const auto tallFromMetadata = makeGallery({
        {QStringLiteral("kind"), QStringLiteral("image")},
        {QStringLiteral("url"), QStringLiteral("fixture://portrait-metadata")},
        {QStringLiteral("width"), 170},
        {QStringLiteral("height"), 1000}}, false);
    auto* metadataItem = qobject_cast<QQuickItem*>(tallFromMetadata.get());
    require(metadataItem);
    require(metadataItem->property("mobileFullBleed").toBool());
    require(std::abs(metadataItem->implicitHeight() - (std::ceil(360.0 * 1000.0 / 170.0) + 8))
           < 2.0);
    QQuickItem* tileLoader = nullptr;
    require(QTest::qWaitFor([&] {
        tileLoader = findVisual(metadataItem, QStringLiteral("galleryTileLoader"));
        return tileLoader != nullptr;
    }, 1000));
    auto* tileSlot = tileLoader->parentItem();
    require(tileSlot);
    require(std::abs(tileSlot->width() - 360.0) < 1.0);
    require(std::abs(tileSlot->height() - std::ceil(360.0 * 1000.0 / 170.0)) < 1.0);

    // A promotional link previously forced a 180px tall letterboxed image.
    // Mobile must ignore that stale reservation from the first layout.
    const auto decoded = makeGallery({
        {QStringLiteral("kind"), QStringLiteral("image")},
        {QStringLiteral("url"), QStringLiteral("fixture://portrait-decoded")},
        {QStringLiteral("fixtureRatio"), 0.18}}, true);
    auto* decodedItem = qobject_cast<QQuickItem*>(decoded.get());
    require(decodedItem && decodedItem->property("mobileFullBleed").toBool());
    require(QTest::qWaitFor([&] {
        return decodedItem->implicitHeight() > 1950;
    }, 1800));
    require(std::abs(decodedItem->implicitHeight() - (2000.0 + 8.0)) < 2.0);
    const qreal phoneHeight = decodedItem->implicitHeight();

    // For a genuinely unknown image size, the dynamic one-shot gate must be
    // enabled on the instantiated tile. Known pre-reserved heights must not
    // wait unnecessarily for a second aspect-discovery round.
    const auto unknownAspect = makeGallery({
        {QStringLiteral("kind"), QStringLiteral("image")},
        {QStringLiteral("url"), QStringLiteral("fixture://unknown-portrait")},
        {QStringLiteral("fixtureRatio"), 0.26}}, true, 0.0);
    auto* unknownItem = qobject_cast<QQuickItem*>(unknownAspect.get());
    require(unknownItem);
    QQuickItem* unknownLoader = nullptr;
    require(QTest::qWaitFor([&] {
        unknownLoader = findVisual(unknownItem, QStringLiteral("galleryTileLoader"));
        return unknownLoader != nullptr
            && unknownLoader->property("item").value<QObject*>() != nullptr;
    }, 1800));
    auto* loadedTile = unknownLoader->property("item").value<QObject*>();
    require(loadedTile->property("awaitFinalAspect").toBool());
    require(loadedTile->property("fullBleedAspect").toBool());
    require(QTest::qWaitFor([&] {
        return unknownItem->implicitHeight() > 1300;
    }, 1800));

    // Desktop Magazine columns can be the exact same width as mobile cards.
    // The viewport breakpoint, not card width, owns the mobile full-height mode.
    // AppShell changes its compact presentation when the viewport crosses
    // the breakpoint. This is an explicit reactive property, not a nested
    // Window.window.width binding that Qt Quick does not invalidate reliably.
    window.resize(1100, 760);
    decodedItem->setProperty("mobilePresentation", false);
    require(QTest::qWaitFor([&] {
        return !decodedItem->property("mobileFullBleed").toBool();
    }, 600));
    require(decodedItem->implicitHeight() < 500);
    window.resize(390, 760);
    decodedItem->setProperty("mobilePresentation", true);
    require(QTest::qWaitFor([&] {
        return decodedItem->property("mobileFullBleed").toBool()
            && std::abs(decodedItem->implicitHeight() - phoneHeight) < 2;
    }, 600));

    window.hide();
    metadataItem->setParentItem(nullptr);
    decodedItem->setParentItem(nullptr);
    unknownItem->setParentItem(nullptr);
    return 0;
}
