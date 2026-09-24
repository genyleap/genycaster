#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

import farcaster.application.executor;
import farcaster.application.write;
import farcaster.capability.write_authority;
import farcaster.capability.write_submit;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.domain.signer;
import farcaster.domain.write;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.protocol.write;
import farcaster.storage.write;

namespace {

namespace application = farcaster::application;
namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace protocol = farcaster::protocol;
namespace storage = farcaster::storage;

class InlineExecutor final : public application::executor::IExecutor {
public:
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override
    {
        if (!job) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Test executor requires a job."));
        }
        job();
        return {};
    }
};

class QueuedExecutor final : public application::executor::IExecutor {
public:
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override
    {
        if (!job) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::InvalidArgument,
                "Test executor requires a job."));
        }
        jobs.push_back(std::move(job));
        return {};
    }

    void runNext()
    {
        assert(!jobs.empty());
        auto job = std::move(jobs.front());
        jobs.erase(jobs.begin());
        job();
    }

    std::vector<application::executor::Job> jobs{};
};

class FakeAuthority final : public capability::write::IWriteAuthority {
public:
    bool allowed{true};
    domain::identity::Fid fid{42U};
    domain::signer::PublicKey key{
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"};
    std::size_t signCalls{};
    std::size_t confirmedWriteCalls{};
    domain::signer::Scope lastScope{domain::signer::Scope::CastAdd};

    [[nodiscard]] static bool permits(const domain::signer::Scope scope) noexcept
    {
        switch (scope) {
        case domain::signer::Scope::CastAdd:
        case domain::signer::Scope::CastRemove:
        case domain::signer::Scope::ReactionAdd:
        case domain::signer::Scope::ReactionRemove:
        case domain::signer::Scope::LinkAdd:
        case domain::signer::Scope::LinkRemove:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> requireSigner(
        const domain::identity::Fid requestedFid,
        const domain::signer::Scope scope) const override
    {
        if (!allowed || requestedFid.value != fid.value || !permits(scope)) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized,
                "Test write authority denied the signer."));
        }
        return key;
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Signature> sign(
        const domain::signer::Scope scope,
        const std::string& bytes) override
    {
        if (!allowed || !permits(scope) || bytes.size() != 20U) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized,
                "Test write authority denied signing."));
        }
        ++signCalls;
        lastScope = scope;
        return domain::signer::Signature{std::string(64U, static_cast<char>(0x5A))};
    }

    [[nodiscard]] foundation::result::Status recordConfirmedWrite(
        const domain::identity::Fid confirmedFid,
        foundation::time::Timestamp) override
    {
        if (confirmedFid.value != fid.value) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unauthorized,
                "Test write confirmation used the wrong FID."));
        }
        ++confirmedWriteCalls;
        return {};
    }
};

enum class SubmitBehavior {
    Succeed,
    Unavailable,
    Timeout,
    Conflict,
    Reject,
};

class FakeSubmitter final : public capability::write::IWriteSubmitter {
public:
    std::deque<capability::write::ConfirmationState> reconciliation{};
    SubmitBehavior submitBehavior{SubmitBehavior::Succeed};
    std::size_t submitCalls{};
    std::size_t reconcileCalls{};
    std::vector<std::string> submittedMessages{};

    [[nodiscard]] foundation::result::Status submit(
        const domain::write::SignedOperation& operation) override
    {
        ++submitCalls;
        submittedMessages.push_back(operation.messageBytes);
        switch (submitBehavior) {
        case SubmitBehavior::Succeed:
            return {};
        case SubmitBehavior::Unavailable:
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Test endpoint unavailable."));
        case SubmitBehavior::Timeout:
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Timeout,
                "Test response timed out after the request was sent."));
        case SubmitBehavior::Conflict:
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Conflict,
                "Test protocol conflict."));
        case SubmitBehavior::Reject:
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::ProtocolFailure,
                "Test protocol rejection."));
        }
        return {};
    }

    [[nodiscard]] foundation::result::Result<capability::write::ConfirmationState> reconcile(
        const domain::write::SignedOperation&) override
    {
        ++reconcileCalls;
        if (reconciliation.empty()) {
            return capability::write::ConfirmationState::Absent;
        }
        const auto result = reconciliation.front();
        reconciliation.pop_front();
        return result;
    }
};

struct DatabaseFixture final {
    std::filesystem::path path{};

