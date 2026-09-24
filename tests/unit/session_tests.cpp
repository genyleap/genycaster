#include <cassert>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

import farcaster.application.executor;
import farcaster.application.session;
import farcaster.capability.auth;
import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.session;
import farcaster.storage.session;

namespace {

namespace application = farcaster::application;
namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace local = farcaster::local;

class ImmediateExecutor final : public application::executor::IExecutor {
public:
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override
    {
        job();
        return {};
    }
};

class MemorySessionStore final : public local::session::IClientSessionStore {
public:
    std::optional<domain::session::ClientSession> value{};
    std::size_t saveCount{};
    std::size_t clearCount{};

    [[nodiscard]] foundation::result::Result<std::optional<domain::session::ClientSession>>
    load() override
    {
        return value;
    }

    [[nodiscard]] foundation::result::Status save(
        const domain::session::ClientSession& session) override
    {
        value = session;
        ++saveCount;
        return {};
    }

    [[nodiscard]] foundation::result::Status clear() override
    {
        value.reset();
        ++clearCount;
        return {};
    }
};

[[nodiscard]] domain::session::ClientSession makeSession(
    const std::uint64_t fid,
    const std::chrono::hours lifetime = std::chrono::hours{1})
{
    const auto authenticatedAt = foundation::time::now();
    return domain::session::ClientSession{
        .fid = domain::identity::Fid{fid},
        .username = std::string{"alice"},
        .displayName = std::string{"Alice"},
        .avatarUrl = std::string{"https://example.com/alice.png"},
        .custodyAddress = std::string{"0x1111111111111111111111111111111111111111"},
        .method = domain::session::AuthenticationMethod::Custody,
        .authenticatedAt = authenticatedAt,
        .expiresAt = authenticatedAt +
            std::chrono::duration_cast<std::chrono::milliseconds>(lifetime),
    };
}

class FakeAttempt final : public capability::auth::IAuthenticationAttempt {
public:
    explicit FakeAttempt(domain::session::ClientSession session)
        : m_session(std::move(session))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> poll() override
    {
        if (!m_pendingReturned) {
            m_pendingReturned = true;
            return capability::auth::PollResult{
                .state = capability::auth::PollState::Pending,
            };
        }
        return capability::auth::PollResult{
            .state = capability::auth::PollState::Completed,
            .session = m_session,
        };
    }

private:
    domain::session::ClientSession m_session;
    bool m_pendingReturned{false};
};

class FakeAuthentication final : public capability::auth::ICompatibilityAuthentication {
public:
    explicit FakeAuthentication(domain::session::ClientSession session)
        : m_session(std::move(session))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::StartedAuthentication> begin(
        const capability::auth::SignInRequest& request) override
    {
        assert(request.domain == "genycaster.example");
        assert(request.siweUri == "https://genycaster.example/sign-in");
        return capability::auth::StartedAuthentication{
            .challenge = capability::auth::AuthenticationChallenge{
                .approvalUrl = "https://farcaster.example/approve",
            },
            .attempt = std::make_unique<FakeAttempt>(m_session),
        };
    }

    [[nodiscard]] foundation::result::Result<std::string> resolveCustodyAddress(
        const domain::identity::Fid fid) override
    {
        ++m_custodyResolutionCount;
        m_lastResolvedFid = fid;
        return m_resolvedCustodyAddress;
    }

    void setResolvedCustodyAddress(std::string value)
    {
        m_resolvedCustodyAddress = std::move(value);
    }

    [[nodiscard]] std::size_t custodyResolutionCount() const noexcept
    {
        return m_custodyResolutionCount;
    }

    [[nodiscard]] std::optional<domain::identity::Fid> lastResolvedFid() const noexcept
    {
        return m_lastResolvedFid;
    }

private:
    domain::session::ClientSession m_session;
    std::string m_resolvedCustodyAddress{"0x2222222222222222222222222222222222222222"};
    std::size_t m_custodyResolutionCount{};
    std::optional<domain::identity::Fid> m_lastResolvedFid{};
};

struct DatabaseFixture final {
    DatabaseFixture()
    {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            ("genycaster-session-" + std::to_string(unique) + ".sqlite3");
    }

    ~DatabaseFixture()
    {
        std::error_code error{};
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + "-wal", error);
        std::filesystem::remove(path.string() + "-shm", error);
    }

    std::filesystem::path path{};
};

void writeAccessRequiresBothSessionAndSigner()
{
    const application::session::SessionState guest{};
    assert(application::session::evaluateWriteAccess(guest, false)
        == application::session::WriteAccess::SignInRequired);
    assert(application::session::evaluateWriteAccess(guest, true)
        == application::session::WriteAccess::SignInRequired);

    const application::session::SessionState authenticated{
        .phase = application::session::SessionPhase::Authenticated,
        .session = makeSession(42),
    };
    assert(application::session::evaluateWriteAccess(authenticated, false)
        == application::session::WriteAccess::SignerRequired);
    assert(application::session::evaluateWriteAccess(authenticated, true)
        == application::session::WriteAccess::Ready);
}

