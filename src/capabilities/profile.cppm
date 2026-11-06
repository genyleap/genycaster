/*!
 * @file        src/capabilities/profile.cppm
 * @brief       Canonical Farcaster profile read interface.
 *
 * @details
 * Defines the replaceable data-source boundary for profile lookup and
 * hydration.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

export module farcaster.capability.profile;

import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::capability::profile {

/**
 * @brief Blocking provider-neutral profile read port.
 *
 * Implementations may perform network I/O and must not be invoked on the GUI thread.
 */
class IProfileSource {
public:
    /** @brief Destroys the profile source. */
    virtual ~IProfileSource() = default;

    /** @brief Reads a canonical profile snapshot for a Farcaster identity. */
    [[nodiscard]] virtual foundation::result::Result<domain::profile::Profile>
    get(domain::identity::Fid fid) = 0;
};

}
