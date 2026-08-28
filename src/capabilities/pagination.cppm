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
