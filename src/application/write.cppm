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
import farcaster.domain.session;
import farcaster.local.write;
import farcaster.application.executor;
import farcaster.capability.write_authority;
import farcaster.capability.write_submit;
import farcaster.capability.write;

export namespace farcaster::application::write {

/** @brief User-authored content accepted by the Phase 5C root-cast use case. */
struct PublishCastRequest final {
    std::string text{};
    std::vector<domain::cast::Embed> embeds{};
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

/** @brief Application-owned snapshot of one durable optimistic write. */
struct WriteRecord final {
    std::string idempotencyKey{};
    domain::cast::Cast optimisticCast{};
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
 * @brief Orchestrates canonical PublishCast creation, durable enqueue and reconciliation.
 *
 * The service never accesses private key material or provider DTOs. It consumes
 * SignerService's verified write gate, a canonical codec, a durable local outbox
 * and an optional provider-neutral submit/reconcile port.
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

    /**
     * @brief Publishes one root cast intent through signer → durable outbox → reconciliation.
     *
     * The signed outbox record and optimistic canonical cast are committed before
     * any network submission. When no submitter is configured, the record remains
     * safely queued for a later resume.
     */
    [[nodiscard]] foundation::result::Status publish(
        std::optional<domain::session::ClientSession> session,
        PublishCastRequest request,
        Observer observer);

    /**
     * @brief Processes due pending records after startup/network/signer recovery.
     *
     * Every record is re-gated against the current SignerService and exact signer
     * public key before network I/O. A revoked/expired/replaced signer therefore
     * cannot be bypassed by a previously signed outbox envelope.
     */
    [[nodiscard]] foundation::result::Status resume(
        std::optional<domain::session::ClientSession> session,
        Observer observer);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace farcaster::application::write
