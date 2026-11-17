/*!
 * @file        ui/app/bootstrap/teardown.cppm
 * @brief       Native QML shutdown sequencing.
 *
 * @details
 * Destroys QML root objects only after network producers stop and bootstrap work joins; Qt releases scene-graph resources during QQuickWindow destruction.
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
     * The function detaches Qt Quick overlays while the window remains valid,
     * deletes roots, then drains deferred cleanup. It must not hide the native
     * window early or restart the event loop.
     *
     * @param engine Engine whose root objects are to be released.
     * @param mainWindow Window whose overlays need safe teardown, or null when absent.
     */
    void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow);
}
