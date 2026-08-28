module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.domain.write;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.time;

export namespace farcaster::domain::write {

/** @brief Protocol-native mutations handled by the shared durable write pipeline. */
enum class OperationKind : std::uint8_t {
    PublishCast,
    RemoveCast,
    Like,
    Unlike,
    Recast,
    UndoRecast,
    Follow,
    Unfollow,
};

/** @brief Stable client-owned idempotency key derived from the canonical message hash. */
struct IdempotencyKey final {
    std::string value{};
};

/** @brief Canonical target of a social protocol mutation. */
struct OperationTarget final {
    std::optional<domain::cast::CastReference> cast{};
    std::optional<domain::identity::Fid> fid{};
};

/** @brief Returns the exact signer scope required by one operation kind. */
[[nodiscard]] domain::signer::Scope requiredScope(const OperationKind kind) noexcept
{
    using domain::signer::Scope;
    switch (kind) {
    case OperationKind::PublishCast: return Scope::CastAdd;
    case OperationKind::RemoveCast: return Scope::CastRemove;
    case OperationKind::Like:
    case OperationKind::Recast: return Scope::ReactionAdd;
    case OperationKind::Unlike:
    case OperationKind::UndoRecast: return Scope::ReactionRemove;
    case OperationKind::Follow: return Scope::LinkAdd;
    case OperationKind::Unfollow: return Scope::LinkRemove;
    }
    return Scope::CastAdd;
}

/**
 * @brief Canonical user intent for publishing a root cast or reply.
 *
 * The author and timestamp are assigned by Application from the authenticated
 * session and wall clock. An optional parent is a protocol-native CastReference.
 */
struct PublishCastIntent final {
    domain::identity::Fid author{};
    std::string text{};
    std::vector<domain::cast::Embed> embeds{};
    std::optional<domain::cast::CastReference> parent{};
    foundation::time::Timestamp createdAt{};
};

/** @brief Canonical intent for deleting a cast owned by the authenticated FID. */
struct RemoveCastIntent final {
    domain::identity::Fid author{};
    domain::cast::CastReference target{};
    foundation::time::Timestamp createdAt{};
};

/** @brief Reaction type whose add/remove form is selected by the application request. */
enum class ReactionKind : std::uint8_t {
    Like,
    Recast,
};

/** @brief Canonical intent for adding or removing a Like/Recast reaction. */
struct ReactionIntent final {
    domain::identity::Fid author{};
    domain::cast::CastReference target{};
    ReactionKind reaction{ReactionKind::Like};
    bool isActive{true};
    foundation::time::Timestamp createdAt{};
};

/** @brief Canonical intent for adding or removing a follow link. */
struct FollowIntent final {
    domain::identity::Fid author{};
    domain::identity::Fid target{};
    bool isFollowing{true};
    foundation::time::Timestamp createdAt{};
};

/**
 * @brief Canonical unsigned protocol operation ready for isolated Ed25519 signing.
 *
 * hashBytes is the exact 20-byte BLAKE3 digest to sign. dataBytes is the
 * canonical Farcaster MessageData protobuf. Neither contains private key data.
 */
struct PreparedOperation final {
    OperationKind kind{OperationKind::PublishCast};
    IdempotencyKey idempotencyKey{};
    domain::identity::Fid author{};
    OperationTarget target{};
    std::optional<domain::cast::Cast> optimisticCast{};
    domain::signer::PublicKey signerPublicKey{};
    std::string dataBytes{};
    std::string hashBytes{};
    foundation::time::Timestamp preparedAt{};
};

/**
 * @brief Fully signed canonical protocol write retained by the durable outbox.
 *
 * messageBytes is the complete protobuf Message envelope. It contains only
 * public protocol material/signatures and never private key or custody secrets.
 */
struct SignedOperation final {
    OperationKind kind{OperationKind::PublishCast};
    IdempotencyKey idempotencyKey{};
    domain::identity::Fid author{};
    OperationTarget target{};
    std::optional<domain::cast::Cast> optimisticCast{};
    domain::signer::PublicKey signerPublicKey{};
    std::string messageBytes{};
    foundation::time::Timestamp signedAt{};
};

/** @brief Durable lifecycle of one protocol write intent. */
enum class OutboxStatus : std::uint8_t {
    Pending,
    Confirmed,
    Failed,
};

/** @brief Stable application-owned classification of a durable write failure. */
enum class FailureKind : std::uint8_t {
    Authorization,
    RateLimited,
    Unavailable,
    Conflict,
    ProtocolRejected,
    Unsupported,
    Internal,
};

/** @brief Persistable failure detail that contains no provider-specific error enum. */
struct OutboxFailure final {
    FailureKind kind{FailureKind::Internal};
    std::string message{};
};

/** @brief Durable outbox record plus retry/reconciliation metadata. */
struct OutboxRecord final {
    SignedOperation operation{};
    OutboxStatus status{OutboxStatus::Pending};
    std::uint32_t attemptCount{};
    foundation::time::Timestamp enqueuedAt{};
    std::optional<foundation::time::Timestamp> nextAttemptAt{};
    std::optional<OutboxFailure> lastFailure{};
    std::optional<foundation::time::Timestamp> confirmedAt{};
};

} // namespace farcaster::domain::write
