// Native visual test fixtures and live snapshots, isolated from production
// application wiring. No QObject internals are exported across this module.
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
