/*!
 * @file        ui/app/diagnostics/snapshots.cppm
 * @brief       Visual and live snapshot diagnostics.
 *
 * @details
 * Schedules bounded UI captures and diagnostic fixtures without enlarging the production bootstrap entry point.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"
export module genycaster.ui.diagnostics.snapshots;
export namespace genycaster::ui::diagnostics {
    // Returns a startup error (3 for a missing QQuickWindow), otherwise
    // schedules bounded capture timers on the owning GUI application's thread.
    [[nodiscard]] int scheduleSnapshots(
        QGuiApplication& application,
        QQmlApplicationEngine& engine,
        PresentationBridge& presentationBridge,
        RichComposerController& richComposerController,
        const QStringList& arguments,
        bool isVisualTest,
        bool openExploreForTest,
        const QString& snapshotPath);
}