    DatabaseFixture()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            ("genycaster-write-service-" + std::to_string(stamp) + ".sqlite3");
        std::error_code ignored{};
        std::filesystem::remove(path, ignored);
    }

    ~DatabaseFixture()
    {
        std::error_code ignored{};
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(path.string() + "-wal", ignored);
        std::filesystem::remove(path.string() + "-shm", ignored);
    }
};

[[nodiscard]] domain::session::ClientSession activeSession()
{
    const auto now = foundation::time::now();
    return domain::session::ClientSession{
        .fid = domain::identity::Fid{42U},
        .username = std::string{"alice"},
        .method = domain::session::AuthenticationMethod::Custody,
        .authenticatedAt = now - std::chrono::minutes{1},
        .expiresAt = now + std::chrono::hours{1},
    };
}

[[nodiscard]] domain::session::ClientSession expiredSession()
{
    auto session = activeSession();
    session.expiresAt = foundation::time::now() - std::chrono::seconds{1};
    return session;
}

[[nodiscard]] application::write::WritePolicy testPolicy()
{
    return application::write::WritePolicy{
        .maxAttempts = 3U,
        .baseBackoff = std::chrono::milliseconds{1},
        .maxBackoff = std::chrono::milliseconds{4},
        .resumeBatchSize = 8U,
    };
}

[[nodiscard]] std::vector<application::write::WriteState> publish(
    application::write::WriteService& service,
    std::optional<domain::session::ClientSession> session = activeSession(),
    application::write::PublishCastRequest request =
        application::write::PublishCastRequest{.text = "hello from Genycaster"})
{
    std::vector<application::write::WriteState> states{};
    auto status = service.publish(
        std::move(session),
        std::move(request),
        [&states](application::write::WriteState state) {
            states.push_back(std::move(state));
        });
    assert(status);
    return states;
}

void unsignedAndExpiredSessionsAreBlockedBeforeSigning()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, nullptr, testPolicy()};

    auto unsignedStates = publish(service, std::nullopt);
    assert(unsignedStates.size() == 1U);
    assert(unsignedStates.back().phase == application::write::WritePhase::Blocked);
    assert(authority.signCalls == 0U);

    auto expiredStates = publish(service, expiredSession());
    assert(expiredStates.size() == 1U);
    assert(expiredStates.back().phase == application::write::WritePhase::Blocked);
    assert(authority.signCalls == 0U);
}

void offlinePublishIsDurableAndOptimistic()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, nullptr, testPolicy()};

    auto states = publish(service);
    assert(states.size() == 2U);
    assert(states.front().phase == application::write::WritePhase::Queued);
    assert(states.back().phase == application::write::WritePhase::Queued);
    assert(states.back().record);
    const auto key = domain::write::IdempotencyKey{states.back().record->idempotencyKey};
    assert(states.back().record->optimisticCast->id.value == key.value);
    assert(authority.signCalls == 1U);

    outbox->reset();
    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto record = (*reopened)->find(key);
    assert(record && *record);
    assert((*record)->status == domain::write::OutboxStatus::Pending);
    assert((*record)->operation.optimisticCast->text == "hello from Genycaster");
    assert(!(*record)->operation.messageBytes.empty());
}


void offlineReplyIsDurableAndRetainsCanonicalParent()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, nullptr, testPolicy()};

    auto states = publish(
        service,
        activeSession(),
        application::write::PublishCastRequest{
            .text = "reply from Genycaster",
            .parent = domain::cast::CastReference{
                .author = domain::identity::Fid{7U},
                .id = domain::cast::CastId{
                    "0x000102030405060708090a0b0c0d0e0f10111213"},
            },
        });
    assert(states.size() == 2U);
    assert(states.back().record);
    assert(states.back().record->optimisticCast->parent);
    assert(states.back().record->optimisticCast->parent->value ==
        "0x000102030405060708090a0b0c0d0e0f10111213");
    const auto key = domain::write::IdempotencyKey{states.back().record->idempotencyKey};

    outbox->reset();
    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    auto record = (*reopened)->find(key);
    assert(record && *record);
    assert((*record)->operation.optimisticCast->parent);
    assert((*record)->operation.optimisticCast->parent->value ==
        "0x000102030405060708090a0b0c0d0e0f10111213");
    assert(!(*record)->operation.messageBytes.empty());
}


