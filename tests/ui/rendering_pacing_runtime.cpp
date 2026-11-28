#include "rendering_controller.hpp"
#include "../../ui/app/diagnostics/gui_event_trace.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QLoggingCategory>
#include <QByteArrayView>
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
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
QStringList traceMessages;
QStringList guiTraceMessages;
QMutex traceMutex;
void captureTrace(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    if (type != QtDebugMsg) return;
    const auto category = QByteArrayView(context.category);
    if (category == "genycaster.render.stalls" || category == "genycaster.render.gui") {
        const QMutexLocker lock{&traceMutex};
        (category == "genycaster.render.stalls" ? traceMessages : guiTraceMessages).append(message);
    }
}

double traceField(const QString& message, const QString& name) {
    const QRegularExpression pattern{QStringLiteral("(?:^| )") + name
        + QStringLiteral(" (-?[0-9]+(?:\\.[0-9]+)?)")};
    const auto match = pattern.match(message);
    return match.hasMatch() ? match.captured(1).toDouble() : -1.0;
}

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
    genycaster::ui::diagnostics::GuiEventTraceApplication<
        QGuiApplication, &RenderingController::traceClockMs> app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    if (app.arguments().contains("--synthetic")) {
        struct SlowReceiver final : QObject {
            bool event(QEvent* event) override {
                if (event->type() == QEvent::User) {
                    QEvent nested{static_cast<QEvent::Type>(QEvent::User + 1)};
                    QCoreApplication::sendEvent(this, &nested);
                    return true;
                }
                if (event->type() == QEvent::User + 1) {
                    QTest::qSleep(22);
                    return true;
                }
                return QObject::event(event);
            }
        } receiver;
        const auto oldHandler = qInstallMessageHandler(captureTrace);
        guiTraceMessages.clear();
        QEvent event{QEvent::User};
        QCoreApplication::sendEvent(&receiver, &event);
        CHECK(guiTraceMessages.isEmpty());
        QLoggingCategory::setFilterRules(QStringLiteral("genycaster.render.gui.debug=true"));
        QCoreApplication::sendEvent(&receiver, &event);
        CHECK(guiTraceMessages.size() == 1);
        CHECK(traceField(guiTraceMessages.first(), QStringLiteral("durationMs")) >= 16.0);
        CHECK(traceField(guiTraceMessages.first(), QStringLiteral("guiEvent"))
              == static_cast<double>(QEvent::User));
        qInstallMessageHandler(oldHandler);
        QLoggingCategory::setFilterRules(QString{});
        qInfo("PASS: opt-in outermost GUI event timing");
    }
    // The queue is independent of Qt and uses actual timestamps in FIFO order.
    {
        FrameObservationQueue queue;
        using namespace std::chrono_literals;
        const FrameObservationQueue::Time start{};
        for (int i = 0; i < 300; ++i)
            CHECK(queue.push(start + i * 1ms, 1) == (i == 0));
        const auto overflow = queue.take();
        CHECK(overflow.count == 256 && overflow.dropped == 44);
        CHECK(overflow.firstQueued == start && overflow.epoch == 1);
        for (std::size_t i = 0; i < overflow.count; ++i)
            CHECK(overflow.timestamps[i] == start + (i + 44) * 1ms);
        CHECK(queue.push(start + 300ms, 1));
        CHECK(!queue.push(start + 301ms, 2));
        const auto next = queue.take();
        CHECK(next.count == 1 && next.dropped == 0 && next.epoch == 2);
        CHECK(next.timestamps[0] == start + 301ms);
        CHECK(queue.take().count == 0);
        qInfo("PASS: bounded FIFO, overflow and epoch replacement");
    }
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
        drain(controller);
        const auto beforeBurst = counter.count;
        QTest::qSleep(300);
        for (int i = 0; i < 85; ++i) {
            replacement.frameSwapped();
            QTest::qSleep(10);
        }
        QTest::qSleep(100);
        drain(controller);
        CHECK(counter.count == beforeBurst + 1);
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
        const auto beforeOverflow = counter.count;
        for (int i = 0; i < 10000; ++i)
            replacement.frameSwapped();
        drain(controller);
        CHECK(counter.count == beforeOverflow + 1);
        CHECK(controller.effectiveQuality() == quality);
        replacement.frameSwapped();
        replacement.hide();
        replacement.show();
        drain(controller);
        CHECK(controller.frameIntervalMs() == interval);
        // A pending notification from the old attachment must not consume
        // samples from the new attachment or mutate its telemetry.
        replacement.frameSwapped();
        controller.attachWindow(&window);
        const auto beforeDetached = counter.count;
        window.frameSwapped();
        drain(controller);
        CHECK(counter.count == beforeDetached + 2);
        qInfo("PASS: frame notification coalescing and detached-window isolation");
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

    if (app.arguments().contains("--synthetic")) {
        // No real window rendering during the synthetic diagnostic sequence.
        QQuickWindow traceWindow;
        CHECK(!controller.stallDiagnosticsEnabled());
        controller.attachWindow(&traceWindow);
        auto previous = qInstallMessageHandler(captureTrace);
        traceMessages.clear();
        traceWindow.beforeFrameBegin();
        traceWindow.beforeRendering();
        QTest::qSleep(30);
        traceWindow.afterRendering();
        traceWindow.afterFrameEnd();
        CHECK(traceMessages.isEmpty());
        controller.attachWindow(nullptr);

        QLoggingCategory::setFilterRules(QStringLiteral("genycaster.render.stalls.debug=true"));
        CHECK(controller.stallDiagnosticsEnabled());
        controller.attachWindow(&traceWindow);
        traceMessages.clear();
        traceWindow.beforeFrameBegin();
        traceWindow.beforeSynchronizing();
        traceWindow.afterSynchronizing();
        traceWindow.beforeRendering();
        QTest::qSleep(10);
        traceWindow.beforeRenderPassRecording();
        QTest::qSleep(30);
        traceWindow.afterRenderPassRecording();
        QTest::qSleep(10);
        traceWindow.afterRendering();
        traceWindow.afterFrameEnd();
        CHECK(traceMessages.size() == 1);
        CHECK(traceField(traceMessages.first(), QStringLiteral("recordingMs")) >= 25.0);
        CHECK(traceMessages.first().contains("newMediaTicket false"));
        CHECK(traceField(traceMessages.first(), QStringLiteral("preMainPassMs")) >= 8.0);
        CHECK(traceField(traceMessages.first(), QStringLiteral("mainPassMs")) >= 25.0);
        CHECK(traceField(traceMessages.first(), QStringLiteral("postMainPassMs")) >= 8.0);
        CHECK(traceField(traceMessages.first(), QStringLiteral("submitTailMs")) >= 0.0);
        CHECK(traceField(traceMessages.first(), QStringLiteral("totalMs"))
              >= traceField(traceMessages.first(), QStringLiteral("recordingMs")));
        traceWindow.afterFrameEnd(); // duplicate cannot report twice
        CHECK(traceMessages.size() == 1);
        traceWindow.beforeFrameBegin();
        traceWindow.beforeRendering();
        traceWindow.afterRendering();
        QTest::qSleep(30);
        traceWindow.afterFrameEnd();
        CHECK(traceMessages.size() == 2);
        CHECK(traceMessages.last().contains("syncMs -1"));
        CHECK(traceMessages.last().contains("mainPassMs -1"));
        CHECK(traceField(traceMessages.last(), QStringLiteral("submitTailMs")) >= 25.0);
        controller.recordUiMarker(QStringLiteral("fixture.route"));
        const double start = controller.diagnosticClockMs();
        QTest::qSleep(15);
        controller.recordUiOperation(QStringLiteral("fixture.projection"), start, 5000);
        CHECK(traceMessages.size() == 4);
        CHECK(traceMessages.at(2).contains("uiMarker"));
        CHECK(traceMessages.at(3).contains("rows 5000"));
        controller.recordUiOperation(QStringLiteral("fixture.fast"),
                                     controller.diagnosticClockMs(), 0);
        CHECK(traceMessages.size() == 4);
        controller.attachWindow(nullptr);
        traceWindow.beforeFrameBegin();
        traceWindow.afterFrameEnd();
        CHECK(traceMessages.size() == 4);
        qInstallMessageHandler(previous);
        QLoggingCategory::setFilterRules(QString{});
        qInfo("PASS: opt-in frame phase diagnostics, incomplete sync, duplicate and detached frame rejection");
    } else {
        // Real threaded RHI path: attach the diagnostic listener before a
        // single controlled slow recording callback. Capture from the render
        // thread and inspect only while holding the mutex.
        QLoggingCategory::setFilterRules(QStringLiteral("genycaster.render.stalls.debug=true"));
        QQuickWindow traceWindow;
        auto previous = qInstallMessageHandler(captureTrace);
        {
            const QMutexLocker lock{&traceMutex};
            traceMessages.clear();
        }
        controller.attachWindow(&traceWindow);
        const auto inject = std::make_shared<std::atomic<bool>>(true);
        QObject::connect(&traceWindow, &QQuickWindow::beforeRendering,
            &traceWindow, [inject] {
                if (inject->exchange(false))
                    QTest::qSleep(35);
            }, Qt::DirectConnection);
        showWindow(traceWindow);
        CHECK(QTest::qWaitFor([] {
            const QMutexLocker lock{&traceMutex};
            for (const auto& message : traceMessages) {
                if (message.startsWith(QStringLiteral("frame "))
                    && traceField(message, QStringLiteral("recordingMs")) >= 30.0
                    && traceField(message, QStringLiteral("totalMs")) >= 30.0
                    && traceField(message, QStringLiteral("submitTailMs")) >= 0.0)
                    return true;
            }
            return false;
        }, 3000));
        traceWindow.hide();
        controller.attachWindow(nullptr);
        qInstallMessageHandler(previous);
        QLoggingCategory::setFilterRules(QString{});
        qInfo("PASS: real RHI frame phase diagnostics with a controlled recording delay");
    }
    // Shutdown cannot be reversed by a late ready image, queued ack or
    // a window activation. Already posted frame drains are harmless.
    controller.attachWindow(&window);
    showWindow(window);
    controller.requestMediaPresentation(&first, 201);
    controller.requestMediaPresentation(&second, 202);
    const auto beforeShutdown = grants.size();
    window.frameSwapped();
    if (app.arguments().contains("--synthetic"))
        renderBarrier(window);
    controller.beginShutdown();
    CHECK(controller.shuttingDown());
    controller.beginShutdown();
    controller.requestMediaPresentation(&first, 203);
    controller.attachWindow(&window);
    drain(controller);
    QTest::qWait(50);
    CHECK(grants.size() == beforeShutdown);
    window.hide();
    qInfo("PASS: irreversible shutdown rejects new media and stale render callbacks");
    return 0;
}
