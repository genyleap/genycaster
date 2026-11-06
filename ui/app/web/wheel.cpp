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

#if defined(__EMSCRIPTEN__)
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


#if defined(__EMSCRIPTEN__)
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

