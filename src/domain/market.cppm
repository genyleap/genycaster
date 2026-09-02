module;

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.domain.market;

export namespace farcaster::domain::market {

/** @brief Provider-neutral EVM token reference qualified by a canonical network id. */
struct TokenReference final {
    std::string network{};
    std::string address{};
};

/** @brief Asset configured for the live market surface. */
struct MarketAsset final {
    std::string key{};
    std::string symbol{};
    std::string displayName{};
    std::optional<TokenReference> token{};
};

/** @brief Live market snapshot with provenance and optional token-liquidity metadata. */
struct MarketSnapshot final {
    MarketAsset asset{};
    double priceUsd{};
    double change24hPercent{};
    std::optional<double> marketCapUsd{};
    std::optional<double> fullyDilutedValuationUsd{};
    std::optional<double> liquidityUsd{};
    std::optional<double> volume24hUsd{};
    std::optional<std::uint64_t> buyers24h{};
    std::optional<std::uint64_t> sellers24h{};
    std::optional<std::chrono::system_clock::time_point> poolCreatedAt{};
    std::string imageUrl{};
    std::vector<double> sparkline{};
    std::chrono::system_clock::time_point observedAt{};
    std::string source{};
};

}
