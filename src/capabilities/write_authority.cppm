/*!
 * @file        src/capabilities/write_authority.cppm
 * @brief       Verified protocol write authority contract.
 *
 * @details
 * Exposes current signer identity and scope verification required by
 * durable writes without sharing private keys.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <string>

export module farcaster.capability.write_authority;

import farcaster.foundation.result;
import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.time;

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
        domain::identity::Fid fid,
        const domain::signer::PublicKey& expectedKey,
        domain::signer::Scope requiredScope,
        const std::string& bytes) = 0;

    /**
     * @brief Records an authoritative confirmed write for sliding signer-TTL state.
     *
     * A confirmed protocol message proves that this signer was accepted at the
     * supplied time. Implementations may persist that observation without
     * exposing or rotating private key material.
     */
    [[nodiscard]] virtual foundation::result::Status recordConfirmedWrite(
        domain::identity::Fid fid,
        foundation::time::Timestamp confirmedAt) = 0;
};

} // namespace farcaster::capability::write
