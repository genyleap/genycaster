/*!
 * @file        src/providers/public_data/config.cppm
 * @brief       Optional public data provider configuration.
 *
 * @details
 * Defines independent market and profile-insight endpoints for derived
 * product data.
 *
 * @author      Kambiz Asadzadeh
 * @since       02 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
