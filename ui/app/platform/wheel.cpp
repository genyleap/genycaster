// Named-module implementation; platform headers are private to this unit.
module;

#include <QCoreApplication>
#include <QDebug>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWheelEvent>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <cmath>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

module genycaster.ui.web.wheel;

// Qt's WebAssembly screen can drop synthesized wheel events even when pointer
// dragging works. Scroll the nearest interactive QQuickFlickable instead of
// depending on accepted() from the synthetic wheel event.
//
// Qt Quick overlays are often full-window QQuickItems. A single childAt()
// would hit that *empty* overlay and never reach the actual feed. Walk items
// in visual stacking order, treating visible popup/modal surfaces as input
// boundaries while skipping noninteractive, empty overlays.
namespace genycaster::ui::browser_wheel {

bool moveAxis(QQuickItem* item, const char* positionName,
                     const char* contentName, const double delta)
{
    if (std::abs(delta) < 0.1 || !std::isfinite(delta)) return false;

    const auto currentValue = item->property(positionName);
    const auto contentValue = item->property(contentName);
    const auto interactiveValue = item->property("interactive");
    if (!currentValue.isValid() || !contentValue.isValid()
        || (interactiveValue.isValid() && !interactiveValue.toBool())) return false;

    const bool vertical = positionName[7] == 'Y'; // contentY / contentX
    const double viewport = vertical ? item->height() : item->width();
    const double content = contentValue.toDouble();
    const double previous = currentValue.toDouble();
    if (!std::isfinite(viewport) || !std::isfinite(content)
        || !std::isfinite(previous) || viewport <= 0) return false;

    // ListView/Flickable can have a non-zero (including negative) origin
    // after reusing and remeasuring delegates. Clamping to absolute zero
    // makes wheel ticks appear stuck at the wrong beginning/end.
    const auto originValue = item->property(vertical ? "originY" : "originX");
    const double minimum = originValue.isValid() ? originValue.toDouble() : 0.0;
    if (!std::isfinite(minimum)) return false;
    const double maximum = minimum + std::max(0.0, content - viewport);
    if (maximum - minimum < 1) return false;
    const double next = std::clamp(previous + delta, minimum, maximum);
    if (std::abs(previous - next) < 0.1) return false;

    // Browser wheel input adjusts contentY directly; unlike a native Flickable
    // gesture it never sets QQuickFlickable::moving. NotificationSurface uses
    // this explicit state to suppress stale viewport-anchor corrections while
    // the user's wheel burst is still in progress.
    const QVariant wheelActive = item->property("browserWheelInProgress");
    if (wheelActive.isValid() && !wheelActive.toBool())
        item->setProperty("browserWheelInProgress", true);
    if (!item->setProperty(positionName, next)) return false;
    return std::abs(item->property(positionName).toDouble() - previous) > 0.1;
}

enum class Result { NotHandled, Scrolled, Blocked };

Result scrollBranch(QQuickItem* item, const QPointF& scenePosition,
                           const QPoint& pixelDelta)
{
    if (!item || !item->isVisible() || !item->isEnabled()) return Result::NotHandled;
    const QPointF local = item->mapFromScene(scenePosition);
    if (!std::isfinite(local.x()) || !std::isfinite(local.y())) return Result::NotHandled;
    const bool contains = item->contains(local);
    if (!contains && item->clip()) return Result::NotHandled;

    // Stacking: larger z appears first; equal z paints later children on top.
    // A full-window overlay without an open popup must be transparent to this
    // *scroll lookup* (not to Qt's actual mouse event routing).
    auto children = item->childItems();
    std::reverse(children.begin(), children.end());
    std::stable_sort(children.begin(), children.end(),
        [](const QQuickItem* a, const QQuickItem* b) { return a->z() > b->z(); });
    for (auto* child : children) {
        const auto result = scrollBranch(child, scenePosition, pixelDelta);
        if (result != Result::NotHandled) return result;
    }

    if (contains) {
        if (moveAxis(item, "contentY", "contentHeight",
                     -static_cast<double>(pixelDelta.y()))
            || moveAxis(item, "contentX", "contentWidth",
                        -static_cast<double>(pixelDelta.x()))) {
            return Result::Scrolled;
        }
        const QString className = QString::fromLatin1(item->metaObject()->className());
        if (className.contains(QStringLiteral("PopupItem"))
            || item->objectName() == QStringLiteral("modalInputGuard")) {
            return Result::Blocked;
        }
    }
    return Result::NotHandled;
}

bool scroll(QQuickWindow& window, const QPointF& position,
                   const QPoint& qtPixelDelta)
{
    auto* root = window.contentItem();
    if (!root || position.x() < 0 || position.y() < 0
        || position.x() >= window.width() || position.y() >= window.height()) {
        return false;
    }
    // Modal popup QObjects can be owned by the screen instead of the visual
    // overlay. Looking them up via findChildren() on *every wheel tick* was a
    // linear scan of the entire feed/delegate QObject tree, especially costly
    // in Chrome and on screens with many casts. Cache only a currently open
    // modal and invalidate it when it closes, is destroyed or focus enters
    // another popup. This does not retain raw pointers across route changes.
    static QPointer<QQuickWindow> cachedWindow{};
    static QPointer<QObject> cachedModal{};
    if (cachedWindow == &window && cachedModal
        && cachedModal->property("modal").toBool()
        && cachedModal->property("visible").toBool()) {
        auto* content = cachedModal->property("contentItem").value<QQuickItem*>();
        auto* focus = window.activeFocusItem();
        if (content && (!focus || focus == content || content->isAncestorOf(focus)))
            return scrollBranch(content, position, qtPixelDelta) == Result::Scrolled;
    }
    cachedWindow = &window;
    cachedModal.clear();
    // Qt Quick's modal Popup/Drawer content lives in an overlay, with the
    // Popup QObject parented to the originating control rather than always
    // to the visual overlay Item. Before walking the feed, route the wheel
    // strictly into the topmost open modal's content item. Otherwise scrolling
    // outside the modal would incorrectly move the feed behind its scrim.
    for (auto* branch : root->childItems()) {
        const QVariant overlayState = branch->property("overlayOpen");
        if (overlayState.isValid() && !overlayState.toBool()) {
            // The main AppShell has no open modal; skip an expensive search
            // through thousands of feed delegates for every wheel tick.
            continue;
        }
        const auto objects = branch->findChildren<QObject*>();
        for (auto it = objects.crbegin(); it != objects.crend(); ++it) {
            QObject* object = *it;
            if (!object->property("modal").toBool()
                || !object->property("visible").toBool()) continue;
            const auto content = object->property("contentItem");
            if (!content.isValid()) continue;
            auto* item = content.value<QQuickItem*>();
            if (!item) continue;
            cachedModal = object;
            return scrollBranch(item, position, qtPixelDelta) == Result::Scrolled;
        }
    }
    return scrollBranch(root, position, qtPixelDelta) == Result::Scrolled;
}

} // namespace genycaster::ui::browser_wheel