void offlineThreadPersistsParentChainAndDependencies()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, nullptr, testPolicy()};

    std::vector<application::write::WriteState> states{};
    auto status = service.publishThread(
        activeSession(),
        {
            application::write::PublishCastRequest{.text = "thread root"},
            application::write::PublishCastRequest{.text = "thread child"},
            application::write::PublishCastRequest{.text = "thread grandchild"},
        },
        [&states](application::write::WriteState state) {
            states.push_back(std::move(state));
        });
    assert(status);
    assert(authority.signCalls == 3U);

    std::vector<application::write::WriteRecord> records{};
    for (const auto& state : states) {
        if (!state.record) {
            continue;
        }
        const auto duplicate = std::ranges::find_if(records, [&state](const auto& record) {
            return record.idempotencyKey == state.record->idempotencyKey;
        });
        if (duplicate == records.end()) {
            records.push_back(*state.record);
        }
    }
    assert(records.size() == 3U);
    assert(!records[0].dependsOn);
    assert(records[1].dependsOn == std::optional<std::string>{records[0].idempotencyKey});
    assert(records[2].dependsOn == std::optional<std::string>{records[1].idempotencyKey});
    assert(records[0].optimisticCast);
    assert(records[1].optimisticCast && records[1].optimisticCast->parent);
    assert(records[2].optimisticCast && records[2].optimisticCast->parent);
    assert(records[1].optimisticCast->parent->value == records[0].optimisticCast->id.value);
    assert(records[2].optimisticCast->parent->value == records[1].optimisticCast->id.value);

    outbox->reset();
    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    for (std::size_t index{}; index < records.size(); ++index) {
        const auto key = domain::write::IdempotencyKey{records[index].idempotencyKey};
        auto durable = (*reopened)->find(key);
        assert(durable && *durable);
        assert((*durable)->status == domain::write::OutboxStatus::Pending);
        if (index == 0U) {
            assert(!(*durable)->operation.dependsOn);
            continue;
        }
        assert((*durable)->operation.dependsOn);
        assert((*durable)->operation.dependsOn->value == records[index - 1U].idempotencyKey);
        assert((*durable)->operation.optimisticCast);
        assert((*durable)->operation.optimisticCast->parent);
        assert((*durable)->operation.optimisticCast->parent->value ==
            records[index - 1U].optimisticCast->id.value);
    }
}

void failedThreadParentBlocksChildrenWithoutNetworkSubmission()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    FakeSubmitter submitter{};
    submitter.submitBehavior = SubmitBehavior::Reject;
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};

    std::vector<application::write::WriteState> states{};
    auto status = service.publishThread(
        activeSession(),
        {
            application::write::PublishCastRequest{.text = "failing thread root"},
            application::write::PublishCastRequest{.text = "must stay blocked"},
            application::write::PublishCastRequest{.text = "must also stay blocked"},
        },
        [&states](application::write::WriteState state) {
            states.push_back(std::move(state));
        });
    assert(status);
    assert(authority.signCalls == 3U);
    assert(submitter.submitCalls == 1U);

    std::vector<std::string> keys{};
    for (const auto& state : states) {
        if (!state.record) {
            continue;
        }
        if (std::ranges::find(keys, state.record->idempotencyKey) == keys.end()) {
            keys.push_back(state.record->idempotencyKey);
        }
    }
    assert(keys.size() == 3U);
    for (const auto& keyText : keys) {
        auto durable = (*outbox)->find(domain::write::IdempotencyKey{keyText});
        assert(durable && *durable);
        assert((*durable)->status == domain::write::OutboxStatus::Failed);
    }
}

void invalidReplyParentFailsBeforeSigningOrEnqueue()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, nullptr, testPolicy()};

    auto states = publish(
        service,
        activeSession(),
        application::write::PublishCastRequest{
            .text = "bad reply",
            .parent = domain::cast::CastReference{
                .author = domain::identity::Fid{7U},
                .id = domain::cast::CastId{"0xdeadbeef"},
            },
        });
    assert(states.size() == 1U);
    assert(states.back().phase == application::write::WritePhase::Failed);
    assert(authority.signCalls == 0U);
}


