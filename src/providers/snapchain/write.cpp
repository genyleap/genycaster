module;

#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    if (!config.readBaseUrl.empty() &&
        ((!config.readBaseUrl.starts_with("https://") &&
          !config.readBaseUrl.starts_with("http://")) ||
         config.readBaseUrl.find_first_of("?#\r\n") != std::string::npos)) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain reconciliation read URL must be a valid HTTP(S) origin."));
    }
    if (config.timeout <= std::chrono::milliseconds::zero() || config.maxBodyBytes == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain write timeout and response-size limit must be positive."));
    }
    const bool hasAuthorizationHeaderName = !config.authorizationHeaderName.empty();
    const bool hasAuthorizationHeaderValue = !config.authorizationHeaderValue.empty();
    if (hasAuthorizationHeaderName != hasAuthorizationHeaderValue) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain authorization header name and value must be configured together."));
    }
    if (hasAuthorizationHeaderName) {
        const auto validHeaderCharacter = [](const unsigned char character) {
            return std::isalnum(character) != 0 ||
                std::string_view{"!#$%&'*+-.^_`|~"}.find(static_cast<char>(character)) !=
                    std::string_view::npos;
        };
        if (!std::ranges::all_of(config.authorizationHeaderName, validHeaderCharacter)) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Snapchain authorization header name contains invalid HTTP token characters."));
        }
        if (config.authorizationHeaderValue.find_first_of("\r\n") != std::string::npos) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Snapchain authorization header value must not contain line breaks."));
        }
    }
    const bool hasReadHeader = !config.readAuthorizationHeaderName.empty();
    if (hasReadHeader != !config.readAuthorizationHeaderValue.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Snapchain read authorization header name and value must be configured together."));
    }
    if (hasReadHeader) {
        const auto validHeaderCharacter = [](const unsigned char character) {
            return std::isalnum(character) != 0 ||
                std::string_view{"!#$%&'*+-.^_`|~"}.find(static_cast<char>(character)) !=
                    std::string_view::npos;
        };
        if (!std::ranges::all_of(config.readAuthorizationHeaderName, validHeaderCharacter)
            || config.readAuthorizationHeaderValue.find_first_of("\r\n") != std::string::npos) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Snapchain read authorization header contains invalid characters."));
        }
    }
    return {};
}

