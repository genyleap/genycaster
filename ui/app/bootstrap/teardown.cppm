/*!
 * @file        ui/app/bootstrap/teardown.cppm
 * @brief       Native QML shutdown sequencing.
 *
 * @details
 * After producers stop and bootstrap joins, destroys QQuickPopup objects, releases QQuickOverlay before its host window, detaches visual roots, then destroys QML roots.
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
     * Deletes QQuickPopup objects before the associated QQuickOverlay, while
     * both the overlay and QQuickWindow are alive. Then detaches remaining
     * visual items, destroys QObject-owned QML roots, and drains deferred
     * cleanup. Does not hide the native window early or restart the event loop.
     *
     * @param engine Engine whose root objects are to be released.
     * @param mainWindow Deprecated window hint; kept for ABI/source compatibility.
     *                   Only live engine root windows are traversed.
     */
    void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow);
}
