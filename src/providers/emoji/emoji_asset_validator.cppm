module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

export module farcaster.provider.emoji.emoji_asset_validator;

import farcaster.foundation.result;

export namespace farcaster::provider::emoji::validator {

/** @brief SHA-256 digest used for asset checksum verification. */
using Sha256 = std::array<std::byte, 32>;

/**
 * @brief Bounded validation limits for one emoji asset.
 *
 * These are the safety envelope for untrusted downloaded assets. All values
 * are deliberately larger than the upstream Noto files but small enough to
 * bound parser and renderer cost.
 */
struct ValidationLimits final {
    std::size_t maxFileBytes{2U * 1024U * 1024U};
    std::uint32_t maxLayers{512U};
    std::uint32_t maxAssets{512U};
    std::uint32_t maxDimension{4096U};
    std::uint32_t minDimension{8U};
    double maxFrameRate{120.0};
    double minFrameRate{1.0};
    std::uint32_t maxFrameCount{3600U};
    std::size_t maxJsonDepth{64U};
    std::size_t maxJsonTokens{200'000U};
};

/**
 * @brief Metadata extracted from a validated Lottie asset.
 */
struct ValidatedEmojiAssetInfo final {
    std::string provider{};
    std::string assetKey{};
    std::uint32_t revision{};
    std::uint32_t width{};
    std::uint32_t height{};
    double frameRate{};
    double durationSeconds{};
    std::uint32_t frameCount{};
    std::uint32_t layerCount{};
    Sha256 assetSha256{};
};

/**
 * @brief Computes the SHA-256 digest of a byte range.
 */
[[nodiscard]] Sha256 sha256(std::span<const std::byte> bytes) noexcept;

/**
 * @brief Validates a Lottie emoji asset before it may enter the cache/renderer.
 *
 * Rejects: oversized files, invalid JSON, unsupported animation primitives
 * (3D layers, image layers, text layers, embedded/external resources),
 * unbounded dimensions/frame rate/duration/layer counts, and any unexpected
 * URL string inside the document. When expectedSha256 is present the digest
 * must match.
 */
[[nodiscard]] foundation::result::Result<ValidatedEmojiAssetInfo> validateEmojiAsset(
    std::string_view provider,
    std::string_view assetKey,
    std::span<const std::byte> bytes,
    ValidationLimits limits = {},
    std::optional<Sha256> expectedSha256 = std::nullopt) noexcept;

} // namespace farcaster::provider::emoji::validator