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

}
