// Startup and runtime smoke checks live outside the production bootstrap.
module;
#include "qt_fwd.hpp"
export module genycaster.ui.diagnostics.smoke;

export namespace genycaster::ui::diagnostics {
    // Returns a startup failure code or arranges an orderly test exit.
    [[nodiscard]] int configureSmokeTests(
        QGuiApplication& application,
        QQmlApplicationEngine& engine,
        PresentationBridge& presentationBridge,
        bool isSmokeTest,
        bool isRuntimeSmokeTest);
}