[[nodiscard]] std::vector<std::string> requestHeaders(
    const Config& config,
    const bool binaryBody)
{
    std::vector<std::string> headers{"Accept: application/json"};
    if (binaryBody) {
        headers.emplace_back("Content-Type: application/octet-stream");
    }
    const bool sameRoute = config.readBaseUrl.empty()
        || trimTrailingSlashes(config.readBaseUrl) == trimTrailingSlashes(config.baseUrl);
    const std::string& headerName = !binaryBody && !config.readAuthorizationHeaderName.empty()
        ? config.readAuthorizationHeaderName
        : !binaryBody && !sameRoute ? config.readAuthorizationHeaderName
                                    : config.authorizationHeaderName;
    const std::string& headerValue = !binaryBody && !config.readAuthorizationHeaderName.empty()
        ? config.readAuthorizationHeaderValue
        : !binaryBody && !sameRoute ? config.readAuthorizationHeaderValue
                                    : config.authorizationHeaderValue;
    if (!headerName.empty()) {
        headers.push_back(headerName + ": " + headerValue);
    }
    return headers;
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
        const bool looksLikeHtml = body.find("<!DOCTYPE") != std::string_view::npos ||
            body.find("<!doctype") != std::string_view::npos ||
            body.find("<html") != std::string_view::npos;
        if (looksLikeHtml) {
            return std::unexpected(protocolError(
                "Snapchain endpoint returned HTML instead of JSON. The configured base URL points "
                "to a documentation/reverse-proxy page rather than a Snapchain HTTP API."));
        }
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
    // Protobuf JSON maps uint64 fields to decimal strings. In particular,
    // HyperSnap's native FIDs start at 2^32 and must not pass through double.
    if (value->is_string()) {
        const auto& jsonText = value->as_string();
        const std::string_view decimal{jsonText.data(), jsonText.size()};
        if (!decimal.empty() && decimal.size() <= 20U
            && std::all_of(decimal.begin(), decimal.end(), [](const char ch) {
                return ch >= '0' && ch <= '9';
            })) {
            std::uint64_t parsed{};
            const auto [end, error] = std::from_chars(
                decimal.data(), decimal.data() + decimal.size(), parsed, 10);
            if (error == std::errc{} && end == decimal.data() + decimal.size()) {
                return parsed;
            }
        }
    }
    return std::unexpected(protocolError(
        std::string{context} + "." + std::string{key}
        + " must be an exact unsigned decimal integer (JSON number or string)."));
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
    case OperationKind::Follow:
    case OperationKind::Block: return "MESSAGE_TYPE_LINK_ADD";
    case OperationKind::Unfollow:
    case OperationKind::Unblock: return "MESSAGE_TYPE_LINK_REMOVE";
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
    case OperationKind::Block:
        return true;
    case OperationKind::RemoveCast:
    case OperationKind::Unlike:
    case OperationKind::UndoRecast:
    case OperationKind::Unfollow:
    case OperationKind::Unblock:
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
    const bool isBlock = operation.kind == OperationKind::Block
        || operation.kind == OperationKind::Unblock;
    if (*type != (isBlock ? "block" : "follow")
        || *targetFid != operation.target.fid->value) {
        return std::unexpected(protocolError("Protocol link type/target does not match the requested FID."));
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
    case OperationKind::Block:
    case OperationKind::Unblock:
        if (!operation.target.fid || operation.target.fid->value == operation.author.value) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Follow reconciliation requires a non-self FID target."));
        }
        break;
    }
    return {};
}

// Decode the canonical protobuf envelope before submitting it. The signer API,
// outbox identity, and node must all agree on the exact 64-bit author. This
// parser checks only wire framing and the author field; cryptographic signing
// is performed by the protocol codec before this adapter is called.
struct ProtoField final {
    std::uint64_t number{};
    std::uint8_t wireType{};
    std::uint64_t scalar{};
    std::string_view bytes{};
};

[[nodiscard]] foundation::result::Result<std::uint64_t> readProtoVarint(
    const std::string_view input,
    std::size_t& offset)
{
    std::uint64_t value{};
    for (unsigned index = 0U; index < 10U; ++index) {
        if (offset >= input.size()) {
            return std::unexpected(protocolError("Truncated signed protobuf varint."));
        }
        const auto octet = static_cast<std::uint8_t>(input[offset++]);
        if (index == 9U && octet > 1U) {
            return std::unexpected(protocolError("Signed protobuf varint exceeds uint64."));
        }
        value |= static_cast<std::uint64_t>(octet & 0x7FU) << (7U * index);
        if ((octet & 0x80U) == 0U) {
            return value;
        }
    }
    return std::unexpected(protocolError("Signed protobuf varint exceeds ten bytes."));
}

