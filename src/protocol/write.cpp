module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

module farcaster.protocol.write;

import farcaster.capability.write;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.protocol.message;

namespace farcaster::protocol::write {
namespace {

[[nodiscard]] foundation::error::Error invalid(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::InvalidArgument,
        std::move(message));
}

[[nodiscard]] std::string hexHash(const std::string& bytes)
{
    constexpr std::array<char, 16U> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string result{"0x"};
    result.reserve(2U + bytes.size() * 2U);
    for (const char raw : bytes) {
        const auto value = static_cast<unsigned char>(raw);
        result.push_back(digits[(value >> 4U) & 0x0FU]);
        result.push_back(digits[value & 0x0FU]);
    }
    return result;
}

[[nodiscard]] foundation::result::Result<domain::write::PreparedOperation> preparedFrom(
    const domain::write::OperationKind kind,
    const domain::identity::Fid author,
    domain::write::OperationTarget target,
    std::optional<domain::cast::Cast> optimisticCast,
    const domain::signer::PublicKey& signerPublicKey,
    protocol::message::UnsignedMessage message,
    const foundation::time::Timestamp preparedAt)
{
    if (message.hashBytes.size() != 20U) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::ProtocolFailure,
            "Canonical Farcaster write hash must contain exactly 20 bytes."));
    }
    return domain::write::PreparedOperation{
        .kind = kind,
        .idempotencyKey = domain::write::IdempotencyKey{hexHash(message.hashBytes)},
        .author = author,
        .target = std::move(target),
        .optimisticCast = std::move(optimisticCast),
        .signerPublicKey = signerPublicKey,
        .dataBytes = std::move(message.dataBytes),
        .hashBytes = std::move(message.hashBytes),
        .preparedAt = preparedAt,
    };
}

class CanonicalWriteCodec final : public capability::write::ICanonicalWriteCodec {
public:
    explicit CanonicalWriteCodec(const protocol::message::Network network)
        : m_network(network)
    {
    }

