module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.protocol.message;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.protocol.hash;

namespace farcaster::protocol::message {
namespace {

constexpr std::uint32_t hashSchemeBlake3 = 1U;
constexpr std::uint32_t signatureSchemeEd25519 = 1U;
constexpr std::uint32_t messageTypeCastAdd = 1U;
constexpr std::uint32_t messageTypeCastRemove = 2U;
constexpr std::uint32_t messageTypeReactionAdd = 3U;
constexpr std::uint32_t messageTypeReactionRemove = 4U;
constexpr std::uint32_t messageTypeLinkAdd = 5U;
constexpr std::uint32_t messageTypeLinkRemove = 6U;
constexpr std::uint32_t reactionTypeLike = 1U;
constexpr std::uint32_t reactionTypeRecast = 2U;
constexpr std::uint32_t messageTypeKeyAdd = 16U;
constexpr std::uint32_t messageTypeKeyRemove = 17U;
constexpr std::uint32_t keyTypeEd25519 = 1U;
constexpr std::uint32_t metadataTypeSignedKeyRequest = 1U;
constexpr std::uint32_t keyRemoveSignatureTypeSelfRevoke = 2U;
constexpr std::uint64_t farcasterEpochUnixSeconds = 1'609'459'200ULL;
constexpr std::size_t publicKeySize = 32U;
constexpr std::size_t signatureSize = 64U;
constexpr std::size_t custodySignatureSize = 65U;
constexpr std::size_t castHashSize = 20U;
constexpr std::size_t maximumCastTextBytes = 320U;
constexpr std::size_t conservativeMaximumCastEmbeds = 2U;
constexpr std::size_t maximumProtocolUrlBytes = 256U;

[[nodiscard]] foundation::error::Error invalid(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::InvalidArgument,
        std::move(message));
}

[[nodiscard]] foundation::error::Error protocolFailure(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::ProtocolFailure,
        std::move(message));
}

void appendVarint(std::string& output, std::uint64_t value)
{
    while (value >= 0x80U) {
        output.push_back(static_cast<char>((value & 0x7FU) | 0x80U));
        value >>= 7U;
    }
    output.push_back(static_cast<char>(value));
}

void appendTag(std::string& output, const std::uint32_t fieldNumber, const std::uint32_t wireType)
{
    appendVarint(output, (static_cast<std::uint64_t>(fieldNumber) << 3U) | wireType);
}

void appendUint(std::string& output, const std::uint32_t fieldNumber, const std::uint64_t value)
{
    if (value == 0U) {
        return;
    }
    appendTag(output, fieldNumber, 0U);
    appendVarint(output, value);
}

void appendBytes(
    std::string& output,
    const std::uint32_t fieldNumber,
    const std::string_view bytes,
    const bool emitEmpty = false)
{
    if (bytes.empty() && !emitEmpty) {
        return;
    }
    appendTag(output, fieldNumber, 2U);
    appendVarint(output, bytes.size());
    output.append(bytes);
}

void appendMessage(
    std::string& output,
    const std::uint32_t fieldNumber,
    const std::string_view messageBytes)
{
    appendBytes(output, fieldNumber, messageBytes, true);
}

