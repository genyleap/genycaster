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
