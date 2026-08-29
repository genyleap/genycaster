module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

module farcaster.application.write;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.application.executor;
import farcaster.capability.write;
import farcaster.capability.write_authority;
import farcaster.capability.write_submit;
import farcaster.domain.session;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.write;

namespace farcaster::application::write {
namespace {

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] foundation::result::Status validatePolicy(const WritePolicy& policy)
{
    if (policy.maxAttempts == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Write retry policy requires at least one network attempt."));
    }
    if (policy.baseBackoff <= std::chrono::milliseconds::zero() ||
        policy.maxBackoff < policy.baseBackoff) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Write retry policy requires a positive base backoff no greater than its maximum."));
    }
    if (policy.resumeBatchSize == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Write resume batch size must be positive."));
    }
    return {};
}

[[nodiscard]] bool isRetryable(const foundation::error::ErrorCode code) noexcept
{
    using foundation::error::ErrorCode;
    switch (code) {
    case ErrorCode::RateLimited:
    case ErrorCode::Timeout:
    case ErrorCode::Unavailable:
    case ErrorCode::TransportFailure:
    case ErrorCode::Conflict:
        return true;
    case ErrorCode::InvalidArgument:
    case ErrorCode::NotFound:
    case ErrorCode::Unauthorized:
    case ErrorCode::Unsupported:
    case ErrorCode::StorageFailure:
    case ErrorCode::ProtocolFailure:
    case ErrorCode::InternalFailure:
        return false;
    }
    return false;
}

[[nodiscard]] domain::write::OutboxFailure persistentFailureFor(
    const foundation::error::ErrorCode code)
{
    using foundation::error::ErrorCode;
    switch (code) {
    case ErrorCode::Unauthorized:
        return {.kind = domain::write::FailureKind::Authorization,
            .message = "Protocol write authority is no longer valid."};
    case ErrorCode::RateLimited:
        return {.kind = domain::write::FailureKind::RateLimited,
            .message = "Protocol write endpoint rate-limited this attempt."};
    case ErrorCode::Timeout:
    case ErrorCode::Unavailable:
    case ErrorCode::TransportFailure:
        return {.kind = domain::write::FailureKind::Unavailable,
            .message = "Protocol write endpoint is temporarily unavailable."};
    case ErrorCode::Conflict:
        return {.kind = domain::write::FailureKind::Conflict,
            .message = "Protocol write conflicted with authoritative network state."};
    case ErrorCode::ProtocolFailure:
    case ErrorCode::InvalidArgument:
        return {.kind = domain::write::FailureKind::ProtocolRejected,
            .message = "Canonical protocol write was rejected or malformed."};
    case ErrorCode::Unsupported:
        return {.kind = domain::write::FailureKind::Unsupported,
            .message = "No compatible protocol write capability is available."};
    case ErrorCode::NotFound:
    case ErrorCode::StorageFailure:
    case ErrorCode::InternalFailure:
        return {.kind = domain::write::FailureKind::Internal,
            .message = "Protocol write could not be completed safely."};
    }
    return {.kind = domain::write::FailureKind::Internal,
        .message = "Protocol write could not be completed safely."};
}

[[nodiscard]] std::chrono::milliseconds retryDelay(
    const WritePolicy& policy,
    const std::uint32_t attemptCount)
{
    auto delay = policy.baseBackoff;
    const std::uint32_t doublings = attemptCount > 0U ? attemptCount - 1U : 0U;
    for (std::uint32_t index{}; index < doublings && delay < policy.maxBackoff; ++index) {
        if (delay > policy.maxBackoff / 2) {
            return policy.maxBackoff;
        }
        delay *= 2;
    }
    return std::min(delay, policy.maxBackoff);
}

[[nodiscard]] foundation::error::Error signerMismatchError()
{
    return makeError(
        foundation::error::ErrorCode::Unauthorized,
        "The queued write was signed by a different or no-longer-current protocol signer.");
}

