/*!
 * @file        src/domain/feed.cppm
 * @brief       Canonical feed entries and viewer reaction state.
 *
 * @details
 * Defines stable feed references and account-scoped interaction overlays
 * separate from remote ranking cursors.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
