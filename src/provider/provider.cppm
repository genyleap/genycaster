module;

#include <cstdint>
#include <string>

export module farcaster.provider.model;

export namespace farcaster::provider::model {

/** @brief Stable infrastructure identifier for a registered provider adapter. */
struct ProviderId final {
    std::string value{};
};

/** @brief Deployment topology of a provider instance. */
enum class ProviderDeployment : std::uint8_t {
    Local,
    SelfHosted,
    PublicManaged,
};

/** @brief Privacy classification used by routing eligibility policy. */
enum class ProviderPrivacy : std::uint8_t {
    Private,
    Standard,
    Managed,
};

/** @brief Provider metadata that contains no vendor SDK or response types. */
struct ProviderDescriptor final {
    ProviderId id{};
    std::string displayName{};
    ProviderDeployment deployment{ProviderDeployment::PublicManaged};
    ProviderPrivacy privacy{ProviderPrivacy::Standard};
    std::uint16_t priority{100};
};

}
