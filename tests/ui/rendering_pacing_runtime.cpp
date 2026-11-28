#include "rendering_controller.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QQmlComponent>
#include <memory>
#include <QSettings>
#include <QTemporaryDir>
#include <QtCore/qtestsupport_core.h>

#include <atomic>
#include <vector>

namespace {
void require(bool condition, int line) {
    if (!condition)
        qFatal("Rendering pacing regression at line %d", line);
}
#define CHECK(condition) require((condition), __LINE__)

struct Grant {
    QObject* requester{};
    quint64 generation{};
    quint64 sequence{};
    unsigned renderPass{};
};

class MetaCallCounter final : public QObject {
public:
    unsigned count{};
protected:
    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::MetaCall)
            ++count;
        return false;
    }
};

void showWindow(QQuickWindow& window) {
    window.resize(320, 240);
    window.show();
    CHECK(QTest::qWaitFor([&] { return window.isExposed(); }, 3000));
}
void renderBarrier(QQuickWindow& window) {
    window.beforeSynchronizing();
    window.afterRendering();
}
void drain(RenderingController& controller) {
    QCoreApplication::sendPostedEvents(&controller, QEvent::MetaCall);
}
} // namespace

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temp;
    CHECK(temp.isValid());
    QSettings settings(temp.filePath("rendering.ini"), QSettings::IniFormat);
    RenderingController controller(settings);
    QQuickWindow window;
    controller.attachWindow(&window);
    std::atomic<unsigned> renderPasses{};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        renderPasses.fetch_add(1, std::memory_order_relaxed);
    }, Qt::DirectConnection);
    std::vector<Grant> grants;
    QObject::connect(&controller, &RenderingController::mediaPresentationGranted,
        &controller, [&](QObject* requester, quint64 generation, quint64 sequence) {
            grants.push_back({requester, generation, sequence,
                              renderPasses.load(std::memory_order_relaxed)});
        });
    QObject first, second, cancelled;
    controller.requestMediaPresentation(&first, 1);
    controller.requestMediaPresentation(&cancelled, 2);
    controller.requestMediaPresentation(&second, 3);
    controller.requestMediaPresentation(&second, 4);
    controller.cancelMediaPresentation(&cancelled);
    {
        QObject destroyed;
        controller.requestMediaPresentation(&destroyed, 9);
    }
    CHECK(grants.empty());
    CHECK(controller.effectScale() == 0.88);
    CHECK(controller.preferredSampleCount() == 0);
    showWindow(window);
    CHECK(QTest::qWaitFor([&] { return grants.size() == 2; }, 3000));
    CHECK(grants[0].requester == &first && grants[0].generation == 1);
    CHECK(grants[1].requester == &second && grants[1].generation == 4);
    CHECK(grants[1].sequence > grants[0].sequence);
    CHECK(grants[1].renderPass > grants[0].renderPass);
    QTest::qWait(100);
    CHECK(grants.size() == 2);
    CHECK(controller.effectScale() == 0.88);
    // Real window lifecycle: no hidden-window grants, then recovery on show.
    window.hide();
    controller.requestMediaPresentation(&first, 5);
    QTest::qWait(100);
    CHECK(grants.size() == 2);
    showWindow(window);
    CHECK(QTest::qWaitFor([&] { return grants.size() == 3; }, 3000));
    CHECK(grants.back().generation == 5);
    QTest::qWait(100);
    qInfo("PASS: real render barriers, latest-request coalescing, cancellation, hide/show");

    // Deterministic signal stress uses the basic software render loop only.
    // The other invocation exercises a real OpenGL scene graph independently.
    if (app.arguments().contains("--synthetic")) {
        MetaCallCounter counter;
        controller.installEventFilter(&counter);
        drain(controller);
        const auto ordinaryBefore = counter.count;
        for (int i = 0; i < 300; ++i)
            window.afterRendering();
        drain(controller);
        CHECK(counter.count == ordinaryBefore);
        controller.requestMediaPresentation(&first, 6);
        controller.requestMediaPresentation(&second, 7);
        CHECK(grants.size() == 4);
        window.beforeSynchronizing();
        const auto beforeAck = counter.count;
        for (int i = 0; i < 300; ++i)
            window.afterRendering();
        drain(controller);
        CHECK(counter.count == beforeAck + 1);
        CHECK(grants.size() == 5 && grants.back().generation == 7);
        for (int i = 0; i < 300; ++i)
            window.afterRendering();
        drain(controller);
        CHECK(counter.count == beforeAck + 1);
        renderBarrier(window);
        drain(controller);
        CHECK(counter.count == beforeAck + 2);
        qInfo("PASS: 900 duplicate/ordinary render signals produce exactly two ticket acknowledgments");

        // A queued acknowledgment from a detached window cannot release a
        // presentation that belongs to another window.
        QQuickWindow replacement;
        showWindow(replacement);
        controller.requestMediaPresentation(&first, 8);
        renderBarrier(window); // acknowledgment queued, not dispatched
        controller.attachWindow(&replacement);
        controller.requestMediaPresentation(&first, 9);
        controller.requestMediaPresentation(&second, 10);
        const auto beforeStale = grants.size();
        drain(controller);
        CHECK(grants.size() == beforeStale);
        renderBarrier(replacement);
        drain(controller);
        CHECK(grants.size() == beforeStale + 1 && grants.back().generation == 10);
        renderBarrier(replacement);
        drain(controller);
        qInfo("PASS: detached window cannot acknowledge a new window's ticket");

        // Queue a burst without delivering the GUI-thread callbacks. The
        // recorded interval must come from signal time, not drain time.
        QTest::qSleep(300);
        for (int i = 0; i < 85; ++i) {
            replacement.frameSwapped();
            QTest::qSleep(10);
        }
        QTest::qSleep(100);
        drain(controller);
        const double interval = controller.frameIntervalMs();
        CHECK(interval > 5.0 && interval < 45.0);
        CHECK(controller.effectScale() == 0.88);
        const auto quality = controller.effectiveQuality();
        for (int i = 0; i < 3; ++i) {
            QTest::qSleep(300);
            replacement.frameSwapped();
            drain(controller);
        }
        CHECK(controller.frameIntervalMs() == interval);
        CHECK(controller.effectiveQuality() == quality);
        qInfo("PASS: timestamped frame burst %.2f ms; isolated idle redraws do not change quality", interval);
        controller.removeEventFilter(&counter);
        controller.attachWindow(nullptr);
        replacement.hide();
    }
    controller.attachWindow(nullptr);
    window.hide();

    // Actual typed QML methods (not C++ substitutes): the same generation
    // reaches only its owner, while the native observation signal remains.
    QQmlEngine engine;
    QQmlComponent receiver(&engine);
    receiver.setData(R"QML(import QtQml
QtObject {
    property int received: 0
    property int calls: 0
    function acceptMediaPresentation(generation: int) {
        received = generation
        ++calls
    }
})QML", QUrl{});
    CHECK(receiver.isReady());
    std::unique_ptr<QObject> a(receiver.create()), b(receiver.create());
    CHECK(a && b);
    controller.attachWindow(&window);
    controller.requestMediaPresentation(a.get(), 101);
    controller.requestMediaPresentation(b.get(), 102);
    controller.requestMediaPresentation(b.get(), 103);
    CHECK(a->property("calls").toInt() == 0 && b->property("calls").toInt() == 0);
    showWindow(window);
    CHECK(QTest::qWaitFor([&] {
        return a->property("calls").toInt() == 1 && b->property("calls").toInt() == 1;
    }, 3000));
    CHECK(a->property("received").toInt() == 101);
    CHECK(b->property("received").toInt() == 103);
    CHECK(grants.back().requester == b.get() && grants.back().generation == 103);
    QTest::qWait(100);
    CHECK(a->property("calls").toInt() == 1 && b->property("calls").toInt() == 1);
    controller.attachWindow(nullptr);
    window.hide();
    qInfo("PASS: targeted delivery to actual typed QML methods");
    return 0;
}
