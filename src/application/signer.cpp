module;

#include <algorithm>
#include <chrono>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.application.signer;

import farcaster.domain.identity;
import farcaster.application.executor;
import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.domain.session;
import farcaster.domain.signer;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.local.signer;

namespace farcaster::application::signer {
namespace {

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] bool containsAllScopes(
    const domain::signer::Registration& registration,
    const std::vector<domain::signer::Scope>& required,
    const foundation::time::Timestamp now)
{
    return std::all_of(required.begin(), required.end(), [&](const auto scope) {
        return domain::signer::permits(registration, scope, now);
    });
}

[[nodiscard]] SignerState stateForRegistration(
    domain::signer::SignerRecord record,
    const std::vector<domain::signer::Scope>& requiredScopes)
{
    const auto now = foundation::time::now();
    if (!record.lastVerifiedRegistration) {
        return SignerState{
            .phase = SignerPhase::AuthorizationRequired,
            .record = std::move(record),
            .requiredScopes = requiredScopes,
        };
    }
    auto& registration = *record.lastVerifiedRegistration;
    if (registration.state == domain::signer::RegistrationState::Revoked) {
        return SignerState{
            .phase = SignerPhase::Revoked,
            .record = std::move(record),
            .requiredScopes = requiredScopes,
        };
    }
    if (registration.state == domain::signer::RegistrationState::Expired ||
        domain::signer::isExpired(registration, now)) {
        return SignerState{
            .phase = SignerPhase::Expired,
            .record = std::move(record),
            .requiredScopes = requiredScopes,
        };
    }
    if (registration.state == domain::signer::RegistrationState::Active &&
        containsAllScopes(registration, requiredScopes, now)) {
        return SignerState{
            .phase = SignerPhase::Ready,
            .record = std::move(record),
            .requiredScopes = requiredScopes,
        };
    }
    return SignerState{
        .phase = SignerPhase::AuthorizationRequired,
        .record = std::move(record),
        .requiredScopes = requiredScopes,
    };
}


[[nodiscard]] foundation::result::Result<std::uint32_t> nextNonce(
    const std::uint32_t current,
    const std::string_view nonceName)
{
    if (current == std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Conflict,
            std::string{nonceName} + " has reached the uint32 protocol limit."));
    }
    return current + 1U;
}

[[nodiscard]] foundation::result::Status validatePolicy(const SignerPolicy& policy)
{
    if (policy.scopes.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Protocol signer policy requires at least one explicit message scope."));
    }
    if (policy.scopes.size() > 16U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Protocol signer policy exceeds Snapchain's 16-scope consensus bound."));
    }
    if (!std::all_of(policy.scopes.begin(), policy.scopes.end(), [](const auto scope) {
            return domain::signer::isAdmissibleScope(scope);
        })) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Protocol signer policy contains a scope Snapchain does not admit for KEY_ADD."));
    }
    if (policy.ttlSeconds == 0U || policy.ttlSeconds > domain::signer::MaxGaslessTtlSeconds) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Gasless signer TTL must be positive and no greater than 90 days."));
    }
    if (policy.authorizationLifetime <= std::chrono::minutes::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Signer authorization lifetime must be positive."));
    }
    return {};
}

}

struct SignerService::Impl final {
    executor::IExecutor& executor;
    capability::signer::ISignerVault& vault;
    local::signer::ISignerMetadataStore& metadataStore;
    capability::signer::ISignerRegistry* registry{};
    capability::signer::ICustodyAuthorizer* authorizer{};
    capability::signer_attestation::ISignedKeyRequestAttestor* attestor{};
    SignerPolicy policy{};
    std::unique_ptr<capability::signer::INativeSigner> nativeSigner{};
    SignerState current{};
};

WriteGate evaluateWriteGate(
    const std::optional<domain::session::ClientSession>& session,
    const SignerState& signerState,
    const domain::signer::Scope requiredScope) noexcept
{
    if (!session) {
        return WriteGate::SignInRequired;
    }
    if (signerState.phase != SignerPhase::Ready || !signerState.record ||
        !signerState.record->lastVerifiedRegistration) {
        return WriteGate::SignerRequired;
    }
    const auto& record = *signerState.record;
    if (record.local.fid.value != session->fid.value) {
        return WriteGate::SignerRequired;
    }
    if (!domain::signer::permits(
            *record.lastVerifiedRegistration,
            requiredScope,
            foundation::time::now())) {
        return WriteGate::ScopeDenied;
    }
    return WriteGate::Ready;
}

