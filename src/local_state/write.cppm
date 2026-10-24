module;

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module farcaster.local.write;

import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.write;

export namespace farcaster::local::write {

/**
 * @brief Infrastructure-only durable binding from a signed canonical message
 * hash to a provider instance. Names are opaque instance IDs, never SDK types.
 *
 * A provider assignment must be durably committed before a network submit.
 * Subsequent retries and reconciliation reuse this exact assignment.
 */
struct WriteRouteBinding final {
    std::string providerId{};
    std::string endpointIdentity{};
};

class IWriteRouteBindings {
public:
    virtual ~IWriteRouteBindings() = default;

    /** @brief Returns the bound provider and immutable endpoint identity, or nullopt. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<WriteRouteBinding>> find(
        const domain::write::IdempotencyKey& key) = 0;

    /**
     * @brief Atomically binds a message once, returning the persisted route identity.
     * If a previous assignment differs, returns Conflict and never overwrites.
     */
    [[nodiscard]] virtual foundation::result::Result<WriteRouteBinding> bindOnce(
        const domain::write::IdempotencyKey& key, WriteRouteBinding binding) = 0;
};


/**
 * @brief Durable account-scoped local desired state for one cast reaction.
 *
 * The projection is independent of provider feed payloads so a refresh cannot
 * erase the authenticated account's last local Like/Recast intent.
 */
struct ReactionProjection final {
    domain::cast::CastReference target{};
    domain::write::ReactionKind reaction{domain::write::ReactionKind::Like};
    bool isActive{false};
    foundation::time::Timestamp updatedAt{};
};

/**
 * @brief Durable account-scoped local desired state for one follow relationship.
 *
 * The projection is independent of profile-provider payloads so a refresh or
 * restart cannot erase the authenticated account's last Follow/Unfollow intent.
 */
struct FollowingProjection final {
    domain::identity::Fid target{};
    bool isFollowing{false};
    foundation::time::Timestamp updatedAt{};
};

/**
 * @brief Durable local outbox plus local-first write projection boundary.
 *
 * enqueue() must atomically persist the signed operation and every local
 * projection required by that operation, including optimistic cast creation or
 * a durable cast-removal tombstone. No provider/network work belongs here.
 */
class IOutboxStore {
public:
    /** @brief Destroys the store. */
    virtual ~IOutboxStore() = default;

    /**
     * @brief Atomically inserts one outbox record and its local-first projection.
     * @return Existing record when the idempotency key was already enqueued.
     */
    [[nodiscard]] virtual foundation::result::Result<domain::write::OutboxRecord> enqueue(
        const domain::write::SignedOperation& operation,
        foundation::time::Timestamp enqueuedAt) = 0;

    /** @brief Loads one record by deterministic idempotency key. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::write::OutboxRecord>> find(
        const domain::write::IdempotencyKey& key) = 0;

    /**
     * @brief Lists the authenticated account's latest durable Like/Recast desired states.
     *
     * This is a local projection read. It performs no provider/network work and
     * must be called from an I/O/application executor rather than the GUI thread.
     */
    [[nodiscard]] virtual foundation::result::Result<std::vector<ReactionProjection>>
    listReactionProjections(domain::identity::Fid author) = 0;

    /**
     * @brief Lists the authenticated account's latest durable Follow/Unfollow desired states.
     *
     * This local projection survives provider refreshes and process restarts and
     * performs no provider/network work.
     */
    [[nodiscard]] virtual foundation::result::Result<std::vector<FollowingProjection>>
    listFollowingProjections(domain::identity::Fid author) = 0;

    /**
     * @brief Returns recent signed CastAdd projections, including retry and
     * terminal failure, for reconstructing the local timeline after restart.
     * Never invokes a remote provider or retries an operation as a side effect.
     */
    [[nodiscard]] virtual foundation::result::Result<std::vector<domain::write::OutboxRecord>>
    listCastProjections(domain::identity::Fid author, std::size_t limit) = 0;

    /** @brief Lists due pending records in deterministic enqueue order. */
    [[nodiscard]] virtual foundation::result::Result<std::vector<domain::write::OutboxRecord>> listReady(
        domain::identity::Fid author,
        foundation::time::Timestamp now,
        std::size_t limit) = 0;

    /**
     * @brief Durably increments attempt_count before any reconciliation/submission network call.
     * @return Updated record, or NotFound if the record disappeared.
     */
    [[nodiscard]] virtual foundation::result::Result<domain::write::OutboxRecord> beginAttempt(
        const domain::write::IdempotencyKey& key,
        foundation::time::Timestamp attemptedAt) = 0;

    /** @brief Schedules a bounded retry while retaining the local projection. */
    [[nodiscard]] virtual foundation::result::Status markRetry(
        const domain::write::IdempotencyKey& key,
        domain::write::OutboxFailure failure,
        foundation::time::Timestamp nextAttemptAt) = 0;

    /** @brief Marks terminal failure without rolling back the local projection. */
    [[nodiscard]] virtual foundation::result::Status markFailed(
        const domain::write::IdempotencyKey& key,
        domain::write::OutboxFailure failure,
        foundation::time::Timestamp failedAt) = 0;

    /** @brief Marks both outbox and local projection network-confirmed. */
    [[nodiscard]] virtual foundation::result::Status markConfirmed(
        const domain::write::IdempotencyKey& key,
        foundation::time::Timestamp confirmedAt) = 0;
};

} // namespace farcaster::local::write
