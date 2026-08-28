module;

#include <cstddef>
#include <span>
#include <string>
#include <vector>

export module farcaster.local.timeline;

import farcaster.domain.feed;
import farcaster.foundation.result;

export namespace farcaster::local::timeline {

/**
 * @brief Stable application-owned local timeline identifier.
 */
struct TimelineId final {
    std::string value{};
};

/**
 * @brief Stable port for ordered local timeline persistence.
 *
 * Implementations may block and therefore must not be called on the GUI thread.
 */
class ITimelineStore {
public:
    /** @brief Destroys the persistence port. */
    virtual ~ITimelineStore() = default;

    /**
     * @brief Atomically replaces one local timeline.
     * @param timelineId Stable local timeline identifier.
     * @param entries Borrowed entries valid only for the duration of the call.
     * @return Success or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Status replaceTimeline(
        const TimelineId& timelineId,
        std::span<const domain::feed::FeedEntry> entries) = 0;

    /**
     * @brief Reads at most limit entries in stable local order.
     * @param timelineId Stable local timeline identifier.
     * @param limit Maximum number of entries to return.
     * @return Owned entries or a structured storage failure.
     */
    [[nodiscard]] virtual foundation::result::Result<std::vector<domain::feed::FeedEntry>>
    readTimeline(const TimelineId& timelineId, std::size_t limit) = 0;
};

}
