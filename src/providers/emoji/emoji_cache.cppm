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

import farcaster.foundation.result;
import farcaster.provider.emoji.emoji_asset_validator;

export namespace farcaster::provider::emoji::cache {

/**
 * @brief Bounded disk cache policy for validated emoji assets.
 */
struct EmojiCacheConfig final {
    std::filesystem::path rootDirectory{};
    std::size_t maxCacheBytes{64U * 1024U * 1024U};
    std::size_t maxEntryBytes{4U * 1024U * 1024U};
    std::uint32_t maxEntries{2048U};
    std::chrono::seconds orphanTempMaxAge{std::chrono::hours{1}};
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
 * pure filesystem logic; network and UI concerns stay outside.
 */
class EmojiAssetCache final {
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
        std::uint32_t revision) noexcept;

    /**
     * @brief Loads a validated cached entry.
     *
     * Recoverable failures: NotFound, StorageFailure. Corrupt entries are
     * evicted automatically and reported as NotFound with diagnostic text.
     */
    [[nodiscard]] foundation::result::Result<CacheLookup> read(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) noexcept;

    /**
     * @brief Atomically stores a validated asset with its digest.
     *
     * The byte range must already have passed the emoji asset validator. The
     * stored digest is verified again on every read.
     */
    [[nodiscard]] foundation::result::Status store(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision,
        std::span<const std::byte> bytes,
        validator::Sha256 sha256) noexcept;

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