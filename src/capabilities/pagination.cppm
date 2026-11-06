/*!
 * @file        src/capabilities/pagination.cppm
 * @brief       Provider-neutral pagination primitives.
 *
 * @details
 * Represents opaque application continuation handles without exposing
 * provider-specific cursor representations.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <compare>
#include <optional>
#include <string>

export module farcaster.capability.pagination;

export namespace farcaster::capability::pagination {

/**
 * @brief Application-owned opaque continuation handle.
 *
 * The token never contains or exposes a provider cursor. Infrastructure maps it
 * to provider-affine continuation state behind the routing/storage boundary.
 */
struct ContinuationToken final {
    std::string value{};
};

/** @brief Provider-neutral paginated read request. */
struct PageRequest final {
    std::size_t limit{30};
    std::optional<ContinuationToken> continuation{};
};

}
