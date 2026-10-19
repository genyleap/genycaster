module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.provider.hypersnap.codec;

import farcaster.capability.search;
import farcaster.capability.notification;
import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.feed;
import farcaster.domain.profile;
import farcaster.foundation.result;
import farcaster.foundation.time;

export namespace farcaster::provider::hypersnap::codec {

/** @brief Canonical feed payload plus provider-private continuation cursor. */
struct FeedPayload final {
    std::vector<domain::cast::Cast> casts{};
    std::vector<domain::profile::Profile> profiles{};
    std::optional<std::string> providerCursor{};
};

/** @brief Canonical single-cast payload with the author snapshot returned beside it. */
struct CastPayload final {
    domain::cast::Cast cast{};
    domain::profile::Profile author{};
};

/** @brief Canonical thread root plus root-author snapshot from a conversation response. */
struct ConversationPayload final {
    domain::cast::Cast cast{};
    domain::profile::Profile author{};
    std::size_t directReplyCount{0};
};

/** @brief Canonical target of one active protocol reaction. */
struct ReactionTarget final {
    domain::identity::Fid author{};
    domain::cast::CastId castId{};
    foundation::time::Timestamp reactedAt{};
};

/** @brief Active reaction targets plus the raw Snapchain pagination cursor. */
struct ReactionTargetsPayload final {
    std::vector<ReactionTarget> targets{};
    std::optional<std::string> providerCursor{};
};

/** @brief Canonical notification rows plus the provider-private cursor. */
struct NotificationPayload final {
    std::vector<capability::notification::NotificationItem> items{};
    std::optional<std::string> providerCursor{};
};


/** @brief One canonical UserData value returned by the protocol-wide FID lookup. */
struct UserDataEntry final {
    std::string type{};
    std::string value{};
};

/** @brief Canonical social-graph users plus provider-private continuation cursor. */
struct SocialPayload final {
    std::vector<domain::profile::Profile> profiles{};
    std::optional<std::string> providerCursor{};
};

/** @brief One confirmed Snapchain root cast identified without user or text fields. */
struct RecentCastEvent final {
    std::string hash{};
    // Verified protocol parent, retained even if the public bulk index omits it.
    std::optional<std::string> parentHash{};
    std::uint64_t protocolTimestamp{};
    std::uint64_t eventId{};
};

/** @brief The confirmed chain height of one data shard. */
struct RecentShardHeight final {
    std::uint64_t shard{};
    std::uint64_t height{};
};

/** @brief Decodes shard heights needed for a deterministic recent-cast window.
 * The protocol identity shard (zero) is excluded. Unexpected schema fails
 * explicitly rather than showing stale ranked results as recent.
 */
[[nodiscard]] foundation::result::Result<std::vector<RecentShardHeight>>
decodeRecentShardInfo(std::string_view body);

/** @brief Decodes confirmed root CastAdd messages inside a bounded block interval.
 * Author identity, text and signatures are hydrated separately from the cast API.
 */
[[nodiscard]] foundation::result::Result<std::vector<RecentCastEvent>>
decodeRecentCastEvents(std::string_view body, std::uint64_t startBlock,
                       std::uint64_t endBlock, bool repliesOnly = false);

/** @brief Decodes the pinned Hypersnap v2 feed schema into canonical entities. */
[[nodiscard]] foundation::result::Result<FeedPayload> decodeFeed(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 single-user schema. */
[[nodiscard]] foundation::result::Result<domain::profile::Profile> decodeUser(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 user-search response. */
[[nodiscard]] foundation::result::Result<std::vector<domain::profile::Profile>> decodeUsers(
    std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 channel-search response. */
[[nodiscard]] foundation::result::Result<std::vector<capability::search::ChannelSuggestion>>
decodeSearchChannels(std::string_view body);

/** @brief Decodes one standard Snapchain UserData message value. */
[[nodiscard]] foundation::result::Result<std::string> decodeUserDataValue(std::string_view body);

/** @brief Decodes all standard Snapchain UserData messages returned for one FID. */
[[nodiscard]] foundation::result::Result<std::vector<UserDataEntry>> decodeUserDataEntries(
    std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 single-cast schema with its author. */
[[nodiscard]] foundation::result::Result<CastPayload> decodeCastPayload(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 single-cast schema. */
[[nodiscard]] foundation::result::Result<domain::cast::Cast> decodeCast(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 conversation root and recursive direct replies. */
[[nodiscard]] foundation::result::Result<ConversationPayload> decodeConversationPayload(
    std::string_view body);

/** @brief Decodes the standard Snapchain reactionsByFid response. */
[[nodiscard]] foundation::result::Result<ReactionTargetsPayload> decodeReactionTargets(
    std::string_view body);

/** @brief Decodes the documented legacy batch cast-interactions response for one viewer. */
[[nodiscard]] foundation::result::Result<domain::feed::ViewerReactionSnapshot> decodeCastInteractions(
    std::string_view body, domain::identity::Fid viewer);

/** @brief Decodes the pinned Hypersnap v2 notifications response schema. */
[[nodiscard]] foundation::result::Result<NotificationPayload> decodeNotifications(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 followers/following response schema. */
[[nodiscard]] foundation::result::Result<SocialPayload> decodeSocial(std::string_view body);

}