[[nodiscard]] OperationKind applicationKind(const domain::write::OperationKind kind) noexcept
{
    using DomainKind = domain::write::OperationKind;
    switch (kind) {
    case DomainKind::PublishCast: return OperationKind::PublishCast;
    case DomainKind::RemoveCast: return OperationKind::RemoveCast;
    case DomainKind::Like: return OperationKind::Like;
    case DomainKind::Unlike: return OperationKind::Unlike;
    case DomainKind::Recast: return OperationKind::Recast;
    case DomainKind::UndoRecast: return OperationKind::UndoRecast;
    case DomainKind::Follow: return OperationKind::Follow;
    case DomainKind::Unfollow: return OperationKind::Unfollow;
    }
    return OperationKind::PublishCast;
}

class AtomicFlagReset final {
public:
    explicit AtomicFlagReset(std::atomic_bool& flag) noexcept
        : m_flag(flag)
    {
    }

    ~AtomicFlagReset()
    {
        m_flag.store(false, std::memory_order_release);
    }

    AtomicFlagReset(const AtomicFlagReset&) = delete;
    auto operator=(const AtomicFlagReset&) -> AtomicFlagReset& = delete;

private:
    std::atomic_bool& m_flag;
};

[[nodiscard]] bool isConflictReconciliationOnly(
    const domain::write::OutboxRecord& record) noexcept
{
    return record.lastFailure && record.lastFailure->kind == domain::write::FailureKind::Conflict;
}

[[nodiscard]] domain::write::ReactionKind domainReaction(const ReactionKind reaction) noexcept
{
    switch (reaction) {
    case ReactionKind::Like: return domain::write::ReactionKind::Like;
    case ReactionKind::Recast: return domain::write::ReactionKind::Recast;
    }
    return domain::write::ReactionKind::Like;
}

[[nodiscard]] DurableWriteStatus durableStatus(const domain::write::OutboxStatus status) noexcept
{
    switch (status) {
    case domain::write::OutboxStatus::Pending: return DurableWriteStatus::Pending;
    case domain::write::OutboxStatus::Confirmed: return DurableWriteStatus::Confirmed;
    case domain::write::OutboxStatus::Failed: return DurableWriteStatus::Failed;
    }
    return DurableWriteStatus::Failed;
}

[[nodiscard]] WriteRecord writeRecord(const domain::write::OutboxRecord& record)
{
    return WriteRecord{
        .idempotencyKey = record.operation.idempotencyKey.value,
        .kind = applicationKind(record.operation.kind),
        .author = record.operation.author,
        .optimisticCast = record.operation.optimisticCast,
        .target = OperationTarget{
            .cast = record.operation.target.cast,
            .fid = record.operation.target.fid,
        },
        .status = durableStatus(record.status),
        .attemptCount = record.attemptCount,
        .nextAttemptAt = record.nextAttemptAt,
    };
}

} // namespace

struct WriteService::Impl final {
    Impl(
        executor::IExecutor& executorValue,
        capability::write::IWriteAuthority& authorityValue,
        capability::write::ICanonicalWriteCodec& codecValue,
        local::write::IOutboxStore& outboxValue,
        capability::write::IWriteSubmitter* submitterValue,
        WritePolicy policyValue)
        : executor(executorValue),
          authority(authorityValue),
          codec(codecValue),
          outbox(outboxValue),
          submitter(submitterValue),
          policy(std::move(policyValue))
    {
    }

    executor::IExecutor& executor;
    capability::write::IWriteAuthority& authority;
    capability::write::ICanonicalWriteCodec& codec;
    local::write::IOutboxStore& outbox;
    capability::write::IWriteSubmitter* submitter{};
    WritePolicy policy{};
    std::atomic_bool isResumeScheduled{false};

