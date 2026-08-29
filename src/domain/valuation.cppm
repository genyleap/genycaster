module;

#include <string>
#include <vector>

export module farcaster.domain.valuation;

import farcaster.foundation.time;

export namespace farcaster::domain::valuation {

/** @brief Canonical symbol for a priced asset such as GENY, ETH or USDC. */
struct AssetId final {
    std::string symbol{};
};

/** @brief Canonical ISO-4217 fiat currency identity used by quote providers. */
struct FiatCurrency final {
    std::string code{};
};

/**
 * @brief Provider-supplied decimal value preserved as canonical text.
 *
 * Price/FX quotes are not stored as binary floating point in the domain. Parsing and arithmetic
 * belong to a valuation implementation that can select an appropriate decimal arithmetic policy.
 */
struct DecimalValue final {
    std::string text{};
};

/** @brief Timestamped asset-to-fiat quote with explicit provenance. */
struct AssetPriceQuote final {
    AssetId asset{};
    FiatCurrency quoteCurrency{};
    DecimalValue pricePerAsset{};
    foundation::time::Timestamp observedAt{};
    std::string source{};
};

/** @brief Timestamped fiat-to-fiat quote with explicit provenance. */
struct FxQuote final {
    FiatCurrency baseCurrency{};
    FiatCurrency quoteCurrency{};
    DecimalValue quotePerBase{};
    foundation::time::Timestamp observedAt{};
    std::string source{};
};

/** @brief Batch returned for one asset and one or more requested fiat currencies. */
struct AssetPriceBatch final {
    AssetId asset{};
    std::vector<AssetPriceQuote> quotes{};
};

/** @brief Batch returned for one base fiat and one or more requested quote currencies. */
struct FxBatch final {
    FiatCurrency baseCurrency{};
    std::vector<FxQuote> quotes{};
};

}
