module;

#include <boost/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

module farcaster.provider.snapchain.write;

import farcaster.capability.write_submit;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.http;

namespace farcaster::provider::snapchain::write {
namespace {

using JsonObject = boost::json::object;
using JsonValue = boost::json::value;

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] foundation::error::Error protocolError(std::string message)
{
    return makeError(foundation::error::ErrorCode::ProtocolFailure, std::move(message));
}

[[nodiscard]] std::string trimTrailingSlashes(std::string value)
{
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

[[nodiscard]] foundation::result::Status validateConfig(const Config& config)
{
    if (config.baseUrl.empty() ||
        (!config.baseUrl.starts_with("https://") && !config.baseUrl.starts_with("http://"))) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain write base URL must use http or https."));
    }
    if (config.baseUrl.find_first_of("?#\r\n") != std::string::npos) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain write base URL must be an origin/root URL without query or fragment."));
    }
    if (config.timeout <= std::chrono::milliseconds::zero() || config.maxBodyBytes == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain write timeout and response-size limit must be positive."));
    }
    return {};
}

[[nodiscard]] bool isHexDigit(const char character)
{
    return (character >= '0' && character <= '9') ||
        (character >= 'a' && character <= 'f') ||
        (character >= 'A' && character <= 'F');
}

[[nodiscard]] foundation::result::Result<std::string> normalizeHash(
    const std::string_view value,
    const std::string_view context)
{
    std::string_view hex = value;
    if (hex.starts_with("0x") || hex.starts_with("0X")) hex.remove_prefix(2U);
    if (hex.size() != 40U || !std::all_of(hex.begin(), hex.end(), isHexDigit)) {
        return std::unexpected(protocolError(
            std::string{context} + " must be a 20-byte hexadecimal cast hash."));
    }
    std::string normalized{"0x"};
    normalized.append(hex);
    std::ranges::transform(normalized, normalized.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return normalized;
}

[[nodiscard]] foundation::result::Result<JsonValue> parseJson(const std::string_view body)
{
    boost::system::error_code error{};
    auto value = boost::json::parse(body, error);
    if (error) {
        return std::unexpected(protocolError("Snapchain write response is not valid JSON."));
    }
    return value;
}

[[nodiscard]] const JsonValue* findValue(const JsonObject& object, const std::string_view key)
{
    const auto iterator = object.find(key);
    return iterator == object.end() ? nullptr : &iterator->value();
}

[[nodiscard]] foundation::result::Result<std::string> requireString(
    const JsonObject& object,
    const std::string_view key,
    const std::string_view context)
{
    const auto* value = findValue(object, key);
    if (value == nullptr || !value->is_string()) {
        return std::unexpected(protocolError(
            std::string{context} + "." + std::string{key} + " must be a string."));
    }
    return std::string{value->as_string()};
}

[[nodiscard]] foundation::result::Result<std::uint64_t> requireUnsigned(
    const JsonObject& object,
    const std::string_view key,
    const std::string_view context)
{
    const auto* value = findValue(object, key);
    if (value == nullptr) {
        return std::unexpected(protocolError(
            std::string{context} + "." + std::string{key} + " is required."));
    }
    if (value->is_uint64()) return value->as_uint64();
    if (value->is_int64() && value->as_int64() >= 0) {
        return static_cast<std::uint64_t>(value->as_int64());
    }
    return std::unexpected(protocolError(
        std::string{context} + "." + std::string{key} + " must be a non-negative integer."));
}

[[nodiscard]] foundation::result::Status verifyAuthoritativeCast(
    const std::string_view body,
    const domain::write::SignedOperation& operation)
{
    auto document = parseJson(body);
    if (!document) return std::unexpected(std::move(document.error()));
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain cast response must be a JSON object."));
    }
    const auto& object = document->as_object();
    auto responseHash = requireString(object, "hash", "message");
    if (!responseHash) return std::unexpected(std::move(responseHash.error()));
    auto normalizedResponse = normalizeHash(*responseHash, "message.hash");
    auto normalizedExpected = normalizeHash(operation.optimisticCast.id.value, "operation.castId");
    if (!normalizedResponse) return std::unexpected(std::move(normalizedResponse.error()));
    if (!normalizedExpected) return std::unexpected(std::move(normalizedExpected.error()));
    if (*normalizedResponse != *normalizedExpected || operation.idempotencyKey.value != *normalizedExpected) {
        return std::unexpected(protocolError(
            "Snapchain returned a different cast hash than the submitted canonical operation."));
    }

    const auto* dataValue = findValue(object, "data");
    if (dataValue == nullptr || !dataValue->is_object()) {
        return std::unexpected(protocolError("Snapchain cast response.data must be an object."));
    }
    const auto& data = dataValue->as_object();
    auto type = requireString(data, "type", "message.data");
    auto fid = requireUnsigned(data, "fid", "message.data");
    if (!type) return std::unexpected(std::move(type.error()));
    if (!fid) return std::unexpected(std::move(fid.error()));
    if (*type != "MESSAGE_TYPE_CAST_ADD") {
        return std::unexpected(protocolError("Snapchain authoritative message is not CAST_ADD."));
    }
    if (*fid != operation.optimisticCast.author.value) {
        return std::unexpected(protocolError("Snapchain authoritative CAST_ADD contains a mismatched author FID."));
    }
    return {};
}