[[nodiscard]] foundation::result::Result<ProtoField> readProtoField(
    const std::string_view input,
    std::size_t& offset)
{
    auto tag = readProtoVarint(input, offset);
    if (!tag || (*tag >> 3U) == 0U || (*tag >> 3U) > 536'870'911U) {
        return std::unexpected(protocolError("Invalid signed protobuf field tag."));
    }
    ProtoField field{
        .number = *tag >> 3U,
        .wireType = static_cast<std::uint8_t>(*tag & 7U),
    };
    switch (field.wireType) {
    case 0U: {
        auto scalar = readProtoVarint(input, offset);
        if (!scalar) return std::unexpected(std::move(scalar.error()));
        field.scalar = *scalar;
        break;
    }
    case 1U:
    case 5U: {
        const auto size = field.wireType == 1U ? 8U : 4U;
        if (size > input.size() - offset) {
            return std::unexpected(protocolError("Truncated signed protobuf fixed-width field."));
        }
        offset += size;
        break;
    }
    case 2U: {
        auto length = readProtoVarint(input, offset);
        if (!length) return std::unexpected(std::move(length.error()));
        if (*length > input.size() - offset) {
            return std::unexpected(protocolError("Truncated signed protobuf bytes field."));
        }
        field.bytes = input.substr(offset, static_cast<std::size_t>(*length));
        offset += static_cast<std::size_t>(*length);
        break;
    }
    default:
        return std::unexpected(protocolError("Unsupported signed protobuf wire type."));
    }
    return field;
}

[[nodiscard]] foundation::result::Result<std::uint64_t> signedEnvelopeAuthor(
    const std::string_view envelope)
{
    std::optional<std::string_view> dataBytes{};
    std::size_t offset{};
    while (offset < envelope.size()) {
        auto field = readProtoField(envelope, offset);
        if (!field) return std::unexpected(std::move(field.error()));
        if (field->number == 7U) {
            if (field->wireType != 2U || dataBytes) {
                return std::unexpected(protocolError(
                    "Signed Message contains invalid or duplicate dataBytes."));
            }
            dataBytes = field->bytes;
        }
    }
    if (!dataBytes || dataBytes->empty()) {
        return std::unexpected(protocolError(
            "Signed Message is missing canonical MessageData bytes."));
    }

    std::optional<std::uint64_t> author{};
    offset = 0U;
    while (offset < dataBytes->size()) {
        auto field = readProtoField(*dataBytes, offset);
        if (!field) return std::unexpected(std::move(field.error()));
        if (field->number == 2U) {
            if (field->wireType != 0U || author) {
                return std::unexpected(protocolError(
                    "Signed MessageData contains invalid or duplicate author FID."));
            }
            author = field->scalar;
        }
    }
    if (!author || *author == 0U) {
        return std::unexpected(protocolError(
            "Signed MessageData is missing a nonzero author FID."));
    }
    return *author;
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
    if (statusCode == 400U
        && body.find("bad_request.validation_failure/unknown fid") != std::string_view::npos) {
        // An accepted local signer is not proof that the target node has
        // indexed this account. Do not reinterpret this as a bad signature
        // or silently reroute the already-signed message to another network.
        message.append(" The selected write node does not recognize the author's FID. "
                       "Confirm this FID and custody in that protocol's authoritative "
                       "registry on the SAME node before attempting another write.");
    }
    return makeError(code, std::move(message));
}

