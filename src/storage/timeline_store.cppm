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