[[nodiscard]] std::string responseExcerpt(const std::string_view body)
{
    constexpr std::size_t maxLength = 320U;
    std::string excerpt{};
    excerpt.reserve(std::min(body.size(), maxLength));
    bool previousWhitespace{false};
    for (const char raw : body) {
        if (excerpt.size() >= maxLength) break;
        const auto character = static_cast<unsigned char>(raw);
        if (std::isspace(character) != 0) {
            if (!excerpt.empty() && !previousWhitespace) excerpt.push_back(' ');
            previousWhitespace = true;
            continue;
        }
        if (std::iscntrl(character) != 0) continue;
        excerpt.push_back(raw);
        previousWhitespace = false;
    }
    while (!excerpt.empty() && excerpt.back() == ' ') excerpt.pop_back();
    if (body.size() > maxLength) excerpt.append("...");
    return excerpt;
}

[[nodiscard]] foundation::error::Error httpStatusError(
    const std::uint16_t statusCode,
    const std::string_view body)
{
    foundation::error::ErrorCode code = foundation::error::ErrorCode::ProtocolFailure;
    switch (statusCode) {
    case 400: code = foundation::error::ErrorCode::ProtocolFailure; break;
    case 401:
    case 403: code = foundation::error::ErrorCode::Unauthorized; break;
    case 404: code = foundation::error::ErrorCode::NotFound; break;
    case 408: code = foundation::error::ErrorCode::Timeout; break;
    case 409: code = foundation::error::ErrorCode::Conflict; break;
    case 429: code = foundation::error::ErrorCode::RateLimited; break;
    default:
        if (statusCode >= 500U) code = foundation::error::ErrorCode::Unavailable;
        break;
    }
    std::string message = "Snapchain write request failed with HTTP status " +
        std::to_string(statusCode) + ".";
    const auto excerpt = responseExcerpt(body);
    if (!excerpt.empty()) {
        message.append(" Node response: ");
        message.append(excerpt);
    }
    return makeError(code, std::move(message));
}

class Submitter final : public capability::write::IWriteSubmitter {
public:
    Submitter(transport::http::IHttpTransport& transport, Config config)
        : m_transport(transport), m_config(std::move(config))
    {
    }

    [[nodiscard]] foundation::result::Status submit(
        const domain::write::SignedOperation& operation) override
    {
        if (auto status = validateConfig(m_config); !status) return status;
        if (operation.kind != domain::write::OperationKind::PublishCast ||
            operation.messageBytes.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Snapchain submitter requires a complete signed PublishCast operation."));
        }
        transport::http::HttpPostRequest request{
            .url = trimTrailingSlashes(m_config.baseUrl) + "/v1/submitMessage",
            .headers = {"Accept: application/json", "Content-Type: application/octet-stream"},
            .body = operation.messageBytes,
            .timeout = m_config.timeout,
            .maxBodyBytes = m_config.maxBodyBytes,
        };
        auto response = m_transport.post(request);
        if (!response) return std::unexpected(std::move(response.error()));
        if (response->statusCode < 200U || response->statusCode >= 300U) {
            return std::unexpected(httpStatusError(response->statusCode, response->body));
        }
        return verifyAuthoritativeCast(response->body, operation);
    }

    [[nodiscard]] foundation::result::Result<capability::write::ConfirmationState> reconcile(
        const domain::write::SignedOperation& operation) override
    {
        if (auto status = validateConfig(m_config); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (operation.kind != domain::write::OperationKind::PublishCast ||
            operation.optimisticCast.author.value == 0U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Snapchain reconciliation requires a complete PublishCast identity."));
        }
        auto normalizedHash = normalizeHash(operation.optimisticCast.id.value, "operation.castId");
        if (!normalizedHash) return std::unexpected(std::move(normalizedHash.error()));
        transport::http::HttpRequest request{
            .url = trimTrailingSlashes(m_config.baseUrl) + "/v1/castById?fid=" +
                std::to_string(operation.optimisticCast.author.value) + "&hash=" + *normalizedHash,
            .headers = {"Accept: application/json"},
            .timeout = m_config.timeout,
            .maxBodyBytes = m_config.maxBodyBytes,
        };
        auto response = m_transport.get(request);
        if (!response) return std::unexpected(std::move(response.error()));
        if (response->statusCode == 404U) {
            return capability::write::ConfirmationState::Absent;
        }
        if (response->statusCode < 200U || response->statusCode >= 300U) {
            return std::unexpected(httpStatusError(response->statusCode, response->body));
        }
        if (auto verified = verifyAuthoritativeCast(response->body, operation); !verified) {
            return std::unexpected(std::move(verified.error()));
        }
        return capability::write::ConfirmationState::Confirmed;
    }

private:
    transport::http::IHttpTransport& m_transport;
    Config m_config{};
};

} // namespace

std::unique_ptr<capability::write::IWriteSubmitter> makeSubmitter(
    transport::http::IHttpTransport& transport,
    Config config)
{
    return std::make_unique<Submitter>(transport, std::move(config));
}

} // namespace farcaster::provider::snapchain::write
