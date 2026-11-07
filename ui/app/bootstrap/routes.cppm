/*!
 * @file        ui/app/bootstrap/routes.cppm
 * @brief       Protocol-specific Read and Write endpoint resolution.
 *
 * @details
 * Resolves HyperSnap node candidates, independent Snapchain Read and Write
 * origins, and the optional media gateway. The primary signed-message writer
 * follows an explicitly selected compatible protocol node; unknown adapter
 * contracts are rejected rather than silently forwarded to another network.
 * Advanced authorization values remain ephemeral process configuration.
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
#include <vector>

export module genycaster.ui.bootstrap.routes;

import genycaster.ui.bootstrap.profiles;

export namespace genycaster::ui::bootstrap {

/**
 * @brief Fully validated routing policy for runtime adapter registration.
 *
 * The listed origins contain no URL credentials. Read and Write authorization
 * headers and values are independent optional pairs, never stored in QSettings.
 */
struct ProviderRoutes final {
    std::vector<std::string> nodeCandidateEndpoints{};
    std::string appliedNodeEndpoint{};
    std::string snapchainReadEndpoint{};
    std::string snapchainWriteEndpoint{};
    std::string mediaGatewayEndpoint{};
    std::optional<std::string> readAuthorizationHeader{};
    std::optional<std::string> readAuthorizationValue{};
    std::optional<std::string> writeAuthorizationHeader{};
    std::optional<std::string> writeAuthorizationValue{};
};

/**
 * @brief Resolve the selected protocol's canonical read/write endpoints.
 *
 * @param settings Persisted non-secret network settings.
 * @param arguments Command-line overrides, which take precedence over settings.
 * @param hypersnapOverride Explicit custom HyperSnap origin or an empty string.
 * @param hypersnapDefault Origin declared by the compiled provider adapter.
 * @param snapchainDefault Default read origin from deployment configuration.
 * @param mediaGatewayDefault Default media gateway from deployment configuration.
 * @param profiles Validated independent Read and Write provider preferences.
 * @return Routing policy, or std::nullopt for unsafe or incompatible settings.
 *
 * The primary writer uses the selected HyperSnap/compatible Read node when
 * appropriate. An explicit separate Write provider is never overridden here.
 * Signer attestation is deliberately not inferred from Read/Write endpoints.
 */
[[nodiscard]] std::optional<ProviderRoutes> loadProviderRoutes(
    QSettings& settings,
    const QStringList& arguments,
    const QString& hypersnapOverride,
    const QString& hypersnapDefault,
    const QString& snapchainDefault,
    const QString& mediaGatewayDefault,
    const ProviderProfileConfiguration& profiles);

} // namespace genycaster::ui::bootstrap
