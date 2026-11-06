/*!
 * @file        src/domain/signer.cppm
 * @brief       Signer identity, scope and registration values.
 *
 * @details
 * Defines canonical public signing metadata, signatures and authorization
 * state without exposing secret key material.
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

export module farcaster.domain.signer;

import farcaster.domain.identity;
import farcaster.foundation.time;

export namespace farcaster::domain::signer {

/** @brief Stable opaque identifier for locally-held protocol signing authority. */
struct SignerId final {
    std::string value{};
};

/** @brief Public Ed25519 key encoded as lowercase hexadecimal without a prefix. */
struct PublicKey final {
    std::string hex{};
};

/** @brief Canonical Ed25519 signature bytes represented as an owned byte string. */
struct Signature final {
    std::string bytes{};
};

/**
 * @brief Protocol message types that a gasless signer may be authorized to sign.
 *
 * Values intentionally match Snapchain MessageType discriminants. KEY_ADD,
 * KEY_REMOVE, and currently non-admissible channel operations are absent so a
 * future protocol enum cannot silently widen client authority.
 */
enum class Scope : std::int32_t {
    CastAdd = 1,
    CastRemove = 2,
    ReactionAdd = 3,
    ReactionRemove = 4,
    LinkAdd = 5,
    LinkRemove = 6,
    VerificationAddEthAddress = 7,
    VerificationRemove = 8,
    UserDataAdd = 11,
    UsernameProof = 12,
    FrameAction = 13,
    LinkCompactState = 14,
    LendStorage = 15,
};

/** @brief Origin of a signer registration visible to Farcaster. */
enum class RegistrationKind : std::uint8_t {
    Gasless,
    LegacyOnchain,
};

/** @brief Network-observed lifecycle state for a protocol signer. */
enum class RegistrationState : std::uint8_t {
    Missing,
    Active,
    Revoked,
    Expired,
};

/**
 * @brief Non-secret local metadata for a signer key held by a secure vault.
 *
 * This value may be persisted in SQLite. It never contains private-key bytes.
 */
struct LocalSigner final {
    domain::identity::Fid fid{};
    SignerId id{};
    PublicKey publicKey{};
    foundation::time::Timestamp createdAt{};
};

/**
 * @brief Network-verified signer registration state safe for Application/UI.
 *
 * Gasless signers carry explicit scopes and a positive sliding TTL. Legacy
 * on-chain signers predate those fields and are treated as grandfathered full
 * authority by the protocol.
 */
struct Registration final {
    domain::identity::Fid fid{};
    PublicKey publicKey{};
    RegistrationKind kind{RegistrationKind::Gasless};
    RegistrationState state{RegistrationState::Missing};
    std::vector<Scope> scopes{};
    std::uint32_t ttlSeconds{};
    std::optional<foundation::time::Timestamp> lastUsedAt{};
    std::optional<domain::identity::Fid> requestFid{};
    foundation::time::Timestamp verifiedAt{};
};

/** @brief Current nonce counters required for replay-safe KEY_ADD/KEY_REMOVE. */
struct NonceState final {
    std::uint32_t currentUserNonce{};
    std::optional<std::uint32_t> currentRequesterNonce{};
};

/** @brief Persistable non-secret signer checkpoint for restart recovery. */
struct SignerRecord final {
    LocalSigner local{};
    std::optional<Registration> lastVerifiedRegistration{};
};

/** @brief Maximum sliding TTL currently accepted by Snapchain: 90 days. */
inline constexpr std::uint32_t MaxGaslessTtlSeconds = 90U * 24U * 60U * 60U;

/** @brief Product default: 30-day sliding signer TTL. */
inline constexpr std::uint32_t DefaultGaslessTtlSeconds = 30U * 24U * 60U * 60U;

/** @brief Returns the least-privilege scope set needed by Genycaster Phase 5C-5E writes. */
[[nodiscard]] std::vector<Scope> standardSocialScopes();

/** @brief Returns true only for an explicitly supported Snapchain gasless scope. */
[[nodiscard]] bool isAdmissibleScope(Scope scope) noexcept;

/** @brief Returns true when a registration authorizes one requested message scope. */
[[nodiscard]] bool permits(
    const Registration& registration,
    Scope requiredScope,
    foundation::time::Timestamp now) noexcept;

/** @brief Returns true when the gasless signer's sliding TTL is locally known to be expired. */
[[nodiscard]] bool isExpired(
    const Registration& registration,
    foundation::time::Timestamp now) noexcept;

}
