/*!
 * @file        ui/app/bootstrap/auth.cppm
 * @brief       Authentication and signer attestation startup configuration.
 *
 * @details
 * Validates the relying-party domain/URI pair, optional Farcaster relay
 * and RPC endpoints, the application FID, and explicit signer-attestation
 * settings. This is configuration only: no signer is approved, no wallet
 * secret is accessed, and an HTTP Read node never becomes a trusted attestor.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "qt_fwd.hpp"
#include <cstdint>
#include <optional>
#include <string>

export module genycaster.ui.bootstrap.auth;

export namespace genycaster::ui::bootstrap {

/**
 * @brief Validated non-persistent identity and authentication preferences.
 *
 * The attestation service must be configured explicitly; this value is never
 * deduced from a Snapchain/HyperSnap Read or signed-message Write origin.
 */
struct AuthenticationOptions final {
    std::optional<std::uint64_t> appFid{};
    std::optional<std::string> signerAttestationUrl{};
    std::string walletLaunchTemplate{};
    std::string siwfDomain{};
    std::string siwfUri{};
    std::optional<std::string> authRelayUrl{};
    std::optional<std::string> optimismRpcUrl{};
    bool authenticationAvailable{false};
};

/**
 * @brief Resolve CLI/environment identity overrides against deployment data.
 *
 * @param arguments Startup command-line arguments.
 * @param defaultAppFid Compiled client attribution FID; zero means absent.
 * @param defaultSiwfDomain Compiled relying-party domain.
 * @param defaultSiwfUri Compiled relying-party login URI.
 * @param openProofAvailable Whether OpenProof login is compiled for this target.
 * @param compatibilityAuthAvailable Whether compatibility SIWF is compiled.
 * @return Validated configuration, or std::nullopt for invalid required fields.
 *
 * Invalid optional explicit SIWF/relay/RPC overrides are discarded without
 * changing the compiled deployment identity. An app FID must be nonzero.
 */
[[nodiscard]] std::optional<AuthenticationOptions> loadAuthenticationOptions(
    const QStringList& arguments,
    std::uint64_t defaultAppFid,
    const QString& defaultSiwfDomain,
    const QString& defaultSiwfUri,
    bool openProofAvailable,
    bool compatibilityAuthAvailable);

} // namespace genycaster::ui::bootstrap
