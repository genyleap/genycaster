/*!
 * @file        src/local_state/signer.cppm
 * @brief       Non-secret signer metadata storage contract.
 *
 * @details
 * Persists signer readiness and registration metadata separately from
 * platform-protected private key material.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <optional>

export module farcaster.local.signer;

import farcaster.domain.identity;
import farcaster.domain.signer;
import farcaster.foundation.result;

export namespace farcaster::local::signer {

/**
 * @brief Persistence port for non-secret protocol-signer metadata.
 *
 * Implementations may persist signer ids, public keys and network-observed
 * registration metadata. Private/seed key bytes are forbidden by contract.
 */
class ISignerMetadataStore {
public:
    /** @brief Destroys the metadata store. */
    virtual ~ISignerMetadataStore() = default;

    /** @brief Loads the checkpoint for one FID, if present. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::signer::SignerRecord>>
    load(domain::identity::Fid fid) = 0;

    /** @brief Replaces the non-secret checkpoint for one FID. */
    [[nodiscard]] virtual foundation::result::Status save(
        const domain::signer::SignerRecord& record) = 0;

    /** @brief Deletes only signer metadata for the FID; secure key deletion is a separate port. */
    [[nodiscard]] virtual foundation::result::Status clear(domain::identity::Fid fid) = 0;
};

}
