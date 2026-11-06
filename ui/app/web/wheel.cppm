// Qt's ABI remains globally attached; full Qt headers are private to implementations.
module;
#include "qt_fwd.hpp"

export module genycaster.ui.web.wheel;

export namespace genycaster::ui::browser_wheel {
    [[nodiscard]] bool scroll(QQuickWindow& window, const QPointF& position,
                              const QPoint& pixelDelta);
}

export namespace genycaster::ui::web {
    void installWheelBridge(QQuickWindow* window);
}
