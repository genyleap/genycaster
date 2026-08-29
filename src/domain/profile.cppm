module;

#include <cstdint>
#include <optional>
#include <string>

export module farcaster.domain.profile;

import farcaster.domain.identity;

export namespace farcaster::domain::profile {

/**
 * @brief Provider-neutral canonical Farcaster profile snapshot.
 */
struct Profile final {
    identity::Fid fid{};
    std::optional<identity::Username> username{};
    std::optional<identity::DisplayName> displayName{};
    std::string bio{};
    std::optional<std::string> avatarUrl{};
    std::optional<std::string> bannerUrl{};
    std::optional<std::string> location{};
    std::optional<std::string> custodyAddress{};
    std::uint64_t followerCount{};
    std::uint64_t followingCount{};
};

}
