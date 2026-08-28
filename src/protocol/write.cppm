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
