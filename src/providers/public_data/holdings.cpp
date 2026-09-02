module;

#include <boost/json.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <expected>
#include <string>
#include <utility>

module farcaster.provider.public_data.holdings;

import farcaster.foundation.error;

namespace farcaster::provider::public_data::holdings {
namespace {

namespace json = boost::json;
using boost::multiprecision::cpp_int;
using foundation::error::Error;
using foundation::error::ErrorCode;

[[nodiscard]] bool isAddress(const std::string& value)
{
    return value.size() == 42U && value.starts_with("0x") &&
        std::ranges::all_of(value.substr(2), [](const unsigned char c) { return std::isxdigit(c) != 0; });
}

[[nodiscard]] std::string paddedAddress(const std::string& address)
{
    return std::string(24U, '0') + address.substr(2);
}

[[nodiscard]] foundation::result::Result<std::string> ethCall(
    transport::http::IHttpTransport& transport,
    const std::string& rpcUrl,
    const std::string& contract,
    const std::string& data,
    const std::chrono::milliseconds timeout)
{
    json::object call{};
    call["to"] = contract;
    call["data"] = data;
    json::array params{};
    params.emplace_back(std::move(call));
    params.emplace_back("latest");
    json::object body{};
    body["jsonrpc"] = "2.0";
    body["id"] = 1;
    body["method"] = "eth_call";
    body["params"] = std::move(params);

    auto response = transport.post(transport::http::HttpPostRequest{
        .url = rpcUrl,
        .headers = {"Content-Type: application/json", "Accept: application/json", "User-Agent: Genycaster/0.4"},
        .body = json::serialize(body),
        .timeout = timeout,
        .maxBodyBytes = 128U * 1024U,
    });
    if (!response) return std::unexpected(std::move(response.error()));
    if (response->statusCode < 200U || response->statusCode >= 300U) {
        return std::unexpected(Error::make(ErrorCode::Unavailable,
            "Base RPC returned HTTP " + std::to_string(response->statusCode)));
    }

    try {
        const json::value payload = json::parse(response->body);
        if (!payload.is_object()) {
            return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "Base RPC response is not an object"));
        }
        const auto* result = payload.as_object().if_contains("result");
        if (result == nullptr || !result->is_string()) {
            return std::unexpected(Error::make(ErrorCode::Unavailable, "Base RPC response has no result"));
        }
        return std::string{result->as_string().c_str()};
    } catch (const std::exception& error) {
        return std::unexpected(Error::make(ErrorCode::ProtocolFailure,
            std::string{"Invalid Base RPC holding JSON: "} + error.what()));
    }
}

[[nodiscard]] std::optional<cpp_int> parseHexInteger(const std::string& text)
{
    if (!text.starts_with("0x") || text.size() <= 2U) return std::nullopt;
    cpp_int value = 0;
    for (const char c : text.substr(2)) {
        unsigned int digit{};
        if (c >= '0' && c <= '9') digit = static_cast<unsigned int>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = 10U + static_cast<unsigned int>(c - 'a');
        else if (c >= 'A' && c <= 'F') digit = 10U + static_cast<unsigned int>(c - 'A');
        else return std::nullopt;
        value <<= 4;
        value += digit;
    }
    return value;
}

} // namespace

WalletAssetSource::WalletAssetSource(
    transport::http::IHttpTransport& transport,
    std::string baseRpcUrl,
    const std::chrono::milliseconds timeout)
    : m_transport(transport), m_baseRpcUrl(std::move(baseRpcUrl)), m_timeout(timeout)
{
}

foundation::result::Result<TokenHolding>
WalletAssetSource::erc20Balance(const std::string& wallet, const std::string& contract)
{
    if (!isAddress(wallet) || !isAddress(contract)) {
        return std::unexpected(Error::make(ErrorCode::InvalidArgument, "Wallet and token contract must be EVM addresses"));
    }

    auto decimalsResult = ethCall(m_transport, m_baseRpcUrl, contract, "0x313ce567", m_timeout);
    if (!decimalsResult) return std::unexpected(std::move(decimalsResult.error()));
    const auto decimalsRaw = parseHexInteger(*decimalsResult);
    if (!decimalsRaw) return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "Invalid ERC-20 decimals result"));
    const unsigned int decimals = decimalsRaw->convert_to<unsigned int>();
    if (decimals > 36U) return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "ERC-20 decimals are out of range"));

    auto balanceResult = ethCall(m_transport, m_baseRpcUrl, contract, "0x70a08231" + paddedAddress(wallet), m_timeout);
    if (!balanceResult) return std::unexpected(std::move(balanceResult.error()));
    const auto balanceRaw = parseHexInteger(*balanceResult);
    if (!balanceRaw) return std::unexpected(Error::make(ErrorCode::ProtocolFailure, "Invalid ERC-20 balance result"));

    long double scale = 1.0L;
    for (unsigned int index = 0; index < decimals; ++index) scale *= 10.0L;
    const long double raw = balanceRaw->convert_to<long double>();
    return TokenHolding{
        .wallet = wallet,
        .contract = contract,
        .balance = static_cast<double>(raw / scale),
        .decimals = decimals,
    };
}

} // namespace farcaster::provider::public_data::holdings
