module;

#include <vector>

export module farcaster.domain.feed;

import farcaster.domain.cast;
import farcaster.foundation.time;

export namespace farcaster::domain::feed {

/**
 * @brief Stable local feed entry referencing canonical cast state.
 */
struct FeedEntry final {
    cast::CastId castId{};
    foundation::time::Timestamp sortTime{};
};

/**
 * @brief Account-scoped active reaction state used as a presentation overlay.
 *
 * Kept in the domain feed model instead of a provider interface so provider
 * modules do not expose STL containers after importing transport modules.
 */
struct ViewerReactionSnapshot final {
    std::vector<cast::CastId> liked{};
    std::vector<cast::CastId> recasted{};
    /** @brief Target-specific evidence. A partial account page can establish
     * presence without establishing absence; like/recast coverage differs. */
    std::vector<cast::CastId> likeExamined{};
    std::vector<cast::CastId> recastExamined{};
    /** @brief Backward-compatible intersection of complete type coverage. */
    std::vector<cast::CastId> examined{};
};

}
