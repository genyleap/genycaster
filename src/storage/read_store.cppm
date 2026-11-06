/*!
 * @file        src/storage/read_store.cppm
 * @brief       SQLite local-first read cache adapter.
 *
 * @details
 * Stores canonical read snapshots and viewer-scoped feed/social
 * projections for offline continuity.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <filesystem>
#include <memory>

export module farcaster.storage.read;

import farcaster.foundation.result;
import farcaster.local.read;

export namespace farcaster::storage::read {

/**
 * @brief Opens the SQLite implementation of local-first read persistence.
 * @param path SQLite database path shared with canonical entity/timeline stores.
 * @return Owned store or a structured open/migration failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<local::read::IReadStore>>
openReadStore(const std::filesystem::path& path);

}
