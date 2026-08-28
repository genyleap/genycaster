module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.protocol.message;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.result;
import farcaster.foundation.time;

export namespace farcaster::protocol::message {

/** @brief Farcaster protocol network discriminants used inside MessageData. */
enum class Network : std::uint32_t {
    Mainnet = 1U,
    Testnet = 2U,
    Devnet = 3U,
};


/** @brief Exact CAST_ADD payload before envelope signing, with optional parent CastId. */
struct CastAddInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    std::string text{};
    std::vector<domain::cast::Embed> embeds{};
    std::optional<domain::cast::CastReference> parent{};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
};

/** @brief Exact KEY_ADD payload fields before protobuf envelope signing. */
struct KeyAddInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    std::vector<domain::signer::Scope> scopes{};
    std::uint32_t ttlSeconds{};
    std::uint32_t nonce{};
    std::uint32_t deadline{};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
    std::string custodySignature{};
    std::string signedKeyRequestMetadata{};
};

/** @brief Exact self-revocation KEY_REMOVE payload fields before envelope signing. */
struct KeyRemoveSelfRevokeInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    std::uint32_t nonce{};
    std::uint32_t deadline{};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
};

/**
 * @brief Canonical protobuf MessageData plus its BLAKE3-20 digest.
 *
 * The digest, not MessageData itself, is the Ed25519 envelope signing input in
 * the current Snapchain validation contract.
 */
struct UnsignedMessage final {
    std::string dataBytes{};
    std::string hashBytes{};
    domain::signer::PublicKey signer{};
};

/** @brief Converts a Unix wall-clock timestamp to uint32 Farcaster epoch seconds. */
[[nodiscard]] foundation::result::Result<std::uint32_t> toFarcasterTimestamp(
    foundation::time::Timestamp timestamp);

/** @brief Builds canonical protobuf MessageData for a root CAST_ADD or reply. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeCastAdd(
    const CastAddInput& input);

/** @brief Builds canonical protobuf MessageData for a custody-authorized KEY_ADD. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeKeyAdd(
    const KeyAddInput& input);

/** @brief Builds canonical protobuf MessageData for KEY_REMOVE signature_type=2. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeKeyRemoveSelfRevoke(
    const KeyRemoveSelfRevokeInput& input);

/**
 * @brief Encodes the final protobuf Message envelope using an Ed25519 signature over hashBytes.
 * @param message Canonical unsigned message returned by an encode function.
 * @param signature Raw 64-byte Ed25519 signature over message.hashBytes.
 */
[[nodiscard]] foundation::result::Result<std::string> seal(
    const UnsignedMessage& message,
    const domain::signer::Signature& signature);

}
