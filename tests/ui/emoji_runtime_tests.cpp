#include "emoji_controller.hpp"
#include "rich_inline_text_item.hpp"

#include <QGuiApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <memory>
#include <filesystem>
#include <span>
#include <print>

import farcaster.runtime.emoji;
import farcaster.provider.emoji.emoji_cache;
import farcaster.provider.emoji.emoji_asset_validator;

namespace {
int failures{};
void expect(const bool condition, const char* message)
{
    if (!condition) {
        std::println("FAIL: {}", message);
        ++failures;
    }
}

QImage painted(RichInlineTextItem& item)
{
    QImage image{400, 200, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::transparent);
    QPainter painter{&image};
    static_cast<QQuickPaintedItem&>(item).paint(&painter);
    return image;
}
}

int main(int argc, char** argv)
{
    QGuiApplication application{argc, argv};
    QGuiApplication::setFont(QFont{QStringLiteral("Helvetica")});
    QTemporaryDir temporary;
    expect(temporary.isValid(), "isolated settings directory");
    QSettings settings{temporary.filePath("settings.ini"), QSettings::IniFormat};
    QStandardPaths::setTestModeEnabled(true);
    EmojiController controller{settings};
    const auto cacheRoot = std::filesystem::path{temporary.filePath("cache with spaces").toStdString()};
    farcaster::runtime::emoji::EmojiRuntime runtime{controller, cacheRoot};
    const QString joy = QString::fromUtf8("😂");
    expect(controller.available(), "offline availability does not depend on a lazy transport");
    const auto specialGroups = controller.specialEmojiGroups();
    expect(specialGroups.size() == 9, "animated emoji retain manifest categories");
    qsizetype specialEmojiCount{};
    for (const auto& group : specialGroups) {
        specialEmojiCount += group.toMap().value("values").toList().size();
    }
    expect(specialEmojiCount == 881,
        "special picker exposes every entry in the vendored Noto manifest");
    expect(controller.searchSpecial(QStringLiteral("fire"), 64)
        .contains(QString::fromUtf8("🔥")), "special search finds curated animation metadata");
    expect(controller.searchSpecial(QStringLiteral("grinning"), 64)
        .contains(QString::fromUtf8("😁")), "special search covers the full provider catalog");
    qsizetype classicEmojiCount{};
    for (const auto& group : controller.emojiGroups()) {
        const auto row = group.toMap();
        if (row.value("id").toString() != QStringLiteral("recent")) {
            classicEmojiCount += row.value("values").toList().size();
        }
    }
    expect(classicEmojiCount == 881,
        "classic picker exposes every entry in the vendored Noto catalog");
    const QString source = controller.assetSourceFor(joy);
    expect(source.startsWith("qrc:/"), "bundled asset is a QML resource URL");
    expect(QFile::exists(QUrl{source}.toString().mid(3)), "bundled URL resolves");
    expect(controller.pendingFetches() == 0, "bundled asset requires no network");
    expect(controller.playbackLoops(1, false, true, true) == 1, "feed plays once");
    expect(controller.playbackLoops(2, false, true, true) == -1, "detail loops");
    expect(controller.playbackLoops(0, false, true, true) == -1, "composer loops");
    expect(controller.playbackLoops(3, false, false, true) == 0, "inactive picker stays static");
    expect(controller.playbackLoops(4, false, true, true) == -1, "picker preview loops");
    expect(controller.playbackLoops(2, true, true, true) == 0, "reduced motion disables playback");
    expect(controller.playbackLoops(2, false, true, false) == 0, "invisible emoji disables playback");

    RichInlineTextItem text;
    text.setWidth(400);
    text.setPixelSize(30);
    text.setFontFamily("Helvetica");
    text.setInlines({QVariantMap{{"kind", "text"}, {"text", joy + joy + QString::fromUtf8(" سلام 🔥")}}});
    text.setHeight(text.contentHeight());
    const auto boxes = text.emojiRects();
    expect(boxes.size() == 3, "adjacent identical emoji each own a layout box");
    for (const auto& value : boxes) {
        const auto box = value.toMap();
        expect(box.value("width").toDouble() > 10.0, "RTL emoji width is not collapsed to one pixel");
        expect(box.value("x").toDouble() >= 0.0, "emoji remains inside layout");
    }
    RichInlineTextItem multilineText;
    multilineText.setWidth(240);
    multilineText.setPixelSize(30);
    multilineText.setFontFamily("Helvetica");
    multilineText.setLineHeight(1.55);
    multilineText.setInlines({QVariantMap{{"kind", "text"},
        {"text", joy + QStringLiteral(" line one\n") + QString::fromUtf8("🔥") + QStringLiteral(" line two")}}});
    expect(multilineText.contentHeight() > 70.0,
        "ratio line height is converted to QText proportional percentage");
    for (const auto& value : multilineText.emojiRects()) {
        const auto box = value.toMap();
        const double top = box.value("y").toDouble();
        const double bottom = top + box.value("height").toDouble();
        expect(top >= -0.5, "emoji overlay never rises above its text layout");
        expect(bottom <= multilineText.contentHeight() + 0.5,
            "emoji overlay never creates a stray line below its text layout");
    }

    // Small desktop windows share the same narrow text renderer as compact
    // WebAssembly feeds. Long prices, transaction hashes and pipe-separated
    // metrics must wrap even when there is no natural word boundary.
    RichInlineTextItem narrowMetrics;
    narrowMetrics.setWidth(114);
    narrowMetrics.setPixelSize(22);
    narrowMetrics.setFontFamily("Helvetica");
    narrowMetrics.setInlines({QVariantMap{
        {"kind", "text"},
        {"text", QStringLiteral("Open:81947|High:82039|Low:81804|Close:81897|"
                                "Volume:2.8K|RSI:45|Stochastic:13|Oversold")}}});
    const qreal narrowMetricHeight = narrowMetrics.contentHeight();
    expect(narrowMetricHeight > 100,
        "unspaced financial metrics wrap within 114px instead of overflowing the Cast");
    narrowMetrics.setWidth(220);
    expect(narrowMetrics.contentHeight() < narrowMetricHeight,
        "wrapping recomputes when a compact desktop window is widened");

    // Native QTextDocument selection must remain interoperable with emoji and
    // bidirectional text, instead of trapping visible cast text in a picture.
    const QImage beforeSelection = painted(text);
    text.selectAllText();
    const QString copied = text.selectedText();
    expect(copied.contains(joy + joy), "copy text preserves adjacent emoji clusters");
    expect(copied.contains(QString::fromUtf8("سلام")),
        "copy text preserves RTL Unicode words");
    expect(copied.contains(QString::fromUtf8("🔥")),
        "copy text restores original emoji instead of U+FFFC objects");
    const QImage selected = painted(text);
    expect(selected != beforeSelection,
        "selection highlight changes the same native rich-text document");
    text.clearSelection();
    expect(text.selectedText().isEmpty(), "selection is cleared after dismissal");

    const QImage fallback = painted(text);
    for (const auto& box : boxes) {
        text.setEmojiAnimated(box.toMap().value("position").toInt(), true);
    }
    expect(painted(text) != fallback, "ready animations suppress underlying glyphs");
    for (const auto& box : boxes) {
        text.setEmojiAnimated(box.toMap().value("position").toInt(), false);
    }
    expect(painted(text) == fallback, "glyphs return when animations unload");

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("emojiController", &controller);
    QQmlComponent component{&engine, QUrl::fromLocalFile(QStringLiteral(EMOJI_COMPONENT_DIRECTORY "/AnimatedEmoji.qml"))};
    expect(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object{component.createWithInitialProperties({
        {"unicodeSequence", joy}, {"active", true}, {"surface", 2}, {"width", 96}, {"height", 96}})};
    expect(object != nullptr, "animated emoji component is creatable");
    if (!object) {
        return 1;
    }
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(160, 160);
    window.setColor(Qt::white);
    QQuickItem viewport{window.contentItem()};
    viewport.setSize(QSizeF{160, 160});
    viewport.setClip(true);
    item->setParentItem(&viewport);
    window.show();
    window.requestActivate();
    expect(QTest::qWaitFor([&] { return QGuiApplication::applicationState() == Qt::ApplicationActive; }, 3000), "test window activates");
    if (controller.lottieAvailable() && window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool(); }, 10000), "real bundled Lottie reaches readiness");
        auto first = window.grabWindow();
        QTest::qWait(450);
        auto second = window.grabWindow();
        expect(!first.isNull() && first != second, "rendered pixels change during animation");
        if (qEnvironmentVariableIsSet("EMOJI_SCREENSHOT_DIRECTORY")) {
            const QString directory = qEnvironmentVariable("EMOJI_SCREENSHOT_DIRECTORY");
            first.save(directory + "/emoji-frame-1.png");
            second.save(directory + "/emoji-frame-2.png");
        }
        emit controller.assetReady(joy, source);
        emit controller.assetReady(joy, source);
        QTest::qWait(30);
        expect(object->property("animationReady").toBool(), "repeated readiness notifications never toggle the source off");
        object->setProperty("reducedMotion", true);
        expect(QTest::qWaitFor([&] { return !object->property("animationReady").toBool(); }), "reduced motion unloads renderer");
        object->setProperty("reducedMotion", false);
        expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool(); }), "playback resumes after reduced motion");
        object->setProperty("unicodeSequence", QString::fromUtf8("🔥"));
        expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool(); }), "reused delegate resolves its new emoji");
        item->setX(200);
        expect(QTest::qWaitFor([&] { return !object->property("animationReady").toBool(); }), "clipped emoji unloads renderer");
        item->setX(0);
        expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool(); }), "scrolling back resumes playback");

        // Preseed the actual cache: this path must never create a network request.
        QFile bundled{QStringLiteral(EMOJI_SOURCE_DIRECTORY "/emoji_u1f602.json")};
        expect(bundled.open(QIODevice::ReadOnly), "cache fixture opens");
        const auto data = bundled.readAll();
        const auto bytes = std::as_bytes(std::span{data.constData(), static_cast<std::size_t>(data.size())});
        farcaster::provider::emoji::cache::EmojiAssetCache diskCache{{.rootDirectory = cacheRoot}};
        expect(diskCache.store("noto", "1f600", 1, bytes,
            farcaster::provider::emoji::validator::sha256(bytes)).has_value(), "validated offline cache seeded");
        object->setProperty("unicodeSequence", QString::fromUtf8("😀"));
        expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool()
            && controller.assetSourceFor(QString::fromUtf8("😀")).startsWith("file:"); }), "cached local file animates without network");
        const auto cachedUrl = QUrl{controller.assetSourceFor(QString::fromUtf8("😀"))};
        expect(controller.assetSourceFor(QString::fromUtf8("😀")).contains("%20"), "local asset URL encodes spaces");
        expect(QFile::exists(cachedUrl.toLocalFile()), "encoded local asset URL round-trips");
        expect(controller.pendingFetches() == 0, "cache completion releases pending request");

        QImage catalogImage{5 * 120, 3 * 120, QImage::Format_ARGB32_Premultiplied};
        catalogImage.fill(Qt::white);
        const QStringList sequences{QString::fromUtf8("😂"), QString::fromUtf8("❤️"), QString::fromUtf8("🔥"),
            QString::fromUtf8("👍"), QString::fromUtf8("😍"), QString::fromUtf8("😭"), QString::fromUtf8("🎉"),
            QString::fromUtf8("🚀"), QString::fromUtf8("👀"), QString::fromUtf8("✨"), QString::fromUtf8("💯"),
            QString::fromUtf8("🤣"), QString::fromUtf8("😊"), QString::fromUtf8("😎"), QString::fromUtf8("🙏")};
        QPainter catalogPainter{&catalogImage};
        const auto visiblePixelCount = [](const QImage& frame) {
            int count{};
            for (int y = 0; y < frame.height(); ++y) {
                for (int x = 0; x < frame.width(); ++x) {
                    const auto color = frame.pixelColor(x, y);
                    if (color.red() < 230 || color.green() < 230 || color.blue() < 230) {
                        ++count;
                    }
                }
            }
            return count;
        };
        int index{};
        for (const QString& sequence : sequences) {
            object->setProperty("unicodeSequence", sequence);
            expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool(); }), "bundled catalog renderer ready");
            QTest::qWait(1750);
            QImage frame{};
            int coloredPixels{};
            // A Lottie may have a deliberately empty authored beat at the
            // exact first capture time. Sample a few adjacent rendered frames
            // and validate the strongest one instead of making the test phase-
            // dependent.
            for (int attempt = 0; attempt < 4 && coloredPixels <= 200; ++attempt) {
                const auto capture = window.grabWindow();
                const int extent = qRound(96 * capture.devicePixelRatio());
                frame = capture.copy(0, 0, extent, extent).scaled(96, 96);
                frame.setDevicePixelRatio(1.0);
                coloredPixels = visiblePixelCount(frame);
                if (coloredPixels <= 200) {
                    QTest::qWait(140);
                }
            }
            expect(!frame.isNull(), "catalog frame rendered");
            catalogPainter.drawImage((index % 5) * 120 + 12, (index / 5) * 120 + 12, frame);
            if (coloredPixels <= 200)
                std::println("Sparse frame for {}: {} pixels", sequence.toStdString(), coloredPixels);
            expect(coloredPixels > 200, "each bundled emoji has visible content, including track mattes");
            if (index == 0) {
                int facePixels{};
                for (int y = 0; y < frame.height(); ++y) {
                    for (int x = 0; x < frame.width(); ++x) {
                        const auto color = frame.pixelColor(x, y);
                        if (color.red() < 150 && color.green() < 120 && color.blue() < 100) {
                            ++facePixels;
                        }
                    }
                }
                expect(facePixels > 30, "face features survive composition (not only a yellow silhouette)");
            }
            ++index;
        }
        catalogPainter.end();
        if (qEnvironmentVariableIsSet("EMOJI_SCREENSHOT_DIRECTORY")) {
            catalogImage.save(qEnvironmentVariable("EMOJI_SCREENSHOT_DIRECTORY") + "/emoji-catalog.png");
        }
        object->setProperty("surface", 1);
        object->setProperty("unicodeSequence", joy);
        QTest::qWait(2500);
        const auto completedFeedFrame = window.grabWindow();
        QTest::qWait(300);
        expect(window.grabWindow() == completedFeedFrame, "feed stops after one actual playback");
        object->setProperty("surface", 3);
        object->setProperty("active", false);
        expect(QTest::qWaitFor([&] { return !object->property("animationReady").toBool(); }),
            "inactive picker without artwork preservation unloads");
        object->setProperty("preserveArtwork", true);
        expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool(); }),
            "picker can keep Noto artwork visible while motion stays inactive");
        const auto pausedPickerFrame = window.grabWindow();
        QTest::qWait(350);
        expect(window.grabWindow() == pausedPickerFrame,
            "inactive preserved picker artwork stays paused");
        object->setProperty("active", true);
        expect(QTest::qWaitFor([&] { return object->property("animationReady").toBool(); }),
            "focused picker reuses preserved artwork for playback");
        item->setVisible(false);
        expect(QTest::qWaitFor([&] { return !object->property("animationReady").toBool(); }), "hidden component unloads renderer");
    } else {
        expect(!object->property("animationReady").toBool(), "renderer-free builds retain Unicode");
        expect(controller.pendingFetches() == 0, "renderer-free builds request no assets");
    }
    item->setParentItem(nullptr);
    std::println("Emoji UI regression failures: {}", failures);
    return failures == 0 ? 0 : 1;
}
