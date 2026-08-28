export module farcaster.capability.catalog;

import farcaster.capability.model;

export namespace farcaster::capability::catalog {

/** @brief Username-to-FID identity resolution capability. */
[[nodiscard]] model::CapabilityId identityFidResolve();
/** @brief Cast read capability. */
[[nodiscard]] model::CapabilityId castRead();
/** @brief Profile read capability. */
[[nodiscard]] model::CapabilityId profileRead();
/** @brief Followers graph read capability. */
[[nodiscard]] model::CapabilityId graphFollowersRead();
/** @brief Following graph read capability. */
[[nodiscard]] model::CapabilityId graphFollowingRead();
/** @brief Following feed read capability. */
[[nodiscard]] model::CapabilityId feedFollowingRead();
/** @brief Trending feed read capability. */
[[nodiscard]] model::CapabilityId feedTrendingRead();
/** @brief Channel feed read capability. */
[[nodiscard]] model::CapabilityId feedChannelRead();
/** @brief Personalized feed read capability. */
[[nodiscard]] model::CapabilityId feedForYouRead();
/** @brief Mini App manifest resolution capability. */
[[nodiscard]] model::CapabilityId miniAppManifestResolve();
/** @brief Mini App discovery index capability. */
[[nodiscard]] model::CapabilityId miniAppDiscoveryRead();
/** @brief Legacy messaging compatibility read capability. */
[[nodiscard]] model::CapabilityId legacyMessagingRead();
/** @brief Open secure messaging extension capability. */
[[nodiscard]] model::CapabilityId secureMessaging();

}
