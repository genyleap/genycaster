/*!
 * @file        src/storage/checkpoint_store.cppm
 * @brief       SQLite synchronization checkpoint adapter.
 *
 * @details
 * Opens durable semantic checkpoint storage independent from
 * provider-internal continuation tokens.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <filesystem>
#include <memory>

export module farcaster.storage.checkpoint;

import farcaster.foundation.result;
import farcaster.sync.checkpoint;

export namespace farcaster::storage::checkpoint {

/**
 * @brief Opens the SQLite implementation of semantic sync-checkpoint persistence.
 * @param path SQLite database path owned by the returned store after open completes.
 * @return Store implementation or a structured storage failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<sync::checkpoint::ICheckpointStore>>
openCheckpointStore(const std::filesystem::path& path);

}