    void emitRecord(
        const domain::write::IdempotencyKey& key,
        const WritePhase phase,
        const Observer& observer,
        std::optional<foundation::error::Error> error = std::nullopt)
    {
        auto record = outbox.find(key);
        if (!record || !*record) {
            observer(WriteState{
                .phase = WritePhase::Failed,
                .error = record
                    ? std::optional<foundation::error::Error>{makeError(
                        foundation::error::ErrorCode::StorageFailure,
                        "Durable write disappeared while emitting state.")}
                    : std::optional<foundation::error::Error>{std::move(record.error())},
            });
            return;
        }
        observer(WriteState{
            .phase = phase,
            .record = writeRecord(**record),
            .error = std::move(error),
        });
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> currentAuthority(
        const std::optional<domain::session::ClientSession>& session,
        const domain::signer::Scope scope)
    {
        if (!session) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "A protocol write requires an authenticated Farcaster session."));
        }
        if (domain::session::isExpired(*session, foundation::time::now())) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "The Farcaster session has expired; sign in again before writing."));
        }
        auto signer = authority.requireSigner(session->fid, scope);
        if (!signer) {
            return std::unexpected(std::move(signer.error()));
        }
        return *signer;
    }

    void noteConfirmed(const domain::write::OutboxRecord& record, const Observer& observer)
    {
        const auto confirmedAt = foundation::time::now();
        auto confirmed = outbox.markConfirmed(record.operation.idempotencyKey, confirmedAt);
        if (!confirmed) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(confirmed.error())});
            return;
        }
        auto refreshed = authority.recordConfirmedWrite(record.operation.author, confirmedAt);
        emitRecord(
            record.operation.idempotencyKey,
            WritePhase::Confirmed,
            observer,
            refreshed ? std::nullopt : std::optional<foundation::error::Error>{std::move(refreshed.error())});
    }

    void scheduleOrFail(
        const domain::write::OutboxRecord& attempted,
        const foundation::error::Error& cause,
        const Observer& observer)
    {
        const auto now = foundation::time::now();
        const auto failure = persistentFailureFor(cause.code);
        if (isRetryable(cause.code) && attempted.attemptCount < policy.maxAttempts) {
            const auto next = now + retryDelay(policy, attempted.attemptCount);
            auto status = outbox.markRetry(attempted.operation.idempotencyKey, failure, next);
            if (!status) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(status.error())});
                return;
            }
            emitRecord(
                attempted.operation.idempotencyKey,
                WritePhase::RetryScheduled,
                observer,
                makeError(cause.code, cause.message));
            return;
        }
        auto status = outbox.markFailed(
            attempted.operation.idempotencyKey,
            failure,
            now);
        if (!status) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(status.error())});
            return;
        }
        emitRecord(
            attempted.operation.idempotencyKey,
            WritePhase::Failed,
            observer,
            makeError(cause.code, cause.message));
    }

    void scheduleConflictReconciliation(
        const domain::write::OutboxRecord& attempted,
        const foundation::error::Error& cause,
        const Observer& observer)
    {
        const auto now = foundation::time::now();
        const bool canRetry =
            (cause.code == foundation::error::ErrorCode::Conflict || isRetryable(cause.code)) &&
            attempted.attemptCount < policy.maxAttempts;
        if (canRetry) {
            const auto next = now + retryDelay(policy, attempted.attemptCount);
            auto status = outbox.markRetry(
                attempted.operation.idempotencyKey,
                persistentFailureFor(foundation::error::ErrorCode::Conflict),
                next);
            if (!status) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(status.error())});
                return;
            }
            emitRecord(
                attempted.operation.idempotencyKey,
                WritePhase::RetryScheduled,
                observer,
                makeError(cause.code, cause.message));
            return;
        }

        auto status = outbox.markFailed(
            attempted.operation.idempotencyKey,
            persistentFailureFor(cause.code),
            now);
        if (!status) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(status.error())});
            return;
        }
        emitRecord(
            attempted.operation.idempotencyKey,
            WritePhase::Failed,
            observer,
            makeError(cause.code, cause.message));
    }

    void processOne(
        const std::optional<domain::session::ClientSession>& session,
        domain::write::OutboxRecord record,
        const Observer& observer)
    {
        if (record.status != domain::write::OutboxStatus::Pending) {
            return;
        }
        auto currentSigner = currentAuthority(session, domain::write::requiredScope(record.operation.kind));
        if (!currentSigner) {
            // A generic not-ready authority can be transient during restore/provisioning.
            // Terminal expiry/revocation is delivered explicitly through
            // invalidatePendingAuthority(), while a different ready key is caught below.
            observer(WriteState{
                .phase = WritePhase::Blocked,
                .record = writeRecord(record),
                .error = std::move(currentSigner.error()),
            });
            return;
        }
        if (currentSigner->hex != record.operation.signerPublicKey.hex) {
            const auto cause = signerMismatchError();
            auto failed = outbox.markFailed(
                record.operation.idempotencyKey,
                persistentFailureFor(cause.code),
                foundation::time::now());
            if (!failed) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(failed.error())});
                return;
            }
            emitRecord(record.operation.idempotencyKey, WritePhase::Failed, observer, cause);
            return;
        }
        if (submitter == nullptr) {
            observer(WriteState{
                .phase = WritePhase::Queued,
                .record = writeRecord(record),
                .error = makeError(
                    foundation::error::ErrorCode::Unsupported,
                    "No protocol write submitter is configured; the durable write remains queued."),
            });
            return;
        }

        if (record.attemptCount >= policy.maxAttempts) {
            auto reconciliation = submitter->reconcile(record.operation);
            if (reconciliation && *reconciliation == capability::write::ConfirmationState::Confirmed) {
                noteConfirmed(record, observer);
                return;
            }
            const auto cause = reconciliation
                ? makeError(
                    foundation::error::ErrorCode::Conflict,
                    "Maximum write attempts were reached without authoritative confirmation.")
                : makeError(
                    reconciliation.error().code,
                    "Maximum write attempts were reached and authoritative confirmation is unavailable.");
            auto failed = outbox.markFailed(
                record.operation.idempotencyKey,
                persistentFailureFor(cause.code),
                foundation::time::now());
            if (!failed) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(failed.error())});
                return;
            }
            emitRecord(record.operation.idempotencyKey, WritePhase::Failed, observer, cause);
            return;
        }

        const bool requiresPreSubmitReconciliation = record.attemptCount > 0U;
        const bool conflictReconciliationOnly = isConflictReconciliationOnly(record);
        auto attempt = outbox.beginAttempt(record.operation.idempotencyKey, foundation::time::now());
        if (!attempt) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(attempt.error())});
            return;
        }
        record = std::move(*attempt);
        observer(WriteState{.phase = WritePhase::Submitting, .record = writeRecord(record)});

        if (requiresPreSubmitReconciliation) {
            auto before = submitter->reconcile(record.operation);
            if (!before) {
                if (conflictReconciliationOnly) {
                    scheduleConflictReconciliation(record, before.error(), observer);
                } else {
                    scheduleOrFail(record, before.error(), observer);
                }
                return;
            }
            if (*before == capability::write::ConfirmationState::Confirmed) {
                noteConfirmed(record, observer);
                return;
            }
            if (conflictReconciliationOnly) {
                scheduleConflictReconciliation(
                    record,
                    makeError(
                        foundation::error::ErrorCode::Conflict,
                        "The protocol write conflicted and authoritative indexing has not confirmed it yet."),
                    observer);
                return;
            }
        }

        auto submitted = submitter->submit(record.operation);
        if (!submitted) {
            if (submitted.error().code == foundation::error::ErrorCode::Conflict) {
                auto conflictState = submitter->reconcile(record.operation);
                if (conflictState &&
                    *conflictState == capability::write::ConfirmationState::Confirmed) {
                    noteConfirmed(record, observer);
                    return;
                }
                if (!conflictState) {
                    scheduleConflictReconciliation(record, conflictState.error(), observer);
                    return;
                }
                scheduleConflictReconciliation(record, submitted.error(), observer);
                return;
            }
            scheduleOrFail(record, submitted.error(), observer);
            return;
        }

        noteConfirmed(record, observer);
    }

    void enqueuePrepared(
        const std::optional<domain::session::ClientSession>& session,
        domain::write::PreparedOperation prepared,
        const Observer& observer)
    {
        auto signature = authority.sign(domain::write::requiredScope(prepared.kind), prepared.hashBytes);
        if (!signature) {
            observer(WriteState{.phase = WritePhase::Blocked, .error = std::move(signature.error())});
            return;
        }
        auto operation = codec.seal(prepared, *signature);
        if (!operation) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(operation.error())});
            return;
        }
        auto queued = outbox.enqueue(*operation, foundation::time::now());
        if (!queued) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(queued.error())});
            return;
        }
        observer(WriteState{.phase = WritePhase::Queued, .record = writeRecord(*queued)});
        processOne(session, std::move(*queued), observer);
    }
};

