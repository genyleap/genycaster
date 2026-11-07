/*!
 * @file        ui/app/bootstrap/profiles.cppm
 * @brief       Provider profile configuration and credential policy.
 *
 * @details
 * Models explicitly configured Read and Write adapter instances, preferred
 * protocol routes, optional compatible Read fallback and per-instance API
 * credentials. Parsing fails closed on invalid identifiers, duplicate
 * profiles, unsafe endpoints or unauthorized credential destinations.
 * Bootstrap data does not own a signer, select a write adapter implicitly
 * or persist API credentials.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "qt_fwd.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

export module genycaster.ui.bootstrap.profiles;

export namespace genycaster::ui::bootstrap {

/** @brief Public settings for one explicitly selected Read adapter. */
struct ReadProviderProfile final {
    std::string id{};
    std::string displayName{};
    std::string endpoint{};
    std::string adapterContract{};
    bool selfHosted{false};
};

/** @brief Public settings for a separate, explicit Write adapter. */
struct WriteProviderProfile final {
    std::string id{};
    std::string displayName{};
    std::string endpoint{};
    std::string readEndpoint{};
    std::string adapterContract{};
    bool selfHosted{false};
};

/**
 * @brief Validated profile values ready for runtime adapter registration.
 *
 * Secrets are sourced only from the process environment or, for a browser
 * session, explicitly supplied ephemeral session storage. They are never
 * serialized into stored profile JSON or reported in diagnostics.
 */
struct ProviderProfileConfiguration final {
    std::vector<ReadProviderProfile> readProviders{};
    std::vector<WriteProviderProfile> writeProviders{};
    std::unordered_map<std::string, std::string> readProviderApiKeys{};
    std::optional<std::string> preferredReadProviderId{};
    std::optional<std::string> preferredWriteProviderId{};
    bool allowCompatibleReadFallback{false};
};

/**
 * @brief Resolve and validate Read/Write provider settings.
 *
 * @param settings Existing non-secret application preferences.
 * @param arguments Command-line overrides, which take highest precedence.
 * @param enableHypersnapDefault Whether build/deployment policy allows
 *        HyperSnap to be the initial Read choice if the user chose nothing.
 * @return Configuration, or std::nullopt when any profile/credential is unsafe.
 *
 * The default Read selection is independent from Write routing. Explicit
 * user choices and strict self-hosted policy are never overwritten.
 */
[[nodiscard]] std::optional<ProviderProfileConfiguration> loadProviderProfiles(
    QSettings& settings, const QStringList& arguments, bool enableHypersnapDefault);

} // namespace genycaster::ui::bootstrap
