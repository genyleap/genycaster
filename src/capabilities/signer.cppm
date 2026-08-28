module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module farcaster.capability.signer;

import farcaster.foundation.result;
import farcaster.foundation.time;
import farcaster.domain.identity;
import farcaster.domain.signer;

export namespace farcaster::capability::signer {

/**
 * @brief Isolated Ed25519 signer handle backed by secure platform storage.
 *
 * The implementation owns all private-key access. Callers may inspect only the
 * stable handle and public key, and may request signatures over canonical bytes.
 */
class INativeSigner {
public:
    /** @brief Destroys the signer handle. */
    virtual ~INativeSigner() = default;

    /** @brief Returns the non-secret stable identifier for this local key. */
    [[nodiscard]] virtual domain::signer::SignerId signerId() const = 0;

    /** @brief Returns the 32-byte Ed25519 public key encoded as lowercase hex. */
    [[nodiscard]] virtual foundation::result::Result<domain::signer::PublicKey> publicKey() const = 0;

    /**
     * @brief Signs canonical protocol bytes with the isolated Ed25519 private key.
     * @param bytes Owned byte string to sign. The signer never mutates it.
     * @return Raw 64-byte Ed25519 signature in an owned byte string.
     */
    [[nodiscard]] virtual foundation::result::Result<domain::signer::Signature>
    sign(const std::string& bytes) = 0;
};

/**
 * @brief Secure local signer-key vault.
 *
 * Production implementations use OS-protected credential/key storage. The
 * contract never exports seed/private-key bytes to callers.
 */
class ISignerVault {
public:
    /** @brief Destroys the vault. */
    virtual ~ISignerVault() = default;

    /** @brief Loads an existing signer for the FID, or returns no value when absent. */
    [[nodiscard]] virtual foundation::result::Result<std::unique_ptr<INativeSigner>> load(
        domain::identity::Fid fid) = 0;

    /** @brief Loads or generates one isolated Ed25519 signer for the FID. */
    [[nodiscard]] virtual foundation::result::Result<std::unique_ptr<INativeSigner>> ensure(
        domain::identity::Fid fid) = 0;

    /** @brief Removes the locally held key for the FID from secure storage. */
    [[nodiscard]] virtual foundation::result::Status erase(domain::identity::Fid fid) = 0;
};

/** @brief Replay-safe authorization payload required before a gasless KEY_ADD. */
struct KeyAddRequest final {
    domain::identity::Fid fid{};
    domain::signer::PublicKey publicKey{};
    std::vector<domain::signer::Scope> scopes{};
    std::uint32_t ttlSeconds{};
    std::uint32_t nonce{};
    foundation::time::Timestamp deadline{};
    /** @brief App FID bound into SignedKeyRequest metadata. Defaults by policy outside the port. */
    domain::identity::Fid requestFid{};
};

/**
 * @brief Opaque non-secret signatures/metadata produced by custody authorization.
 *
 * These bytes authorize a specific public key, scopes, TTL, nonce and deadline.
 * They are not reusable login/session credentials and contain no private key.
 */
struct KeyAddAuthorization final {
    std::string custodySignature{};
    std::string signedKeyRequestMetadata{};
};

/**
 * @brief Interactive custody-authorizer port for Snapchain KEY_ADD semantics.
 *
 * SIWF authentication does not satisfy this contract. Implementations may use
 * a wallet/deep-link/device flow, but must produce signatures for the exact
 * request supplied by Application.
 */
class ICustodyAuthorizer {
public:
    /** @brief Destroys the authorizer. */
    virtual ~ICustodyAuthorizer() = default;

    /** @brief Requests custody authorization for one exact gasless signer registration. */
    [[nodiscard]] virtual foundation::result::Result<KeyAddAuthorization> authorize(
        const KeyAddRequest& request) = 0;
};

/** @brief Unified Snapchain/on-chain signer lookup result. */
struct RegistrySnapshot final {
    domain::signer::NonceState nonces{};
    std::optional<domain::signer::Registration> registration{};
};

/**
 * @brief Provider-neutral signer registration/revocation capability.
 *
 * Implementations map this contract onto the unified GetSignersByFid surface
 * and KEY_ADD/KEY_REMOVE or a legacy on-chain compatibility path. Protocol
 * wire DTOs and provider credentials remain inside the adapter.
 */
class ISignerRegistry {
public:
    /** @brief Destroys the registry adapter. */
    virtual ~ISignerRegistry() = default;

    /**
     * @brief Looks up one local public key and current replay-protection nonces.
     * @param fid User FID whose signer is being inspected.
     * @param publicKey Local Ed25519 public key.
     * @param requestFid Optional app/request FID whose self-revoke nonce is needed.
     */
    [[nodiscard]] virtual foundation::result::Result<RegistrySnapshot> inspect(
        domain::identity::Fid fid,
        const domain::signer::PublicKey& publicKey,
        std::optional<domain::identity::Fid> requestFid) = 0;

    /**
     * @brief Registers a gasless signer using custody-authorized KEY_ADD semantics.
     * @param request Exact request whose fields are custody-signed.
     * @param authorization Custody signature plus SignedKeyRequest metadata.
     * @param proofSigner New Ed25519 key; adapter uses it only to sign the KEY_ADD envelope.
     * @return Network-confirmed/observed signer registration snapshot.
     */
    [[nodiscard]] virtual foundation::result::Result<domain::signer::Registration> registerGasless(
        const KeyAddRequest& request,
        const KeyAddAuthorization& authorization,
        INativeSigner& proofSigner) = 0;

    /**
     * @brief Self-revokes the active gasless signer via KEY_REMOVE signature_type=2.
     * @param fid User FID whose signer is removed.
     * @param requestFid App/request FID that owns the self-revocation nonce namespace.
     * @param nonce Strictly increasing requester/app nonce.
     * @param deadline Farcaster-compatible authorization deadline.
     * @param signer Key being revoked; it signs the self-revocation envelope.
     */
    [[nodiscard]] virtual foundation::result::Status selfRevoke(
        domain::identity::Fid fid,
        domain::identity::Fid requestFid,
        std::uint32_t nonce,
        foundation::time::Timestamp deadline,
        INativeSigner& signer) = 0;
};

}