#if defined(Q_OS_WASM)
namespace {
QPointer<QQuickWindow> browserWheelWindow{};
}

// Emscripten routes DOM wheel input directly into C++. Do not depend on
// Module exports, which are not guaranteed by the browser loader.
namespace {
double normalizedCanvasCoordinate(int clientX, int clientY, int axis)
{
    return EM_ASM_DOUBLE({
        const x = $0;
        const y = $1;
        // The capture-phase DOM observer sees the real composed event path
        // even when Qt's screen canvas lives inside a shadow root or has
        // pointer-events retargeted to a wrapper. No Module export required.
        const hit = window.__genycasterWheelHit;
        if (hit && hit.clientX === x && hit.clientY === y
            && hit.x >= 0 && hit.y >= 0)
            return $2 === 0 ? hit.x : hit.y;
        let element = document.elementFromPoint(x, y);
        while (element && element.shadowRoot) {
            const child = element.shadowRoot.elementFromPoint(x, y);
            if (!child || child === element) break;
            element = child;
        }
        // Qt can make its canvas transparent to pointer input and target the
        // owning window wrapper instead, including inside the screen shadow root.
        if (!(element instanceof HTMLCanvasElement))
            element = element?.closest?.('.qt-window')?.querySelector('canvas.qt-window-canvas');
        if (!(element instanceof HTMLCanvasElement)) return -1;
        const rect = element.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) return -1;
        return $2 === 0 ? (x - rect.left) / rect.width
            : (y - rect.top) / rect.height;
    }, clientX, clientY, axis);
}

