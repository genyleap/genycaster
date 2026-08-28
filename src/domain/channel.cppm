module;

#include <compare>
#include <string>

export module farcaster.domain.channel;

export namespace farcaster::domain::channel {

/** @brief Stable channel identifier. */
struct ChannelId final {
    std::string value{};
};

/** @brief Minimal provider-neutral channel reference. */
struct ChannelRef final {
    ChannelId id{};
    std::string name{};
};

}
