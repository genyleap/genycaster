/*!
 * @file        ui/app/bootstrap/teardown.cppm
 * @brief       Native QML shutdown sequencing.
 *
 * @details
 * Releases QML root objects and scene-graph resources after network producers stop and bootstrap work joins.
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
     * The function synchronously releases scene-graph resources, deletes roots
     * and drains deferred cleanup; it must not restart the event loop.
     *
     * @param engine Engine whose root objects are to be released.
     * @param mainWindow Window to hide and release, or null when absent.
     */
    void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow);
}
