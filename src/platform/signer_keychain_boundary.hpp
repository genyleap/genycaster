// Textual compatibility boundary: isolates Apple Security/CoreFoundation C APIs
// from the C++26 named-module implementation unit.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace farcaster::platform::signer_vault::keychain_boundary {

inline constexpr std::size_t seedSize{32U};

enum class LoadState { Found, NotFound };

struct Failure final {
    std::string context{};
    std::optional<std::int64_t> nativeStatus{};
};

using Status = std::expected<void, Failure>;
using LoadResult = std::expected<LoadState, Failure>;

/** @brief Loads a fixed-size signer seed into caller-owned secure storage. */
[[nodiscard]] LoadResult load(
    std::string_view service,
    std::string_view account,
    std::span<unsigned char, seedSize> destination);

/** @brief Saves a fixed-size signer seed in the non-synchronizable Keychain item. */
[[nodiscard]] Status save(
    std::string_view service,
    std::string_view account,
    std::span<const unsigned char, seedSize> seed);

/** @brief Deletes a signer seed; a missing item is treated as successful deletion. */
[[nodiscard]] Status erase(std::string_view service, std::string_view account);

}
