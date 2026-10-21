#include "../../ui/mobile/browser_wheel_fallback.hpp"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QQuickStyle>
#include <QCoreApplication>

#include <cmath>
#include <print>

namespace {
int failures{};
void expect(const bool condition, const char* label)
{
    if (!condition) {
        ++failures;
        std::println("FAIL: {}", label);
    }
}
}

int main(int argc, char** argv)
{
    QGuiApplication application{argc, argv};
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQmlEngine engine{};
    QQuickWindow window{};
    window.resize(480, 440);
    QQmlComponent component{&engine};
    component.setData(R"(
import QtQuick
import QtQuick.Controls
Item {
  width: 480; height: 440
  ListView {
    id: list
    objectName: "primaryList"
    width: 420; height: 420
    model: 100
    delegate: Rectangle { width: 420; height: 40 }
  }
  ListView {
    id: disabled
    objectName: "disabledList"
    x: 425; width: 48; height: 420
    interactive: false
    model: 100
    delegate: Rectangle { width: 48; height: 40 }
  }
  Item {
    objectName: "offsetViewport"
    x: 250; y: 270; width: 165; height: 125
    clip: true
    property real contentY: -120
    property real originY: -120
    property real contentHeight: 800
    property bool interactive: true
  }
  Popup {
    objectName: "testPopup"
    modal: true
    x: 80; y: 80; width: 250; height: 195
    padding: 0
    contentItem: ListView {
      objectName: "popupList"
      model: 100
      delegate: Rectangle { width: 220; height: 35 }
    }
  }
})", QUrl{QStringLiteral("qrc:/wheel-fallback-test.qml")});
    if (!component.isReady()) {
        std::println("FAIL: {}", component.errorString().toStdString());
        return 2;
    }
    auto root = std::unique_ptr<QQuickItem>{
        qobject_cast<QQuickItem*>(component.create())};
    if (!root) return 3;
    root->setParentItem(window.contentItem());
    window.show();
    QTimer::singleShot(180, [&] {
        auto* list = root->findChild<QQuickItem*>(QStringLiteral("primaryList"));
        auto* disabled = root->findChild<QQuickItem*>(QStringLiteral("disabledList"));
        expect(list != nullptr && disabled != nullptr, "fixture ListViews exist");
        if (list && disabled) {
            using genycaster::ui::browser_wheel::scroll;
            expect(scroll(window, QPointF{120, 120}, QPoint{0, -96}),
                   "wheel scrolls an interactive ListView");
            expect(std::abs(list->property("contentY").toDouble() - 96.) < 0.1,
                   "content position reflects pixel wheel delta");
            expect(!scroll(window, QPointF{447, 120}, QPoint{0, -96}),
                   "disabled Flickable ignores wheel");
            expect(std::abs(disabled->property("contentY").toDouble()) < 0.1,
                   "disabled Flickable stays at zero");
            expect(!scroll(window, QPointF{490, 120}, QPoint{0, -96}),
                   "outside viewport never scrolls");
            auto* offset = root->findChild<QQuickItem*>(QStringLiteral("offsetViewport"));
            expect(offset != nullptr, "offset-origin viewport exists");
            if (offset) {
                expect(scroll(window, QPointF{280, 300}, QPoint{0, -70}),
                       "wheel advances a list with a negative origin");
                expect(std::abs(offset->property("contentY").toDouble() + 50.) < 0.1,
                       "wheel does not clamp the negative list origin to zero");
                expect(scroll(window, QPointF{280, 300}, QPoint{0, 500}),
                       "wheel can return toward a shifted origin");
                expect(std::abs(offset->property("contentY").toDouble() + 120.) < 0.1,
                       "wheel returns exactly to the shifted origin");
            }
            auto* popup = root->findChild<QObject*>(QStringLiteral("testPopup"));
            expect(popup != nullptr, "popup fixture exists");
            if (popup) {
                const bool opened = QMetaObject::invokeMethod(popup, "open");
                QCoreApplication::processEvents();
                auto* popupList = popup->findChild<QQuickItem*>(
                    QStringLiteral("popupList"));
                expect(opened && popupList, "popup content ListView exists");
                if (popupList) {
                    expect(scroll(window, QPointF{145, 140}, QPoint{0, -75}),
                           "wheel over modal popup scrolls its own ListView");
                    expect(popupList->property("contentY").toDouble() > 70.,
                           "popup content offset advances");
                    expect(std::abs(list->property("contentY").toDouble() - 96.) < 0.1,
                           "background list must not scroll beneath popup");
                    expect(!scroll(window, QPointF{30, 30}, QPoint{0, -75}),
                           "modal popup must block background wheel outside its bounds");
                    expect(std::abs(list->property("contentY").toDouble() - 96.) < 0.1,
                           "modal popup outside area leaves background feed static");
                }
                QMetaObject::invokeMethod(popup, "close");
                QCoreApplication::processEvents();
            }
            expect(scroll(window, QPointF{120, 120}, QPoint{0, 36}),
                   "wheel reverses direction");
            expect(std::abs(list->property("contentY").toDouble() - 60.) < 0.1,
                   "reverse wheel gives expected offset");
            list->setProperty("contentY",
                              list->property("contentHeight").toDouble() - list->height());
            expect(!scroll(window, QPointF{120, 120}, QPoint{0, -80}),
                   "wheel at lower scroll bound is not swallowed");
        }
        application.quit();
    });
    const int result = application.exec();
    std::println("WASM browser wheel fallback: {} failure(s)", failures);
    return failures ? 1 : result;
}
