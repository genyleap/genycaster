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

/** @brief Owned bounded local timeline slice, not a provider page. */
struct TimelineWindow final {
    std::vector<domain::feed::FeedEntry> entries{};
    bool hasMore{false};
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

    /**
     * @brief Reads a bounded slice of non-tombstoned local entries.
     * @param timelineId Stable local timeline identifier.
     * @param offset Number of visible ordered entries to skip.
     * @param limit Maximum entries to return (1..256).
     * @return Owned window and local hasMore, or structured validation/storage error.
     * Provider continuations remain an independent concept.
     */
    [[nodiscard]] virtual foundation::result::Result<TimelineWindow>
    readTimelineWindow(const TimelineId& timelineId,
        std::size_t offset, std::size_t limit) = 0;

    /**
     * @brief Reads the complete locally materialized timeline in stable order.
     * @param timelineId Stable local timeline identifier.
     * @return Every persisted entry or a structured storage failure.
     *
     * Network page size is intentionally not reused as a local snapshot limit.
     * Pagination appends provider pages into one materialized timeline; callers
     * that reconcile UI state must observe the complete accumulated snapshot.
     */
    [[nodiscard]] virtual foundation::result::Result<std::vector<domain::feed::FeedEntry>>
    readAllTimeline(const TimelineId& timelineId) = 0;
};

}