WriteService::WriteService(
    executor::IExecutor& executor,
    capability::write::IWriteAuthority& authority,
    capability::write::ICanonicalWriteCodec& codec,
    local::write::IOutboxStore& outbox,
    capability::write::IWriteSubmitter* submitter,
    WritePolicy policy)
    : m_impl(std::make_unique<Impl>(
        executor, authority, codec, outbox, submitter, std::move(policy)))
{
}

WriteService::~WriteService() = default;

foundation::result::Status WriteService::publish(
    std::optional<domain::session::ClientSession> session,
    PublishCastRequest request,
    Observer observer)
{
    if (!observer) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "PublishCast requires a write-state observer."));
    }
    if (auto valid = validatePolicy(m_impl->policy); !valid) {
        return valid;
    }
    return m_impl->executor.post(
        [this, session = std::move(session), request = std::move(request), observer = std::move(observer)]() mutable {
            auto signer = m_impl->currentAuthority(session, domain::signer::Scope::CastAdd);
            if (!signer) {
                observer(WriteState{.phase = WritePhase::Blocked, .error = std::move(signer.error())});
                return;
            }
            auto prepared = m_impl->codec.preparePublishCast(
                domain::write::PublishCastIntent{
                    .author = session->fid,
                    .text = std::move(request.text),
                    .embeds = std::move(request.embeds),
                    .mentions = std::move(request.mentions),
                    .mentionPositions = std::move(request.mentionPositions),
                    .parent = std::move(request.parent),
                    .createdAt = foundation::time::now(),
                },
                *signer);
            if (!prepared) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(prepared.error())});
                return;
            }
            m_impl->enqueuePrepared(session, std::move(*prepared), observer);
        });
}

