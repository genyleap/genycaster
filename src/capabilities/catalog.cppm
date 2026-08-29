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
/** @brief User-authored root-post feed read capability. */
[[nodiscard]] model::CapabilityId feedUserPostsRead();
/** @brief User-authored quote-post feed read capability. */
[[nodiscard]] model::CapabilityId feedUserQuotesRead();
/** @brief User replies feed read capability. */
[[nodiscard]] model::CapabilityId feedUserRepliesRead();
/** @brief Casts liked by a user feed read capability. */
[[nodiscard]] model::CapabilityId feedUserLikesRead();
/** @brief User popular-casts feed read capability. */
[[nodiscard]] model::CapabilityId feedUserPopularRead();
/** @brief Personalized feed read capability. */
[[nodiscard]] model::CapabilityId feedForYouRead();
/** @brief Timestamped asset-to-fiat valuation read capability. */
[[nodiscard]] model::CapabilityId valuationAssetPriceRead();
/** @brief Timestamped fiat FX valuation read capability. */
[[nodiscard]] model::CapabilityId valuationFxRead();
/** @brief Mini App manifest resolution capability. */
[[nodiscard]] model::CapabilityId miniAppManifestResolve();
/** @brief Mini App discovery index capability. */
[[nodiscard]] model::CapabilityId miniAppDiscoveryRead();
/** @brief Legacy messaging compatibility read capability. */
[[nodiscard]] model::CapabilityId legacyMessagingRead();
/** @brief Open secure messaging extension capability. */
[[nodiscard]] model::CapabilityId secureMessaging();

}