SignerService::SignerService(
    executor::IExecutor& executor,
    capability::signer::ISignerVault& vault,
    local::signer::ISignerMetadataStore& metadataStore,
    capability::signer::ISignerRegistry* const registry,
    capability::signer::ICustodyAuthorizer* const authorizer,
    capability::signer_attestation::ISignedKeyRequestAttestor* const attestor,
    SignerPolicy policy)
    : m_impl(std::make_unique<Impl>(Impl{
          .executor = executor,
          .vault = vault,
          .metadataStore = metadataStore,
          .registry = registry,
          .authorizer = authorizer,
          .attestor = attestor,
          .policy = std::move(policy),
      }))
{
    if (auto valid = validatePolicy(m_impl->policy); !valid) {
        m_impl->current = SignerState{
            .phase = SignerPhase::Failed,
            .requiredScopes = m_impl->policy.scopes,
            .error = std::move(valid.error()),
        };
    }
}

SignerService::~SignerService() = default;

foundation::result::Status SignerService::restore(
    std::optional<domain::session::ClientSession> session,
    Observer observer)
{
    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            if (!session) {
                m_impl->nativeSigner.reset();
                m_impl->current = SignerState{.phase = SignerPhase::SignedOut,
                    .requiredScopes = m_impl->policy.scopes};
                observer(m_impl->current);
                return;
            }
            m_impl->current = SignerState{.phase = SignerPhase::Checking,
                .requiredScopes = m_impl->policy.scopes};
            observer(m_impl->current);

            auto signer = m_impl->vault.load(session->fid);
            if (!signer) {
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .requiredScopes = m_impl->policy.scopes, .error = std::move(signer.error())};
                observer(m_impl->current);
                return;
            }
            if (!*signer) {
                m_impl->nativeSigner.reset();
                m_impl->current = SignerState{.phase = SignerPhase::Missing,
                    .requiredScopes = m_impl->policy.scopes};
                observer(m_impl->current);
                return;
            }
            m_impl->nativeSigner = std::move(*signer);

            auto publicKey = m_impl->nativeSigner->publicKey();
            if (!publicKey) {
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .requiredScopes = m_impl->policy.scopes, .error = std::move(publicKey.error())};
                observer(m_impl->current);
                return;
            }
            domain::signer::SignerRecord record{
                .local = domain::signer::LocalSigner{
                    .fid = session->fid,
                    .id = m_impl->nativeSigner->signerId(),
                    .publicKey = std::move(*publicKey),
                    .createdAt = foundation::time::now(),
                },
            };
            auto stored = m_impl->metadataStore.load(session->fid);
            if (!stored) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(stored.error())};
                observer(m_impl->current);
                return;
            }
            if (*stored) {
                if ((*stored)->local.id.value != record.local.id.value ||
                    (*stored)->local.publicKey.hex != record.local.publicKey.hex) {
                    m_impl->current = SignerState{.phase = SignerPhase::Failed,
                        .record = record, .requiredScopes = m_impl->policy.scopes,
                        .error = makeError(foundation::error::ErrorCode::StorageFailure,
                            "Persisted signer metadata does not match the secure-vault key.")};
                    observer(m_impl->current);
                    return;
                }
                record = std::move(**stored);
            }

            if (m_impl->registry == nullptr) {
                m_impl->current = SignerState{
                    .phase = SignerPhase::AuthorizationRequired,
                    .record = record,
                    .requiredScopes = m_impl->policy.scopes,
                    .error = makeError(foundation::error::ErrorCode::Unsupported,
                        "Signer registry verification is not available in this build."),
                };
                observer(m_impl->current);
                return;
            }

            const auto requestFid = record.lastVerifiedRegistration
                ? record.lastVerifiedRegistration->requestFid
                : m_impl->policy.requestFid;
            auto inspected = m_impl->registry->inspect(session->fid, record.local.publicKey, requestFid);
            if (!inspected) {
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(inspected.error())};
                observer(m_impl->current);
                return;
            }
            record.lastVerifiedRegistration = std::move(inspected->registration);
            if (auto saved = m_impl->metadataStore.save(record); !saved) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(saved.error())};
                observer(m_impl->current);
                return;
            }
            m_impl->current = stateForRegistration(std::move(record), m_impl->policy.scopes);
            observer(m_impl->current);
        });
}

