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

/** @brief Protocol-native write operations currently implemented by the durable outbox. */
enum class OperationKind : std::uint8_t {
    PublishCast,
};

/** @brief Stable client-owned idempotency key derived from canonical protocol identity. */
struct IdempotencyKey final {
    std::string value{};
};

/**
 * @brief Canonical user intent for publishing a root cast.
 *
 * The author and timestamp are assigned by Application from the authenticated
 * session and wall clock. Presentation code supplies only user-authored content.
 */
struct PublishCastIntent final {
    domain::identity::Fid author{};
    std::string text{};
    std::vector<domain::cast::Embed> embeds{};
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
    domain::cast::Cast optimisticCast{};
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
    domain::cast::Cast optimisticCast{};
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
