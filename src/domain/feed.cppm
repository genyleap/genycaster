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

}
