/*!
 * @file        src/domain/social.cppm
 * @brief       Canonical social relationship entities.
 *
 * @details
 * Defines directed social graph edges and their protocol meaning
 * independently from feed providers.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>

export module farcaster.domain.social;

import farcaster.domain.identity;

export namespace farcaster::domain::social {

/**
 * @brief Protocol-social relationship represented by a canonical edge.
 */
enum class SocialRelationKind : std::uint8_t {
    Follows,
    Blocks,
};

/**
 * @brief Directed provider-neutral social graph edge.
 */
struct SocialRelation final {
    identity::Fid source{};
    identity::Fid target{};
    SocialRelationKind kind{SocialRelationKind::Follows};
};

}
