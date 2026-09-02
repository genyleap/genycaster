module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module farcaster.application.read.model;

import farcaster.capability.pagination;
import farcaster.capability.search;
import farcaster.domain.cast;
import farcaster.domain.channel;
import farcaster.domain.feed;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.domain.social;
import farcaster.foundation.error;

export namespace farcaster::application::read::model {

/** @brief Lifecycle phase for a local-first read snapshot. */
enum class ReadPhase : std::uint8_t { LoadingCache, Refreshing, Ready, Failed };

/** @brief Provider-neutral recovery intent for one routed refresh attempt. */
struct ReadOptions final {
    bool allowRecoveryProbe{false};
};

/** @brief Provider-neutral lifecycle metadata presented alongside usable data. */
struct ReadMetadata final {
    ReadPhase phase{ReadPhase::LoadingCache};
    bool isStale{false};
    bool isSourceUnavailable{false};
    std::optional<foundation::error::Error> error{};
};

/** @brief Operation-safe feed selector owned by Application. */
struct FeedQuery final {
    enum class Kind : std::uint8_t { Following, Trending, Channel, UserPosts, UserQuotes, UserReplies, UserLikes, UserPopular };
    Kind kind{Kind::Trending};
    std::optional<domain::identity::Fid> fid{};
    std::optional<domain::channel::ChannelId> channel{};
    std::size_t limit{10};
    std::optional<capability::pagination::ContinuationToken> continuation{};
};

/** @brief Canonical materialized feed plus local-first lifecycle metadata. */
struct FeedState final {
    FeedQuery query{};
    std::vector<domain::cast::Cast> casts{};
    std::vector<domain::profile::Profile> profiles{};
    std::vector<domain::feed::FeedEntry> entries{};
    std::optional<capability::pagination::ContinuationToken> next{};
    ReadMetadata metadata{};
};

/** @brief Canonical identity/profile result and lifecycle metadata. */
struct IdentityState final {
    std::string username{};
    std::optional<domain::identity::Fid> fid{};
    std::optional<domain::profile::Profile> profile{};
    ReadMetadata metadata{};
};

/** @brief Canonical profile result and lifecycle metadata. */
struct ProfileState final {
    domain::identity::Fid fid{};
    std::optional<domain::profile::Profile> profile{};
    ReadMetadata metadata{};
};

/** @brief Canonical cast result and lifecycle metadata. */
struct CastState final {
    domain::cast::CastId castId{};
    std::optional<domain::cast::Cast> cast{};
    std::optional<domain::profile::Profile> author{};
    ReadMetadata metadata{};
};

/** @brief Operation-safe interactive search selector owned by Application. */
struct SearchQuery final {
    std::string text{};
    std::size_t limit{6};
};

/** @brief Derived interactive search snapshot and lifecycle metadata. */
struct SearchState final {
    SearchQuery query{};
    capability::search::SearchPage page{};
    ReadMetadata metadata{};
};

/** @brief Operation-safe social graph selector owned by Application. */
struct SocialQuery final {
    enum class Kind : std::uint8_t { Followers, Following };
    Kind kind{Kind::Followers};
    domain::identity::Fid fid{};
    std::size_t limit{30};
    std::optional<capability::pagination::ContinuationToken> continuation{};
};

/** @brief Canonical social graph materialization and lifecycle metadata. */
struct SocialState final {
    SocialQuery query{};
    std::vector<domain::profile::Profile> profiles{};
    std::vector<domain::social::SocialRelation> relations{};
    std::optional<capability::pagination::ContinuationToken> next{};
    ReadMetadata metadata{};
};

}
