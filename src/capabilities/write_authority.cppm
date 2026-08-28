module;

#include <string>

export module farcaster.capability.write_authority;

import farcaster.foundation.result;
import farcaster.domain.identity;
import farcaster.domain.signer;

export namespace farcaster::capability::write {

/**
 * @brief Minimal verified signer authority consumed by durable protocol writes.
 *
 * Implementations keep private-key access isolated. Callers can only require a
 * current network-authorized public key for an exact FID/scope and request a
 * signature over already-canonical protocol bytes.
 */
class IWriteAuthority {
public:
    virtual ~IWriteAuthority() = default;

    /** @brief Returns the exact current public key authorized for fid/scope. */
    [[nodiscard]] virtual foundation::result::Result<domain::signer::PublicKey> requireSigner(
        domain::identity::Fid fid,
        domain::signer::Scope requiredScope) const = 0;

    /** @brief Signs canonical bytes only when the current signer still permits the exact scope. */
    [[nodiscard]] virtual foundation::result::Result<domain::signer::Signature> sign(
        domain::signer::Scope requiredScope,
        const std::string& bytes) = 0;
};

} // namespace farcaster::capability::write
