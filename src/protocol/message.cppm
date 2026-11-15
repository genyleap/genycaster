/*!
 * @file        src/protocol/message.cppm
 * @brief       Canonical Farcaster message construction.
 *
 * @details
 * Defines protocol message variants, network fields and serialization
 * inputs for signed cast and social operations.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.protocol.message;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.foundation.utf8;

export namespace farcaster::protocol::message {

/** @brief Farcaster protocol network discriminants used inside MessageData. */
enum class Network : std::uint32_t {
    Mainnet = 1U,
    Testnet = 2U,
    Devnet = 3U,
};

/** @brief CAST_ADD text class used by current Farcaster protocol validation. */
enum class CastType : std::uint32_t {
    Cast = 0U,
    LongCast = 1U,
    TenKCast = 2U,
};

/** @brief Exact CAST_ADD payload before envelope signing, with optional parent CastId. */
struct CastAddInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    std::string text{};
    std::vector<domain::cast::Embed> embeds{};
    std::vector<domain::identity::Fid> mentions{};
    std::vector<std::uint32_t> mentionPositions{};
    std::optional<domain::cast::CastReference> parent{};
    std::optional<std::string> parentUrl{};
    CastType type{CastType::Cast};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
};

/** @brief Exact CAST_REMOVE payload before envelope signing. */
struct CastRemoveInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    domain::cast::CastReference target{};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
};

/** @brief Exact REACTION_ADD/REACTION_REMOVE payload before envelope signing. */
struct ReactionInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    domain::cast::CastReference target{};
    domain::write::ReactionKind reaction{domain::write::ReactionKind::Like};
    bool isActive{true};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
};

/** @brief Exact LINK_ADD/LINK_REMOVE follow payload before envelope signing. */
struct FollowInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    domain::identity::Fid target{};
    std::string linkType{"follow"};
    bool isFollowing{true};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
};

/** @brief Signed UserDataAdd for a single profile field (never a username proof). */
struct UserDataInput final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    domain::write::ProfileField field{domain::write::ProfileField::DisplayName};
    std::string value{};
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
 * A HyperSnap native registration message. Every field is publicly
 * distributable; the custody key never enters this data structure.
 * The caller must verify network activation, consent, and PoW before encoding.
 */
struct NativeOnboardInput final {
    std::string custodyAddress{}; // 0x-prefixed 20 bytes
    std::string custodySignature{}; // 0x-prefixed 65 bytes, EIP-712
    std::uint64_t anchorHeight{};
    std::string anchorHash{}; // 0x-prefixed 32 bytes
    std::string powNonce{}; // 0x-prefixed 8 bytes
    std::uint32_t difficultyBits{};
    std::uint32_t timestamp{};
    Network network{Network::Mainnet};
};

/** Exact protobuf NativeOnboard envelope accepted by HyperSnap /v1/submitMessage.
 * Hash scheme is BLAKE3, signature scheme NONE and FID zero. */
[[nodiscard]] foundation::result::Result<std::string> encodeNativeOnboard(
    const NativeOnboardInput& input);

/** @brief Canonical protobuf MessageData plus its BLAKE3-20 digest. */
struct UnsignedMessage final {
    std::string dataBytes{};
    std::string hashBytes{};
    domain::signer::PublicKey signer{};
};

/** @brief Converts a Unix wall-clock timestamp to uint32 Farcaster epoch seconds. */
[[nodiscard]] foundation::result::Result<std::uint32_t> toFarcasterTimestamp(
    foundation::time::Timestamp timestamp);

/** @brief Converts uint32 Farcaster-epoch seconds to a Unix wall-clock timestamp. */
[[nodiscard]] foundation::time::Timestamp fromFarcasterTimestamp(
    std::uint32_t timestamp) noexcept;

/** @brief Builds canonical protobuf MessageData for a root CAST_ADD or reply. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeCastAdd(
    const CastAddInput& input);

/** @brief Builds canonical protobuf MessageData for CAST_REMOVE. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeCastRemove(
    const CastRemoveInput& input);

/** @brief Builds canonical protobuf MessageData for REACTION_ADD/REACTION_REMOVE. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeReaction(
    const ReactionInput& input);

/** @brief Builds canonical protobuf MessageData for LINK_ADD/LINK_REMOVE follow. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeFollow(
    const FollowInput& input);

/** @brief Builds the standard UserDataAdd MessageData for a profile field. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeUserDataAdd(
    const UserDataInput& input);

/** @brief Builds canonical protobuf MessageData for a custody-authorized KEY_ADD. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeKeyAdd(
    const KeyAddInput& input);

/** @brief Builds canonical protobuf MessageData for KEY_REMOVE signature_type=2. */
[[nodiscard]] foundation::result::Result<UnsignedMessage> encodeKeyRemoveSelfRevoke(
    const KeyRemoveSelfRevokeInput& input);

/** @brief Encodes the final protobuf Message envelope with an Ed25519 hash signature. */
[[nodiscard]] foundation::result::Result<std::string> seal(
    const UnsignedMessage& message,
    const domain::signer::Signature& signature);

}
