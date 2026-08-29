module;

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.provider.hypersnap.codec;

import farcaster.domain.cast;
import farcaster.domain.identity;
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

/** @brief Canonical social-graph users plus provider-private continuation cursor. */
struct SocialPayload final {
    std::vector<domain::profile::Profile> profiles{};
    std::optional<std::string> providerCursor{};
};

/** @brief Decodes the pinned Hypersnap v2 feed schema into canonical entities. */
[[nodiscard]] foundation::result::Result<FeedPayload> decodeFeed(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 single-user schema. */
[[nodiscard]] foundation::result::Result<domain::profile::Profile> decodeUser(std::string_view body);

/** @brief Decodes one standard Snapchain UserData message value. */
[[nodiscard]] foundation::result::Result<std::string> decodeUserDataValue(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 single-cast schema with its author. */
[[nodiscard]] foundation::result::Result<CastPayload> decodeCastPayload(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 single-cast schema. */
[[nodiscard]] foundation::result::Result<domain::cast::Cast> decodeCast(std::string_view body);

/** @brief Decodes the standard Snapchain reactionsByFid response. */
[[nodiscard]] foundation::result::Result<ReactionTargetsPayload> decodeReactionTargets(
    std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 followers/following response schema. */
[[nodiscard]] foundation::result::Result<SocialPayload> decodeSocial(std::string_view body);

}
