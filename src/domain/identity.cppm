/*!
 * @file        src/domain/identity.cppm
 * @brief       Farcaster identity domain values.
 *
 * @details
 * Defines canonical FIDs, usernames and display names shared across
 * reads, routing and signed operations.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <compare>
#include <cstdint>
#include <string>

export module farcaster.domain.identity;

export namespace farcaster::domain::identity {

/** @brief Farcaster identifier for a protocol identity. */
struct Fid final {
    std::uint64_t value{};

    [[nodiscard]] constexpr bool operator==(const Fid&) const noexcept = default;
};

/** @brief Canonical Farcaster username snapshot. */
struct Username final {
    std::string value{};
};

/** @brief User-facing profile display name. */
struct DisplayName final {
    std::string value{};
};

}
