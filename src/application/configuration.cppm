/*!
 * @file        src/application/configuration.cppm
 * @brief       Application composition and deployment configuration.
 *
 * @details
 * Defines feature availability, privacy profiles and endpoint identity as
 * application-level configuration independent of any provider adapter.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <string_view>

export module farcaster.application.configuration;

export namespace farcaster::application::configuration {

/**
 * @brief Deployment privacy profile selected by composition policy.
 */
enum class DeploymentProfile : std::uint8_t {
    Standard,
    Private,
    StrictSelfHosted,
};

/**
 * @brief Compile-time product feature availability independent of provider choice.
 */
struct FeatureSet final {
    bool hasSocial{false};
    bool hasFeed{false};
    bool hasChannels{false};
    bool hasMiniApps{false};
    bool hasChat{false};
    bool hasSearch{false};
    bool hasNotifications{false};
    bool hasWallet{false};
};

/**
 * @brief First-party shipping origins used by the executable composition root.
 *
 * Provider-specific upstream identities and credentials deliberately do not
 * belong here. The Snapchain origin is the stable Genyleap gateway.
 */
struct DeploymentEndpoints final {
    std::string_view siwfDomain{};
    std::string_view siwfUri{};
    std::string_view snapchainBaseUrl{};
    std::string_view mediaGatewayBaseUrl{};
};

/**
 * @brief Canonical public product identity used for client attribution.
 *
 * The FID is public metadata only. Farcaster resolves the externally displayed
 * client identity from this FID's Farcaster profile; appName is only a local
 * product label and cannot override protocol attribution. The custody secret
 * for this FID must never ship in the desktop client; production signer
 * attestation remains a backend responsibility.
 */
struct DeploymentIdentity final {
    std::uint64_t appFid{};
    std::string_view appName{};
};

/**
 * @brief Returns the feature set configured by the current CMake build profile.
 * @return Immutable feature-availability snapshot for composition code.
 */
[[nodiscard]] FeatureSet configuredFeatures() noexcept;

/**
 * @brief Returns the deployment profile configured by the current CMake build.
 * @return StrictSelfHosted when public/managed infrastructure is disabled by build policy.
 */
[[nodiscard]] DeploymentProfile configuredDeploymentProfile() noexcept;

/**
 * @brief Returns first-party shipping identity/protocol origins for composition.
 * @return Build-configured defaults; runtime overrides are applied by the executable.
 */
[[nodiscard]] DeploymentEndpoints configuredDeploymentEndpoints() noexcept;

/**
 * @brief Returns the canonical public Genycaster App identity.
 * @return Build-configured App FID and display name.
 */
[[nodiscard]] DeploymentIdentity configuredDeploymentIdentity() noexcept;

}