void replyRestartUsesExactStoredEnvelopeWithoutResigning()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);

    domain::write::IdempotencyKey key{};
    std::string storedEnvelope{};
    {
        application::write::WriteService offline{
            executor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto states = publish(
            offline,
            activeSession(),
            application::write::PublishCastRequest{
                .text = "durable retry reply",
                .parent = domain::cast::CastReference{
                    .author = domain::identity::Fid{7U},
                    .id = domain::cast::CastId{
                        "0x000102030405060708090a0b0c0d0e0f10111213"},
                },
            });
        assert(states.back().record);
        key.value = states.back().record->idempotencyKey;
        auto record = (*outbox)->find(key);
        assert(record && *record);
        storedEnvelope = (*record)->operation.messageBytes;
        assert(!storedEnvelope.empty());
    }
    assert(authority.signCalls == 1U);

    FakeSubmitter submitter{};
    application::write::WriteService resumed{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};
    assert(resumed.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(states.back().phase == application::write::WritePhase::Confirmed);
    assert(submitter.submitCalls == 1U);
    assert(submitter.submittedMessages.size() == 1U);
    assert(submitter.submittedMessages.front() == storedEnvelope);
    assert(authority.signCalls == 1U);
}

void successfulPublishSubmitsAndConfirms()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    FakeSubmitter submitter{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};

    auto states = publish(service);
    assert(!states.empty());
    assert(states.back().phase == application::write::WritePhase::Confirmed);
    assert(states.back().record);
    assert(states.back().record->status == application::write::DurableWriteStatus::Confirmed);
    assert(states.back().record->attemptCount == 1U);
    assert(submitter.submitCalls == 1U);
    assert(submitter.reconcileCalls == 0U);
    assert(authority.confirmedWriteCalls == 1U);
}

