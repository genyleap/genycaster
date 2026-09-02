module;

#include <chrono>
#include <string>

export module farcaster.provider.public_data.config;

export namespace farcaster::provider::public_data::config {

/** @brief Adapter-local endpoints for optional public market/profile insight sources. */
struct Config final {
    std::string coinGeckoBaseUrl{"https://api.coingecko.com/api/v3"};
    std::string geckoTerminalBaseUrl{"https://api.geckoterminal.com/api/v2"};
    std::string baseRpcUrl{"https://mainnet.base.org"};
    std::string neynarScoreContract{"0xd3C43A38D1D3E47E9c420a733e439B03FAAdebA8"};
    std::chrono::milliseconds timeout{8'000};
};

}