void consumeBrowserWheelEvent(int clientX, int clientY)
{
    EM_ASM({
        const e = window.__genycasterWheelEvent;
        if (e && e.clientX === $0 && e.clientY === $1) {
            e.stopImmediatePropagation();
            window.__genycasterWheelEvent = null;
        }
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.forwarded += 1;
    }, clientX, clientY);
}

// Wheel bursts can arrive faster than Qt can paint on the browser main
// thread. Resolve the scene and commit its geometry once per animation frame.
struct PendingWheelInput final {
    QPointer<QQuickWindow> window{};
    QPointF position{};
    QPoint pixels{};
    QPoint angles{};
    Qt::KeyboardModifiers modifiers{};
    bool frameScheduled{false};
};
PendingWheelInput pendingWheelInput{};

void flushBrowserWheelInput()
{
    auto* window = pendingWheelInput.window.data();
    const QPoint pixels = pendingWheelInput.pixels;
    const QPoint angles = pendingWheelInput.angles;
    pendingWheelInput.pixels = {};
    pendingWheelInput.angles = {};
    if (!window || !window->isVisible() || pixels.isNull()) {
        return;
    }
    const QPointF local = pendingWheelInput.position;
    if (genycaster::ui::browser_wheel::scroll(*window, local, pixels)) {
        EM_ASM({
            if (window.__genycasterWheelDiagnostics) {
                window.__genycasterWheelDiagnostics.directScrolled += 1;
            }
        });
        return;
    }
    // Non-Flickable controls still receive one Qt event. The original DOM
    // event was already consumed, including when Qt declines it at a bound.
    QWheelEvent wheel{local, window->mapToGlobal(local.toPoint()), pixels, angles,
                      Qt::NoButton, pendingWheelInput.modifiers, Qt::NoScrollPhase, false};
    wheel.setAccepted(false);
    QCoreApplication::sendEvent(window, &wheel);
}

EM_BOOL deliverBrowserWheelFrame(double, void*)
{
    pendingWheelInput.frameScheduled = false;
    flushBrowserWheelInput();
    return EM_FALSE;
}

EM_BOOL browserWheelCallback(int type, const EmscriptenWheelEvent* event, void*)
{
    QQuickWindow* window = browserWheelWindow.data();
    if (type != EMSCRIPTEN_EVENT_WHEEL || !event || !window ||
        !window->isVisible() || event->mouse.ctrlKey) return EM_FALSE;
    EM_ASM({
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.received += 1;
    });
    const double x = normalizedCanvasCoordinate(event->mouse.clientX, event->mouse.clientY, 0);
    const double y = normalizedCanvasCoordinate(event->mouse.clientX, event->mouse.clientY, 1);
    if (x < 0 || x > 1 || y < 0 || y > 1 ||
        !std::isfinite(event->deltaX) || !std::isfinite(event->deltaY)) return EM_FALSE;
    EM_ASM({
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.onCanvas += 1;
    });
    const double scale = event->deltaMode == DOM_DELTA_LINE ? 16.0
        : (event->deltaMode == DOM_DELTA_PAGE ? window->height() : 1.0);
    const auto bounded = [](double v) {
        return static_cast<int>(std::lround(std::clamp(v, -2000.0, 2000.0)));
    };
    // Chromium touchpads can emit sub-pixel deltas in rapid succession.
    // Rounding each event independently to zero makes scrolling feel as if
    // some wheel ticks never arrive. Carry fractional pixels to the next tick.
    static QPointF wheelRemainder{};
    const auto pixelsForAxis = [](double value, double& remainder) {
        const double accumulated = std::clamp(value, -2000.0, 2000.0) + remainder;
        const int whole = static_cast<int>(std::trunc(accumulated));
        remainder = accumulated - whole;
        return whole;
    };
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    if (event->mouse.shiftKey) modifiers |= Qt::ShiftModifier;
    if (event->mouse.altKey) modifiers |= Qt::AltModifier;
    if (event->mouse.metaKey) modifiers |= Qt::MetaModifier;
    const QPointF local{x * window->width(), y * window->height()};
    const QPoint pixels{
        pixelsForAxis(-event->deltaX * scale, wheelRemainder.rx()),
        pixelsForAxis(-event->deltaY * scale, wheelRemainder.ry())};
    const QPoint angles{bounded(-event->deltaX * scale * 1.2),
                        bounded(-event->deltaY * scale * 1.2)};
    // Own each eligible DOM tick before either direct or synthetic delivery.
    // In particular a fractional tick must not reach Qt while its remainder
    // is also carried into the next direct scroll (two competing scroll paths).
    consumeBrowserWheelEvent(event->mouse.clientX, event->mouse.clientY);
    if (pixels.isNull()) {
        return EM_FALSE;
    }
    // Preserve the old target when the pointer or modifiers change between
    // ticks; stationary bursts share one scene traversal and viewport commit.
    if (pendingWheelInput.window != window || pendingWheelInput.position != local
        || pendingWheelInput.modifiers != modifiers) {
        flushBrowserWheelInput();
    }
    pendingWheelInput.window = window;
    pendingWheelInput.position = local;
    pendingWheelInput.modifiers = modifiers;
    pendingWheelInput.pixels += pixels;
    pendingWheelInput.angles += angles;
    if (!pendingWheelInput.frameScheduled) {
        pendingWheelInput.frameScheduled = true;
        emscripten_request_animation_frame(deliverBrowserWheelFrame, nullptr);
    }

    // Cancellation belongs to the explicitly non-passive DOM observer below.
    // Returning true asks Emscripten's passive root listener to preventDefault,
    // which Chromium ignores and reports as an intervention on every tick.
    return EM_FALSE;
}

