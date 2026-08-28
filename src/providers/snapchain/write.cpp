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

module farcaster.provider.snapchain.write;

import farcaster.domain.identity;
import farcaster.capability.write_submit;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.http;

namespace farcaster::provider::snapchain::write {
namespace {

using JsonObject = boost::json::object;
using JsonValue = boost::json::value;
using domain::write::OperationKind;

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
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
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
    if (hex.starts_with("0x") || hex.starts_with("0X")) {
        hex.remove_prefix(2U);
    }
    if (hex.size() != 40U || !std::all_of(hex.begin(), hex.end(), isHexDigit)) {
        return std::unexpected(protocolError(
            std::string{context} + " must be a 20-byte hexadecimal message hash."));
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
    if (value->is_uint64()) {
        return value->as_uint64();
    }
    if (value->is_int64() && value->as_int64() >= 0) {
        return static_cast<std::uint64_t>(value->as_int64());
    }
    return std::unexpected(protocolError(
        std::string{context} + "." + std::string{key} + " must be a non-negative integer."));
}

[[nodiscard]] std::string_view expectedMessageType(const OperationKind kind) noexcept
{
    switch (kind) {
    case OperationKind::PublishCast: return "MESSAGE_TYPE_CAST_ADD";
    case OperationKind::RemoveCast: return "MESSAGE_TYPE_CAST_REMOVE";
    case OperationKind::Like:
    case OperationKind::Recast: return "MESSAGE_TYPE_REACTION_ADD";
    case OperationKind::Unlike:
    case OperationKind::UndoRecast: return "MESSAGE_TYPE_REACTION_REMOVE";
    case OperationKind::Follow: return "MESSAGE_TYPE_LINK_ADD";
    case OperationKind::Unfollow: return "MESSAGE_TYPE_LINK_REMOVE";
    }
    return "MESSAGE_TYPE_NONE";
}

[[nodiscard]] std::string_view reactionType(const OperationKind kind) noexcept
{
    switch (kind) {
    case OperationKind::Like:
    case OperationKind::Unlike: return "REACTION_TYPE_LIKE";
    case OperationKind::Recast:
    case OperationKind::UndoRecast: return "REACTION_TYPE_RECAST";
    default: return "REACTION_TYPE_NONE";
    }
}

[[nodiscard]] bool desiredStateIsPresent(const OperationKind kind) noexcept
{
    switch (kind) {
    case OperationKind::PublishCast:
    case OperationKind::Like:
    case OperationKind::Recast:
    case OperationKind::Follow:
        return true;
    case OperationKind::RemoveCast:
    case OperationKind::Unlike:
    case OperationKind::UndoRecast:
    case OperationKind::Unfollow:
        return false;
    }
    return false;
}

[[nodiscard]] foundation::result::Status verifyEnvelope(
    const std::string_view body,
    const domain::write::SignedOperation& operation,
    const std::string_view expectedType,
    const bool requireOperationHash)
{
    auto document = parseJson(body);
    if (!document) {
        return std::unexpected(std::move(document.error()));
    }
    if (!document->is_object()) {
        return std::unexpected(protocolError("Snapchain message response must be a JSON object."));
    }
    const auto& object = document->as_object();
    auto responseHash = requireString(object, "hash", "message");
    if (!responseHash) {
        return std::unexpected(std::move(responseHash.error()));
    }
    auto normalizedResponse = normalizeHash(*responseHash, "message.hash");
    if (!normalizedResponse) {
        return std::unexpected(std::move(normalizedResponse.error()));
    }
    if (requireOperationHash) {
        auto normalizedExpected = normalizeHash(operation.idempotencyKey.value, "operation.idempotencyKey");
        if (!normalizedExpected) {
            return std::unexpected(std::move(normalizedExpected.error()));
        }
        if (*normalizedResponse != *normalizedExpected) {
            return std::unexpected(protocolError(
                "Snapchain returned a different message hash than the submitted canonical operation."));
        }
    }

    const auto* dataValue = findValue(object, "data");
    if (dataValue == nullptr || !dataValue->is_object()) {
        return std::unexpected(protocolError("Snapchain message response.data must be an object."));
    }
    const auto& data = dataValue->as_object();
    auto type = requireString(data, "type", "message.data");
    auto fid = requireUnsigned(data, "fid", "message.data");
    if (!type) {
        return std::unexpected(std::move(type.error()));
    }
    if (!fid) {
        return std::unexpected(std::move(fid.error()));
    }
    if (*type != expectedType) {
        return std::unexpected(protocolError(
            "Snapchain returned a different protocol message type than expected."));
    }
    if (*fid != operation.author.value) {
        return std::unexpected(protocolError("Snapchain message contains a mismatched author FID."));
    }
    return {};
}

[[nodiscard]] foundation::result::Status verifyReactionState(
    const std::string_view body,
    const domain::write::SignedOperation& operation)
{
    if (!operation.target.cast) {
        return std::unexpected(protocolError("Reaction reconciliation is missing its cast target."));
    }
    if (auto verified = verifyEnvelope(body, operation, "MESSAGE_TYPE_REACTION_ADD", false); !verified) {
        return verified;
    }
    auto document = parseJson(body);
    if (!document || !document->is_object()) {
        return std::unexpected(protocolError("Snapchain reaction response must be a JSON object."));
    }
    const auto& data = findValue(document->as_object(), "data")->as_object();
    const auto* reactionBodyValue = findValue(data, "reactionBody");
    if (reactionBodyValue == nullptr || !reactionBodyValue->is_object()) {
        return std::unexpected(protocolError("Snapchain reaction response is missing reactionBody."));
    }
    const auto& reactionBody = reactionBodyValue->as_object();
    auto type = requireString(reactionBody, "type", "message.data.reactionBody");
    if (!type) {
        return std::unexpected(std::move(type.error()));
    }
    if (*type != reactionType(operation.kind)) {
        return std::unexpected(protocolError("Snapchain reaction type does not match the requested interaction."));
    }
    const auto* targetValue = findValue(reactionBody, "targetCastId");
    if (targetValue == nullptr || !targetValue->is_object()) {
        return std::unexpected(protocolError("Snapchain reaction response is missing targetCastId."));
    }
    const auto& target = targetValue->as_object();
    auto targetFid = requireUnsigned(target, "fid", "message.data.reactionBody.targetCastId");
    auto targetHash = requireString(target, "hash", "message.data.reactionBody.targetCastId");
    if (!targetFid) {
        return std::unexpected(std::move(targetFid.error()));
    }
    if (!targetHash) {
        return std::unexpected(std::move(targetHash.error()));
    }
    auto actualHash = normalizeHash(*targetHash, "reaction.targetCastId.hash");
    auto expectedHash = normalizeHash(operation.target.cast->id.value, "operation.target.cast.id");
    if (!actualHash) {
        return std::unexpected(std::move(actualHash.error()));
    }
    if (!expectedHash) {
        return std::unexpected(std::move(expectedHash.error()));
    }
    if (*targetFid != operation.target.cast->author.value || *actualHash != *expectedHash) {
        return std::unexpected(protocolError("Snapchain reaction target does not match the requested cast."));
    }
    return {};
}

[[nodiscard]] foundation::result::Status verifyFollowState(
    const std::string_view body,
    const domain::write::SignedOperation& operation)
{
    if (!operation.target.fid) {
        return std::unexpected(protocolError("Follow reconciliation is missing its FID target."));
    }
    if (auto verified = verifyEnvelope(body, operation, "MESSAGE_TYPE_LINK_ADD", false); !verified) {
        return verified;
    }
    auto document = parseJson(body);
    if (!document || !document->is_object()) {
        return std::unexpected(protocolError("Snapchain link response must be a JSON object."));
    }
    const auto& data = findValue(document->as_object(), "data")->as_object();
    const auto* linkBodyValue = findValue(data, "linkBody");
    if (linkBodyValue == nullptr || !linkBodyValue->is_object()) {
        return std::unexpected(protocolError("Snapchain link response is missing linkBody."));
    }
    const auto& linkBody = linkBodyValue->as_object();
    auto type = requireString(linkBody, "type", "message.data.linkBody");
    auto targetFid = requireUnsigned(linkBody, "targetFid", "message.data.linkBody");
    if (!type) {
        return std::unexpected(std::move(type.error()));
    }
    if (!targetFid) {
        return std::unexpected(std::move(targetFid.error()));
    }
    if (*type != "follow" || *targetFid != operation.target.fid->value) {
        return std::unexpected(protocolError("Snapchain follow target does not match the requested FID."));
    }
    return {};
}

[[nodiscard]] foundation::result::Status verifyCastState(
    const std::string_view body,
    const domain::write::SignedOperation& operation)
{
    const auto* reference = operation.kind == OperationKind::PublishCast
        ? nullptr : (operation.target.cast ? &*operation.target.cast : nullptr);
    const domain::identity::Fid expectedAuthor = operation.kind == OperationKind::PublishCast
        ? operation.author : reference->author;
    const std::string_view expectedHash = operation.kind == OperationKind::PublishCast
        ? std::string_view{operation.optimisticCast->id.value}
        : std::string_view{reference->id.value};

    auto document = parseJson(body);
    if (!document || !document->is_object()) {
        return std::unexpected(protocolError("Snapchain cast response must be a JSON object."));
    }
    const auto& object = document->as_object();
    auto responseHash = requireString(object, "hash", "message");
    if (!responseHash) {
        return std::unexpected(std::move(responseHash.error()));
    }
    auto normalizedResponse = normalizeHash(*responseHash, "message.hash");
    auto normalizedExpected = normalizeHash(expectedHash, "operation.castHash");
    if (!normalizedResponse) {
        return std::unexpected(std::move(normalizedResponse.error()));
    }
    if (!normalizedExpected) {
        return std::unexpected(std::move(normalizedExpected.error()));
    }
    if (*normalizedResponse != *normalizedExpected) {
        return std::unexpected(protocolError("Snapchain cast lookup returned a different cast hash."));
    }
    const auto* dataValue = findValue(object, "data");
    if (dataValue == nullptr || !dataValue->is_object()) {
        return std::unexpected(protocolError("Snapchain cast response.data must be an object."));
    }
    const auto& data = dataValue->as_object();
    auto type = requireString(data, "type", "message.data");
    auto fid = requireUnsigned(data, "fid", "message.data");
    if (!type) {
        return std::unexpected(std::move(type.error()));
    }
    if (!fid) {
        return std::unexpected(std::move(fid.error()));
    }
    if (*type != "MESSAGE_TYPE_CAST_ADD" || *fid != expectedAuthor.value) {
        return std::unexpected(protocolError("Snapchain cast lookup returned a mismatched CAST_ADD."));
    }
    return {};
}

[[nodiscard]] foundation::result::Status validateOperation(const domain::write::SignedOperation& operation)
{
    if (operation.author.value == 0U || operation.idempotencyKey.value.empty() ||
        operation.messageBytes.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain submitter requires a complete signed protocol operation."));
    }
    auto messageHash = normalizeHash(operation.idempotencyKey.value, "operation.idempotencyKey");
    if (!messageHash) {
        return std::unexpected(std::move(messageHash.error()));
    }
    switch (operation.kind) {
    case OperationKind::PublishCast:
        if (!operation.optimisticCast || operation.optimisticCast->author.value != operation.author.value) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "PublishCast reconciliation requires its optimistic cast identity."));
        }
        break;
    case OperationKind::RemoveCast:
        if (!operation.target.cast || operation.target.cast->author.value != operation.author.value) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "RemoveCast reconciliation requires a cast owned by the author."));
        }
        break;
    case OperationKind::Like:
    case OperationKind::Unlike:
    case OperationKind::Recast:
    case OperationKind::UndoRecast:
        if (!operation.target.cast) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Reaction reconciliation requires a cast target."));
        }
        break;
    case OperationKind::Follow:
    case OperationKind::Unfollow:
        if (!operation.target.fid || operation.target.fid->value == operation.author.value) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Follow reconciliation requires a non-self FID target."));
        }
        break;
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
        if (excerpt.size() >= maxLength) {
            break;
        }
        const auto character = static_cast<unsigned char>(raw);
        if (std::isspace(character) != 0) {
            if (!excerpt.empty() && !previousWhitespace) {
                excerpt.push_back(' ');
            }
            previousWhitespace = true;
            continue;
        }
        if (std::iscntrl(character) != 0) {
            continue;
        }
        excerpt.push_back(raw);
        previousWhitespace = false;
    }
    while (!excerpt.empty() && excerpt.back() == ' ') {
        excerpt.pop_back();
    }
    if (body.size() > maxLength) {
        excerpt.append("...");
    }
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
        if (statusCode >= 500U) {
            code = foundation::error::ErrorCode::Unavailable;
        }
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

