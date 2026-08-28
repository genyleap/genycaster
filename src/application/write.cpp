module;

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

module farcaster.application.write;

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
        return true;
    case ErrorCode::InvalidArgument:
    case ErrorCode::NotFound:
    case ErrorCode::Unauthorized:
    case ErrorCode::Conflict:
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
        .optimisticCast = record.operation.optimisticCast,
        .status = durableStatus(record.status),
        .attemptCount = record.attemptCount,
        .nextAttemptAt = record.nextAttemptAt,
    };
}

} // namespace

struct WriteService::Impl final {
    executor::IExecutor& executor;
    capability::write::IWriteAuthority& authority;
    capability::write::ICanonicalWriteCodec& codec;
    local::write::IOutboxStore& outbox;
    capability::write::IWriteSubmitter* submitter{};
    WritePolicy policy{};

    void emitRecord(
        const domain::write::IdempotencyKey& key,
        const WritePhase phase,
        const Observer& observer,
        std::optional<foundation::error::Error> error = std::nullopt)
    {
        auto record = outbox.find(key);
        if (!record) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(record.error())});
            return;
        }
        observer(WriteState{
            .phase = phase,
            .record = writeRecord(**record),
            .error = std::move(error),
        });
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> currentAuthority(
        const std::optional<domain::session::ClientSession>& session)
    {
        if (!session) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "Publishing requires an authenticated Farcaster session."));
        }
        if (domain::session::isExpired(*session, foundation::time::now())) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::Unauthorized,
                "The Farcaster session has expired; sign in again before publishing."));
        }
        auto signer = authority.requireSigner(session->fid, domain::signer::Scope::CastAdd);
        if (!signer) return std::unexpected(std::move(signer.error()));
        return *signer;
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
                makeError(cause.code, failure.message));
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
            makeError(cause.code, failure.message));
    }

    void processOne(
        const std::optional<domain::session::ClientSession>& session,
        domain::write::OutboxRecord record,
        const Observer& observer)
    {
        if (record.status != domain::write::OutboxStatus::Pending) {
            return;
        }
        auto currentSigner = currentAuthority(session);
        if (!currentSigner) {
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
                auto confirmed = outbox.markConfirmed(
                    record.operation.idempotencyKey,
                    foundation::time::now());
                if (!confirmed) {
                    observer(WriteState{.phase = WritePhase::Failed, .error = std::move(confirmed.error())});
                    return;
                }
                emitRecord(record.operation.idempotencyKey, WritePhase::Confirmed, observer);
                return;
            }
            const auto cause = reconciliation
                ? makeError(
                    foundation::error::ErrorCode::Conflict,
                    "Maximum write attempts were reached without authoritative confirmation.")
                : makeError(reconciliation.error().code,
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

        auto attempt = outbox.beginAttempt(record.operation.idempotencyKey, foundation::time::now());
        if (!attempt) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(attempt.error())});
            return;
        }
        record = std::move(*attempt);
        observer(WriteState{.phase = WritePhase::Submitting, .record = writeRecord(record)});

        auto before = submitter->reconcile(record.operation);
        if (!before) {
            scheduleOrFail(record, before.error(), observer);
            return;
        }
        if (*before == capability::write::ConfirmationState::Confirmed) {
            auto status = outbox.markConfirmed(
                record.operation.idempotencyKey,
                foundation::time::now());
            if (!status) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(status.error())});
                return;
            }
            emitRecord(record.operation.idempotencyKey, WritePhase::Confirmed, observer);
            return;
        }

        auto submitted = submitter->submit(record.operation);
        if (!submitted) {
            auto afterFailure = submitter->reconcile(record.operation);
            if (afterFailure && *afterFailure == capability::write::ConfirmationState::Confirmed) {
                auto status = outbox.markConfirmed(
                    record.operation.idempotencyKey,
                    foundation::time::now());
                if (!status) {
                    observer(WriteState{.phase = WritePhase::Failed, .error = std::move(status.error())});
                    return;
                }
                emitRecord(record.operation.idempotencyKey, WritePhase::Confirmed, observer);
                return;
            }
            scheduleOrFail(record, submitted.error(), observer);
            return;
        }

        auto after = submitter->reconcile(record.operation);
        if (!after) {
            scheduleOrFail(record, after.error(), observer);
            return;
        }
        if (*after != capability::write::ConfirmationState::Confirmed) {
            scheduleOrFail(
                record,
                makeError(
                    foundation::error::ErrorCode::Unavailable,
                    "Submitted protocol write is not yet visible in authoritative state."),
                observer);
            return;
        }
        auto confirmed = outbox.markConfirmed(
            record.operation.idempotencyKey,
            foundation::time::now());
        if (!confirmed) {
            observer(WriteState{.phase = WritePhase::Failed, .error = std::move(confirmed.error())});
            return;
        }
        emitRecord(record.operation.idempotencyKey, WritePhase::Confirmed, observer);
    }
};

WriteService::WriteService(
    executor::IExecutor& executor,
    capability::write::IWriteAuthority& authority,
    capability::write::ICanonicalWriteCodec& codec,
    local::write::IOutboxStore& outbox,
    capability::write::IWriteSubmitter* submitter,
    WritePolicy policy)
    : m_impl(std::make_unique<Impl>(Impl{
        .executor = executor,
        .authority = authority,
        .codec = codec,
        .outbox = outbox,
        .submitter = submitter,
        .policy = std::move(policy),
    }))
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
        [this, session = std::move(session), request = std::move(request),
            observer = std::move(observer)]() mutable {
            auto signer = m_impl->currentAuthority(session);
            if (!signer) {
                observer(WriteState{.phase = WritePhase::Blocked, .error = std::move(signer.error())});
                return;
            }

            const auto now = foundation::time::now();
            auto prepared = m_impl->codec.preparePublishCast(
                domain::write::PublishCastIntent{
                    .author = session->fid,
                    .text = std::move(request.text),
                    .embeds = std::move(request.embeds),
                    .createdAt = now,
                },
                *signer);
            if (!prepared) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(prepared.error())});
                return;
            }
            auto signature = m_impl->authority.sign(
                domain::signer::Scope::CastAdd,
                prepared->hashBytes);
            if (!signature) {
                observer(WriteState{.phase = WritePhase::Blocked, .error = std::move(signature.error())});
                return;
            }
            auto operation = m_impl->codec.seal(*prepared, *signature);
            if (!operation) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(operation.error())});
                return;
            }
            auto queued = m_impl->outbox.enqueue(*operation, foundation::time::now());
            if (!queued) {
                observer(WriteState{.phase = WritePhase::Failed, .error = std::move(queued.error())});
                return;
            }
            observer(WriteState{.phase = WritePhase::Queued, .record = writeRecord(*queued)});
            m_impl->processOne(session, std::move(*queued), observer);
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
    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
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
}

} // namespace farcaster::application::write
