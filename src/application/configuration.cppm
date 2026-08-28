module;

#include <cstdint>

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
 * @brief Returns the feature set configured by the current CMake build profile.
 * @return Immutable feature-availability snapshot for composition code.
 */
[[nodiscard]] FeatureSet configuredFeatures() noexcept;

/**
 * @brief Returns the deployment profile configured by the current CMake build.
 * @return StrictSelfHosted when public/managed infrastructure is disabled by build policy.
 */
[[nodiscard]] DeploymentProfile configuredDeploymentProfile() noexcept;

}
