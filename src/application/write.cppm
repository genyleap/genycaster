module;

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module farcaster.application.write;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.local.write;
import farcaster.application.executor;
import farcaster.capability.write_authority;
import farcaster.capability.write_submit;
import farcaster.capability.write;

export namespace farcaster::application::write {


/** @brief Application-facing classification of one protocol mutation. */
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

/** @brief Application-facing target snapshot for one social mutation. */
struct OperationTarget final {
    std::optional<domain::cast::CastReference> cast{};
    std::optional<domain::identity::Fid> fid{};
};

/** @brief Reaction family used by the public application write API. */
enum class ReactionKind : std::uint8_t {
    Like,
    Recast,
};

/** @brief User-authored content accepted by the shared root/reply composer use case. */
struct PublishCastRequest final {
    std::string text{};
    std::vector<domain::cast::Embed> embeds{};
    std::vector<domain::identity::Fid> mentions{};
    std::vector<std::uint32_t> mentionPositions{};
    std::optional<domain::cast::CastReference> parent{};
    std::optional<std::string> parentUrl{};
};

/** @brief Product-neutral lifecycle emitted while one durable write is handled. */
enum class WritePhase {
    Queued,
    Submitting,
    RetryScheduled,
    Confirmed,
    Failed,
    Blocked,
};

/** @brief Durable status projected outward without exposing local-storage internals. */
enum class DurableWriteStatus {
    Pending,
    Confirmed,
    Failed,
};

/** @brief Application-owned snapshot of one durable protocol write. */
struct WriteRecord final {
    std::string idempotencyKey{};
    OperationKind kind{OperationKind::PublishCast};
    domain::identity::Fid author{};
    std::optional<domain::cast::Cast> optimisticCast{};
    OperationTarget target{};
    std::optional<std::string> dependsOn{};
    DurableWriteStatus status{DurableWriteStatus::Pending};
    std::uint32_t attemptCount{};
    std::optional<foundation::time::Timestamp> nextAttemptAt{};
};

/** @brief Observable write state safe for presentation adapters. */
struct WriteState final {
    WritePhase phase{WritePhase::Queued};
    std::optional<WriteRecord> record{};
    std::optional<foundation::error::Error> error{};
};

/** @brief Bounded retry policy for durable outbox processing. */
struct WritePolicy final {
    std::uint32_t maxAttempts{5U};
    std::chrono::milliseconds baseBackoff{1'000};
    std::chrono::milliseconds maxBackoff{60'000};
    std::size_t resumeBatchSize{32U};
};

/** @brief Observer called from the configured application executor. */
using Observer = std::function<void(WriteState)>;

/**
 * @brief Orchestrates all implemented Farcaster social writes through one durable pipeline.
 *
 * Every operation is gated by the exact signer scope, canonicalized before
 * signing, committed to the local outbox before network I/O, reconciled before
 * re-submission after an ambiguous prior attempt, and confirmed by a validated
 * canonical submit response or authoritative reconciliation.
 */
class WriteService final {
public:
    /** @brief Creates the service over explicit provider-neutral boundaries. */
    WriteService(
        executor::IExecutor& executor,
        capability::write::IWriteAuthority& authority,
        capability::write::ICanonicalWriteCodec& codec,
        local::write::IOutboxStore& outbox,
        capability::write::IWriteSubmitter* submitter,
        WritePolicy policy = {});
    ~WriteService();

    WriteService(const WriteService&) = delete;
    auto operator=(const WriteService&) -> WriteService& = delete;
    WriteService(WriteService&&) = delete;
    auto operator=(WriteService&&) -> WriteService& = delete;

    /** @brief Publishes one root or reply cast through the durable protocol pipeline. */
    [[nodiscard]] foundation::result::Status publish(
        std::optional<domain::session::ClientSession> session,
        PublishCastRequest request,
        Observer observer);

    /**
     * @brief Publishes an ordered Farcaster thread as normal CastAdd messages linked by parent_cast_id.
     *
     * The first request may itself be a reply. Every later request is rewritten
     * to reply to the immediately preceding locally hashed cast and is durably
     * blocked behind that parent's outbox confirmation.
     */
    [[nodiscard]] foundation::result::Status publishThread(
        std::optional<domain::session::ClientSession> session,
        std::vector<PublishCastRequest> requests,
        Observer observer);

    /** @brief Deletes one cast owned by the authenticated user through CAST_REMOVE. */
    [[nodiscard]] foundation::result::Status removeCast(
        std::optional<domain::session::ClientSession> session,
        domain::cast::CastReference target,
        Observer observer);

    /** @brief Adds or removes a Like/Recast reaction through REACTION_ADD/REMOVE. */
    [[nodiscard]] foundation::result::Status setReaction(
        std::optional<domain::session::ClientSession> session,
        domain::cast::CastReference target,
        ReactionKind reaction,
        bool isActive,
        Observer observer);

    /** @brief Adds or removes a follow relationship through LINK_ADD/LINK_REMOVE. */
    [[nodiscard]] foundation::result::Status setFollowing(
        std::optional<domain::session::ClientSession> session,
        domain::identity::Fid target,
        bool isFollowing,
        Observer observer);

    /** @brief Adds/removes an account-wide protocol block via LINK_ADD/REMOVE. */
    [[nodiscard]] foundation::result::Status setBlocked(
        std::optional<domain::session::ClientSession> session,
        domain::identity::Fid target,
        bool isBlocked,
        Observer observer);

    /** @brief Processes due pending records after startup/network/signer recovery. */
    [[nodiscard]] foundation::result::Status resume(
        std::optional<domain::session::ClientSession> session,
        Observer observer);

    /**
     * @brief Fails every queued write for the account after authoritative signer expiry/revocation.
     *
     * This performs no network submission. Durable optimistic projections are
     * retained, but stale signed envelopes cannot cross a signer-authority lifecycle.
     */
    [[nodiscard]] foundation::result::Status invalidatePendingAuthority(
        std::optional<domain::session::ClientSession> session,
        Observer observer);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace farcaster::application::write
