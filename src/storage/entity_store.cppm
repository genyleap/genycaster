/*!
 * @file        src/storage/entity_store.cppm
 * @brief       SQLite canonical entity storage adapter.
 *
 * @details
 * Opens the persistence implementation for normalized domain entities and
 * identity state.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <filesystem>
#include <memory>

export module farcaster.storage.entity;

import farcaster.foundation.result;
import farcaster.local.entity;

export namespace farcaster::storage::entity {

/**
 * @brief Opens the SQLite implementation of canonical entity persistence.
 * @param path SQLite database path owned by the returned store after open completes.
 * @return Store implementation or a structured storage failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<local::entity::IEntityStore>>
openEntityStore(const std::filesystem::path& path);

}
