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