    [[nodiscard]] foundation::result::Result<domain::write::PreparedOperation>
    preparePublishCast(
        const domain::write::PublishCastIntent& intent,
        const domain::signer::PublicKey& signerPublicKey) override
    {
        if (intent.author.value == 0U) {
            return std::unexpected(invalid("PublishCast intent requires a non-zero authenticated author FID."));
        }
        auto timestamp = protocol::message::toFarcasterTimestamp(intent.createdAt);
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }
        auto message = protocol::message::encodeCastAdd(protocol::message::CastAddInput{
            .fid = intent.author,
            .publicKey = signerPublicKey,
            .text = intent.text,
            .embeds = intent.embeds,
            .mentions = intent.mentions,
            .mentionPositions = intent.mentionPositions,
            .parent = intent.parent,
            .parentUrl = intent.parentUrl,
            .type = intent.text.size() <= 320U
                ? protocol::message::CastType::Cast
                : protocol::message::CastType::LongCast,
            .timestamp = *timestamp,
            .network = m_network,
        });
        if (!message) {
            return std::unexpected(std::move(message.error()));
        }
        const std::string castId = hexHash(message->hashBytes);
        auto optimisticCast = domain::cast::Cast{
            .id = domain::cast::CastId{castId},
            .author = intent.author,
            .text = intent.text,
            .parent = intent.parent
                ? std::optional<domain::cast::CastId>{intent.parent->id}
                : std::nullopt,
            .parentUrl = intent.parentUrl,
            .embeds = intent.embeds,
            .createdAt = intent.createdAt,
        };
        return preparedFrom(
            domain::write::OperationKind::PublishCast,
            intent.author,
            {},
            std::move(optimisticCast),
            signerPublicKey,
            std::move(*message),
            intent.createdAt);
    }

    [[nodiscard]] foundation::result::Result<domain::write::PreparedOperation>
    prepareRemoveCast(
        const domain::write::RemoveCastIntent& intent,
        const domain::signer::PublicKey& signerPublicKey) override
    {
        auto timestamp = protocol::message::toFarcasterTimestamp(intent.createdAt);
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }
        auto message = protocol::message::encodeCastRemove(protocol::message::CastRemoveInput{
            .fid = intent.author,
            .publicKey = signerPublicKey,
            .target = intent.target,
            .timestamp = *timestamp,
            .network = m_network,
        });
        if (!message) {
            return std::unexpected(std::move(message.error()));
        }
        return preparedFrom(
            domain::write::OperationKind::RemoveCast,
            intent.author,
            domain::write::OperationTarget{.cast = intent.target},
            std::nullopt,
            signerPublicKey,
            std::move(*message),
            intent.createdAt);
    }

    [[nodiscard]] foundation::result::Result<domain::write::PreparedOperation>
    prepareReaction(
        const domain::write::ReactionIntent& intent,
        const domain::signer::PublicKey& signerPublicKey) override
    {
        auto timestamp = protocol::message::toFarcasterTimestamp(intent.createdAt);
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }
        auto message = protocol::message::encodeReaction(protocol::message::ReactionInput{
            .fid = intent.author,
            .publicKey = signerPublicKey,
            .target = intent.target,
            .reaction = intent.reaction,
            .isActive = intent.isActive,
            .timestamp = *timestamp,
            .network = m_network,
        });
        if (!message) {
            return std::unexpected(std::move(message.error()));
        }
        const auto kind = intent.reaction == domain::write::ReactionKind::Like
            ? (intent.isActive ? domain::write::OperationKind::Like : domain::write::OperationKind::Unlike)
            : (intent.isActive ? domain::write::OperationKind::Recast : domain::write::OperationKind::UndoRecast);
        return preparedFrom(
            kind,
            intent.author,
            domain::write::OperationTarget{.cast = intent.target},
            std::nullopt,
            signerPublicKey,
            std::move(*message),
            intent.createdAt);
    }

    [[nodiscard]] foundation::result::Result<domain::write::PreparedOperation>
    prepareFollow(
        const domain::write::FollowIntent& intent,
        const domain::signer::PublicKey& signerPublicKey) override
    {
        auto timestamp = protocol::message::toFarcasterTimestamp(intent.createdAt);
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }
        auto message = protocol::message::encodeFollow(protocol::message::FollowInput{
            .fid = intent.author,
            .publicKey = signerPublicKey,
            .target = intent.target,
            .linkType = intent.kind == domain::write::LinkKind::Block ? "block" : "follow",
            .isFollowing = intent.isFollowing,
            .timestamp = *timestamp,
            .network = m_network,
        });
        if (!message) {
            return std::unexpected(std::move(message.error()));
        }
        return preparedFrom(
            intent.kind == domain::write::LinkKind::Block
                ? (intent.isFollowing ? domain::write::OperationKind::Block : domain::write::OperationKind::Unblock)
                : (intent.isFollowing ? domain::write::OperationKind::Follow : domain::write::OperationKind::Unfollow),
            intent.author,
            domain::write::OperationTarget{.fid = intent.target},
            std::nullopt,
            signerPublicKey,
            std::move(*message),
            intent.createdAt);
    }

    [[nodiscard]] foundation::result::Result<domain::write::PreparedOperation>
    prepareProfileField(
        const domain::write::ProfileFieldIntent& intent,
        const domain::signer::PublicKey& signerPublicKey) override
    {
        auto timestamp = protocol::message::toFarcasterTimestamp(intent.createdAt);
        if (!timestamp) {
            return std::unexpected(std::move(timestamp.error()));
        }
        auto message = protocol::message::encodeUserDataAdd(protocol::message::UserDataInput{
            .fid = intent.author,
            .publicKey = signerPublicKey,
            .field = intent.field,
            .value = intent.value,
            .timestamp = *timestamp,
            .network = m_network,
        });
        if (!message) {
            return std::unexpected(std::move(message.error()));
        }
        return preparedFrom(domain::write::OperationKind::UpdateProfile,
            intent.author, {}, std::nullopt, signerPublicKey, std::move(*message),
            intent.createdAt);
    }

    [[nodiscard]] foundation::result::Result<domain::write::SignedOperation> seal(
        const domain::write::PreparedOperation& operation,
        const domain::signer::Signature& signature) override
    {
        auto envelope = protocol::message::seal(
            protocol::message::UnsignedMessage{
                .dataBytes = operation.dataBytes,
                .hashBytes = operation.hashBytes,
                .signer = operation.signerPublicKey,
            },
            signature);
        if (!envelope) {
            return std::unexpected(std::move(envelope.error()));
        }
        return domain::write::SignedOperation{
            .kind = operation.kind,
            .idempotencyKey = operation.idempotencyKey,
            .author = operation.author,
            .target = operation.target,
            .optimisticCast = operation.optimisticCast,
            .dependsOn = operation.dependsOn,
            .signerPublicKey = operation.signerPublicKey,
            .messageBytes = std::move(*envelope),
            .signedAt = operation.preparedAt,
        };
    }

private:
    protocol::message::Network m_network{protocol::message::Network::Mainnet};
};

} // namespace

std::unique_ptr<capability::write::ICanonicalWriteCodec> makeCanonicalWriteCodec()
{
    return std::make_unique<CanonicalWriteCodec>(protocol::message::Network::Mainnet);
}

} // namespace farcaster::protocol::write
