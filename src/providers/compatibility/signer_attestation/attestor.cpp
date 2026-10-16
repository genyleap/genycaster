module;

#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

module farcaster.provider.compatibility.signer_attestation;

import farcaster.capability.signer_attestation;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.protocol.message;
import farcaster.transport.http;

namespace farcaster::provider::compatibility::signer_attestation {
namespace {

namespace json = boost::json;
constexpr std::size_t EthereumAddressBytes = 20U;
constexpr std::size_t EcdsaSignatureBytes = 65U;

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] bool isAllowedEndpoint(const std::string_view url)
{
    if (url.starts_with("https://")) {
        return true;
    }
    return url.starts_with("http://127.0.0.1") || url.starts_with("http://localhost");
}

[[nodiscard]] foundation::result::Status validateConfig(const Config& config)
{
    if (config.endpointUrl.empty() || !isAllowedEndpoint(config.endpointUrl) ||
        config.endpointUrl.find_first_of("\r\n") != std::string::npos) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Signer-attestation endpoint must use HTTPS, or loopback HTTP for local development."));
    }
    if (config.timeout <= std::chrono::milliseconds::zero() || config.maxBodyBytes == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Signer-attestation timeout and response-size limit must be positive."));
    }
    return {};
}

[[nodiscard]] bool isHex(const std::string_view value)
{
    return std::all_of(value.begin(), value.end(), [](const char character) {
        return std::isxdigit(static_cast<unsigned char>(character)) != 0;
    });
}

[[nodiscard]] foundation::result::Result<std::string> decodeHex(
    const std::string_view value,
    const std::size_t requiredBytes,
    const std::string_view label)
{
    auto hex = value;
    if (hex.starts_with("0x") || hex.starts_with("0X")) {
        hex.remove_prefix(2U);
    }
    if (hex.size() != requiredBytes * 2U || !isHex(hex)) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            std::string{label} + " must be exactly " + std::to_string(requiredBytes) +
                " bytes of hexadecimal data."));
    }
    auto nibble = [](const char character) -> unsigned char {
        if (character >= '0' && character <= '9') return static_cast<unsigned char>(character - '0');
        if (character >= 'a' && character <= 'f') return static_cast<unsigned char>(10 + character - 'a');
        return static_cast<unsigned char>(10 + character - 'A');
    };
    std::string bytes(requiredBytes, '\0');
    for (std::size_t index{}; index < requiredBytes; ++index) {
        bytes[index] = static_cast<char>(
            (nibble(hex[index * 2U]) << 4U) | nibble(hex[index * 2U + 1U]));
    }
    return bytes;
}

[[nodiscard]] foundation::result::Result<std::string> requireString(
    const json::object& object,
    const std::string_view key)
{
    const auto* value = object.if_contains(key);
    if (value == nullptr || !value->is_string()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Signer-attestation response is missing string field '" + std::string{key} + "'."));
    }
    return std::string{value->as_string()};
}

[[nodiscard]] foundation::result::Result<std::uint64_t> requireUnsigned(
    const json::object& object,
    const std::string_view key)
{
    const auto* value = object.if_contains(key);
    if (value == nullptr) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::ProtocolFailure,
            "Signer-attestation response is missing field '" + std::string{key} + "'."));
    }
    if (value->is_uint64()) return value->as_uint64();
    if (value->is_int64() && value->as_int64() >= 0) {
        return static_cast<std::uint64_t>(value->as_int64());
    }
    if (value->is_string()) {
        try {
            return std::stoull(std::string{value->as_string()});
        } catch (...) {
        }
    }
    return std::unexpected(makeError(
        foundation::error::ErrorCode::ProtocolFailure,
        "Signer-attestation response field '" + std::string{key} + "' must be an unsigned integer."));
}

[[nodiscard]] foundation::error::Error statusError(
    const std::uint16_t statusCode,
    const std::string_view body)
{
    foundation::error::ErrorCode code = foundation::error::ErrorCode::ProtocolFailure;
    if (statusCode == 401U || statusCode == 403U) code = foundation::error::ErrorCode::Unauthorized;
    else if (statusCode == 408U) code = foundation::error::ErrorCode::Timeout;
    else if (statusCode == 404U) code = foundation::error::ErrorCode::NotFound;
    else if (statusCode == 429U) code = foundation::error::ErrorCode::RateLimited;
    else if (statusCode >= 500U) code = foundation::error::ErrorCode::Unavailable;
    std::string message = "Signer-attestation endpoint returned HTTP " + std::to_string(statusCode) + ".";
    if (!body.empty()) {
        message += " Response: ";
        message.append(body.substr(0U, std::min<std::size_t>(body.size(), 240U)));
    }
    return makeError(code, std::move(message));
}

