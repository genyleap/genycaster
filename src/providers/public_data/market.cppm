/*!
 * @file        src/providers/public_data/market.cppm
 * @brief       Public market data source adapter.
 *
 * @details
 * Normalizes market pricing and token snapshots for optional product
 * widgets.
 *
 * @author      Kambiz Asadzadeh
 * @since       02 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <vector>

export module farcaster.provider.public_data.market;

import farcaster.capability.market;
import farcaster.domain.market;
import farcaster.foundation.result;
import farcaster.provider.public_data.config;
import farcaster.transport.http;

export namespace farcaster::provider::public_data::market {

/** @brief CoinGecko/GeckoTerminal adapter used only by graphical composition. */
class MarketDataSource final : public capability::market::IMarketDataSource {
public:
    MarketDataSource(transport::http::IHttpTransport& transport, config::Config config);

    [[nodiscard]] foundation::result::Result<std::vector<domain::market::MarketSnapshot>>
    snapshots(const std::vector<domain::market::MarketAsset>& assets) override;

    [[nodiscard]] foundation::result::Result<domain::market::MarketSnapshot>
    tokenSnapshot(const domain::market::TokenReference& token) override;

private:
    transport::http::IHttpTransport& m_transport;
    config::Config m_config{};
};

}
