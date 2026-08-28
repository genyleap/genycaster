module;

#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.provider.hypersnap.codec;

import farcaster.domain.cast;
import farcaster.domain.identity;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::provider::hypersnap::codec {

/** @brief Canonical feed payload plus provider-private continuation cursor. */
struct FeedPayload final {
    std::vector<domain::cast::Cast> casts{};
    std::vector<domain::profile::Profile> profiles{};
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

/** @brief Decodes the pinned Hypersnap v2 single-cast schema. */
[[nodiscard]] foundation::result::Result<domain::cast::Cast> decodeCast(std::string_view body);

/** @brief Decodes the pinned Hypersnap v2 followers/following response schema. */
[[nodiscard]] foundation::result::Result<SocialPayload> decodeSocial(std::string_view body);

}
