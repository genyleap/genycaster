#include "Wheel.hpp"
#include "../browser_wheel_fallback.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QPointer>
#include <QQuickWindow>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#if defined(__EMSCRIPTEN__)
namespace {
QPointer<QQuickWindow> browserWheelWindow{};
}

// Emscripten routes DOM wheel input directly into C++. Do not depend on
// Module exports, which are not guaranteed with Qt 6.12's JS loader.
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
        if (!(element instanceof HTMLCanvasElement)) return -1;
        const rect = element.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) return -1;
        return $2 === 0 ? (x - rect.left) / rect.width
            : (y - rect.top) / rect.height;
    }, clientX, clientY, axis);
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
    // WebAssembly Qt's synthesized wheel events are not reliable for
    // Flickables: they can be discarded by the screen/pointer handler even
    // while dragging scrolls normally. Move the nearest interactive
    // Flickable's viewport directly, including ComboBox popup ListViews.
    if (genycaster::ui::browser_wheel::scroll(*window, local, pixels)) {
        EM_ASM({
            // The listener on window is registered in capture phase, before
            // Qt's own canvas listener. Chromium may otherwise deliver the
            // same wheel tick twice (native viewport + Qt synthetic event),
            // sometimes scrolling back to the previous location.
            const e = window.__genycasterWheelEvent;
            if (e && e.clientX === $0 && e.clientY === $1) {
                e.stopImmediatePropagation();
                if (e.cancelable) e.preventDefault();
                window.__genycasterWheelEvent = null;
            }
            if (window.__genycasterWheelDiagnostics) {
                window.__genycasterWheelDiagnostics.forwarded += 1;
                window.__genycasterWheelDiagnostics.directScrolled += 1;
            }
        }, event->mouse.clientX, event->mouse.clientY);
        return EM_TRUE;
    }
    // Retain Qt wheel delivery for non-Flickable controls, e.g. an editable
    // spin box; don't consume events that Qt itself declines.
    QWheelEvent wheel{local, window->mapToGlobal(local.toPoint()), pixels, angles,
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false};
    wheel.setAccepted(false);
    QCoreApplication::sendEvent(window, &wheel);
    // Do not swallow wheel events Qt Quick did not handle. In particular,
    // popup/list bounds and early screen initialization may leave events
    // unaccepted; allow Qt's own web event routing to handle those.
    if (!wheel.isAccepted()) return EM_FALSE;
    EM_ASM({
        const e = window.__genycasterWheelEvent;
        if (e && e.clientX === $0 && e.clientY === $1) {
            e.stopImmediatePropagation();
            if (e.cancelable) e.preventDefault();
            window.__genycasterWheelEvent = null;
        }
        if (window.__genycasterWheelDiagnostics)
            window.__genycasterWheelDiagnostics.forwarded += 1;
    }, event->mouse.clientX, event->mouse.clientY);
    return EM_TRUE;
}

void installBrowserWheelBridge(QQuickWindow* window)
{
    browserWheelWindow = window;
    EM_ASM({
        if (!window.__genycasterWheelPathInstalled) {
            window.__genycasterWheelPathInstalled = true;
            window.addEventListener('wheel', function(event) {
                const path = event.composedPath ? event.composedPath() : [event.target];
                const canvas = path.find(node => node instanceof HTMLCanvasElement);
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
            }, {capture: true, passive: true});
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
#if defined(__EMSCRIPTEN__)
    installBrowserWheelBridge(window);
#else
    Q_UNUSED(window);
#endif
}
} // namespace genycaster::ui::web
