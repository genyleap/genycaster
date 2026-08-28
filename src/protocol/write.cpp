module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>

module farcaster.protocol.write;

import farcaster.capability.write;
import farcaster.domain.cast;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
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
            .timestamp = *timestamp,
            .network = m_network,
        });
        if (!message) {
            return std::unexpected(std::move(message.error()));
        }
        if (message->hashBytes.size() != 20U) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::ProtocolFailure,
                "Canonical CAST_ADD hash must contain exactly 20 bytes."));
        }
        const std::string castId = hexHash(message->hashBytes);
        return domain::write::PreparedOperation{
            .kind = domain::write::OperationKind::PublishCast,
            .idempotencyKey = domain::write::IdempotencyKey{castId},
            .optimisticCast = domain::cast::Cast{
                .id = domain::cast::CastId{castId},
                .author = intent.author,
                .text = intent.text,
                .embeds = intent.embeds,
                .createdAt = intent.createdAt,
            },
            .signerPublicKey = signerPublicKey,
            .dataBytes = std::move(message->dataBytes),
            .hashBytes = std::move(message->hashBytes),
            .preparedAt = intent.createdAt,
        };
    }

    [[nodiscard]] foundation::result::Result<domain::write::SignedOperation> seal(
        const domain::write::PreparedOperation& operation,
        const domain::signer::Signature& signature) override
    {
        if (operation.kind != domain::write::OperationKind::PublishCast) {
            return std::unexpected(invalid("Canonical write codec received an unsupported operation kind."));
        }
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
            .optimisticCast = operation.optimisticCast,
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
