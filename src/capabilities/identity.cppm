/*!
 * @file        src/capabilities/identity.cppm
 * @brief       Farcaster identity resolution contract.
 *
 * @details
 * Maps user identity queries to canonical protocol identifiers through a
 * replaceable provider interface.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <string_view>

export module farcaster.capability.identity;

import farcaster.domain.identity;
import farcaster.foundation.result;

export namespace farcaster::capability::identity {

/**
 * @brief Blocking provider-neutral identity resolution port.
 *
 * The initial contract resolves an existing Farcaster username to its FID.
 */
class IIdentityResolver {
public:
    /** @brief Destroys the identity resolver. */
    virtual ~IIdentityResolver() = default;

    /** @brief Resolves a canonical Farcaster username to a Farcaster identity. */
    [[nodiscard]] virtual foundation::result::Result<domain::identity::Fid>
    resolveUsername(std::string_view username) = 0;
};

}
