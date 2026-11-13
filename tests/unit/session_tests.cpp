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

class FakeLinkingAttempt final : public capability::auth::IAuthenticationAttempt {
public:
    explicit FakeLinkingAttempt(domain::session::ClientSession session)
        : m_session(std::move(session))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::PollResult> poll() override
    {
        if (!m_linkReturned) {
            m_linkReturned = true;
            return capability::auth::PollResult{
                .state = capability::auth::PollState::FarcasterLinkRequired,
                .approvalUrl = std::string{"https://farcaster.example/link"},
            };
        }
        return capability::auth::PollResult{
            .state = capability::auth::PollState::Completed,
            .session = m_session,
        };
    }

private:
    domain::session::ClientSession m_session;
    bool m_linkReturned{false};
};

class FakeLinkingAuthentication final : public capability::auth::IAuthenticationProvider {
public:
    explicit FakeLinkingAuthentication(domain::session::ClientSession session)
        : m_session(std::move(session))
    {
    }

    [[nodiscard]] foundation::result::Result<capability::auth::StartedAuthentication> begin(
        const capability::auth::SignInRequest& request) override
    {
        assert(request.route == capability::auth::SignInRoute::OpenProof);
        return capability::auth::StartedAuthentication{
            .challenge = capability::auth::AuthenticationChallenge{
                .approvalUrl = "https://genyleap.example/account",
            },
            .attempt = std::make_unique<FakeLinkingAttempt>(m_session),
        };
    }

private:
    domain::session::ClientSession m_session;
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
        if (m_failCustodyResolution) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::ProtocolFailure,
                "Synthetic custody lookup failure."));
        }
        return m_resolvedCustodyAddress;
    }

    void setResolvedCustodyAddress(std::string value)
    {
        m_resolvedCustodyAddress = std::move(value);
    }

    void setCustodyResolutionFailure(const bool value) noexcept
    {
        m_failCustodyResolution = value;
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
    bool m_failCustodyResolution{false};
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
    application::session::SessionService service{executor, store, nullptr, nullptr};

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
    application::session::SessionService service{
        executor, store, &authentication, &authentication, &authentication};

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
    application::session::SessionService service{
        executor, store, &authentication, &authentication, &authentication};
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

void openProofSignInBlocksOnRequiredFarcasterLink()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    auto session = makeSession(888);
    session.identityIssuer = std::string{"https://openproof.example"};
    session.identitySubject = std::string{"identity-888"};
    FakeLinkingAuthentication authentication{session};
    application::session::SessionService service{executor, store, &authentication, nullptr};
    std::vector<application::session::SessionState> states{};
    const auto observer = [&states](auto state) { states.push_back(std::move(state)); };

    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .route = capability::auth::SignInRoute::OpenProof,
        },
        observer));
    assert(states.size() == 2U);
    assert(states.back().phase == application::session::SessionPhase::AwaitingApproval);
    assert(states.back().approvalUrl == "https://genyleap.example/account");
    assert(!store.value);

    assert(service.pollSignIn(observer));
    assert(states.back().phase == application::session::SessionPhase::LinkingFarcaster);
    assert(states.back().approvalUrl == "https://farcaster.example/link");
    assert(!store.value);

    assert(service.pollSignIn(observer));
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->fid.value == 888U);
    assert(states.back().session->identitySubject &&
        *states.back().session->identitySubject == "identity-888");
    assert(store.value && store.value->fid.value == 888U);
}

