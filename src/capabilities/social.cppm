/*!
 * @file        src/capabilities/social.cppm
 * @brief       Provider-neutral social graph reads.
 *
 * @details
 * Represents follows and related relationship pages as canonical graph
 * state with scoped pagination.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <optional>
#include <vector>

export module farcaster.capability.social;

import farcaster.capability.pagination;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.social;
import farcaster.foundation.result;

export namespace farcaster::capability::social {

/** @brief Canonical paginated social-graph data. */
struct SocialGraphPage final {
    std::vector<domain::profile::Profile> profiles{};
    std::vector<domain::social::SocialRelation> relations{};
    std::optional<pagination::ContinuationToken> next{};
};

/**
 * @brief Blocking provider-neutral social graph read port.
 *
 * Implementations may perform network I/O and must not be invoked on the GUI thread.
 */
class ISocialGraphSource {
public:
    /** @brief Destroys the social graph source. */
    virtual ~ISocialGraphSource() = default;

    /** @brief Reads users that follow the requested identity. */
    [[nodiscard]] virtual foundation::result::Result<SocialGraphPage> followers(
        domain::identity::Fid fid,
        const pagination::PageRequest& pageRequest) = 0;

    /** @brief Reads users followed by the requested identity. */
    [[nodiscard]] virtual foundation::result::Result<SocialGraphPage> following(
        domain::identity::Fid fid,
        const pagination::PageRequest& pageRequest) = 0;
};

}