void installBrowserWheelBridge(QQuickWindow* window)
{
    browserWheelWindow = window;
    EM_ASM({
        if (!window.__genycasterWheelPathInstalled) {
            window.__genycasterWheelPathInstalled = true;
            window.addEventListener('wheel', function(event) {
                const path = event.composedPath ? event.composedPath() : [event.target];
                let canvas = path.find(node => node instanceof HTMLCanvasElement);
                if (!canvas) {
                    const owner = path.find(node => node?.classList?.contains('qt-window'));
                    canvas = owner?.querySelector('canvas.qt-window-canvas');
                }
                const hit = Object.create(null);
                hit.clientX = event.clientX;
                hit.clientY = event.clientY;
                hit.x = -1;
                hit.y = -1;
                if (canvas) {
                    const rect = canvas.getBoundingClientRect();
                    if (rect.width > 0 && rect.height > 0) {
                        hit.x = (event.clientX - rect.left) / rect.width;
                        hit.y = (event.clientY - rect.top) / rect.height;
                    }
                }
                window.__genycasterWheelHit = hit;
                window.__genycasterWheelEvent = event;
                if (hit.x >= 0 && hit.x <= 1 && hit.y >= 0 && hit.y <= 1
                    && !event.ctrlKey && Number.isFinite(event.deltaX)
                    && Number.isFinite(event.deltaY) && event.cancelable)
                    event.preventDefault();
            }, {capture: true, passive: false});
        }
    });
    EM_ASM({
        if (!window.__genycasterWheelDiagnostics) {
            window.__genycasterWheelDiagnostics = Object.create(null);
            window.__genycasterWheelDiagnostics.received = 0;
            window.__genycasterWheelDiagnostics.onCanvas = 0;
            window.__genycasterWheelDiagnostics.forwarded = 0;
            window.__genycasterWheelDiagnostics.directScrolled = 0;
        }
    });
    const EMSCRIPTEN_RESULT result = emscripten_set_wheel_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, browserWheelCallback);
    if (result != EMSCRIPTEN_RESULT_SUCCESS)
        qWarning() << "WASM wheel callback registration failed:" << result;
}
}
#endif

namespace genycaster::ui::web {
void installWheelBridge(QQuickWindow* window)
{
#if defined(Q_OS_WASM)
    installBrowserWheelBridge(window);
#else
    Q_UNUSED(window);
#endif
}
} // namespace genycaster::ui::web

