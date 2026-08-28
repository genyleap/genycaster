module;

#include <optional>
#include <vector>

export module farcaster.capability.feed;

import farcaster.capability.pagination;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::capability::feed {

/**
 * @brief Canonical data returned by a paginated feed capability.
 *
 * The page owns the canonical entity snapshots needed to materialize the feed
 * locally. Provider response objects and cursors never cross this boundary.
 */
struct FeedPage final {
    std::vector<domain::cast::Cast> casts{};
    std::vector<domain::profile::Profile> profiles{};
    std::vector<domain::feed::FeedEntry> entries{};
    std::optional<pagination::ContinuationToken> next{};
};

/**
 * @brief Blocking provider-neutral feed read port.
 *
 * Implementations may perform network I/O. Callers must execute these methods
 * on an I/O executor and never on the Qt GUI thread.
 */
class IFeedSource {
public:
    /** @brief Destroys the feed source. */
    virtual ~IFeedSource() = default;

    /** @brief Reads the following feed for one Farcaster identity. */
    [[nodiscard]] virtual foundation::result::Result<FeedPage> following(
        domain::identity::Fid fid,
        const pagination::PageRequest& pageRequest) = 0;

    /** @brief Reads the provider's trending feed. */
    [[nodiscard]] virtual foundation::result::Result<FeedPage> trending(
        const pagination::PageRequest& pageRequest) = 0;

    /** @brief Reads a channel feed by canonical channel id. */
    [[nodiscard]] virtual foundation::result::Result<FeedPage> channel(
        const domain::channel::ChannelId& channelId,
        const pagination::PageRequest& pageRequest) = 0;
};

}