foundation::result::Status SignerService::provision(
    std::optional<domain::session::ClientSession> session,
    Observer observer)
{
    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            if (!session) {
                m_impl->current = SignerState{.phase = SignerPhase::SignedOut,
                    .requiredScopes = m_impl->policy.scopes};
                observer(m_impl->current);
                return;
            }
            if (auto valid = validatePolicy(m_impl->policy); !valid) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .requiredScopes = m_impl->policy.scopes, .error = std::move(valid.error())};
                observer(m_impl->current);
                return;
            }
            m_impl->current = SignerState{.phase = SignerPhase::Provisioning,
                .requiredScopes = m_impl->policy.scopes};
            observer(m_impl->current);

            auto ensured = m_impl->vault.ensure(session->fid);
            if (!ensured || !*ensured) {
                auto error = ensured
                    ? makeError(foundation::error::ErrorCode::InternalFailure,
                        "Secure signer vault returned no signer after ensure().")
                    : std::move(ensured.error());
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .requiredScopes = m_impl->policy.scopes, .error = std::move(error)};
                observer(m_impl->current);
                return;
            }
            m_impl->nativeSigner = std::move(*ensured);
            auto publicKey = m_impl->nativeSigner->publicKey();
            if (!publicKey) {
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .requiredScopes = m_impl->policy.scopes, .error = std::move(publicKey.error())};
                observer(m_impl->current);
                return;
            }
            domain::signer::SignerRecord record{
                .local = domain::signer::LocalSigner{
                    .fid = session->fid,
                    .id = m_impl->nativeSigner->signerId(),
                    .publicKey = std::move(*publicKey),
                    .createdAt = foundation::time::now(),
                },
            };
            auto stored = m_impl->metadataStore.load(session->fid);
            if (!stored) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(stored.error())};
                observer(m_impl->current);
                return;
            }
            if (*stored) {
                if ((*stored)->local.id.value != record.local.id.value ||
                    (*stored)->local.publicKey.hex != record.local.publicKey.hex) {
                    m_impl->current = SignerState{.phase = SignerPhase::Failed,
                        .record = record, .requiredScopes = m_impl->policy.scopes,
                        .error = makeError(foundation::error::ErrorCode::StorageFailure,
                            "Persisted signer metadata does not match the secure-vault key.")};
                    observer(m_impl->current);
                    return;
                }
                record = std::move(**stored);
            } else if (auto saved = m_impl->metadataStore.save(record); !saved) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(saved.error())};
                observer(m_impl->current);
                return;
            }

            if (m_impl->registry == nullptr) {
                m_impl->current = SignerState{.phase = SignerPhase::AuthorizationRequired,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = makeError(foundation::error::ErrorCode::Unsupported,
                        "A signer registry adapter is required before interaction signing can be enabled.")};
                observer(m_impl->current);
                return;
            }

            const auto requestFid = m_impl->policy.requestFid.value_or(session->fid);
            auto inspected = m_impl->registry->inspect(session->fid, record.local.publicKey, requestFid);
            if (!inspected) {
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(inspected.error())};
                observer(m_impl->current);
                return;
            }
            if (inspected->registration) {
                record.lastVerifiedRegistration = inspected->registration;
                if (auto ready = stateForRegistration(record, m_impl->policy.scopes);
                    ready.phase == SignerPhase::Ready) {
                    (void)m_impl->metadataStore.save(record);
                    m_impl->current = std::move(ready);
                    observer(m_impl->current);
                    return;
                }
            }

            if (m_impl->authorizer == nullptr) {
                m_impl->current = SignerState{.phase = SignerPhase::AuthorizationRequired,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = makeError(foundation::error::ErrorCode::Unsupported,
                        "Secure key is prepared, but a custody EIP-712 authorization transport is not configured.")};
                observer(m_impl->current);
                return;
            }
            if (!session->custodyAddress || session->custodyAddress->empty()) {
                m_impl->current = SignerState{.phase = SignerPhase::AuthorizationRequired,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = makeError(foundation::error::ErrorCode::Unauthorized,
                        "The current session has no verified Farcaster custody address. Session restore normally refreshes legacy sessions automatically; verify Optimism RPC connectivity or sign in again before KEY_ADD.")};
                observer(m_impl->current);
                return;
            }

            auto nonce = nextNonce(inspected->nonces.currentUserNonce, "Signer user nonce");
            if (!nonce) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(nonce.error())};
                observer(m_impl->current);
                return;
            }
            const auto now = foundation::time::now();
            capability::signer::KeyAddRequest request{
                .fid = session->fid,
                .publicKey = record.local.publicKey,
                .scopes = m_impl->policy.scopes,
                .ttlSeconds = m_impl->policy.ttlSeconds,
                .nonce = *nonce,
                .deadline = now + std::chrono::duration_cast<std::chrono::milliseconds>(
                    m_impl->policy.authorizationLifetime),
                .requestFid = requestFid,
                .custodyAddress = *session->custodyAddress,
            };
            std::optional<capability::signer_attestation::SignedKeyRequestAttestation> appAttestation{};
            if (requestFid.value != session->fid.value) {
                if (m_impl->attestor == nullptr) {
                    m_impl->current = SignerState{.phase = SignerPhase::AuthorizationRequired,
                        .record = record, .requiredScopes = m_impl->policy.scopes,
                        .error = makeError(foundation::error::ErrorCode::Unsupported,
                            "Genycaster App-FID signing is configured, but no SignedKeyRequest attestation provider is available.")};
                    observer(m_impl->current);
                    return;
                }
                auto attestation = m_impl->attestor->attest(request.requestFid.value, request.publicKey.hex, request.deadline);
                if (!attestation) {
                    m_impl->current = SignerState{.phase = SignerPhase::AuthorizationRequired,
                        .record = record, .requiredScopes = m_impl->policy.scopes,
                        .error = std::move(attestation.error())};
                    observer(m_impl->current);
                    return;
                }
                appAttestation = std::move(*attestation);
            }
            auto authorization = m_impl->authorizer->authorize(request, std::move(appAttestation));
            if (!authorization) {
                m_impl->current = SignerState{.phase = SignerPhase::AuthorizationRequired,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(authorization.error())};
                observer(m_impl->current);
                return;
            }
            auto registered = m_impl->registry->registerGasless(
                request, *authorization, *m_impl->nativeSigner);
            if (!registered) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(registered.error())};
                observer(m_impl->current);
                return;
            }
            record.lastVerifiedRegistration = std::move(*registered);
            if (auto saved = m_impl->metadataStore.save(record); !saved) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(saved.error())};
                observer(m_impl->current);
                return;
            }
            m_impl->current = stateForRegistration(std::move(record), m_impl->policy.scopes);
            observer(m_impl->current);
        });
}

