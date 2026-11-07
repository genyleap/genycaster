/*!
 * @file        ui/app/web/wheel.cppm
 * @brief       WebAssembly wheel input bridge.
 *
 * @details
 * Defines native-to-browser wheel routing and a Qt Quick fallback that preserves scroll and modal interaction semantics.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
