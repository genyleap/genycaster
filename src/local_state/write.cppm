module;

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

export module farcaster.local.write;

import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.domain.identity;
import farcaster.domain.write;

export namespace farcaster::local::write {

/**
 * @brief Durable local outbox plus optimistic canonical projection boundary.
 *
 * enqueue() must atomically persist both the signed operation and its optimistic
 * canonical entity. No provider/network work belongs in this interface.
 */
class IOutboxStore {
public:
    /** @brief Destroys the store. */
    virtual ~IOutboxStore() = default;

    /**
     * @brief Atomically inserts one outbox record and optimistic canonical cast.
     * @return Existing record when the idempotency key was already enqueued.
     */
    [[nodiscard]] virtual foundation::result::Result<domain::write::OutboxRecord> enqueue(
        const domain::write::SignedOperation& operation,
        foundation::time::Timestamp enqueuedAt) = 0;

    /** @brief Loads one record by deterministic idempotency key. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::write::OutboxRecord>> find(
        const domain::write::IdempotencyKey& key) = 0;

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

    /** @brief Schedules a bounded retry while retaining the optimistic cast. */
    [[nodiscard]] virtual foundation::result::Status markRetry(
        const domain::write::IdempotencyKey& key,
        domain::write::OutboxFailure failure,
        foundation::time::Timestamp nextAttemptAt) = 0;

    /** @brief Marks terminal failure without rolling back the optimistic cast. */
    [[nodiscard]] virtual foundation::result::Status markFailed(
        const domain::write::IdempotencyKey& key,
        domain::write::OutboxFailure failure,
        foundation::time::Timestamp failedAt) = 0;

    /** @brief Marks both outbox and optimistic projection network-confirmed. */
    [[nodiscard]] virtual foundation::result::Status markConfirmed(
        const domain::write::IdempotencyKey& key,
        foundation::time::Timestamp confirmedAt) = 0;
};

} // namespace farcaster::local::write
