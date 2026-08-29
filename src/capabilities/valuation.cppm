module;

#include <vector>

export module farcaster.capability.valuation;

import farcaster.domain.valuation;
import farcaster.foundation.result;

export namespace farcaster::capability::valuation {

/**
 * @brief Blocking provider-neutral market valuation read port.
 *
 * Implementations may use one or more market/FX sources, but must return timestamped canonical
 * quotes with provenance. Callers execute this port on an I/O executor; QML never fetches rates.
 */
class IValuationSource {
public:
    /** @brief Destroys the valuation source. */
    virtual ~IValuationSource() = default;

    /** @brief Reads one asset in the requested canonical ISO fiat currencies. */
    [[nodiscard]] virtual foundation::result::Result<domain::valuation::AssetPriceBatch>
    assetPrices(
        const domain::valuation::AssetId& asset,
        const std::vector<domain::valuation::FiatCurrency>& quoteCurrencies) = 0;

    /** @brief Reads FX quotes for one base fiat into requested canonical ISO fiat currencies. */
    [[nodiscard]] virtual foundation::result::Result<domain::valuation::FxBatch>
    fxRates(
        const domain::valuation::FiatCurrency& baseCurrency,
        const std::vector<domain::valuation::FiatCurrency>& quoteCurrencies) = 0;
};

}