void custodyResolutionFailureDoesNotInvalidateOpenProofAuthentication()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    auto session = makeSession(889);
    session.identityIssuer = std::string{"https://openproof.example"};
    session.identitySubject = std::string{"identity-889"};
    session.custodyAddress.reset();

    FakeLinkingAuthentication authentication{session};
    FakeAuthentication resolver{makeSession(777)};
    resolver.setCustodyResolutionFailure(true);
    application::session::SessionService service{executor, store, &authentication, &resolver};
    std::vector<application::session::SessionState> states{};
    const auto observer = [&states](auto state) { states.push_back(std::move(state)); };

    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .route = capability::auth::SignInRoute::OpenProof,
        },
        observer));
    assert(service.pollSignIn(observer));
    assert(states.back().phase == application::session::SessionPhase::LinkingFarcaster);
    assert(service.pollSignIn(observer));
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->fid.value == 889U);
    assert(!states.back().session->custodyAddress);
    assert(store.value && store.value->fid.value == 889U);
    assert(!store.value->custodyAddress);
    assert(resolver.custodyResolutionCount() == 1U);
}

void custodyResolutionFailureDoesNotInvalidateRestore()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    auto session = makeSession(890);
    session.identityIssuer = std::string{"https://openproof.example"};
    session.identitySubject = std::string{"identity-890"};
    session.custodyAddress.reset();
    store.value = session;

    FakeAuthentication resolver{makeSession(777)};
    resolver.setCustodyResolutionFailure(true);
    application::session::SessionService service{executor, store, nullptr, &resolver};
    std::vector<application::session::SessionState> states{};

    assert(service.restore([&states](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->fid.value == 890U);
    assert(!states.back().session->custodyAddress);
    assert(store.saveCount == 0U);
    assert(resolver.custodyResolutionCount() == 1U);
}

void reauthenticationKeepsExistingSessionUntilReplacementCompletes()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    store.value = makeSession(42);
    FakeAuthentication authentication{makeSession(777)};
    application::session::SessionService service{
        executor, store, &authentication, &authentication, &authentication};
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

void independentFarcasterSignInDoesNotInvokeOpenProof()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    auto genyleapSession = makeSession(901);
    genyleapSession.identityIssuer = std::string{"https://openproof.example"};
    genyleapSession.identitySubject = std::string{"genyleap-901"};
    FakeLinkingAuthentication genyleap{genyleapSession};
    FakeAuthentication farcaster{makeSession(902)};
    application::session::SessionService service{
        executor, store, &genyleap, &farcaster, &farcaster};
    std::vector<application::session::SessionState> states{};
    const auto observe = [&states](auto state) { states.push_back(std::move(state)); };

    // This must invoke the native SIWF adapter, never the OpenProof fake
    // (which asserts that only the OpenProof route is accepted).
    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .route = capability::auth::SignInRoute::Farcaster,
            .domain = "genycaster.example",
            .siweUri = "https://genycaster.example/sign-in",
        }, observe));
    assert(states.back().approvalUrl == "https://farcaster.example/approve");
    assert(store.saveCount == 0U);
    assert(service.pollSignIn(observe));
    assert(service.pollSignIn(observe));
    assert(states.back().session && states.back().session->fid.value == 902U);
    assert(!states.back().session->identityIssuer);
    assert(store.saveCount == 1U);

    // Genyleap remains the independent OpenProof route.
    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .route = capability::auth::SignInRoute::OpenProof,
        }, observe));
    assert(states.back().approvalUrl == "https://genyleap.example/account");
    assert(service.pollSignIn(observe));
    assert(states.back().phase == application::session::SessionPhase::LinkingFarcaster);
    assert(service.pollSignIn(observe));
    assert(states.back().session && states.back().session->fid.value == 901U);
    assert(states.back().session->identityIssuer);
    assert(store.saveCount == 2U);
}

void missingDirectFarcasterProviderNeverFallsBackToOpenProof()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    FakeLinkingAuthentication openProof{makeSession(999)};
    application::session::SessionService service{
        executor, store, &openProof, nullptr, nullptr};
    std::vector<application::session::SessionState> states{};
    assert(service.beginSignIn(
        capability::auth::SignInRequest{
            .route = capability::auth::SignInRoute::Farcaster,
            .domain = "genycaster.example",
            .siweUri = "https://genycaster.example/sign-in",
        },
        [&states](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 1U);
    assert(states.back().phase == application::session::SessionPhase::Failed);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::Unsupported);
    assert(store.saveCount == 0U);
}

