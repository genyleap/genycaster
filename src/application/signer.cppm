module;

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module farcaster.application.signer;

import farcaster.domain.session;
import farcaster.domain.signer;
import farcaster.domain.identity;
import farcaster.foundation.time;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.capability.signer;
import farcaster.capability.signer_attestation;
import farcaster.capability.write_authority;
import farcaster.local.signer;
import farcaster.application.executor;

export namespace farcaster::application::signer {

/** @brief Product-visible phase of protocol-signer readiness. */
enum class SignerPhase {
    SignedOut,
    Checking,
    Missing,
    AuthorizationRequired,
    Provisioning,
    Ready,
    Revoked,
    Expired,
    Unavailable,
    Failed,
};

/** @brief Normalized signer state safe for presentation adapters. */
struct SignerState final {
    SignerPhase phase{SignerPhase::SignedOut};
    std::optional<domain::signer::SignerRecord> record{};
    std::vector<domain::signer::Scope> requiredScopes{};
    std::optional<foundation::error::Error> error{};
};

/** @brief Least-privilege policy for one Genycaster protocol signer. */
struct SignerPolicy final {
    std::vector<domain::signer::Scope> scopes{domain::signer::standardSocialScopes()};
    std::uint32_t ttlSeconds{domain::signer::DefaultGaslessTtlSeconds};
    std::chrono::minutes authorizationLifetime{60};
    /** @brief Canonical app FID for SignedKeyRequest attribution. Empty explicitly permits self-client attribution. */
    std::optional<domain::identity::Fid> requestFid{};
    /** @brief Rejects missing or viewer-owned request FIDs when product attribution must identify an external client. */
    bool requireExternalAppAttribution{false};
};

/** @brief Result of checking one specific protocol-changing interaction. */
enum class WriteGate {
    SignInRequired,
    SignerRequired,
    ScopeDenied,
    Ready,
};

/**
 * @brief Evaluates account + signer + scope gates without performing I/O.
 * @param session Current authenticated client session, when any.
 * @param signerState Current network-verified signer state.
 * @param requiredScope Exact protocol message type the interaction needs.
 */
[[nodiscard]] WriteGate evaluateWriteGate(
    const std::optional<domain::session::ClientSession>& session,
    const SignerState& signerState,
    domain::signer::Scope requiredScope) noexcept;

/** @brief Observer called from the configured application executor. */
using Observer = std::function<void(SignerState)>;

/**
 * @brief Orchestrates secure local signer creation, authorization and readiness.
 *
 * The service owns only opaque signer handles. Private key bytes stay inside
 * ISignerVault/INativeSigner implementations and never enter Application.
 */
class SignerService final : public capability::write::IWriteAuthority {
public:
    /** @brief Creates the service over explicit secure-storage/network/authorization ports. */
    SignerService(
        executor::IExecutor& executor,
        capability::signer::ISignerVault& vault,
        local::signer::ISignerMetadataStore& metadataStore,
        capability::signer::ISignerRegistry* registry,
        capability::signer::ICustodyAuthorizer* authorizer,
        capability::signer_attestation::ISignedKeyRequestAttestor* attestor,
        SignerPolicy policy = {});
    /** @brief Destroys the service after executor-owned work has drained. */
    ~SignerService();

    SignerService(const SignerService&) = delete;
    auto operator=(const SignerService&) -> SignerService& = delete;
    SignerService(SignerService&&) = delete;
    auto operator=(SignerService&&) -> SignerService& = delete;

    /** @brief Restores local signer metadata and re-verifies network registration when possible. */
    [[nodiscard]] foundation::result::Status restore(
        std::optional<domain::session::ClientSession> session,
        Observer observer);

    /** @brief Ensures a secure local key and provisions it when authorization/registry ports exist. */
    [[nodiscard]] foundation::result::Status provision(
        std::optional<domain::session::ClientSession> session,
        Observer observer);

    /** @brief Self-revokes an active gasless signer, then removes its local secure key. */
    [[nodiscard]] foundation::result::Status revoke(
        std::optional<domain::session::ClientSession> session,
        Observer observer);

    /**
     * @brief Signs canonical protocol bytes only when the current signer is ready for the scope.
     * @return Signature or a structured authorization/signer failure.
     */
    /** @brief Returns the current network-verified signer key for an exact FID/scope. */
    [[nodiscard]] foundation::result::Result<domain::signer::PublicKey> requireSigner(
        domain::identity::Fid fid,
        domain::signer::Scope requiredScope) const override;

    [[nodiscard]] foundation::result::Result<domain::signer::Signature> sign(
        domain::signer::Scope requiredScope,
        const std::string& bytes) override;

    /** @brief Advances locally verified sliding-TTL state after an authoritative write confirmation. */
    [[nodiscard]] foundation::result::Status recordConfirmedWrite(
        domain::identity::Fid fid,
        foundation::time::Timestamp confirmedAt) override;

    /** @brief Returns the current normalized state snapshot. */
    [[nodiscard]] SignerState state() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
