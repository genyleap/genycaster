/*!
 * @file        src/storage/timeline_store.cppm
 * @brief       SQLite timeline persistence adapter.
 *
 * @details
 * Implements stable ordered local timeline windows for incremental
 * rendering and offline access.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <filesystem>
#include <memory>

export module farcaster.storage.timeline;

import farcaster.foundation.result;
import farcaster.local.timeline;

export namespace farcaster::storage::timeline {

/**
 * @brief Opens the SQLite implementation of ordered timeline persistence.
 * @param path SQLite database path owned by the returned store after open completes.
 * @return Store implementation or a structured storage failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<local::timeline::ITimelineStore>>
openTimelineStore(const std::filesystem::path& path);

}
