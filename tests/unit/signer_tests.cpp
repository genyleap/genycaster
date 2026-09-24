#include <cassert>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

import farcaster.application.executor;
import farcaster.application.signer;
import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.domain.identity;
import farcaster.domain.session;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.signer;
import farcaster.storage.signer;

namespace {

namespace application = farcaster::application;
namespace capability = farcaster::capability;
namespace domain = farcaster::domain;
namespace foundation = farcaster::foundation;
namespace local = farcaster::local;

constexpr std::string_view PublicKeyHex =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

[[nodiscard]] foundation::error::Error error(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

class ImmediateExecutor final : public application::executor::IExecutor {
public:
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override
    {
        job();
        return {};
    }
};

[[nodiscard]] domain::session::ClientSession makeSession(const std::uint64_t fid)
{
    const auto now = foundation::time::now();
    return domain::session::ClientSession{
        .fid = domain::identity::Fid{fid},
        .username = std::string{"alice"},
        .displayName = std::string{"Alice"},
        .custodyAddress = std::string{"0x1111111111111111111111111111111111111111"},
        .method = domain::session::AuthenticationMethod::Custody,
        .authenticatedAt = now,
        .expiresAt = now + std::chrono::hours{1},
    };
}

class FakeNativeSigner final : public capability::signer::INativeSigner {
public:
    explicit FakeNativeSigner(domain::identity::Fid fid) : m_fid(fid) {}

    [[nodiscard]] domain::signer::SignerId signerId() const override
    {
        return domain::signer::SignerId{"fake-vault:fid:" + std::to_string(m_fid.value)};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> publicKey() const override
    {
        return domain::signer::PublicKey{std::string{PublicKeyHex}};
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Signature>
    sign(const std::string& bytes) override
    {
        if (bytes.empty()) {
            return std::unexpected(error(
                foundation::error::ErrorCode::InvalidArgument,
                "Fake signer rejects empty bytes."));
        }
        ++signCount;
        return domain::signer::Signature{std::string(64U, 's')};
    }

    std::size_t signCount{};

private:
    domain::identity::Fid m_fid{};
};

class MemoryVault final : public capability::signer::ISignerVault {
public:
    bool exists{};
    bool failLoad{};
    bool failEnsure{};
    std::size_t eraseCount{};

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    load(const domain::identity::Fid fid) override
    {
        if (failLoad) {
            return std::unexpected(error(
                foundation::error::ErrorCode::StorageFailure,
                "Secure vault load failure."));
        }
        if (!exists) {
            return std::unique_ptr<capability::signer::INativeSigner>{};
        }
        return std::unique_ptr<capability::signer::INativeSigner>{
            std::make_unique<FakeNativeSigner>(fid)};
    }

    [[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer::INativeSigner>>
    ensure(const domain::identity::Fid fid) override
    {
        if (failEnsure) {
            return std::unexpected(error(
                foundation::error::ErrorCode::StorageFailure,
                "Secure vault generation failure."));
        }
        exists = true;
        return std::unique_ptr<capability::signer::INativeSigner>{
            std::make_unique<FakeNativeSigner>(fid)};
    }

    [[nodiscard]] foundation::result::Status erase(domain::identity::Fid) override
    {
        exists = false;
        ++eraseCount;
        return {};
    }
};

class MemoryMetadataStore final : public local::signer::ISignerMetadataStore {
public:
    std::optional<domain::signer::SignerRecord> record{};
    std::size_t saveCount{};

    [[nodiscard]] foundation::result::Result<std::optional<domain::signer::SignerRecord>>
    load(domain::identity::Fid) override
    {
        return record;
    }

    [[nodiscard]] foundation::result::Status save(
        const domain::signer::SignerRecord& value) override
    {
        record = value;
        ++saveCount;
        return {};
    }

    [[nodiscard]] foundation::result::Status clear(domain::identity::Fid) override
    {
        record.reset();
        return {};
    }
};

[[nodiscard]] domain::signer::Registration activeRegistration(
    const domain::identity::Fid fid,
    const std::vector<domain::signer::Scope>& scopes = domain::signer::standardSocialScopes())
{
    const auto now = foundation::time::now();
    return domain::signer::Registration{
        .fid = fid,
        .publicKey = domain::signer::PublicKey{std::string{PublicKeyHex}},
        .kind = domain::signer::RegistrationKind::Gasless,
        .state = domain::signer::RegistrationState::Active,
        .scopes = scopes,
        .ttlSeconds = domain::signer::DefaultGaslessTtlSeconds,
        .lastUsedAt = now,
        .requestFid = fid,
        .verifiedAt = now,
    };
}

class FakeAuthorizer final : public capability::signer::ICustodyAuthorizer {
public:
    std::optional<capability::signer::KeyAddRequest> lastRequest{};
    bool fail{};
    std::optional<capability::signer_attestation::SignedKeyRequestAttestation> lastAttestation{};

    [[nodiscard]] foundation::result::Result<capability::signer::KeyAddAuthorization> authorize(
        const capability::signer::KeyAddRequest& request,
        std::optional<capability::signer_attestation::SignedKeyRequestAttestation> appAttestation) override
    {
        lastRequest = request;
        lastAttestation = std::move(appAttestation);
        if (fail) {
            return std::unexpected(error(
                foundation::error::ErrorCode::Unauthorized,
                "Custody authorization declined."));
        }
        return capability::signer::KeyAddAuthorization{
            .custodySignature = std::string(65U, 'c'),
            .signedKeyRequestMetadata = "signed-key-request-metadata",
        };
    }
};

class FakeAttestor final : public capability::signer_attestation::ISignedKeyRequestAttestor {
public:
    std::optional<std::uint64_t> lastRequestFid{};
    std::optional<std::string> lastPublicKeyHex{};
    std::optional<foundation::time::Timestamp> lastDeadline{};
    bool fail{};

    [[nodiscard]] foundation::result::Result<capability::signer_attestation::SignedKeyRequestAttestation> attest(
        const std::uint64_t requestFid,
        const std::string_view publicKeyHex,
        const foundation::time::Timestamp deadline) override
    {
        lastRequestFid = requestFid;
        lastPublicKeyHex = std::string{publicKeyHex};
        lastDeadline = deadline;
        if (fail) {
            return std::unexpected(error(
                foundation::error::ErrorCode::Unavailable,
                "App-FID attestation endpoint unavailable."));
        }
        return capability::signer_attestation::SignedKeyRequestAttestation{
            .requestSignerAddress = std::string(20U, 'a'),
            .signature = std::string(65U, 'b'),
        };
    }
};

class FakeRegistry final : public capability::signer::ISignerRegistry {
public:
    capability::signer::RegistrySnapshot snapshot{};
    std::optional<capability::signer::KeyAddRequest> registeredRequest{};
    std::size_t registrationCount{};
    std::size_t selfRevokeCount{};
    std::uint32_t selfRevokeNonce{};

    [[nodiscard]] foundation::result::Result<capability::signer::RegistrySnapshot> inspect(
        domain::identity::Fid,
        const domain::signer::PublicKey&,
        std::optional<domain::identity::Fid>) override
    {
        return snapshot;
    }

    [[nodiscard]] foundation::result::Result<domain::signer::Registration> registerGasless(
        const capability::signer::KeyAddRequest& request,
        const capability::signer::KeyAddAuthorization& authorization,
        capability::signer::INativeSigner& proofSigner) override
    {
        assert(!authorization.custodySignature.empty());
        assert(!authorization.signedKeyRequestMetadata.empty());
        assert(request.nonce > snapshot.nonces.currentUserNonce);
        assert(request.ttlSeconds > 0U);
        assert(request.ttlSeconds <= domain::signer::MaxGaslessTtlSeconds);
        assert(!request.scopes.empty());
        auto proof = proofSigner.sign("key-add-proof-of-possession");
        assert(proof && proof->bytes.size() == 64U);
        registeredRequest = request;
        ++registrationCount;
        snapshot.nonces.currentUserNonce = request.nonce;
        snapshot.registration = activeRegistration(request.fid, request.scopes);
        snapshot.registration->requestFid = request.requestFid;
        return *snapshot.registration;
    }

    [[nodiscard]] foundation::result::Status selfRevoke(
        domain::identity::Fid,
        domain::identity::Fid,
        const std::uint32_t nonce,
        foundation::time::Timestamp,
        capability::signer::INativeSigner& signer) override
    {
        assert(snapshot.nonces.currentRequesterNonce);
        assert(nonce > *snapshot.nonces.currentRequesterNonce);
        auto proof = signer.sign("key-remove-self-revocation");
        assert(proof);
        selfRevokeNonce = nonce;
        ++selfRevokeCount;
        snapshot.nonces.currentRequesterNonce = nonce;
        if (snapshot.registration) {
            snapshot.registration->state = domain::signer::RegistrationState::Revoked;
        }
        return {};
    }
};

struct DatabaseFixture final {
    DatabaseFixture()
    {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            ("genycaster-signer-" + std::to_string(unique) + ".sqlite3");
    }

    ~DatabaseFixture()
    {
        std::error_code ec{};
        std::filesystem::remove(path, ec);
        std::filesystem::remove(path.string() + "-wal", ec);
        std::filesystem::remove(path.string() + "-shm", ec);
    }

    std::filesystem::path path{};
};

void writeGateRequiresSessionReadySignerAndExactScope()
{
    const auto session = makeSession(42);
    const application::signer::SignerState missing{
        .phase = application::signer::SignerPhase::Missing,
        .requiredScopes = domain::signer::standardSocialScopes(),
    };
    assert(application::signer::evaluateWriteGate(
        std::nullopt, missing, domain::signer::Scope::CastAdd)
        == application::signer::WriteGate::SignInRequired);
    assert(application::signer::evaluateWriteGate(
        session, missing, domain::signer::Scope::CastAdd)
        == application::signer::WriteGate::SignerRequired);

    const auto registration = activeRegistration(session.fid);
    const application::signer::SignerState ready{
        .phase = application::signer::SignerPhase::Ready,
        .record = domain::signer::SignerRecord{
            .local = domain::signer::LocalSigner{
                .fid = session.fid,
                .id = domain::signer::SignerId{"fake-vault:fid:42"},
                .publicKey = domain::signer::PublicKey{std::string{PublicKeyHex}},
                .createdAt = foundation::time::now(),
            },
            .lastVerifiedRegistration = registration,
        },
        .requiredScopes = domain::signer::standardSocialScopes(),
    };
    assert(application::signer::evaluateWriteGate(
        session, ready, domain::signer::Scope::CastAdd)
        == application::signer::WriteGate::Ready);
    assert(application::signer::evaluateWriteGate(
        session, ready, domain::signer::Scope::UserDataAdd)
        == application::signer::WriteGate::ScopeDenied);
}

void provisionUsesLeastPrivilegeTtlAndFreshNonce()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    registry.snapshot.nonces.currentUserNonce = 7U;
    registry.snapshot.nonces.currentRequesterNonce = 3U;
    FakeAuthorizer authorizer{};
    application::signer::SignerService service{
        executor, vault, metadata, &registry, &authorizer, nullptr};
    std::vector<application::signer::SignerState> states{};

    const auto session = makeSession(42);
    assert(service.provision(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.size() == 2U);
    assert(states.front().phase == application::signer::SignerPhase::Provisioning);
    assert(states.back().phase == application::signer::SignerPhase::Ready);
    assert(authorizer.lastRequest);
    assert(authorizer.lastRequest->fid.value == 42U);
    assert(authorizer.lastRequest->requestFid.value == 42U);
    assert(authorizer.lastRequest->custodyAddress ==
        "0x1111111111111111111111111111111111111111");
    assert(authorizer.lastRequest->nonce == 8U);
    assert(authorizer.lastRequest->ttlSeconds == domain::signer::DefaultGaslessTtlSeconds);
    assert(authorizer.lastRequest->scopes == domain::signer::standardSocialScopes());
    assert(registry.registrationCount == 1U);
    assert(metadata.record && metadata.record->lastVerifiedRegistration);

    auto signature = service.sign(domain::signer::Scope::ReactionAdd, "canonical-message-bytes");
    assert(signature && signature->bytes.size() == 64U);
    auto denied = service.sign(domain::signer::Scope::UserDataAdd, "canonical-message-bytes");
    assert(!denied && denied.error().code == foundation::error::ErrorCode::Unauthorized);
}

void legacySessionWithoutCustodyAddressFailsBeforeWalletAuthorization()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    FakeAuthorizer authorizer{};
    application::signer::SignerService service{
        executor, vault, metadata, &registry, &authorizer, nullptr};
    auto session = makeSession(42);
    session.custodyAddress.reset();
    std::vector<application::signer::SignerState> states{};

    assert(service.provision(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::AuthorizationRequired);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::Unauthorized);
    assert(states.back().error->message.find("custody address") != std::string::npos);
    assert(!authorizer.lastRequest);
    assert(registry.registrationCount == 0U);
}

void appFidProvisioningUsesAttestationAndOneUserAuthorization()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    registry.snapshot.nonces.currentUserNonce = 4U;
    FakeAuthorizer authorizer{};
    FakeAttestor attestor{};
    application::signer::SignerService service{
        executor,
        vault,
        metadata,
        &registry,
        &authorizer,
        &attestor,
        application::signer::SignerPolicy{
            .requestFid = domain::identity::Fid{314U},
            .requireExternalAppAttribution = true,
        }};
    std::vector<application::signer::SignerState> states{};

    assert(service.provision(makeSession(42), [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Ready);
    assert(attestor.lastRequestFid);
    assert(*attestor.lastRequestFid == 314U);
    assert(attestor.lastPublicKeyHex);
    assert(*attestor.lastPublicKeyHex == PublicKeyHex);
    assert(attestor.lastDeadline);
    assert(authorizer.lastRequest);
    assert(authorizer.lastRequest->requestFid.value == 314U);
    assert(authorizer.lastAttestation);
    assert(authorizer.lastAttestation->requestSignerAddress.size() == 20U);
    assert(authorizer.lastAttestation->signature.size() == 65U);
    assert(registry.registeredRequest);
    assert(registry.registeredRequest->requestFid.value == 314U);
    assert(metadata.record && metadata.record->lastVerifiedRegistration);
    assert(metadata.record->lastVerifiedRegistration->requestFid);
    assert(metadata.record->lastVerifiedRegistration->requestFid->value == 314U);
}

void appFidProvisioningFailsClosedWithoutAttestor()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    FakeAuthorizer authorizer{};
    application::signer::SignerService service{
        executor,
        vault,
        metadata,
        &registry,
        &authorizer,
        nullptr,
        application::signer::SignerPolicy{
            .requestFid = domain::identity::Fid{314U},
            .requireExternalAppAttribution = true,
        }};
    std::vector<application::signer::SignerState> states{};

    assert(service.provision(makeSession(42), [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::AuthorizationRequired);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::Unsupported);
    assert(!authorizer.lastRequest);
    assert(registry.registrationCount == 0U);
}

void shippingAttributionRejectsViewerOwnedAppFidDuringProvisioning()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    FakeAuthorizer authorizer{};
    FakeAttestor attestor{};
    application::signer::SignerService service{
        executor,
        vault,
        metadata,
        &registry,
        &authorizer,
        &attestor,
        application::signer::SignerPolicy{
            .requestFid = domain::identity::Fid{42U},
            .requireExternalAppAttribution = true,
        }};
    std::vector<application::signer::SignerState> states{};

    assert(service.provision(makeSession(42), [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Failed);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::InvalidArgument);
    assert(states.back().error->message.find("Refusing self-attribution") != std::string::npos);
    assert(!authorizer.lastRequest);
    assert(!attestor.lastRequestFid);
    assert(registry.registrationCount == 0U);
    assert(!vault.exists);
}

void shippingAttributionRejectsViewerOwnedAppFidDuringRestore()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    vault.exists = true;
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    application::signer::SignerService service{
        executor,
        vault,
        metadata,
        &registry,
        nullptr,
        nullptr,
        application::signer::SignerPolicy{
            .requestFid = domain::identity::Fid{42U},
            .requireExternalAppAttribution = true,
        }};
    std::vector<application::signer::SignerState> states{};

    assert(service.restore(makeSession(42), [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Failed);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::InvalidArgument);
    assert(states.back().error->message.find("Refusing self-attribution") != std::string::npos);
}

void confirmedWriteRefreshesSlidingTtlCheckpoint()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    vault.exists = true;
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    const auto session = makeSession(42);
    auto registration = activeRegistration(session.fid);
    registration.lastUsedAt = foundation::time::now() - std::chrono::hours{1};
    registry.snapshot.registration = registration;
    metadata.record = domain::signer::SignerRecord{
        .local = domain::signer::LocalSigner{
            .fid = session.fid,
            .id = domain::signer::SignerId{"fake-vault:fid:42"},
            .publicKey = domain::signer::PublicKey{std::string{PublicKeyHex}},
            .createdAt = foundation::time::now() - std::chrono::hours{2},
        },
        .lastVerifiedRegistration = registration,
    };
    application::signer::SignerService service{
        executor, vault, metadata, &registry, nullptr, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(service.restore(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Ready);

    const auto confirmedAt = foundation::time::now();
    assert(service.recordConfirmedWrite(session.fid, confirmedAt));
    assert(metadata.record && metadata.record->lastVerifiedRegistration);
    assert(metadata.record->lastVerifiedRegistration->lastUsedAt);
    assert(*metadata.record->lastVerifiedRegistration->lastUsedAt == confirmedAt);
    assert(metadata.record->lastVerifiedRegistration->verifiedAt == confirmedAt);
}

void authoritativeRestoreOverridesOptimisticSlidingTtlCheckpoint()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    vault.exists = true;
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    const auto session = makeSession(42);

    auto localCheckpoint = activeRegistration(session.fid);
    localCheckpoint.lastUsedAt = foundation::time::now();
    metadata.record = domain::signer::SignerRecord{
        .local = domain::signer::LocalSigner{
            .fid = session.fid,
            .id = domain::signer::SignerId{"fake-vault:fid:42"},
            .publicKey = domain::signer::PublicKey{std::string{PublicKeyHex}},
            .createdAt = foundation::time::now() - std::chrono::hours{2},
        },
        .lastVerifiedRegistration = localCheckpoint,
    };

    auto authoritative = activeRegistration(session.fid);
    authoritative.lastUsedAt = foundation::time::now() - std::chrono::hours{24 * 31};
    registry.snapshot.registration = authoritative;

    application::signer::SignerService service{
        executor, vault, metadata, &registry, nullptr, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(service.restore(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Expired);
    assert(metadata.record && metadata.record->lastVerifiedRegistration);
    assert(metadata.record->lastVerifiedRegistration->lastUsedAt);
    assert(*metadata.record->lastVerifiedRegistration->lastUsedAt == *authoritative.lastUsedAt);
}

void missingLiveAdaptersNeverPretendWriteReady()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    application::signer::SignerService service{executor, vault, metadata, nullptr, nullptr, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(service.provision(makeSession(42), [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::AuthorizationRequired);
    assert(states.back().error);
    assert(vault.exists);
    assert(metadata.record);
    assert(!metadata.record->lastVerifiedRegistration);
}

void expiredAndRevokedRegistrationsStayNonWritable()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    vault.exists = true;
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    const auto session = makeSession(42);

    auto expired = activeRegistration(session.fid);
    expired.lastUsedAt = foundation::time::now() - std::chrono::hours{24 * 31};
    registry.snapshot.registration = expired;
    metadata.record = domain::signer::SignerRecord{
        .local = domain::signer::LocalSigner{
            .fid = session.fid,
            .id = domain::signer::SignerId{"fake-vault:fid:42"},
            .publicKey = domain::signer::PublicKey{std::string{PublicKeyHex}},
            .createdAt = foundation::time::now(),
        },
        .lastVerifiedRegistration = expired,
    };
    application::signer::SignerService service{executor, vault, metadata, &registry, nullptr, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(service.restore(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Expired);
    assert(application::signer::evaluateWriteGate(
        session, states.back(), domain::signer::Scope::CastAdd)
        == application::signer::WriteGate::SignerRequired);

    auto revoked = activeRegistration(session.fid);
    revoked.state = domain::signer::RegistrationState::Revoked;
    registry.snapshot.registration = revoked;
    states.clear();
    assert(service.restore(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Revoked);
}

void vaultFailureIsExplicitAndGuestReadsRemainIndependent()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    vault.failEnsure = true;
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    FakeAuthorizer authorizer{};
    application::signer::SignerService service{
        executor, vault, metadata, &registry, &authorizer, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(service.provision(makeSession(42), [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Unavailable);
    assert(states.back().error);
    assert(states.back().error->code == foundation::error::ErrorCode::StorageFailure);
    assert(application::signer::evaluateWriteGate(
        std::nullopt, states.back(), domain::signer::Scope::CastAdd)
        == application::signer::WriteGate::SignInRequired);
}

void nonceOverflowFailsClosedBeforeAuthorization()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    registry.snapshot.nonces.currentUserNonce = std::numeric_limits<std::uint32_t>::max();
    FakeAuthorizer authorizer{};
    application::signer::SignerService service{
        executor, vault, metadata, &registry, &authorizer, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(service.provision(makeSession(42), [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Failed);
    assert(states.back().error && states.back().error->code == foundation::error::ErrorCode::Conflict);
    assert(!authorizer.lastRequest);
    assert(registry.registrationCount == 0U);
}

void restartRestoreRechecksNetworkWithoutSecretPersistence()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    registry.snapshot.nonces.currentUserNonce = 2U;
    registry.snapshot.nonces.currentRequesterNonce = 4U;
    FakeAuthorizer authorizer{};
    const auto session = makeSession(42);

    {
        application::signer::SignerService service{
            executor, vault, metadata, &registry, &authorizer, nullptr};
        std::vector<application::signer::SignerState> states{};
        assert(service.provision(session, [&](auto state) { states.push_back(std::move(state)); }));
        assert(states.back().phase == application::signer::SignerPhase::Ready);
    }

    assert(vault.exists);
    assert(metadata.record);
    assert(metadata.record->local.publicKey.hex == PublicKeyHex);
    assert(metadata.record->local.id.value == "fake-vault:fid:42");

    application::signer::SignerService restarted{
        executor, vault, metadata, &registry, &authorizer, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(restarted.restore(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.front().phase == application::signer::SignerPhase::Checking);
    assert(states.back().phase == application::signer::SignerPhase::Ready);
}

void selfRevokeUsesRequesterNonceAndErasesSecureKey()
{
    ImmediateExecutor executor{};
    MemoryVault vault{};
    vault.exists = true;
    MemoryMetadataStore metadata{};
    FakeRegistry registry{};
    const auto session = makeSession(42);
    auto registration = activeRegistration(session.fid);
    registry.snapshot.registration = registration;
    registry.snapshot.nonces.currentRequesterNonce = 9U;
    metadata.record = domain::signer::SignerRecord{
        .local = domain::signer::LocalSigner{
            .fid = session.fid,
            .id = domain::signer::SignerId{"fake-vault:fid:42"},
            .publicKey = domain::signer::PublicKey{std::string{PublicKeyHex}},
            .createdAt = foundation::time::now(),
        },
        .lastVerifiedRegistration = registration,
    };
    application::signer::SignerService service{executor, vault, metadata, &registry, nullptr, nullptr};
    std::vector<application::signer::SignerState> states{};
    assert(service.restore(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Ready);
    states.clear();
    assert(service.revoke(session, [&](auto state) { states.push_back(std::move(state)); }));
    assert(states.back().phase == application::signer::SignerPhase::Revoked);
    assert(registry.selfRevokeCount == 1U);
    assert(registry.selfRevokeNonce == 10U);
    assert(vault.eraseCount == 1U);
    assert(!vault.exists);
}

void sqliteSignerMetadataRoundTripContainsOnlyPublicCheckpoint()
{
    DatabaseFixture database{};
    auto store = farcaster::storage::signer::openSignerMetadataStore(database.path);
    assert(store);
    const auto fid = domain::identity::Fid{42};
    auto registration = activeRegistration(fid);
    const domain::signer::SignerRecord record{
        .local = domain::signer::LocalSigner{
            .fid = fid,
            .id = domain::signer::SignerId{"macos-keychain:fid:42"},
            .publicKey = domain::signer::PublicKey{std::string{PublicKeyHex}},
            .createdAt = foundation::time::now(),
        },
        .lastVerifiedRegistration = registration,
    };
    assert((*store)->save(record));
    auto loaded = (*store)->load(fid);
    assert(loaded && *loaded);
    assert((*loaded)->local.id.value == record.local.id.value);
    assert((*loaded)->local.publicKey.hex == record.local.publicKey.hex);
    assert((*loaded)->lastVerifiedRegistration);
    assert((*loaded)->lastVerifiedRegistration->scopes == registration.scopes);
    assert((*loaded)->lastVerifiedRegistration->ttlSeconds == registration.ttlSeconds);
    assert((*loaded)->lastVerifiedRegistration->requestFid);
    assert((*loaded)->lastVerifiedRegistration->requestFid->value == 42U);
    assert((*store)->clear(fid));
    loaded = (*store)->load(fid);
    assert(loaded && !*loaded);
}

}

int main()
{
    writeGateRequiresSessionReadySignerAndExactScope();
    provisionUsesLeastPrivilegeTtlAndFreshNonce();
    legacySessionWithoutCustodyAddressFailsBeforeWalletAuthorization();
    appFidProvisioningUsesAttestationAndOneUserAuthorization();
    appFidProvisioningFailsClosedWithoutAttestor();
    shippingAttributionRejectsViewerOwnedAppFidDuringProvisioning();
    shippingAttributionRejectsViewerOwnedAppFidDuringRestore();
    confirmedWriteRefreshesSlidingTtlCheckpoint();
    authoritativeRestoreOverridesOptimisticSlidingTtlCheckpoint();
    missingLiveAdaptersNeverPretendWriteReady();
    expiredAndRevokedRegistrationsStayNonWritable();
    vaultFailureIsExplicitAndGuestReadsRemainIndependent();
    nonceOverflowFailsClosedBeforeAuthorization();
    restartRestoreRechecksNetworkWithoutSecretPersistence();
    selfRevokeUsesRequesterNonceAndErasesSecureKey();
    sqliteSignerMetadataRoundTripContainsOnlyPublicCheckpoint();
    return 0;
}
