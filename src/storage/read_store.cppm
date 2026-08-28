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
