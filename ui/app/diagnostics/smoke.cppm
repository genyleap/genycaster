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
    // Returns a startup failure code or arranges an orderly test exit.
    [[nodiscard]] int configureSmokeTests(
        QGuiApplication& application,
        QQmlApplicationEngine& engine,
        PresentationBridge& presentationBridge,
        bool isSmokeTest,
        bool isRuntimeSmokeTest);
}
