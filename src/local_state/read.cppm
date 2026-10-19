module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.local.read;

import farcaster.capability.feed;
import farcaster.capability.social;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::local::read {

/** @brief Stable local key for one materialized feed. */
struct FeedKey final {
    enum class Kind : std::uint8_t { Following, Trending, Channel, UserPosts, UserQuotes, UserReplies, UserLikes, UserPopular, Recent };
    Kind kind{Kind::Trending};
    std::optional<domain::identity::Fid> fid{};
    std::optional<domain::channel::ChannelId> channel{};
    // Provider page size carried with the request. It is intentionally not part
    // of cache identity and must not truncate an accumulated local snapshot.
    std::size_t limit{30};
};

/** @brief Stable local key for one materialized social graph list. */
struct SocialKey final {
    enum class Kind : std::uint8_t { Followers, Following };
    Kind kind{Kind::Followers};
    domain::identity::Fid fid{};
    // Provider page size; local reads return the complete materialized list.
    std::size_t limit{30};
};

/** @brief Stable persistence boundary for complete local-first read snapshots. */
class IReadStore {
public:
    /** @brief Destroys the persistence port. */
    virtual ~IReadStore() = default;
    /** @brief Finds a cached profile using a canonical username comparison. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfileByUsername(std::string_view username) = 0;
    /** @brief Finds a cached profile by FID. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::profile::Profile>>
    findProfile(domain::identity::Fid fid) = 0;
    /** @brief Finds a cached cast by canonical identifier. */
    [[nodiscard]] virtual foundation::result::Result<std::optional<domain::cast::Cast>>
    findCast(const domain::cast::CastId& castId) = 0;
    /** @brief Atomically upserts one canonical profile. */
    [[nodiscard]] virtual foundation::result::Status
    putProfile(const domain::profile::Profile& profile) = 0;
    /** @brief Atomically upserts one canonical cast. */
    [[nodiscard]] virtual foundation::result::Status
    putCast(const domain::cast::Cast& cast) = 0;
    /** @brief Materializes the complete cached feed accumulated for a query. */
    [[nodiscard]] virtual foundation::result::Result<capability::feed::FeedPage>
    readFeed(const FeedKey& key) = 0;
    /** @brief Upserts entities, then atomically commits feed ordering and continuation. */
    [[nodiscard]] virtual foundation::result::Status writeFeed(
        const FeedKey& key,
        const capability::feed::FeedPage& page,
        bool append) = 0;
    /** @brief Materializes the complete cached social graph list accumulated for a query. */
    [[nodiscard]] virtual foundation::result::Result<capability::social::SocialGraphPage>
    readSocial(const SocialKey& key) = 0;
    /** @brief Upserts profiles, then atomically commits social ordering and continuation. */
    [[nodiscard]] virtual foundation::result::Status writeSocial(
        const SocialKey& key,
        const capability::social::SocialGraphPage& page,
        bool append) = 0;
};

}