class HttpAttestor final : public capability::signer_attestation::ISignedKeyRequestAttestor {
public:
    HttpAttestor(transport::http::IHttpTransport& transport, Config config)
        : m_transport(transport), m_config(std::move(config))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::signer_attestation::SignedKeyRequestAttestation> attest(
        const std::uint64_t requestFid,
        const std::string_view publicKeyHex,
        const foundation::time::Timestamp deadlineValue) override
    {
        if (auto valid = validateConfig(m_config); !valid) {
            return std::unexpected(std::move(valid.error()));
        }
        if (requestFid == 0U || publicKeyHex.size() != 64U) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "App signer attestation requires a non-zero requestFid and 32-byte Ed25519 public key."));
        }
        auto deadline = protocol::message::toFarcasterTimestamp(deadlineValue);
        if (!deadline) {
            return std::unexpected(std::move(deadline.error()));
        }

        json::object payload{};
        payload["requestFid"] = std::to_string(requestFid);
        payload["publicKey"] = "0x" + std::string{publicKeyHex};
        payload["deadline"] = *deadline;
        transport::http::HttpPostRequest post{
            .url = m_config.endpointUrl,
            .headers = {"Accept: application/json", "Content-Type: application/json"},
            .body = json::serialize(payload),
            .timeout = m_config.timeout,
            .maxBodyBytes = m_config.maxBodyBytes,
        };
        auto response = m_transport.post(post);
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (response->statusCode < 200U || response->statusCode >= 300U) {
            return std::unexpected(statusError(response->statusCode, response->body));
        }

        boost::system::error_code parseError{};
        auto document = json::parse(response->body, parseError);
        if (parseError || !document.is_object()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::ProtocolFailure,
                "Signer-attestation response must be a JSON object."));
        }
        const auto& object = document.as_object();
        auto responseFid = requireUnsigned(object, "requestFid");
        auto responseKey = requireString(object, "publicKey");
        auto responseDeadline = requireUnsigned(object, "deadline");
        auto address = requireString(object, "requestSignerAddress");
        auto signature = requireString(object, "signedKeyRequestSignature");
        if (!responseFid) return std::unexpected(std::move(responseFid.error()));
        if (!responseKey) return std::unexpected(std::move(responseKey.error()));
        if (!responseDeadline) return std::unexpected(std::move(responseDeadline.error()));
        if (!address) return std::unexpected(std::move(address.error()));
        if (!signature) return std::unexpected(std::move(signature.error()));

        std::string expectedKey = "0x" + std::string{publicKeyHex};
        std::ranges::transform(*responseKey, responseKey->begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        if (*responseFid != requestFid || *responseDeadline != *deadline ||
            *responseKey != expectedKey) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Conflict,
                "Signer-attestation response is not bound to the exact requested FID/key/deadline."));
        }
        auto addressBytes = decodeHex(*address, EthereumAddressBytes, "requestSignerAddress");
        auto signatureBytes = decodeHex(*signature, EcdsaSignatureBytes, "signedKeyRequestSignature");
        if (!addressBytes) return std::unexpected(std::move(addressBytes.error()));
        if (!signatureBytes) return std::unexpected(std::move(signatureBytes.error()));
        return capability::signer_attestation::SignedKeyRequestAttestation{
            .requestSignerAddress = std::move(*addressBytes),
            .signature = std::move(*signatureBytes),
        };
    }

private:
    transport::http::IHttpTransport& m_transport;
    Config m_config{};
};

} // namespace

foundation::result::Result<std::unique_ptr<capability::signer_attestation::ISignedKeyRequestAttestor>>
makeAttestor(transport::http::IHttpTransport& transport, Config config)
{
    if (auto valid = validateConfig(config); !valid) {
        return std::unexpected(std::move(valid.error()));
    }
    std::unique_ptr<capability::signer_attestation::ISignedKeyRequestAttestor> result =
        std::make_unique<HttpAttestor>(transport, std::move(config));
    return result;
}

} // namespace farcaster::provider::compatibility::signer_attestation
