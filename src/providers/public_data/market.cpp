module;

#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <expected>
#include <exception>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.public_data.market;

import farcaster.foundation.error;

namespace farcaster::provider::public_data::market {
namespace {

namespace json = boost::json;
using foundation::error::Error;
using foundation::error::ErrorCode;
using domain::market::MarketAsset;
using domain::market::MarketSnapshot;
using domain::market::TokenReference;

[[nodiscard]] foundation::result::Result<json::value> getJson(
    transport::http::IHttpTransport& transport,
    const std::string& url,
    const std::chrono::milliseconds timeout)
{
    auto response = transport.get(transport::http::HttpRequest{
        .url = url,
        .headers = {"Accept: application/json", "User-Agent: Genycaster/0.4"},
        .timeout = timeout,
        .maxBodyBytes = 2U * 1024U * 1024U,
    });
    if (!response) {
        return std::unexpected(std::move(response.error()));
    }
    if (response->statusCode == 429U) {
        return std::unexpected(Error::make(ErrorCode::RateLimited, "Public market source rate limited the request"));
    }
    if (response->statusCode < 200U || response->statusCode >= 300U) {
        return std::unexpected(Error::make(ErrorCode::Unavailable,
            "Public market source returned HTTP " + std::to_string(response->statusCode)));
    }
    try {
        return json::parse(response->body);
    } catch (const std::exception& error) {
        return std::unexpected(Error::make(ErrorCode::ProtocolFailure,
            std::string{"Invalid public market JSON: "} + error.what()));
    }
}

[[nodiscard]] const json::value* find(const json::object& object, const std::string_view key)
{
    const auto iterator = object.find(key);
    return iterator == object.end() ? nullptr : &iterator->value();
}

[[nodiscard]] std::optional<std::string> stringValue(const json::object& object, const std::string_view key)
{
    const json::value* value = find(object, key);
    if (value == nullptr || value->is_null()) {
        return std::nullopt;
    }
    if (value->is_string()) {
        return std::string{value->as_string().c_str()};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<double> numberValue(const json::value& value)
{
    if (value.is_double()) return value.as_double();
    if (value.is_int64()) return static_cast<double>(value.as_int64());
    if (value.is_uint64()) return static_cast<double>(value.as_uint64());
    if (value.is_string()) {
        try {
            const double parsed = std::stod(std::string{value.as_string().c_str()});
            return std::isfinite(parsed) ? std::optional<double>{parsed} : std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<double> numberValue(const json::object& object, const std::string_view key)
{
    const json::value* value = find(object, key);
    return value == nullptr ? std::nullopt : numberValue(*value);
}

[[nodiscard]] std::optional<std::uint64_t> unsignedValue(const json::object& object, const std::string_view key)
{
    if (const auto value = numberValue(object, key); value && *value >= 0.0) {
        return static_cast<std::uint64_t>(*value);
    }
    return std::nullopt;
}

[[nodiscard]] std::string lower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

[[nodiscard]] bool validTokenReference(const TokenReference& token)
{
    if (token.network.empty() || token.address.size() != 42U || !token.address.starts_with("0x")) {
        return false;
    }
    return std::ranges::all_of(token.address.substr(2), [](const unsigned char character) {
        return std::isxdigit(character) != 0;
    });
}

[[nodiscard]] std::optional<std::chrono::system_clock::time_point> parseIso8601(std::string value)
{
    // GeckoTerminal timestamps are ISO-8601. The age is presentation-only; use a
    // conservative parser for the common UTC form and leave it unavailable on variants.
    if (value.size() < 20U || value.back() != 'Z') {
        return std::nullopt;
    }
    std::tm tm{};
    if (std::sscanf(value.c_str(), "%4d-%2d-%2dT%2d:%2d:%2dZ",
            &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return std::nullopt;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#if defined(_WIN32)
    const std::time_t instant = _mkgmtime(&tm);
#else
    const std::time_t instant = timegm(&tm);
#endif
    if (instant < 0) return std::nullopt;
    return std::chrono::system_clock::from_time_t(instant);
}

[[nodiscard]] foundation::result::Result<MarketSnapshot> nativeSnapshot(
    transport::http::IHttpTransport& transport,
    const config::Config& config,
    const MarketAsset& asset)
{
    std::string coinId{};
    if (asset.symbol == "BTC") coinId = "bitcoin";
    else if (asset.symbol == "ETH") coinId = "ethereum";
    else {
        return std::unexpected(Error::make(ErrorCode::Unsupported,
            "No native public-market mapping exists for " + asset.symbol));
    }

    const std::string url = config.coinGeckoBaseUrl +
        "/coins/markets?vs_currency=usd&ids=" + coinId +
        "&sparkline=true&price_change_percentage=24h";
    auto payload = getJson(transport, url, config.timeout);
    if (!payload) return std::unexpected(std::move(payload.error()));
    if (!payload->is_array() || payload->as_array().empty() || !payload->as_array().front().is_object()) {
        return std::unexpected(Error::make(ErrorCode::NotFound, "CoinGecko returned no asset row"));
    }

    const json::object& row = payload->as_array().front().as_object();
    const auto price = numberValue(row, "current_price");
    if (!price || *price <= 0.0) {
        return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "CoinGecko asset row has no usable price"));
    }

    MarketSnapshot result{};
    result.asset = asset;
    result.priceUsd = *price;
    result.change24hPercent = numberValue(row, "price_change_percentage_24h").value_or(0.0);
    result.marketCapUsd = numberValue(row, "market_cap");
    result.fullyDilutedValuationUsd = numberValue(row, "fully_diluted_valuation");
    result.volume24hUsd = numberValue(row, "total_volume");
    result.imageUrl = stringValue(row, "image").value_or(std::string{});
    result.observedAt = std::chrono::system_clock::now();
    result.source = "CoinGecko";

    if (const json::value* sparkline = find(row, "sparkline_in_7d"); sparkline && sparkline->is_object()) {
        if (const json::value* prices = find(sparkline->as_object(), "price"); prices && prices->is_array()) {
            const auto& values = prices->as_array();
            const std::size_t start = values.size() > 24U ? values.size() - 24U : 0U;
            for (std::size_t index = start; index < values.size(); ++index) {
                if (const auto point = numberValue(values[index])) result.sparkline.push_back(*point);
            }
        }
    }
    return result;
}

[[nodiscard]] foundation::result::Result<MarketSnapshot> contractSnapshot(
    transport::http::IHttpTransport& transport,
    const config::Config& config,
    const TokenReference& token,
    const MarketAsset* configuredAsset)
{
    if (!validTokenReference(token)) {
        return std::unexpected(Error::make(ErrorCode::InvalidArgument, "Invalid EVM token reference"));
    }

    const std::string tokenUrl = config.geckoTerminalBaseUrl + "/networks/" + token.network +
        "/tokens/" + token.address;
    auto tokenPayload = getJson(transport, tokenUrl, config.timeout);
    if (!tokenPayload) return std::unexpected(std::move(tokenPayload.error()));
    if (!tokenPayload->is_object()) {
        return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "GeckoTerminal token payload is not an object"));
    }
    const json::value* tokenData = find(tokenPayload->as_object(), "data");
    if (tokenData == nullptr || !tokenData->is_object()) {
        return std::unexpected(Error::make(ErrorCode::NotFound, "GeckoTerminal returned no token"));
    }
    const json::value* attributesValue = find(tokenData->as_object(), "attributes");
    if (attributesValue == nullptr || !attributesValue->is_object()) {
        return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "GeckoTerminal token has no attributes"));
    }
    const json::object& attributes = attributesValue->as_object();

    MarketSnapshot result{};
    result.asset = configuredAsset ? *configuredAsset : MarketAsset{};
    result.asset.token = token;
    if (result.asset.symbol.empty()) result.asset.symbol = stringValue(attributes, "symbol").value_or("TOKEN");
    if (result.asset.displayName.empty()) result.asset.displayName = stringValue(attributes, "name").value_or(result.asset.symbol);
    if (result.asset.key.empty()) result.asset.key = token.network + ":" + lower(token.address);
    result.priceUsd = numberValue(attributes, "price_usd").value_or(0.0);
    result.marketCapUsd = numberValue(attributes, "market_cap_usd");
    result.fullyDilutedValuationUsd = numberValue(attributes, "fdv_usd");
    result.imageUrl = stringValue(attributes, "image_url").value_or(std::string{});
    result.observedAt = std::chrono::system_clock::now();
    result.source = "GeckoTerminal";

    const std::string poolsUrl = config.geckoTerminalBaseUrl + "/networks/" + token.network +
        "/tokens/" + token.address + "/pools?page=1";
    auto poolsPayload = getJson(transport, poolsUrl, config.timeout);
    if (poolsPayload && poolsPayload->is_object()) {
        if (const json::value* data = find(poolsPayload->as_object(), "data");
            data && data->is_array() && !data->as_array().empty() && data->as_array().front().is_object()) {
            const json::object& pool = data->as_array().front().as_object();
            const json::value* poolAttributesValue = find(pool, "attributes");
            if (poolAttributesValue && poolAttributesValue->is_object()) {
                const json::object& poolAttributes = poolAttributesValue->as_object();
                result.liquidityUsd = numberValue(poolAttributes, "reserve_in_usd");
                if (!result.marketCapUsd) result.marketCapUsd = numberValue(poolAttributes, "market_cap_usd");
                if (!result.fullyDilutedValuationUsd) result.fullyDilutedValuationUsd = numberValue(poolAttributes, "fdv_usd");
                if (const json::value* changes = find(poolAttributes, "price_change_percentage"); changes && changes->is_object()) {
                    result.change24hPercent = numberValue(changes->as_object(), "h24").value_or(0.0);
                }
                if (const json::value* volumes = find(poolAttributes, "volume_usd"); volumes && volumes->is_object()) {
                    result.volume24hUsd = numberValue(volumes->as_object(), "h24");
                }
                if (const json::value* transactions = find(poolAttributes, "transactions"); transactions && transactions->is_object()) {
                    if (const json::value* h24 = find(transactions->as_object(), "h24"); h24 && h24->is_object()) {
                        result.buyers24h = unsignedValue(h24->as_object(), "buyers");
                        result.sellers24h = unsignedValue(h24->as_object(), "sellers");
                    }
                }
                if (auto created = stringValue(poolAttributes, "pool_created_at")) {
                    result.poolCreatedAt = parseIso8601(*created);
                }

                const auto poolAddress = stringValue(poolAttributes, "address");
                if (poolAddress && !poolAddress->empty()) {
                    const std::string ohlcvUrl = config.geckoTerminalBaseUrl + "/networks/" + token.network +
                        "/pools/" + *poolAddress + "/ohlcv/hour?aggregate=1&limit=24&currency=usd&token=" + token.address;
                    auto ohlcvPayload = getJson(transport, ohlcvUrl, config.timeout);
                    if (ohlcvPayload && ohlcvPayload->is_object()) {
                        if (const json::value* ohlcvData = find(ohlcvPayload->as_object(), "data"); ohlcvData && ohlcvData->is_object()) {
                            if (const json::value* ohlcvAttributes = find(ohlcvData->as_object(), "attributes"); ohlcvAttributes && ohlcvAttributes->is_object()) {
                                if (const json::value* rows = find(ohlcvAttributes->as_object(), "ohlcv_list"); rows && rows->is_array()) {
                                    for (const json::value& row : rows->as_array()) {
                                        if (!row.is_array() || row.as_array().size() < 5U) continue;
                                        if (const auto close = numberValue(row.as_array()[4])) result.sparkline.push_back(*close);
                                    }
                                    std::ranges::reverse(result.sparkline);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (result.priceUsd <= 0.0) {
        return std::unexpected(Error::make(ErrorCode::NotFound, "GeckoTerminal returned no usable token price"));
    }
    return result;
}

} // namespace

MarketDataSource::MarketDataSource(transport::http::IHttpTransport& transport, config::Config config)
    : m_transport(transport), m_config(std::move(config))
{
}

foundation::result::Result<std::vector<MarketSnapshot>>
MarketDataSource::snapshots(const std::vector<MarketAsset>& assets)
{
    std::vector<MarketSnapshot> output{};
    output.reserve(assets.size());
    for (const MarketAsset& asset : assets) {
        auto snapshot = asset.token
            ? contractSnapshot(m_transport, m_config, *asset.token, &asset)
            : nativeSnapshot(m_transport, m_config, asset);
        if (!snapshot) {
            // A single optional asset must not blank the entire pulse. Missing rows
            // are represented by the caller's stale/loading state instead.
            continue;
        }
        output.push_back(std::move(*snapshot));
    }
    if (output.empty() && !assets.empty()) {
        return std::unexpected(Error::make(ErrorCode::Unavailable, "No configured market asset could be refreshed"));
    }
    return output;
}

foundation::result::Result<MarketSnapshot>
MarketDataSource::tokenSnapshot(const TokenReference& token)
{
    return contractSnapshot(m_transport, m_config, token, nullptr);
}

} // namespace farcaster::provider::public_data::market