void missingCompatibilityAuthDoesNotCreateSession()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    application::session::SessionService service{executor, store, nullptr, nullptr};
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

void verifiedNativeRecoveryUsesAuthoritativeSessionStore()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    application::session::SessionService service{executor, store, nullptr, nullptr};
    std::vector<application::session::SessionState> states{};
    const auto observer = [&states](auto state) {
        states.push_back(std::move(state));
    };

    auto recovered = makeSession(123);
    recovered.identityIssuer = "https://openproof.genyleap.com";
    recovered.identitySubject = "account-123";
    recovered.expiresAt = recovered.authenticatedAt + std::chrono::minutes{35};
    assert(service.acceptVerifiedRecovery(recovered, observer));
    assert(store.saveCount == 1U);
    assert(store.value && store.value->fid.value == 123U);
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session
        && states.back().session->custodyAddress == recovered.custodyAddress);

    // The same store is durable across SessionService reconstruction and
    // signOut must clear it for recovery just as for ordinary sign-in.
    {
        application::session::SessionService restored{executor, store, nullptr, nullptr};
        assert(restored.restore(observer));
        assert(states.back().session && states.back().session->fid.value == 123U);
    }
    assert(service.signOut(observer));
    assert(!store.value && store.clearCount == 1U);
    assert(states.back().phase == application::session::SessionPhase::Guest);

    // Invalid issuer, owner, address and expired/overlong proof never publish
    // a login or overwrite an existing stored session.
    const auto rejected = [&](domain::session::ClientSession input) {
        const auto count = store.saveCount;
        assert(service.acceptVerifiedRecovery(std::move(input), observer));
        assert(store.saveCount == count);
        assert(states.back().phase == application::session::SessionPhase::Failed);
        assert(!states.back().session);
        assert(states.back().error);
        assert(states.back().error->code == foundation::error::ErrorCode::Unauthorized);
    };
    auto invalid = recovered;
    invalid.identityIssuer = "https://attacker.example";
    rejected(invalid);
    invalid = recovered;
    invalid.fid = domain::identity::Fid{0};
    rejected(invalid);
    invalid = recovered;
    invalid.identitySubject = "";
    rejected(invalid);
    invalid = recovered;
    invalid.custodyAddress = "0xNOTVALID";
    rejected(invalid);
    invalid = recovered;
    invalid.expiresAt = foundation::time::now() - std::chrono::seconds{1};
    rejected(invalid);
    invalid = recovered;
    invalid.expiresAt = foundation::time::now() + std::chrono::hours{2};
    rejected(invalid);
}

void verifiedHyperSnapCustodyUsesDistinctNativeSession()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    application::session::SessionService service{executor, store, nullptr, nullptr};
    std::vector<application::session::SessionState> states{};
    const auto observe = [&states](auto state) {
        states.push_back(std::move(state));
    };
    constexpr std::uint64_t nativeFid = 1ULL << 32;
    auto valid = makeSession(nativeFid);
    valid.identityIssuer = "hypersnap:native-registry";
    valid.identitySubject = "native-fid:4294967296";
    valid.expiresAt = valid.authenticatedAt + std::chrono::minutes{30};
    assert(service.acceptVerifiedNativeCustody(valid, observe));
    assert(store.saveCount == 1U);
    assert(store.value && store.value->fid.value == nativeFid);
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->identityIssuer
        == "hypersnap:native-registry");
    assert(service.signOut(observe));
    assert(!store.value);

    const auto rejected = [&](domain::session::ClientSession input) {
        const auto originalSaves = store.saveCount;
        assert(service.acceptVerifiedNativeCustody(std::move(input), observe));
        assert(store.saveCount == originalSaves);
        assert(states.back().phase == application::session::SessionPhase::Failed);
        assert(states.back().error);
        assert(states.back().error->code == foundation::error::ErrorCode::Unauthorized);
    };
    auto invalid = valid;
    invalid.fid = domain::identity::Fid{123}; // Never accept Farcaster FID.
    rejected(invalid);
    invalid = valid;
    invalid.identityIssuer = "https://openproof.genyleap.com";
    rejected(invalid);
    invalid = valid;
    invalid.identitySubject = "native-fid:4294967297"; // FID spoofing.
    rejected(invalid);
    invalid = valid;
    invalid.custodyAddress = "0xBAD";
    rejected(invalid);
    invalid = valid;
    invalid.expiresAt = invalid.authenticatedAt + std::chrono::hours{2};
    rejected(invalid);
    invalid = valid;
    invalid.expiresAt = foundation::time::now() - std::chrono::seconds{1};
    rejected(invalid);
}

