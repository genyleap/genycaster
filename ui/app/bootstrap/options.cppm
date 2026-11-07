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
    /** @brief Read the first matching command-line override.
     *  @return Trimmed option value, or an empty string if absent.
     */
    [[nodiscard]] QString optionValue(const QStringList& arguments, const QString& prefix);
    /** @brief Resolve the initial graphics profile before UI construction.
     *  @return Canonical profile chosen from CLI, environment or preferences.
     */
    [[nodiscard]] QString startupRenderingProfile(int argc, char* argv[]);
    /** @brief Remove surrounding whitespace and supported transport quoting. */
    [[nodiscard]] QString normalizeConfigurationToken(QString value);
    /** @brief Reject whitespace and escape characters inside endpoint tokens. */
    [[nodiscard]] bool hasUnsafeConfigurationCharacters(QStringView value);
    /** @brief Validate a credential-free HTTP(S) origin or service URL.
     *  @param allowQuery Permit URL query parameters when required by a protocol.
     */
    [[nodiscard]] bool isValidHttpServiceUrl(const QString& value, bool allowQuery = false);
    /** @brief Validate the login relying-party URI without embedded credentials. */
    [[nodiscard]] bool isValidSiwfUri(const QString& value);
    /** @brief Validate the relying-party hostname and optional port. */
    [[nodiscard]] bool isValidSiwfDomain(const QString& value);
    /** @brief Require the login domain and URI to share the same authority. */
    [[nodiscard]] bool siwfDomainMatchesUri(const QString& domain, const QString& uri);
}