void socialWritesUseExactScopesAndDurableTargets()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, nullptr, testPolicy()};
    const domain::cast::CastReference target{
        .author = domain::identity::Fid{7U},
        .id = domain::cast::CastId{
            "0x000102030405060708090a0b0c0d0e0f10111213"},
    };

    std::vector<application::write::WriteState> states{};
    assert(service.setReaction(
        activeSession(), target, application::write::ReactionKind::Like, true,
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(!states.empty() && states.back().record);
    assert(states.back().record->kind == application::write::OperationKind::Like);
    assert(states.back().record->author.value == 42U);
    assert(states.back().record->target.cast);
    assert(states.back().record->target.cast->author.value == 7U);
    assert(!states.back().record->optimisticCast);
    assert(authority.lastScope == domain::signer::Scope::ReactionAdd);

    states.clear();
    assert(service.setReaction(
        activeSession(), target, application::write::ReactionKind::Like, false,
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(!states.empty() && states.back().record);
    assert(states.back().record->kind == application::write::OperationKind::Unlike);
    assert(authority.lastScope == domain::signer::Scope::ReactionRemove);

    states.clear();
    assert(service.setReaction(
        activeSession(), target, application::write::ReactionKind::Recast, true,
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(!states.empty() && states.back().record);
    assert(states.back().record->kind == application::write::OperationKind::Recast);
    assert(authority.lastScope == domain::signer::Scope::ReactionAdd);

    states.clear();
    assert(service.setReaction(
        activeSession(), target, application::write::ReactionKind::Recast, false,
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(!states.empty() && states.back().record);
    assert(states.back().record->kind == application::write::OperationKind::UndoRecast);
    assert(authority.lastScope == domain::signer::Scope::ReactionRemove);

    states.clear();
    assert(service.setFollowing(
        activeSession(), domain::identity::Fid{99U}, true,
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(!states.empty() && states.back().record);
    assert(states.back().record->kind == application::write::OperationKind::Follow);
    assert(states.back().record->target.fid && states.back().record->target.fid->value == 99U);
    assert(authority.lastScope == domain::signer::Scope::LinkAdd);

    states.clear();
    assert(service.setFollowing(
        activeSession(), domain::identity::Fid{99U}, false,
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(!states.empty() && states.back().record);
    assert(states.back().record->kind == application::write::OperationKind::Unfollow);
    assert(authority.lastScope == domain::signer::Scope::LinkRemove);
}

void removeCastUsesCastRemoveAndRejectsForeignOwnership()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, nullptr, testPolicy()};

    std::vector<application::write::WriteState> states{};
    assert(service.removeCast(
        activeSession(),
        domain::cast::CastReference{
            .author = domain::identity::Fid{42U},
            .id = domain::cast::CastId{
                "0x000102030405060708090a0b0c0d0e0f10111213"},
        },
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(!states.empty() && states.back().record);
    assert(states.back().record->kind == application::write::OperationKind::RemoveCast);
    assert(authority.lastScope == domain::signer::Scope::CastRemove);

    const auto signedBefore = authority.signCalls;
    states.clear();
    assert(service.removeCast(
        activeSession(),
        domain::cast::CastReference{
            .author = domain::identity::Fid{7U},
            .id = domain::cast::CastId{
                "0x000102030405060708090a0b0c0d0e0f10111213"},
        },
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::write::WritePhase::Failed);
    assert(authority.signCalls == signedBefore);
}

void restartReconcilesBeforeResubmit()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    {
        application::write::WriteService offline{
            executor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto states = publish(offline);
        assert(states.back().record);
    }

    auto pending = (*outbox)->listReady(
        domain::identity::Fid{42U}, foundation::time::now(), 1U);
    assert(pending && pending->size() == 1U);
    assert((*outbox)->beginAttempt(
        pending->front().operation.idempotencyKey, foundation::time::now()));

    FakeSubmitter submitter{};
    submitter.reconciliation = {capability::write::ConfirmationState::Confirmed};
    application::write::WriteService resumed{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};
    auto status = resumed.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    });
    assert(status);
    assert(!states.empty());
    assert(states.back().phase == application::write::WritePhase::Confirmed);
    assert(submitter.submitCalls == 0U);
    assert(submitter.reconcileCalls == 1U);
}

void transientFailureSchedulesBoundedRetry()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    FakeSubmitter submitter{};
    submitter.submitBehavior = SubmitBehavior::Unavailable;
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};

    auto states = publish(service);
    assert(states.back().phase == application::write::WritePhase::RetryScheduled);
    assert(states.back().record);
    assert(states.back().record->status == application::write::DurableWriteStatus::Pending);
    assert(states.back().record->attemptCount == 1U);
    assert(states.back().record->nextAttemptAt);
    assert(states.back().error);
    assert(states.back().error->message == "Test endpoint unavailable.");
    assert(submitter.submitCalls == 1U);
    assert(submitter.reconcileCalls == 0U);
}

void timeoutAfterMergeReconcilesBeforeAnyResubmit()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    FakeSubmitter submitter{};
    submitter.submitBehavior = SubmitBehavior::Timeout;
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};

    auto states = publish(service);
    assert(states.back().phase == application::write::WritePhase::RetryScheduled);
    assert(submitter.submitCalls == 1U);
    assert(submitter.reconcileCalls == 0U);

    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    submitter.submitBehavior = SubmitBehavior::Succeed;
    submitter.reconciliation = {capability::write::ConfirmationState::Confirmed};
    states.clear();
    assert(service.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(!states.empty());
    assert(states.back().phase == application::write::WritePhase::Confirmed);
    assert(submitter.reconcileCalls == 1U);
    assert(submitter.submitCalls == 1U);
}

void conflictWithDelayedIndexingNeverBlindlyResubmits()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    FakeSubmitter submitter{};
    submitter.submitBehavior = SubmitBehavior::Conflict;
    submitter.reconciliation = {capability::write::ConfirmationState::Absent};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);

    domain::write::IdempotencyKey key{};
    {
        application::write::WriteService service{
            executor, authority, *codec, **outbox, &submitter, testPolicy()};
        auto states = publish(service);
        assert(states.back().phase == application::write::WritePhase::RetryScheduled);
        assert(states.back().record);
        assert(submitter.submitCalls == 1U);
        assert(submitter.reconcileCalls == 1U);
        key.value = states.back().record->idempotencyKey;
        auto afterConflict = (*outbox)->find(key);
        assert(afterConflict && *afterConflict);
        assert((*afterConflict)->lastFailure);
        assert((*afterConflict)->lastFailure->kind == domain::write::FailureKind::Conflict);
    }

    // Restart after the ambiguous conflict. The durable Conflict marker must
    // keep the operation reconciliation-only in the new process/service.
    outbox->reset();
    auto reopened = storage::write::openOutboxStore(database.path);
    assert(reopened);
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    submitter.submitBehavior = SubmitBehavior::Succeed;
    submitter.reconciliation = {capability::write::ConfirmationState::Absent};
    application::write::WriteService resumed{
        executor, authority, *codec, **reopened, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};
    assert(resumed.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(!states.empty());
    assert(states.back().phase == application::write::WritePhase::RetryScheduled);
    assert(submitter.submitCalls == 1U);
    assert(submitter.reconcileCalls == 2U);

    std::this_thread::sleep_for(std::chrono::milliseconds{3});
    submitter.reconciliation = {capability::write::ConfirmationState::Confirmed};
    states.clear();
    assert(resumed.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(!states.empty());
    assert(states.back().phase == application::write::WritePhase::Confirmed);
    assert(submitter.submitCalls == 1U);
    assert(submitter.reconcileCalls == 3U);
}

void protocolRejectionIsTerminalButKeepsProjection()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    FakeSubmitter submitter{};
    submitter.submitBehavior = SubmitBehavior::Reject;
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    application::write::WriteService service{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};

    auto states = publish(service);
    assert(states.back().phase == application::write::WritePhase::Failed);
    assert(states.back().record);
    assert(states.back().record->status == application::write::DurableWriteStatus::Failed);
    assert(states.back().record->optimisticCast->text == "hello from Genycaster");
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::ProtocolFailure);
    assert(states.back().error->message == "Test protocol rejection.");
    assert(submitter.reconcileCalls == 0U);
}


void maximumAttemptsFailWithoutAnotherSubmit()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);

    domain::write::IdempotencyKey key{};
    {
        application::write::WriteService offline{
            executor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto states = publish(offline);
        assert(states.back().record);
        key.value = states.back().record->idempotencyKey;
    }
    assert((*outbox)->beginAttempt(key, foundation::time::now()));
    assert((*outbox)->beginAttempt(key, foundation::time::now()));
    assert((*outbox)->beginAttempt(key, foundation::time::now()));

    FakeSubmitter submitter{};
    submitter.reconciliation = {capability::write::ConfirmationState::Absent};
    application::write::WriteService resumed{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};
    assert(resumed.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::write::WritePhase::Failed);
    assert(states.back().record);
    assert(states.back().record->status == application::write::DurableWriteStatus::Failed);
    assert(states.back().record->attemptCount == 3U);
    assert(submitter.reconcileCalls == 1U);
    assert(submitter.submitCalls == 0U);
}

void concurrentResumeWakeupsAreCoalesced()
{
    DatabaseFixture database{};
    InlineExecutor seedExecutor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    {
        application::write::WriteService offline{
            seedExecutor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto states = publish(offline);
        assert(states.back().record);
    }

    QueuedExecutor queuedExecutor{};
    FakeSubmitter submitter{};
    application::write::WriteService resumed{
        queuedExecutor, authority, *codec, **outbox, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};
    const auto observer = [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    };

    assert(resumed.resume(activeSession(), observer));
    assert(resumed.resume(activeSession(), observer));
    assert(queuedExecutor.jobs.size() == 1U);
    queuedExecutor.runNext();
    assert(states.back().phase == application::write::WritePhase::Confirmed);

    // Once the in-flight resume completes, a later OS/timer wakeup may schedule
    // another scan normally.
    assert(resumed.resume(activeSession(), observer));
    assert(queuedExecutor.jobs.size() == 1U);
    queuedExecutor.runNext();
}

void resumeNeverCrossesAccountBoundary()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);
    {
        application::write::WriteService offline{
            executor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto states = publish(
            offline,
            activeSession(),
            application::write::PublishCastRequest{
                .text = "account isolated reply",
                .parent = domain::cast::CastReference{
                    .author = domain::identity::Fid{7U},
                    .id = domain::cast::CastId{
                        "0x000102030405060708090a0b0c0d0e0f10111213"},
                },
            });
        assert(states.back().record);
        assert(states.back().record->optimisticCast->parent);
    }

    auto otherSession = activeSession();
    otherSession.fid = domain::identity::Fid{99U};
    authority.fid = domain::identity::Fid{99U};
    FakeSubmitter submitter{};
    application::write::WriteService otherAccount{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};
    assert(otherAccount.resume(otherSession, [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(states.empty());
    assert(submitter.reconcileCalls == 0U);
    assert(submitter.submitCalls == 0U);
}

void terminalSignerStateInvalidatesEvenFutureRetriesWithoutNetwork()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);

    domain::write::IdempotencyKey key{};
    {
        application::write::WriteService offline{
            executor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto states = publish(offline);
        assert(states.back().record);
        key.value = states.back().record->idempotencyKey;
    }
    assert((*outbox)->markRetry(
        key,
        domain::write::OutboxFailure{
            .kind = domain::write::FailureKind::Unavailable,
            .message = "offline"},
        foundation::time::now() + std::chrono::hours{1}));

    FakeSubmitter submitter{};
    application::write::WriteService service{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};
    assert(service.invalidatePendingAuthority(
        activeSession(),
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::write::WritePhase::Failed);
    assert(states.back().record);
    assert(states.back().record->status == application::write::DurableWriteStatus::Failed);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::Unauthorized);
    assert(submitter.submitCalls == 0U);
    assert(submitter.reconcileCalls == 0U);

    auto record = (*outbox)->find(key);
    assert(record && *record);
    assert((*record)->status == domain::write::OutboxStatus::Failed);
    assert((*record)->lastFailure);
    assert((*record)->lastFailure->kind == domain::write::FailureKind::Authorization);
}

void revokedOrReplacedSignerStopsQueuedWriteBeforeNetwork()
{
    DatabaseFixture database{};
    InlineExecutor executor{};
    FakeAuthority authority{};
    auto codec = protocol::write::makeCanonicalWriteCodec();
    auto outbox = storage::write::openOutboxStore(database.path);
    assert(codec && outbox);

    domain::write::IdempotencyKey revokedKey{};
    {
        application::write::WriteService offline{
            executor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto states = publish(offline);
        assert(states.back().record);
        revokedKey.value = states.back().record->idempotencyKey;
    }

    FakeSubmitter submitter{};
    authority.allowed = false;
    application::write::WriteService resumed{
        executor, authority, *codec, **outbox, &submitter, testPolicy()};
    std::vector<application::write::WriteState> states{};

    // A generic temporarily-not-ready authority blocks rather than destroying a
    // durable record. Authoritative revocation/expiry is a distinct lifecycle
    // event and explicitly invalidates all queued envelopes.
    assert(resumed.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::write::WritePhase::Blocked);
    assert(submitter.submitCalls == 0U);
    assert(submitter.reconcileCalls == 0U);

    states.clear();
    assert(resumed.invalidatePendingAuthority(
        activeSession(),
        [&states](application::write::WriteState state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::write::WritePhase::Failed);
    assert(states.back().record);
    assert(states.back().record->status == application::write::DurableWriteStatus::Failed);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::Unauthorized);
    auto revokedRecord = (*outbox)->find(revokedKey);
    assert(revokedRecord && *revokedRecord);
    assert((*revokedRecord)->status == domain::write::OutboxStatus::Failed);

    // A separately queued envelope signed by the old key also fails closed if
    // the account later provisions a replacement signer.
    authority.allowed = true;
    authority.key.hex =
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    {
        application::write::WriteService offline{
            executor, authority, *codec, **outbox, nullptr, testPolicy()};
        auto queued = publish(
            offline,
            activeSession(),
            application::write::PublishCastRequest{.text = "replacement signer boundary"});
        assert(queued.back().record);
    }
    authority.key.hex =
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    states.clear();
    assert(resumed.resume(activeSession(), [&states](application::write::WriteState state) {
        states.push_back(std::move(state));
    }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::write::WritePhase::Failed);
    assert(states.back().record);
    assert(states.back().record->status == application::write::DurableWriteStatus::Failed);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::Unauthorized);
    assert(submitter.submitCalls == 0U);
    assert(submitter.reconcileCalls == 0U);
}

} // namespace

int main()
{
    unsignedAndExpiredSessionsAreBlockedBeforeSigning();
    offlinePublishIsDurableAndOptimistic();
    offlineReplyIsDurableAndRetainsCanonicalParent();
    offlineThreadPersistsParentChainAndDependencies();
    failedThreadParentBlocksChildrenWithoutNetworkSubmission();
    socialWritesUseExactScopesAndDurableTargets();
    removeCastUsesCastRemoveAndRejectsForeignOwnership();
    invalidReplyParentFailsBeforeSigningOrEnqueue();
    replyRestartUsesExactStoredEnvelopeWithoutResigning();
    successfulPublishSubmitsAndConfirms();
    restartReconcilesBeforeResubmit();
    transientFailureSchedulesBoundedRetry();
    timeoutAfterMergeReconcilesBeforeAnyResubmit();
    conflictWithDelayedIndexingNeverBlindlyResubmits();
    protocolRejectionIsTerminalButKeepsProjection();
    maximumAttemptsFailWithoutAnotherSubmit();
    concurrentResumeWakeupsAreCoalesced();
    resumeNeverCrossesAccountBoundary();
    terminalSignerStateInvalidatesEvenFutureRetriesWithoutNetwork();
    revokedOrReplacedSignerStopsQueuedWriteBeforeNetwork();
    return 0;
}
