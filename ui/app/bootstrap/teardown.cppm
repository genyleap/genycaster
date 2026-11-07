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
    // Caller must stop runtime producers and join bootstrap threads first.
    // Destroy root objects before QGuiApplication and borrowed controller owners.
    void releaseQmlRoots(QQmlApplicationEngine& engine, QQuickWindow* mainWindow);
}
