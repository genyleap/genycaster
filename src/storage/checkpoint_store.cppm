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
