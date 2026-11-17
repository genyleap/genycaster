/*!
 * @file        ui/app/bootstrap/teardown.cppm
 * @brief       Native QML shutdown sequencing.
 *
 * @details
 * Detaches Qt Quick visual roots before the corresponding windows are destroyed, then releases QML roots after network producers stop and bootstrap work joins.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"

export module genycaster.ui.bootstrap.teardown;

export namespace genycaster::ui::bootstrap {
    /**
     * @brief Destroy all loaded QML roots while a GUI dispatcher still exists.
     *
     * Invoke only after stopping producers and joining runtime bootstrap.
     * Detaches each loaded window's visual root items (including overlays)
     * while the window is still valid, then deletes QObject-owned roots and
     * drains deferred cleanup. It must not hide the native window early or
     * restart the event loop.
     *
     * @param engine Engine whose root objects are to be released.
     * @param mainWindow Window whose overlays need safe teardown, or null when absent.
     */
    void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow);
}