// This is diagnostic only, after the signed write has already been rejected.
// A shard-0 lookup proves registration there, not that the user's data shard
// has replayed the native registration. Never retry or switch network here.
[[nodiscard]] std::string nativeUnknownFidEvidence(
    transport::http::IHttpTransport& transport,
    const Config& config,
    const domain::identity::Fid author)
{
    constexpr std::uint64_t FirstNativeFid = 1ULL << 32;
    if (!config.probeNativeRegistryOnUnknownFid || author.value < FirstNativeFid) {
        return {};
    }
    const auto& origin = config.readBaseUrl.empty() ? config.baseUrl : config.readBaseUrl;
    auto response = transport.get(transport::http::HttpRequest{
        .url = trimTrailingSlashes(origin) + "/v1/nativeAccount?fid="
            + std::to_string(author.value),
        .headers = requestHeaders(config, false),
        .timeout = config.timeout,
        .maxBodyBytes = std::min(config.maxBodyBytes, std::size_t{32768}),
    });
    if (!response) {
        return " Native registry lookup on the selected node was unavailable; "
               "the account's registration status is unknown. No automatic retry.";
    }
    if ((response->statusCode == 400U || response->statusCode == 404U)
        && response->body.find("native account not found") != std::string::npos) {
        return " The selected node's shard-0 native registry has NO confirmed "
               "registration for this FID. Check the original registration node "
               "and custody-wallet recovery; do not submit another registration blindly.";
    }
    if (response->statusCode != 200U) {
        return " Shard-0 native account lookup returned HTTP "
            + std::to_string(response->statusCode)
            + "; the registration cannot be independently verified. No automatic retry.";
    }
    auto parsed = parseJson(response->body);
    if (!parsed || !parsed->is_object()) {
        return " Native account lookup returned an invalid JSON response; "
               "the registration cannot be verified.";
    }
    const auto& record = parsed->as_object();
    auto fid = requireUnsigned(record, "fid", "nativeAccount");
    auto custody = requireString(record, "custodyAddress", "nativeAccount");
    const auto* message = findValue(record, "message");
    const auto* block = findValue(record, "blockNumber");
    const bool validCustody = custody && custody->size() == 42U
        && custody->starts_with("0x")
        && std::all_of(custody->begin() + 2, custody->end(), isHexDigit);
    const bool validBlock = block
        && ((block->is_uint64() && block->as_uint64() > 0U)
            || (block->is_int64() && block->as_int64() > 0));
    if (!fid || *fid != author.value || !validCustody || !validBlock
        || !message || !message->is_object()) {
        return " Native account lookup returned an incomplete or mismatched "
               "shard-0 record; the selected node cannot safely verify this FID.";
    }
    return " Shard 0 on the selected HyperSnap node CONFIRMS this native FID, "
           "but the write path still reports unknown fid. Its author data shard "
           "may be behind or missing replicated registration events. The node "
           "operator should inspect shard sync and block_receiver replication; "
           "do not re-register the wallet or silently cross-post to Farcaster.";
}

[[nodiscard]] std::string reconciliationUrl(
    const Config& config,
    const domain::write::SignedOperation& operation)
{
    const std::string base = trimTrailingSlashes(
        config.readBaseUrl.empty() ? config.baseUrl : config.readBaseUrl);
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
    case OperationKind::Block:
    case OperationKind::Unblock:
        return base + "/v1/linkById?fid=" + std::to_string(operation.author.value) +
            "&target_fid=" + std::to_string(operation.target.fid->value) +
            "&link_type=" + ((operation.kind == OperationKind::Block
                || operation.kind == OperationKind::Unblock) ? "block" : "follow");
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
        auto envelopeAuthor = signedEnvelopeAuthor(operation.messageBytes);
        if (!envelopeAuthor) {
            return std::unexpected(std::move(envelopeAuthor.error()));
        }
        if (*envelopeAuthor != operation.author.value) {
            return std::unexpected(protocolError(
                "Signed canonical MessageData FID " + std::to_string(*envelopeAuthor)
                + " differs from selected protocol author FID "
                + std::to_string(operation.author.value)
                + "; refusing a cross-account write."));
        }
        transport::http::HttpPostRequest request{
            .url = trimTrailingSlashes(m_config.baseUrl) + "/v1/submitMessage",
            .headers = requestHeaders(m_config, true),
            .body = operation.messageBytes,
            .timeout = m_config.timeout,
            .maxBodyBytes = m_config.maxBodyBytes,
        };
        auto response = m_transport.post(request);
        if (!response) {
            return std::unexpected(std::move(response.error()));
        }
        if (response->statusCode < 200U || response->statusCode >= 300U) {
            auto failure = httpStatusError(response->statusCode, response->body);
            if (response->statusCode == 400U
                && response->body.find("bad_request.validation_failure/unknown fid")
                    != std::string::npos) {
                failure.message.append(nativeUnknownFidEvidence(
                    m_transport, m_config, operation.author));
            }
            return std::unexpected(std::move(failure));
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
            .headers = requestHeaders(m_config, false),
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
        case OperationKind::Block:
        case OperationKind::Unblock:
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
