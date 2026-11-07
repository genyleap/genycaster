/*!
 * @file        ui/app/bootstrap/context.cppm
 * @brief       QML context publication and presentation property contract.
 *
 * @details
 * Registers controller objects and non-secret font families in the root QML
 * context. Validates the complete controller list before publication so an
 * invalid or duplicate property never creates a partially initialized shell.
 * All QObject instances remain C++-owned by the application entry point and
 * must outlive the QQmlApplicationEngine during shutdown.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "qt_fwd.hpp"
#include <initializer_list>

export module genycaster.ui.bootstrap.context;

import genycaster.ui.bootstrap.fonts;

export namespace genycaster::ui::bootstrap {

/**
 * @brief Binding between a stable QML-visible name and a C++-owned QObject.
 *
 * The pointer is borrowed. The application retains ownership and releases
 * the QML roots before destroying any referenced controller.
 */
struct ContextObject final {
    const char* name{};
    QObject* object{};
};

/**
 * @brief Publish validated controller pointers into the root QML context.
 *
 * @param engine Loaded application engine, prior to Main.qml construction.
 * @param objects All context objects in a single non-owning registration batch.
 * @return False for empty names, missing objects, or duplicate names.
 *
 * Validation completes before the first context property is published.
 * The caller must not hand over a QObject that will be destroyed before the
 * engine's QML roots.
 */
[[nodiscard]] bool exposeControllers(
    QQmlApplicationEngine& engine,
    std::initializer_list<ContextObject> objects);

/**
 * @brief Publish platform and bundled font families as immutable values.
 *
 * @param engine Application QML engine.
 * @param fonts Initialized font policy whose values are copied into QML.
 */
void exposeFontFamilies(QQmlApplicationEngine& engine, const FontFamilies& fonts);

/**
 * @brief Publish UI platform flags without exposing native permissions.
 *
 * @param engine Application QML engine.
 * @param webReadOnly True only when browser operations have read-only policy.
 * @param nativeAudioAvailable False for offscreen or minimal backends.
 */
void exposePlatformFlags(
    QQmlApplicationEngine& engine, bool webReadOnly, bool nativeAudioAvailable);

} // namespace genycaster::ui::bootstrap
