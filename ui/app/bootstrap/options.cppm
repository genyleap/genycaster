/*!
 * @file        ui/app/bootstrap/options.cppm
 * @brief       Bootstrap argument and service URL validation.
 *
 * @details
 * Defines command-line option extraction, rendering preference selection and strict service endpoint checks shared by startup paths.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"

export module genycaster.ui.bootstrap;

export namespace genycaster::ui::bootstrap {
    [[nodiscard]] QString optionValue(const QStringList& arguments, const QString& prefix);
    [[nodiscard]] QString startupRenderingProfile(int argc, char* argv[]);
    [[nodiscard]] QString normalizeConfigurationToken(QString value);
    [[nodiscard]] bool hasUnsafeConfigurationCharacters(QStringView value);
    [[nodiscard]] bool isValidHttpServiceUrl(const QString& value, bool allowQuery = false);
    [[nodiscard]] bool isValidSiwfUri(const QString& value);
    [[nodiscard]] bool isValidSiwfDomain(const QString& value);
    [[nodiscard]] bool siwfDomainMatchesUri(const QString& domain, const QString& uri);
}