foundation::result::Status SignerService::revoke(
    std::optional<domain::session::ClientSession> session,
    Observer observer)
{
    return m_impl->executor.post(
        [this, session = std::move(session), observer = std::move(observer)]() mutable {
            if (!session) {
                m_impl->current = SignerState{.phase = SignerPhase::SignedOut,
                    .requiredScopes = m_impl->policy.scopes};
                observer(m_impl->current);
                return;
            }
            if (!m_impl->nativeSigner || !m_impl->current.record ||
                !m_impl->current.record->lastVerifiedRegistration) {
                m_impl->current = SignerState{.phase = SignerPhase::Missing,
                    .requiredScopes = m_impl->policy.scopes};
                observer(m_impl->current);
                return;
            }
            auto record = *m_impl->current.record;
            auto& registration = *record.lastVerifiedRegistration;
            if (registration.kind != domain::signer::RegistrationKind::Gasless ||
                !registration.requestFid) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = makeError(foundation::error::ErrorCode::Unsupported,
                        "Self-revocation is available only for gasless signers with verified request-FID metadata.")};
                observer(m_impl->current);
                return;
            }
            if (m_impl->registry == nullptr) {
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = makeError(foundation::error::ErrorCode::Unsupported,
                        "Signer registry adapter is unavailable for revocation.")};
                observer(m_impl->current);
                return;
            }
            auto inspected = m_impl->registry->inspect(
                session->fid, record.local.publicKey, registration.requestFid);
            if (!inspected || !inspected->nonces.currentRequesterNonce) {
                auto error = inspected
                    ? makeError(foundation::error::ErrorCode::ProtocolFailure,
                        "Signer registry did not return the requester nonce required for self-revocation.")
                    : std::move(inspected.error());
                m_impl->current = SignerState{.phase = SignerPhase::Unavailable,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(error)};
                observer(m_impl->current);
                return;
            }
            auto nonce = nextNonce(*inspected->nonces.currentRequesterNonce, "Signer requester nonce");
            if (!nonce) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(nonce.error())};
                observer(m_impl->current);
                return;
            }
            const auto deadline = foundation::time::now() + std::chrono::hours{1};
            auto revoked = m_impl->registry->selfRevoke(
                session->fid,
                *registration.requestFid,
                *nonce,
                deadline,
                *m_impl->nativeSigner);
            if (!revoked) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(revoked.error())};
                observer(m_impl->current);
                return;
            }
            registration.state = domain::signer::RegistrationState::Revoked;
            registration.verifiedAt = foundation::time::now();
            if (auto saved = m_impl->metadataStore.save(record); !saved) {
                m_impl->current = SignerState{.phase = SignerPhase::Failed,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(saved.error())};
                observer(m_impl->current);
                return;
            }
            if (auto erased = m_impl->vault.erase(session->fid); !erased) {
                m_impl->current = SignerState{.phase = SignerPhase::Revoked,
                    .record = record, .requiredScopes = m_impl->policy.scopes,
                    .error = std::move(erased.error())};
                observer(m_impl->current);
                return;
            }
            m_impl->nativeSigner.reset();
            m_impl->current = SignerState{.phase = SignerPhase::Revoked,
                .record = std::move(record), .requiredScopes = m_impl->policy.scopes};
            observer(m_impl->current);
        });
}

