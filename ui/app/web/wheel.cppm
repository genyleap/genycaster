// Qt's public types are imported from the global module fragment.
// Keep the module interface small: no implementation detail headers.
module;

#include <QPoint>
#include <QPointF>
#include <QQuickWindow>

export module genycaster.ui.web.wheel;

export namespace genycaster::ui::browser_wheel {
    // Direct-scroll fallback used by the WASM wheel adapter and GUI tests.
    [[nodiscard]] bool scroll(QQuickWindow& window, const QPointF& position,
                              const QPoint& pixelDelta);
}

export namespace genycaster::ui::web {
    // No-op on native targets; registers the browser wheel bridge in WASM.
    void installWheelBridge(QQuickWindow* window);
}
