/*!
 * @file        ui/app/diagnostics/smoke.cppm
 * @brief       UI and runtime smoke diagnostics.
 *
 * @details
 * Declares readiness tests for QML navigation, application shell and runtime initialization.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"
export module genycaster.ui.diagnostics.smoke;

export namespace genycaster::ui::diagnostics {
    /**
     * @brief Install optional GUI and runtime readiness assertions.
     *
     * Observes the constructed QML tree and presentation runtime without
     * creating extra protocol sessions or modifying production setup state.
     *
     * @param application GUI application lifetime owner.
     * @param engine Loaded QML root engine.
     * @param presentationBridge Current runtime readiness facade.
     * @param isSmokeTest Enable the immediate QML shell test.
     * @param isRuntimeSmokeTest Enable the asynchronous runtime test.
     * @return Zero on successful setup or a startup error code.
     */
    [[nodiscard]] int configureSmokeTests(
        QGuiApplication& application,
        QQmlApplicationEngine& engine,
        PresentationBridge& presentationBridge,
        bool isSmokeTest,
        bool isRuntimeSmokeTest);
}
