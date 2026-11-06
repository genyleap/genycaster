/*!
 * @file        src/storage/signer_store.cppm
 * @brief       SQLite signer metadata storage adapter.
 *
 * @details
 * Stores non-secret signer registration and readiness state separate from
 * secure-vault key material.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