[[nodiscard]] int hexDigit(const char character) noexcept
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] foundation::result::Result<std::string> decodePublicKey(
    const domain::signer::PublicKey& publicKey)
{
    if (publicKey.hex.size() != publicKeySize * 2U) {
        return std::unexpected(invalid("Farcaster Ed25519 public key must contain exactly 64 hexadecimal characters."));
    }

    std::string bytes(publicKeySize, '\0');
    for (std::size_t index{}; index < publicKeySize; ++index) {
        const auto high = hexDigit(publicKey.hex[index * 2U]);
        const auto low = hexDigit(publicKey.hex[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return std::unexpected(invalid("Farcaster Ed25519 public key contains non-hexadecimal characters."));
        }
        bytes[index] = static_cast<char>((high << 4) | low);
    }
    return bytes;
}

[[nodiscard]] foundation::result::Status validateCommon(
    const domain::identity::Fid fid,
    const std::uint32_t timestamp,
    const std::uint32_t deadline,
    const Network network)
{
    if (fid.value == 0U) {
        return std::unexpected(invalid("Protocol messages require a non-zero FID."));
    }
    if (timestamp == 0U) {
        return std::unexpected(invalid("Protocol messages require a non-zero Farcaster timestamp."));
    }
    if (deadline < timestamp) {
        return std::unexpected(invalid("Signer authorization deadline must not precede the message timestamp."));
    }
    switch (network) {
    case Network::Mainnet:
    case Network::Testnet:
    case Network::Devnet:
        break;
    }
    return {};
}

[[nodiscard]] foundation::result::Result<std::string> decodeCastHash(
    const std::string_view value,
    const std::string_view context)
{
    std::string_view hex = value;
    if (hex.starts_with("0x") || hex.starts_with("0X")) {
        hex.remove_prefix(2U);
    }
    if (hex.size() != castHashSize * 2U) {
        return std::unexpected(invalid(
            std::string{context} + " must contain exactly 20 bytes of hexadecimal cast hash."));
    }
    std::string bytes(castHashSize, '\0');
    for (std::size_t index{}; index < castHashSize; ++index) {
        const auto high = hexDigit(hex[index * 2U]);
        const auto low = hexDigit(hex[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return std::unexpected(invalid(
                std::string{context} + " contains non-hexadecimal cast hash characters."));
        }
        bytes[index] = static_cast<char>((high << 4) | low);
    }
    return bytes;
}

[[nodiscard]] foundation::result::Status validateCastAdd(const CastAddInput& input)
{
    if (input.fid.value == 0U) {
        return std::unexpected(invalid("CAST_ADD requires a non-zero FID."));
    }
    if (input.timestamp == 0U) {
        return std::unexpected(invalid("CAST_ADD requires a non-zero Farcaster timestamp."));
    }
    switch (input.network) {
    case Network::Mainnet:
    case Network::Testnet:
    case Network::Devnet:
        break;
    default:
        return std::unexpected(invalid("CAST_ADD requires a recognized Farcaster network."));
    }
    if (input.text.size() > maximumCastTextBytes) {
        return std::unexpected(invalid("CAST_ADD text must not exceed 320 UTF-8 bytes."));
    }
    if (input.embeds.size() > conservativeMaximumCastEmbeds) {
        return std::unexpected(invalid(
            "CAST_ADD supports at most two embeds for cross-version compatibility."));
    }
    if (input.text.empty() && input.embeds.empty()) {
        return std::unexpected(invalid("CAST_ADD must contain text or at least one embed."));
    }
    if (input.parent) {
        if (input.parent->author.value == 0U) {
            return std::unexpected(invalid(
                "CAST_ADD parent cast requires a non-zero parent-author FID."));
        }
        if (auto hash = decodeCastHash(input.parent->id.value, "CAST_ADD parent cast"); !hash) {
            return std::unexpected(std::move(hash.error()));
        }
    }
    for (const auto& embed : input.embeds) {
        if (embed.kind == domain::cast::EmbedKind::Url) {
            if (embed.value.empty() || embed.value.size() > maximumProtocolUrlBytes) {
                return std::unexpected(invalid(
                    "CAST_ADD URL embeds must contain between 1 and 256 bytes."));
            }
            if (embed.referencedAuthor) {
                return std::unexpected(invalid(
                    "CAST_ADD URL embeds must not carry a referenced-author FID."));
            }
            continue;
        }
        if (!embed.referencedAuthor || embed.referencedAuthor->value == 0U) {
            return std::unexpected(invalid(
                "CAST_ADD cast-reference embeds require a non-zero referenced-author FID."));
        }
        if (auto hash = decodeCastHash(embed.value, "CAST_ADD cast-reference embed"); !hash) {
            return std::unexpected(std::move(hash.error()));
        }
    }
    return {};
}

[[nodiscard]] std::string encodeCastId(
    const domain::identity::Fid fid,
    const std::string_view hashBytes)
{
    std::string castId{};
    appendUint(castId, 1U, fid.value);
    appendBytes(castId, 2U, hashBytes);
    return castId;
}

[[nodiscard]] foundation::result::Result<std::string> encodeCastAddBody(const CastAddInput& input)
{
    std::string body{};
    if (input.parent) {
        auto parentHash = decodeCastHash(input.parent->id.value, "CAST_ADD parent cast");
        if (!parentHash) {
            return std::unexpected(std::move(parentHash.error()));
        }
        appendMessage(body, 3U, encodeCastId(input.parent->author, *parentHash));
    }
    appendBytes(body, 4U, input.text);
    for (const auto& embed : input.embeds) {
        std::string encodedEmbed{};
        if (embed.kind == domain::cast::EmbedKind::Url) {
            appendBytes(encodedEmbed, 1U, embed.value);
        } else {
            auto hash = decodeCastHash(embed.value, "CAST_ADD cast-reference embed");
            if (!hash) {
                return std::unexpected(std::move(hash.error()));
            }
            appendMessage(encodedEmbed, 2U, encodeCastId(*embed.referencedAuthor, *hash));
        }
        appendMessage(body, 6U, encodedEmbed);
    }
    return body;
}

[[nodiscard]] foundation::result::Status validateSimpleMessage(
    const domain::identity::Fid fid,
    const domain::signer::PublicKey& publicKey,
    const std::uint32_t timestamp,
    const Network network,
    const std::string_view context)
{
    if (fid.value == 0U) {
        return std::unexpected(invalid(std::string{context} + " requires a non-zero FID."));
    }
    if (timestamp == 0U) {
        return std::unexpected(invalid(std::string{context} + " requires a non-zero Farcaster timestamp."));
    }
    switch (network) {
    case Network::Mainnet:
    case Network::Testnet:
    case Network::Devnet:
        break;
    default:
        return std::unexpected(invalid(std::string{context} + " requires a recognized Farcaster network."));
    }
    if (auto decoded = decodePublicKey(publicKey); !decoded) {
        return std::unexpected(std::move(decoded.error()));
    }
    return {};
}

[[nodiscard]] foundation::result::Result<std::string> targetCastId(
    const domain::cast::CastReference& target,
    const std::string_view context)
{
    if (target.author.value == 0U) {
        return std::unexpected(invalid(std::string{context} + " requires a non-zero target-author FID."));
    }
    auto hash = decodeCastHash(target.id.value, context);
    if (!hash) {
        return std::unexpected(std::move(hash.error()));
    }
    return encodeCastId(target.author, *hash);
}

[[nodiscard]] foundation::result::Result<std::string> encodeCastRemoveBody(
    const CastRemoveInput& input)
{
    if (input.target.author.value != input.fid.value) {
        return std::unexpected(invalid("CAST_REMOVE may target only a cast owned by the authenticated FID."));
    }
    auto hash = decodeCastHash(input.target.id.value, "CAST_REMOVE target");
    if (!hash) {
        return std::unexpected(std::move(hash.error()));
    }
    std::string body{};
    appendBytes(body, 1U, *hash);
    return body;
}

[[nodiscard]] foundation::result::Result<std::string> encodeReactionBody(
    const ReactionInput& input)
{
    auto castId = targetCastId(input.target, "REACTION target cast");
    if (!castId) {
        return std::unexpected(std::move(castId.error()));
    }
    std::string body{};
    appendUint(body, 1U, input.reaction == domain::write::ReactionKind::Like
        ? reactionTypeLike : reactionTypeRecast);
    appendMessage(body, 2U, *castId);
    return body;
}

[[nodiscard]] foundation::result::Result<std::string> encodeFollowBody(const FollowInput& input)
{
    if (input.target.value == 0U || input.target.value == input.fid.value) {
        return std::unexpected(invalid("LINK follow requires a non-zero target FID different from the author."));
    }
    std::string body{};
    appendBytes(body, 1U, "follow");
    appendUint(body, 3U, input.target.value);
    return body;
}

[[nodiscard]] foundation::result::Status validateKeyAdd(const KeyAddInput& input)
{
    if (auto status = validateCommon(input.fid, input.timestamp, input.deadline, input.network); !status) {
        return status;
    }
    if (input.scopes.empty() || input.scopes.size() > 16U) {
        return std::unexpected(invalid("KEY_ADD requires between 1 and 16 explicit message scopes."));
    }
    if (input.ttlSeconds == 0U || input.ttlSeconds > domain::signer::MaxGaslessTtlSeconds) {
        return std::unexpected(invalid("KEY_ADD TTL must be positive and no greater than 90 days."));
    }
    if (input.nonce == 0U) {
        return std::unexpected(invalid("KEY_ADD nonce must be non-zero."));
    }
    if (input.custodySignature.size() != custodySignatureSize) {
        return std::unexpected(invalid("KEY_ADD custody authorization must be a 65-byte EIP-712 signature."));
    }
    if (input.signedKeyRequestMetadata.empty() || input.signedKeyRequestMetadata.size() % 32U != 0U) {
        return std::unexpected(invalid("KEY_ADD SignedKeyRequest metadata must be non-empty ABI-encoded 32-byte words."));
    }

    std::set<std::int32_t> uniqueScopes{};
    for (const auto scope : input.scopes) {
        if (!domain::signer::isAdmissibleScope(scope)) {
            return std::unexpected(invalid("KEY_ADD contains a scope Snapchain does not admit."));
        }
        if (!uniqueScopes.insert(static_cast<std::int32_t>(scope)).second) {
            return std::unexpected(invalid("KEY_ADD scope list contains a duplicate message type."));
        }
    }
    return {};
}

[[nodiscard]] std::string encodePackedScopes(const std::vector<domain::signer::Scope>& scopes)
{
    std::string packed{};
    for (const auto scope : scopes) {
        appendVarint(packed, static_cast<std::uint32_t>(scope));
    }
    return packed;
}

[[nodiscard]] std::string encodeKeyAddBody(
    const KeyAddInput& input,
    const std::string_view publicKeyBytes)
{
    std::string body{};
    appendBytes(body, 1U, publicKeyBytes);
    appendUint(body, 2U, keyTypeEd25519);
    appendBytes(body, 3U, input.custodySignature);
    appendUint(body, 4U, input.deadline);
    appendUint(body, 5U, input.nonce);
    appendBytes(body, 6U, input.signedKeyRequestMetadata);
    appendUint(body, 7U, metadataTypeSignedKeyRequest);
    appendBytes(body, 9U, encodePackedScopes(input.scopes));
    appendUint(body, 10U, input.ttlSeconds);
    return body;
}

[[nodiscard]] std::string encodeKeyRemoveSelfRevokeBody(
    const KeyRemoveSelfRevokeInput& input,
    const std::string_view publicKeyBytes)
{
    std::string body{};
    appendBytes(body, 1U, publicKeyBytes);
    appendUint(body, 3U, keyRemoveSignatureTypeSelfRevoke);
    appendUint(body, 4U, input.deadline);
    appendUint(body, 5U, input.nonce);
    return body;
}

[[nodiscard]] UnsignedMessage encodeData(
    const domain::identity::Fid fid,
    const domain::signer::PublicKey& publicKey,
    const std::uint32_t messageType,
    const std::uint32_t timestamp,
    const Network network,
    const std::uint32_t bodyField,
    const std::string_view bodyBytes)
{
    std::string data{};
    appendUint(data, 1U, messageType);
    appendUint(data, 2U, fid.value);
    appendUint(data, 3U, timestamp);
    appendUint(data, 4U, static_cast<std::uint32_t>(network));
    appendMessage(data, bodyField, bodyBytes);

    const auto bytes = std::as_bytes(std::span{data.data(), data.size()});
    const auto digest = protocol::hash::blake3Digest20(bytes);
    std::string hashBytes(digest.size(), '\0');
    std::transform(digest.begin(), digest.end(), hashBytes.begin(), [](const std::byte value) {
        return static_cast<char>(std::to_integer<unsigned char>(value));
    });

    return UnsignedMessage{
        .dataBytes = std::move(data),
        .hashBytes = std::move(hashBytes),
        .signer = publicKey,
    };
}

}

foundation::result::Result<std::uint32_t> toFarcasterTimestamp(
    const foundation::time::Timestamp timestamp)
{
    const auto unixSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    if (unixSeconds < 0 || static_cast<std::uint64_t>(unixSeconds) < farcasterEpochUnixSeconds) {
        return std::unexpected(invalid("Timestamp precedes the Farcaster epoch."));
    }
    const auto farcasterSeconds = static_cast<std::uint64_t>(unixSeconds) - farcasterEpochUnixSeconds;
    if (farcasterSeconds > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(invalid("Timestamp exceeds the uint32 Farcaster protocol range."));
    }
    return static_cast<std::uint32_t>(farcasterSeconds);
}

foundation::time::Timestamp fromFarcasterTimestamp(const std::uint32_t timestamp) noexcept
{
    const std::uint64_t unixSeconds = farcasterEpochUnixSeconds +
        static_cast<std::uint64_t>(timestamp);
    return foundation::time::Timestamp{
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{unixSeconds})};
}