foundation::result::Status WriteService::removeCast(
    std::optional<domain::session::ClientSession> session,
    domain::cast::CastReference target,
    Observer observer)
{
    if (!observer) {
        return std::unexpected(makeError(foundation::error::ErrorCode::InvalidArgument,
            "RemoveCast requires a write-state observer."));
    }
    return m_impl->executor.post(
        [this, session = std::move(session), target = std::move(target), observer = std::move(observer)]() mutable {
            auto signer = m_impl->currentAuthority(session, domain::signer::Scope::CastRemove);
            if (!signer) {
                observer(WriteState{.phase = WritePhase::Blocked, .error = std::move(signer.error())});
                return;
            }
            auto prepared = m_impl->codec.prepareRemoveCast(
                domain::write::RemoveCastIntent{
                    .author = session->fid,
                    .target = std::move(target),
                    .createdAt = foundation::time::now(),
                },
                *signer);
            if (!prepared) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(prepared.error())});
                return;
            }
            m_impl->enqueuePrepared(session, std::move(*prepared), observer);
        });
}

foundation::result::Status WriteService::setReaction(
    std::optional<domain::session::ClientSession> session,
    domain::cast::CastReference target,
    const ReactionKind reaction,
    const bool isActive,
    Observer observer)
{
    if (!observer) {
        return std::unexpected(makeError(foundation::error::ErrorCode::InvalidArgument,
            "Reaction mutation requires a write-state observer."));
    }
    return m_impl->executor.post(
        [this, session = std::move(session), target = std::move(target), reaction, isActive,
            observer = std::move(observer)]() mutable {
            const auto scope = isActive ? domain::signer::Scope::ReactionAdd : domain::signer::Scope::ReactionRemove;
            auto signer = m_impl->currentAuthority(session, scope);
            if (!signer) {
                observer(WriteState{.phase = WritePhase::Blocked, .error = std::move(signer.error())});
                return;
            }
            auto prepared = m_impl->codec.prepareReaction(
                domain::write::ReactionIntent{
                    .author = session->fid,
                    .target = std::move(target),
                    .reaction = domainReaction(reaction),
                    .isActive = isActive,
                    .createdAt = foundation::time::now(),
                },
                *signer);
            if (!prepared) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(prepared.error())});
                return;
            }
            m_impl->enqueuePrepared(session, std::move(*prepared), observer);
        });
}