void verifiedOptimismFarcasterCustodyRemainsDistinctFromHyperSnap()
{
    ImmediateExecutor executor{};
    MemorySessionStore store{};
    application::session::SessionService service{executor, store, nullptr, nullptr};
    std::vector<application::session::SessionState> states{};
    const auto observe = [&states](auto state) {
        states.push_back(std::move(state));
    };
    auto valid = makeSession(1234567);
    valid.identityIssuer = "farcaster:optimism-id-registry";
    valid.identitySubject = "farcaster:1234567";
    valid.expiresAt = valid.authenticatedAt + std::chrono::minutes{30};
    assert(service.acceptVerifiedFarcasterCustody(valid, observe));
    assert(store.saveCount == 1U);
    assert(states.back().phase == application::session::SessionPhase::Authenticated);
    assert(states.back().session && states.back().session->identityIssuer
        == "farcaster:optimism-id-registry");
    assert(service.signOut(observe));
    assert(!store.value);
    const auto reject = [&](domain::session::ClientSession input) {
        const auto initial = store.saveCount;
        assert(service.acceptVerifiedFarcasterCustody(std::move(input), observe));
        assert(store.saveCount == initial);
        assert(states.back().phase == application::session::SessionPhase::Failed);
        assert(states.back().error
            && states.back().error->code == foundation::error::ErrorCode::Unauthorized);
    };
    auto bad = valid;
    bad.identityIssuer = "hypersnap:native-registry";
    reject(bad);
    bad = valid;
    bad.identitySubject = "farcaster:1234568";
    reject(bad);
    bad = valid;
    bad.custodyAddress = "0xBAD";
    reject(bad);
    bad = valid;
    bad.fid = domain::identity::Fid{0};
    reject(bad);
    bad = valid;
    bad.expiresAt = foundation::time::now() - std::chrono::seconds{1};
    reject(bad);
    bad = valid;
    bad.expiresAt = bad.authenticatedAt + std::chrono::hours{2};
    reject(bad);
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
    session.identityIssuer = std::string{"https://openproof.example"};
    session.identitySubject = std::string{"identity-991"};
    session.method = domain::session::AuthenticationMethod::AuthAddress;
    assert((*store)->save(session));

    auto loaded = (*store)->load();
    assert(loaded && *loaded);
    assert((*loaded)->identityIssuer &&
        *(*loaded)->identityIssuer == "https://openproof.example");
    assert((*loaded)->identitySubject &&
        *(*loaded)->identitySubject == "identity-991");
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
    openProofSignInBlocksOnRequiredFarcasterLink();
    custodyResolutionFailureDoesNotInvalidateOpenProofAuthentication();
    custodyResolutionFailureDoesNotInvalidateRestore();
    reauthenticationKeepsExistingSessionUntilReplacementCompletes();
    independentFarcasterSignInDoesNotInvokeOpenProof();
    missingDirectFarcasterProviderNeverFallsBackToOpenProof();
    missingCompatibilityAuthDoesNotCreateSession();
    verifiedNativeRecoveryUsesAuthoritativeSessionStore();
    verifiedHyperSnapCustodyUsesDistinctNativeSession();
    verifiedOptimismFarcasterCustodyRemainsDistinctFromHyperSnap();
    sqliteSessionRoundTrip();
    return 0;
}
