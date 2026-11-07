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
    /**
     * @brief Arrange requested visual snapshots after QML construction.
     *
     * Capture jobs remain on the GUI thread and are scheduled only for
     * explicit diagnostic command-line options.
     *
     * @param application Owner of capture timers.
     * @param engine Loaded QML application engine.
     * @param presentationBridge Existing presentation facade.
     * @param richComposerController Existing composer state.
     * @param arguments Startup option list.
     * @param isVisualTest Whether automated visual testing was requested.
     * @param openExploreForTest Whether to activate Explore for a fixture.
     * @param snapshotPath Optional capture destination.
     * @return Zero on successful scheduling; nonzero on invalid setup.
     */
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
