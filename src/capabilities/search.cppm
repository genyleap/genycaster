module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.capability.search;

import farcaster.domain.channel;
import farcaster.domain.profile;
import farcaster.foundation.result;

export namespace farcaster::capability::search {

/** @brief Provider-neutral channel suggestion returned by a derived search index. */
struct ChannelSuggestion final {
    domain::channel::ChannelId id{};
    std::string name{};
    /** Exact protocol parent_url, never inferred from channel ID. */
    std::optional<std::string> parentUrl{};
    std::optional<std::string> imageUrl{};
    std::optional<std::string> description{};
    std::uint64_t followerCount{};
};

/** @brief One provider-neutral search snapshot suitable for interactive suggestions. */
struct SearchPage final {
    std::vector<domain::profile::Profile> profiles{};
    std::vector<ChannelSuggestion> channels{};
};

/**
 * @brief Blocking derived-search capability used only from an I/O executor.
 *
 * Implementations own provider/index semantics. Returned suggestions contain
 * only provider-neutral profile/channel values and are not canonical ordering
 * guarantees.
 */
class ISearchSource {
public:
    /** @brief Destroys the search capability boundary. */
    virtual ~ISearchSource() = default;

    /**
     * @brief Finds profiles and channels matching one query.
     * @param query User-entered query without provider-specific syntax.
     * @param limit Maximum suggestions requested for each result class.
     * @return Search snapshot or a recoverable provider/transport failure.
     */
    [[nodiscard]] virtual foundation::result::Result<SearchPage>
    search(std::string_view query, std::size_t limit) = 0;
};

}
