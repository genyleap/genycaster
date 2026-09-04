module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

export module farcaster.capability.animated_emoji;

import farcaster.domain.emoji;
import farcaster.foundation.result;

export namespace farcaster::capability::animated_emoji {

/**
 * @brief Provider-neutral request for one emoji sequence.
 *
 * sequence is the canonical Unicode text (the only content identity). key is
 * the provider-normalized lowercase-hex identifier used for asset lookup.
 */
struct EmojiAssetRequest final {
    std::string sequence{};
    std::string key{};
    std::uint32_t revision{};
    bool preferOffline{false};
};

/** @brief Outcome of resolving one emoji sequence against a provider. */
enum class AssetAvailability : std::uint8_t {
    /** @brief The provider knows the sequence but no validated asset is ready. */
    NotAvailable,
    /** @brief A validated animated asset is ready for rendering. */
    Ready,
    /** @brief Only static fallback information is known. */
    StaticOnly,
};

/**
 * @brief Provider-neutral emoji asset capability.
 *
 * Concrete providers map normalized Unicode sequences to their own asset
 * identifiers. Cache, network, and storage semantics never appear here.
 */
class IAnimatedEmojiProvider {
public:
    virtual ~IAnimatedEmojiProvider() = default;

    /** @brief Returns the stable provider identifier (e.g. "noto"). */
    [[nodiscard]] virtual std::string_view providerId() const = 0;

    /**
     * @brief Reports whether the provider may serve an animation for the key.
     *
     * This is metadata knowledge only; the asset may still be missing/corrupt.
     */
    [[nodiscard]] virtual bool supports(std::string_view key) const = 0;

    /**
     * @brief Resolves asset metadata without guaranteeing local availability.
     *
     * Recoverable failures (unsupported sequence, unknown key, revision
     * mismatch) return an error describing the exact reason.
     */
    [[nodiscard]] virtual foundation::result::Result<domain::emoji::AnimatedEmojiAsset>
    resolveAsset(const EmojiAssetRequest& request) const = 0;

    /** @brief Returns the static fallback description for a sequence, if any. */
    [[nodiscard]] virtual std::optional<domain::emoji::StaticEmojiFallback>
    staticFallback(std::string_view sequence) const = 0;

    /**
     * @brief Returns the canonical fetch URL for one normalized key.
     *
     * Returns empty when the key is unsupported or the provider has no remote
     * source. URLs are provider metadata consumed by the download boundary.
     */
    [[nodiscard]] virtual std::string downloadUrlFor(std::string_view key) const = 0;
};

/** @brief One validated asset stored by a concrete cache implementation. */
struct CachedEmojiAsset final {
    std::filesystem::path path{};
    std::size_t sizeBytes{};
    std::uint32_t revision{};
    std::string provider{};
    std::string assetKey{};
    std::array<std::byte, 32> sha256{};
};

/**
 * @brief Provider-neutral emoji asset cache port.
 *
 * Implementations own deterministic keys, atomic writes, corruption detection
 * and bounded eviction; consumers only read and store validated assets.
 */
class IEmojiAssetCache {
public:
    virtual ~IEmojiAssetCache() = default;

    /** @brief Loads a validated cached asset, evicting corrupt entries. */
    [[nodiscard]] virtual foundation::result::Result<CachedEmojiAsset> read(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) = 0;

    /** @brief Atomically stores a validated asset with its digest. */
    [[nodiscard]] virtual foundation::result::Status store(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision,
        std::span<const std::byte> bytes,
        std::array<std::byte, 32> sha256) = 0;

    /** @brief Reports whether a valid entry exists without loading it. */
    [[nodiscard]] virtual bool contains(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) = 0;
};

} // namespace farcaster::capability::animated_emoji