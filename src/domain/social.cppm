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
