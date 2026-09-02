module;

#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <exception>
#include <optional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

module farcaster.provider.public_data.score;

import farcaster.foundation.error;

namespace farcaster::provider::public_data::score {
namespace {

namespace json = boost::json;
using foundation::error::Error;
using foundation::error::ErrorCode;

[[nodiscard]] std::string scoreCallData(const std::uint64_t fid)
{
    // Selector for getScoreWithEvent(uint256). eth_call executes this read path
    // without persisting the event while avoiding an API key dependency.
    std::ostringstream stream{};
    stream << "0xc35070e5" << std::hex << std::nouppercase << std::setw(64) << std::setfill('0') << fid;
    return stream.str();
}

[[nodiscard]] std::optional<std::uint64_t> parseHexResult(const std::string& text)
{
    if (!text.starts_with("0x") || text.size() <= 2U) return std::nullopt;
    try {
        return std::stoull(text.substr(text.size() > 18U ? text.size() - 16U : 2U), nullptr, 16);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

ProfileQualitySource::ProfileQualitySource(transport::http::IHttpTransport& transport, config::Config config)
    : m_transport(transport), m_config(std::move(config))
{
}

foundation::result::Result<capability::profile_quality::ProfileQualityScore>
ProfileQualitySource::get(const domain::identity::Fid fid)
{
    if (fid.value == 0U) {
        return std::unexpected(Error::make(ErrorCode::InvalidArgument, "FID must be non-zero"));
    }

    json::object call{};
    call["to"] = m_config.neynarScoreContract;
    call["data"] = scoreCallData(fid.value);
    json::array parameters{};
    parameters.emplace_back(std::move(call));
    parameters.emplace_back("latest");
    json::object body{};
    body["jsonrpc"] = "2.0";
    body["id"] = 1;
    body["method"] = "eth_call";
    body["params"] = std::move(parameters);

    auto response = m_transport.post(transport::http::HttpPostRequest{
        .url = m_config.baseRpcUrl,
        .headers = {"Content-Type: application/json", "Accept: application/json", "User-Agent: Genycaster/0.4"},
        .body = json::serialize(body),
        .timeout = m_config.timeout,
        .maxBodyBytes = 256U * 1024U,
    });
    if (!response) return std::unexpected(std::move(response.error()));
    if (response->statusCode < 200U || response->statusCode >= 300U) {
        return std::unexpected(Error::make(ErrorCode::Unavailable,
            "Base RPC returned HTTP " + std::to_string(response->statusCode)));
    }

    try {
        const json::value payload = json::parse(response->body);
        if (!payload.is_object()) {
            return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "Base RPC score response is not an object"));
        }
        const auto result = payload.as_object().if_contains("result");
        if (result == nullptr || !result->is_string()) {
            return std::unexpected(Error::make(ErrorCode::Unavailable, "Base RPC score response has no result"));
        }
        const auto raw = parseHexResult(std::string{result->as_string().c_str()});
        if (!raw || *raw == 0U) {
            return std::unexpected(Error::make(ErrorCode::NotFound, "Neynar publishes no onchain score for this FID"));
        }
        constexpr double scale = 1'000'000.0;
        const double normalized = static_cast<double>(*raw) / scale;
        if (normalized < 0.0 || normalized > 1.0) {
            return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "Neynar onchain score is outside the documented range"));
        }
        return capability::profile_quality::ProfileQualityScore{
            .value = normalized,
            .source = "Neynar onchain · Base",
            .observedAt = std::chrono::system_clock::now(),
        };
    } catch (const std::exception& error) {
        return std::unexpected(Error::make(ErrorCode::ProtocolFailure,
            std::string{"Invalid Base RPC score JSON: "} + error.what()));
    }
}

} // namespace farcaster::provider::public_data::score