[[nodiscard]] std::string reconciliationUrl(
    const Config& config,
    const domain::write::SignedOperation& operation)
{
    const std::string base = trimTrailingSlashes(config.baseUrl);
    switch (operation.kind) {
    case OperationKind::PublishCast:
        return base + "/v1/castById?fid=" + std::to_string(operation.author.value) +
            "&hash=" + operation.optimisticCast->id.value;
    case OperationKind::RemoveCast:
        return base + "/v1/castById?fid=" + std::to_string(operation.target.cast->author.value) +
            "&hash=" + operation.target.cast->id.value;
    case OperationKind::Like:
    case OperationKind::Unlike:
    case OperationKind::Recast:
    case OperationKind::UndoRecast:
        return base + "/v1/reactionById?fid=" + std::to_string(operation.author.value) +
            "&target_fid=" + std::to_string(operation.target.cast->author.value) +
            "&target_hash=" + operation.target.cast->id.value +
            "&reaction_type=" + ((operation.kind == OperationKind::Like ||
                operation.kind == OperationKind::Unlike) ? "Like" : "Recast");
    case OperationKind::Follow:
    case OperationKind::Unfollow:
        return base + "/v1/linkById?fid=" + std::to_string(operation.author.value) +
            "&target_fid=" + std::to_string(operation.target.fid->value) + "&link_type=follow";
    }
    return base;
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
        if (auto status = validateConfig(m_config); !status) {
            return status;
        }
        if (auto status = validateOperation(operation); !status) {
            return status;
        }
        transport::http::HttpPostRequest request{
            .url = trimTrailingSlashes(m_config.baseUrl) + "/v1/submitMessage",
            .headers = {"Accept: application/json", "Content-Type: application/octet-stream"},
            .body = operation.messageBytes,
            .timeout = m_config.timeout,
            .maxBodyBytes = m_config.maxBodyBytes,
        };
        auto response = m_transport.post(request);
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (response->statusCode < 200U || response->statusCode >= 300U) {
            return std::unexpected(httpStatusError(response->statusCode, response->body));
        }
        return verifyEnvelope(response->body, operation, expectedMessageType(operation.kind), true);
    }

    [[nodiscard]] foundation::result::Result<capability::write::ConfirmationState> reconcile(
        const domain::write::SignedOperation& operation) override
    {
        if (auto status = validateConfig(m_config); !status) {
            return std::unexpected(std::move(status.error()));
        }
        if (auto status = validateOperation(operation); !status) {
            return std::unexpected(std::move(status.error()));
        }
        transport::http::HttpRequest request{
            .url = reconciliationUrl(m_config, operation),
            .headers = {"Accept: application/json"},
            .timeout = m_config.timeout,
            .maxBodyBytes = m_config.maxBodyBytes,
        };
        auto response = m_transport.get(request);
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (response->statusCode == 404U) {
            return desiredStateIsPresent(operation.kind)
                ? capability::write::ConfirmationState::Absent
                : capability::write::ConfirmationState::Confirmed;
        }
        if (response->statusCode < 200U || response->statusCode >= 300U) {
            return std::unexpected(httpStatusError(response->statusCode, response->body));
        }

        foundation::result::Status verified{};
        switch (operation.kind) {
        case OperationKind::PublishCast:
        case OperationKind::RemoveCast:
            verified = verifyCastState(response->body, operation);
            break;
        case OperationKind::Like:
        case OperationKind::Unlike:
        case OperationKind::Recast:
        case OperationKind::UndoRecast:
            verified = verifyReactionState(response->body, operation);
            break;
        case OperationKind::Follow:
        case OperationKind::Unfollow:
            verified = verifyFollowState(response->body, operation);
            break;
        }
        if (!verified) {
            return std::unexpected(std::move(verified.error()));
        }
        return desiredStateIsPresent(operation.kind)
            ? capability::write::ConfirmationState::Confirmed
            : capability::write::ConfirmationState::Absent;
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
