module;

#include <array>
#include <cstddef>
#include <span>

export module farcaster.protocol.hash;

export namespace farcaster::protocol::hash {

/** @brief Canonical 20-byte Farcaster message hash. */
using Hash20 = std::array<std::byte, 20>;

/**
 * @brief Computes the BLAKE3 digest used by Farcaster and truncates it to 20 bytes.
 * @param bytes Canonical protobuf-encoded MessageData bytes.
 * @return First 20 bytes of the unkeyed BLAKE3 digest.
 */
[[nodiscard]] Hash20 blake3Digest20(std::span<const std::byte> bytes) noexcept;

}
