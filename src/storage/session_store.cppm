/*!
 * @file        src/storage/session_store.cppm
 * @brief       SQLite client-session persistence adapter.
 *
 * @details
 * Stores normalized session metadata without persisting protocol-signing
 * secrets.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <filesystem>
#include <memory>

export module farcaster.storage.session;

import farcaster.foundation.result;
import farcaster.local.session;

export namespace farcaster::storage::session {

/**
 * @brief Opens the SQLite-backed normalized client-session store.
 * @param path Database path shared with public read cache.
 * @return Owned session store or a structured migration/open failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<local::session::IClientSessionStore>>
openSessionStore(const std::filesystem::path& path);

}