foundation::result::Status SignerService::recordConfirmedWrite(
    const domain::identity::Fid fid,
    const foundation::time::Timestamp confirmedAt)
{
    if (fid.value == 0U || !m_impl->current.record ||
        !m_impl->current.record->lastVerifiedRegistration) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "Cannot refresh signer TTL without an active verified registration."));
    }
    auto record = *m_impl->current.record;
    if (record.local.fid.value != fid.value ||
        record.lastVerifiedRegistration->fid.value != fid.value ||
        record.lastVerifiedRegistration->state != domain::signer::RegistrationState::Active) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "Confirmed write belongs to a different or inactive signer registration."));
    }
    auto& registration = *record.lastVerifiedRegistration;
    if (!registration.lastUsedAt || confirmedAt > *registration.lastUsedAt) {
        registration.lastUsedAt = confirmedAt;
    }
    registration.verifiedAt = confirmedAt;
    if (auto saved = m_impl->metadataStore.save(record); !saved) {
        return std::unexpected(std::move(saved.error()));
    }
    m_impl->current = stateForRegistration(std::move(record), m_impl->policy.scopes);
    return {};
}

foundation::result::Result<domain::signer::PublicKey> SignerService::requireSigner(
    const domain::identity::Fid fid,
    const domain::signer::Scope requiredScope) const
{
    if (fid.value == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "A signed-in Farcaster FID is required before resolving write authority."));
    }
    if (!m_impl->nativeSigner || m_impl->current.phase != SignerPhase::Ready ||
        !m_impl->current.record || !m_impl->current.record->lastVerifiedRegistration) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "A verified protocol signer is required before writing."));
    }
    const auto& record = *m_impl->current.record;
    if (record.local.fid.value != fid.value ||
        record.lastVerifiedRegistration->fid.value != fid.value) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "The current protocol signer belongs to a different Farcaster account."));
    }
    if (!domain::signer::permits(
            *record.lastVerifiedRegistration,
            requiredScope,
            foundation::time::now())) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "The active protocol signer is not authorized for this message type."));
    }
    return record.local.publicKey;
}

foundation::result::Result<domain::signer::Signature> SignerService::sign(
    const domain::signer::Scope requiredScope,
    const std::string& bytes)
{
    if (bytes.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Canonical protocol bytes to sign must not be empty."));
    }
    if (!m_impl->nativeSigner || m_impl->current.phase != SignerPhase::Ready ||
        !m_impl->current.record || !m_impl->current.record->lastVerifiedRegistration) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "A verified protocol signer is required before signing a write."));
    }
    if (!domain::signer::permits(
            *m_impl->current.record->lastVerifiedRegistration,
            requiredScope,
            foundation::time::now())) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unauthorized,
            "The active protocol signer is not authorized for this message type."));
    }
    return m_impl->nativeSigner->sign(bytes);
}

SignerState SignerService::state() const
{
    return m_impl->current;
}

}
