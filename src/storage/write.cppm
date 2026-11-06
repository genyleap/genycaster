/*!
 * @file        src/storage/write.cppm
 * @brief       SQLite durable write outbox adapter.
 *
 * @details
 * Persists exact signed envelopes, chosen route binding and local write
 * projections for restart-safe reconciliation.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <filesystem>
#include <memory>

export module farcaster.storage.write;

import farcaster.foundation.result;
import farcaster.local.write;

export namespace farcaster::storage::write {

/**
 * @brief Opens the SQLite durable outbox/local-write-projection store.
 * @param path SQLite database path shared with canonical read/entity storage.
 * @return Store implementation or a structured storage failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<local::write::IOutboxStore>>
openOutboxStore(const std::filesystem::path& path);

/**
 * @brief Opens a dedicated SQLite route-binding store for canonical writes.
 * The backing database must be the same file used by the durable outbox.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<local::write::IWriteRouteBindings>>
openWriteRouteBindings(const std::filesystem::path& path);

} // namespace farcaster::storage::write
