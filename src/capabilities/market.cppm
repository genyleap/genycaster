module;

#include <vector>

export module farcaster.capability.market;

import farcaster.domain.market;
import farcaster.foundation.result;

export namespace farcaster::capability::market {

/** @brief Blocking provider-neutral source for market and contract-token snapshots. */
class IMarketDataSource {
public:
    virtual ~IMarketDataSource() = default;

    /** @brief Resolves configured assets into live market snapshots. */
    [[nodiscard]] virtual foundation::result::Result<std::vector<domain::market::MarketSnapshot>>
    snapshots(const std::vector<domain::market::MarketAsset>& assets) = 0;

    /** @brief Resolves one contract token for a rich token-address embed. */
    [[nodiscard]] virtual foundation::result::Result<domain::market::MarketSnapshot>
    tokenSnapshot(const domain::market::TokenReference& token) = 0;
};

}