foundation::result::Status WriteService::setFollowing(
    std::optional<domain::session::ClientSession> session,
    const domain::identity::Fid target,
    const bool isFollowing,
    Observer observer)
{
    if (!observer) {
        return std::unexpected(makeError(foundation::error::ErrorCode::InvalidArgument,
            "Follow mutation requires a write-state observer."));
    }
    return m_impl->executor.post(
        [this, session = std::move(session), target, isFollowing, observer = std::move(observer)]() mutable {
            const auto scope = isFollowing ? domain::signer::Scope::LinkAdd : domain::signer::Scope::LinkRemove;
            auto signer = m_impl->currentAuthority(session, scope);
            if (!signer) {
                observer(WriteState{.phase = WritePhase::Blocked, .error = std::move(signer.error())});
                return;
            }
            auto prepared = m_impl->codec.prepareFollow(
                domain::write::FollowIntent{
                    .author = session->fid,
                    .target = target,
                    .isFollowing = isFollowing,
                    .createdAt = foundation::time::now(),
                },
                *signer);
            if (!prepared) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(prepared.error())});
                return;
            }
            m_impl->enqueuePrepared(session, std::move(*prepared), observer);
        });
}

foundation::result::Status WriteService::resume(
    std::optional<domain::session::ClientSession> session,
    Observer observer)
{
    if (!observer) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Write resume requires a write-state observer."));
    }
    if (auto valid = validatePolicy(m_impl->policy); !valid) {
        return valid;
    }
    if (m_impl->isResumeScheduled.exchange(true, std::memory_order_acq_rel)) {
        return {};
    }
    auto posted = m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            AtomicFlagReset reset{m_impl->isResumeScheduled};
            if (!session || domain::session::isExpired(*session, foundation::time::now())) {
                observer(WriteState{
                    .phase = WritePhase::Blocked,
                    .error = makeError(
                        foundation::error::ErrorCode::Unauthorized,
                        "A current authenticated Farcaster session is required before resuming writes."),
                });
                return;
            }
            auto records = m_impl->outbox.listReady(
                session->fid,
                foundation::time::now(),
                m_impl->policy.resumeBatchSize);
            if (!records) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(records.error())});
                return;
            }
            for (auto& record : *records) {
                m_impl->processOne(session, std::move(record), observer);
            }
        });
    if (!posted) {
        m_impl->isResumeScheduled.store(false, std::memory_order_release);
    }
    return posted;
}

foundation::result::Status WriteService::invalidatePendingAuthority(
    std::optional<domain::session::ClientSession> session,
    Observer observer)
{
    if (!observer) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Signer invalidation requires a write-state observer."));
    }
    if (!session || domain::session::isExpired(*session, foundation::time::now())) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "A current authenticated Farcaster session is required to invalidate queued writes."));
    }
    if (auto valid = validatePolicy(m_impl->policy); !valid) {
        return valid;
    }

    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            const auto failure = persistentFailureFor(foundation::error::ErrorCode::Unauthorized);
            while (true) {
                auto records = m_impl->outbox.listReady(
                    session->fid,
                    foundation::time::Timestamp::max(),
                    m_impl->policy.resumeBatchSize);
                if (!records) {
                    observer(WriteState{.phase = WritePhase::Failed, .error = std::move(records.error())});
                    return;
                }
                if (records->empty()) {
                    return;
                }
                for (const auto& record : *records) {
                    auto failed = m_impl->outbox.markFailed(
                        record.operation.idempotencyKey,
                        failure,
                        foundation::time::now());
                    if (!failed) {
                        observer(WriteState{.phase = WritePhase::Failed, .error = std::move(failed.error())});
                        return;
                    }
                    m_impl->emitRecord(
                        record.operation.idempotencyKey,
                        WritePhase::Failed,
                        observer,
                        makeError(
                            foundation::error::ErrorCode::Unauthorized,
                            "Queued write was invalidated because its protocol signer expired or was revoked."));
                }
            }
        });
}

} // namespace farcaster::application::write
