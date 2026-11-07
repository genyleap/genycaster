/*!
 * @file        ui/app/platform/wheel.cppm
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
    /**
     * @brief Scroll the nearest eligible Qt Quick item below a browser pointer.
     *
     * Resolves hit order, ignores noninteractive overlays, and respects
     * visible modal surfaces instead of scrolling background feed content.
     * @param window Active QML scene window.
     * @param position Pointer location in window coordinates.
     * @param pixelDelta Browser wheel displacement.
     * @return True when a suitable scroll target consumed the motion.
     */
    [[nodiscard]] bool scroll(QQuickWindow& window, const QPointF& position,
                              const QPoint& pixelDelta);
}

export namespace genycaster::ui::web {
    /** @brief Install WASM browser wheel callbacks for the active QML window.
     *  A no-op for native targets. The callback remains owned by the adapter.
     */
    void installWheelBridge(QQuickWindow* window);
}
