/*!
 * @file        src/protocol/write.cppm
 * @brief       Protocol write codec creation.
 *
 * @details
 * Creates the mainnet canonical message encoder behind the
 * provider-neutral write capability.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <memory>

export module farcaster.protocol.write;

import farcaster.capability.write;

export namespace farcaster::protocol::write {

/**
 * @brief Creates the canonical mainnet Farcaster write codec.
 *
 * Network selection is intentionally not exposed to Application. Alternate
 * protocol-network composition belongs at the outer composition boundary.
 */
[[nodiscard]] std::unique_ptr<capability::write::ICanonicalWriteCodec> makeCanonicalWriteCodec();

} // namespace farcaster::protocol::write
