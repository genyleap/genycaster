module;

#include <filesystem>
#include <memory>

export module farcaster.storage.signer;

import farcaster.foundation.result;
import farcaster.local.signer;

export namespace farcaster::storage::signer {

/**
 * @brief Opens SQLite-backed non-secret protocol-signer metadata storage.
 * @param path Shared Genycaster SQLite database path.
 * @return Store or a structured storage failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<local::signer::ISignerMetadataStore>>
openSignerMetadataStore(const std::filesystem::path& path);

}
