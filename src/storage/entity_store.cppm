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
