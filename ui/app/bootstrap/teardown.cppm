/*!
 * @file        ui/app/bootstrap/teardown.cppm
 * @brief       Native QML shutdown sequencing.
 *
 * @details
 * After producers stop and bootstrap joins, releases QML-owned scene items while their window lives, then remaining Popups and overlays, followed by the QML window roots.
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
     * Advances pending QML incubation within a final time budget and detaches
     * the controller before destroying the object tree.
     * Deletes QML visual owners before standalone Popups and their associated
     * QQuickOverlay, keeping the QQuickWindow alive throughout. Loaders release
     * their own Popups before the overlay is destroyed. Finally destroys
     * QObject-owned roots and drains deferred cleanup on the GUI thread.
     *
     * @param engine Engine whose root objects are to be released.
     * @param mainWindow Deprecated window hint; kept for ABI/source compatibility.
     *                   Only live engine root windows are traversed.
     */
    void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow);
}