void expiredRestoreFallsBackToGuest()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    auto expired = makeSession(42);
    expired.expiresAt = foundation::time::now() - std::chrono::milliseconds{1};
    store.value = expired;
    application::session::SessionService service{executor, store, nullptr};

    std::vector<application::session::SessionState> states{};
    assert(service.restore([&states](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1);
    assert(states.back().phase == application::session::SessionPhase::Guest);
    assert(!store.value);
    assert(store.clearCount == 1);
}

void legacyRestoreRefreshesCustodyWithoutNewSignIn()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    auto legacy = makeSession(42);
    legacy.custodyAddress.reset();
    store.value = legacy;

    FakeAuthentication authentication{makeSession(777)};
    authentication.setResolvedCustodyAddress(
        "0x3333333333333333333333333333333333333333");
    application::session::SessionService service{executor, store, &authentication};

    std::vector<application::session::SessionState> states{};
    assert(service.restore([&states](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->fid.value == 42U);
    assert(states.back().session->custodyAddress);
    assert(*states.back().session->custodyAddress ==
        "0x3333333333333333333333333333333333333333");
    assert(authentication.custodyResolutionCount() == 1U);
    assert(authentication.lastResolvedFid() && authentication.lastResolvedFid()->value == 42U);
    assert(store.saveCount == 1U);
    assert(store.value && store.value->custodyAddress == states.back().session->custodyAddress);
}

void signInPersistsOnlyAfterVerifiedCompletion()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    FakeAuthentication authentication{makeSession(777)};
    application::session::SessionService service{executor, store, &authentication};
    std::vector<application::session::SessionState> states{};
    const auto observer = [&states](auto state) { states.push_back(std::move(state)); };

    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .domain = "genycaster.example",
            .siweUri = "https://genycaster.example/sign-in",
        },
        observer));
    assert(states.size() == 2);
    assert(states[0].phase == application::session::SessionPhase::Starting);
    assert(states[1].phase == application::session::SessionPhase::AwaitingApproval);
    assert(states[1].approvalUrl == "https://farcaster.example/approve");
    assert(store.saveCount == 0);

    assert(service.pollSignIn(observer));
    assert(states.back().phase == application::session::SessionPhase::AwaitingApproval);
    assert(store.saveCount == 0);

    assert(service.pollSignIn(observer));
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->fid.value == 777);
    assert(store.value && store.value->fid.value == 777);
    assert(store.saveCount == 1);

    assert(service.signOut(observer));
    assert(states.back().phase == application::session::SessionPhase::Guest);
    assert(!store.value);
    assert(store.clearCount == 1);
}

void reauthenticationKeepsExistingSessionUntilReplacementCompletes()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    store.value = makeSession(42);
    FakeAuthentication authentication{makeSession(777)};
    application::session::SessionService service{executor, store, &authentication};
    std::vector<application::session::SessionState> states{};
    const auto observer = [&states](auto state) { states.push_back(std::move(state)); };

    assert(service.restore(observer));
    assert(states.back().session && states.back().session->fid.value == 42);

    states.clear();
    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .domain = "genycaster.example",
            .siweUri = "https://genycaster.example/sign-in",
        },
        observer));
    assert(states.size() == 2);
    for (const auto& state : states) {
        assert(state.session && state.session->fid.value == 42);
        assert(application::session::evaluateWriteAccess(state, false)
            == application::session::WriteAccess::SignerRequired);
        assert(application::session::evaluateWriteAccess(state, true)
            == application::session::WriteAccess::Ready);
    }

    assert(service.cancelSignIn(observer));
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->fid.value == 42);
    assert(store.value && store.value->fid.value == 42);
}

void missingCompatibilityAuthDoesNotCreateSession()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    application::session::SessionService service{executor, store, nullptr};
    std::vector<application::session::SessionState> states{};
    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .domain = "genycaster.example",
            .siweUri = "https://genycaster.example/sign-in",
        },
        [&states](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1);
    assert(states.back().phase == application::session::SessionPhase::Failed);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::Unsupported);
    assert(!store.value);
}

void sqliteSessionRoundTrip()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::session::openSessionStore(database.path);
    assert(store);
    auto initial = (*store)->load();
    assert(initial);
    assert(!*initial);

    auto session = makeSession(991);
    session.method = domain::session::AuthenticationMethod::AuthAddress;
    assert((*store)->save(session));

    auto loaded = (*store)->load();
    assert(loaded && *loaded);
    assert((*loaded)->fid.value == 991);
    assert((*loaded)->username && *(*loaded)->username == "alice");
    assert((*loaded)->custodyAddress && *(*loaded)->custodyAddress ==
        "0x1111111111111111111111111111111111111111");
    assert((*loaded)->method == domain::session::AuthenticationMethod::AuthAddress);
    assert((*loaded)->expiresAt == session.expiresAt);

    assert((*store)->clear());
    loaded = (*store)->load();
    assert(loaded && !*loaded);
}

}

int main()
{
    writeAccessRequiresBothSessionAndSigner();
    expiredRestoreFallsBackToGuest();
    legacyRestoreRefreshesCustodyWithoutNewSignIn();
    signInPersistsOnlyAfterVerifiedCompletion();
    reauthenticationKeepsExistingSessionUntilReplacementCompletes();
    missingCompatibilityAuthDoesNotCreateSession();
    sqliteSessionRoundTrip();
    return 0;
}
