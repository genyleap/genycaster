module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.provider.emoji.emoji_cache;

import farcaster.capability.animated_emoji;
import farcaster.foundation.result;
import farcaster.provider.emoji.emoji_asset_validator;

export namespace farcaster::provider::emoji::cache {

/** @brief On-disk representation; prepared raster frames never alias source Lottie assets. */
enum class EmojiCacheFormat : std::uint8_t {
    Lottie,
    SpritePng,
};

/**
 * @brief Bounded disk cache policy for validated emoji assets.
 */
struct EmojiCacheConfig final {
    std::filesystem::path rootDirectory{};
    std::size_t maxCacheBytes{64U * 1024U * 1024U};
    std::size_t maxEntryBytes{4U * 1024U * 1024U};
    std::uint32_t maxEntries{2048U};
    std::chrono::seconds orphanTempMaxAge{std::chrono::hours{1}};
    EmojiCacheFormat format{EmojiCacheFormat::Lottie};
};

/**
 * @brief Outcome of one cache lookup.
 */
struct CacheLookup final {
    std::filesystem::path path{};
    std::size_t sizeBytes{};
    std::uint32_t revision{};
    std::string provider{};
    std::string assetKey{};
    validator::Sha256 sha256{};
};

/**
 * @brief Deterministic, revision-aware, bounded emoji asset disk cache.
 *
 * Writes are atomic (temp file + rename) and every entry carries a sidecar
 * metadata file so corruption and tampering are detected on read. The cache is
 * pure filesystem logic; network and UI concerns stay outside. Implements the
 * provider-neutral cache port so the application layer never sees disk paths.
 */
class EmojiAssetCache final : public capability::animated_emoji::IEmojiAssetCache {
public:
    explicit EmojiAssetCache(EmojiCacheConfig config);

    /**
     * @brief Deterministic cache file path for one provider/key/revision.
     *
     * Does not create the file; the path is derived from a SHA-256 key.
     */
    [[nodiscard]] std::filesystem::path pathFor(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) const;

    /**
     * @brief Returns true when a valid cached entry exists.
     *
     * Corrupted entries are removed and reported as absent.
     */
    [[nodiscard]] bool contains(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) noexcept override;

    /**
     * @brief Loads a validated cached entry.
     *
     * Recoverable failures: NotFound, StorageFailure. Corrupt entries are
     * evicted automatically and reported as NotFound with diagnostic text.
     */
    [[nodiscard]] foundation::result::Result<capability::animated_emoji::CachedEmojiAsset> read(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) noexcept override;

    /**
     * @brief Atomically stores a validated asset with its digest.
     *
     * Lottie bytes must have passed the emoji asset validator. SpritePng bytes
     * must be generated from bounded, validated frames by the native renderer.
     * The stored digest is verified again on every read.
     */
    [[nodiscard]] foundation::result::Status store(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision,
        std::span<const std::byte> bytes,
        validator::Sha256 sha256) noexcept override;

    /** @brief Enforces the byte/entry budget, evicting oldest entries first. */
    void evict() noexcept;

    /** @brief Removes all cache entries and metadata. */
    void clear() noexcept;

    /** @brief Total validated bytes currently stored. */
    [[nodiscard]] std::size_t totalBytes() const noexcept;

private:
    struct EntryState final {
        std::filesystem::path assetPath{};
        std::filesystem::path metaPath{};
        std::uint64_t sizeBytes{};
        std::filesystem::file_time_type lastWrite{};
    };

    void prepareDirectories() noexcept;
    [[nodiscard]] std::filesystem::path metaPathFor(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) const;
    [[nodiscard]] std::vector<EntryState> enumerateEntries() const noexcept;
    [[nodiscard]] std::optional<CacheLookup> readEntry(const EntryState& entry) const noexcept;
    void removeEntry(const EntryState& entry) const noexcept;
    void pruneOrphanTemps() noexcept;

    EmojiCacheConfig m_config{};
};

} // namespace farcaster::provider::emoji::cache
