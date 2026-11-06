/*!
 * @file        src/provider/provider.cppm
 * @brief       Provider instance identity and metadata.
 *
 * @details
 * Represents configured adapter identity, deployment topology and privacy
 * characteristics independently from capability semantics.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