foundation::result::Result<UnsignedMessage> encodeCastAdd(const CastAddInput& input)
{
    if (auto status = validateCastAdd(input); !status) {
        return std::unexpected(std::move(status.error()));
    }
    if (auto publicKeyBytes = decodePublicKey(input.publicKey); !publicKeyBytes) {
        return std::unexpected(std::move(publicKeyBytes.error()));
    }
    auto body = encodeCastAddBody(input);
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    return encodeData(
        input.fid,
        input.publicKey,
        messageTypeCastAdd,
        input.timestamp,
        input.network,
        5U,
        *body);
}

foundation::result::Result<UnsignedMessage> encodeCastRemove(const CastRemoveInput& input)
{
    if (auto status = validateSimpleMessage(input.fid, input.publicKey, input.timestamp, input.network, "CAST_REMOVE"); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto body = encodeCastRemoveBody(input);
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    return encodeData(
        input.fid,
        input.publicKey,
        messageTypeCastRemove,
        input.timestamp,
        input.network,
        6U,
        *body);
}

foundation::result::Result<UnsignedMessage> encodeReaction(const ReactionInput& input)
{
    if (auto status = validateSimpleMessage(input.fid, input.publicKey, input.timestamp, input.network, "REACTION"); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto body = encodeReactionBody(input);
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    return encodeData(
        input.fid,
        input.publicKey,
        input.isActive ? messageTypeReactionAdd : messageTypeReactionRemove,
        input.timestamp,
        input.network,
        7U,
        *body);
}

foundation::result::Result<UnsignedMessage> encodeFollow(const FollowInput& input)
{
    if (auto status = validateSimpleMessage(input.fid, input.publicKey, input.timestamp, input.network, "LINK"); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto body = encodeFollowBody(input);
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    return encodeData(
        input.fid,
        input.publicKey,
        input.isFollowing ? messageTypeLinkAdd : messageTypeLinkRemove,
        input.timestamp,
        input.network,
        14U,
        *body);
}

foundation::result::Result<UnsignedMessage> encodeKeyAdd(const KeyAddInput& input)
{
    if (auto status = validateKeyAdd(input); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto publicKeyBytes = decodePublicKey(input.publicKey);
    if (!publicKeyBytes) {
        return std::unexpected(std::move(publicKeyBytes.error()));
    }
    const auto body = encodeKeyAddBody(input, *publicKeyBytes);
    return encodeData(
        input.fid,
        input.publicKey,
        messageTypeKeyAdd,
        input.timestamp,
        input.network,
        19U,
        body);
}

foundation::result::Result<UnsignedMessage> encodeKeyRemoveSelfRevoke(
    const KeyRemoveSelfRevokeInput& input)
{
    if (auto status = validateCommon(input.fid, input.timestamp, input.deadline, input.network); !status) {
        return std::unexpected(std::move(status.error()));
    }
    if (input.nonce == 0U) {
        return std::unexpected(invalid("KEY_REMOVE self-revocation nonce must be non-zero."));
    }
    auto publicKeyBytes = decodePublicKey(input.publicKey);
    if (!publicKeyBytes) {
        return std::unexpected(std::move(publicKeyBytes.error()));
    }
    const auto body = encodeKeyRemoveSelfRevokeBody(input, *publicKeyBytes);
    return encodeData(
        input.fid,
        input.publicKey,
        messageTypeKeyRemove,
        input.timestamp,
        input.network,
        20U,
        body);
}

foundation::result::Result<std::string> seal(
    const UnsignedMessage& message,
    const domain::signer::Signature& signature)
{
    if (message.dataBytes.empty() || message.hashBytes.size() != 20U) {
        return std::unexpected(protocolFailure("Cannot seal an incomplete canonical Farcaster message."));
    }
    if (signature.bytes.size() != signatureSize) {
        return std::unexpected(invalid("Farcaster Ed25519 envelope signature must contain exactly 64 bytes."));
    }
    auto publicKeyBytes = decodePublicKey(message.signer);
    if (!publicKeyBytes) {
        return std::unexpected(std::move(publicKeyBytes.error()));
    }

    std::string envelope{};
    appendBytes(envelope, 2U, message.hashBytes);
    appendUint(envelope, 3U, hashSchemeBlake3);
    appendBytes(envelope, 4U, signature.bytes);
    appendUint(envelope, 5U, signatureSchemeEd25519);
    appendBytes(envelope, 6U, *publicKeyBytes);
    appendBytes(envelope, 7U, message.dataBytes);
    return envelope;
}

}
