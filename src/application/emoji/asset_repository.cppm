/*!
 * @file        src/application/emoji/asset_repository.cppm
 * @brief       Emoji asset orchestration and cache selection.
 *
 * @details
 * Coordinates validated provider assets, disk lookup and memory state
 * without placing fetching or decoding in presentation code.
 *
 * @author      Kambiz Asadzadeh
 * @since       05 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

export module farcaster.application.emoji.asset_repository;

import farcaster.application.emoji.catalog;
import farcaster.application.emoji.unicode_normalizer;
import farcaster.capability.animated_emoji;
import farcaster.domain.emoji;
import farcaster.foundation.result;

export namespace farcaster::application::emoji::repository {

/**
 * @brief I/O-free rendering decision for one emoji sequence.
 */
struct AssetDecision final {
    domain::emoji::EmojiPresentation presentation{domain::emoji::EmojiPresentation::SystemFallback};
    std::optional<domain::emoji::AnimatedEmojiAsset> asset{};
    std::optional<domain::emoji::StaticEmojiFallback> staticFallback{};
};

/**
 * @brief Coordinates the provider, disk cache and in-memory registry.
 *
 * The repository is the single entry point the presentation layer talks to.
 * Synchronous APIs never touch disk or network; the Qt boundary drives the
 * asynchronous fetch pipeline and reports results back through
 * registerValidatedAsset / markUnavailable.
 */
class EmojiAssetRepository final {
public:
    /**
     * @brief Creates the repository over borrowed provider/cache/catalog.
     *
     * All three dependencies must outlive the repository.
     */
    explicit EmojiAssetRepository(
        capability::animated_emoji::IAnimatedEmojiProvider& provider,
        capability::animated_emoji::IEmojiAssetCache& cache,
        catalog::EmojiCatalog& catalog,
        std::size_t memoryBudget = 512U);

    /**
     * @brief Resolves the rendering decision for a Unicode emoji sequence.
     *
     * Never blocks: bundled assets and already-registered assets are returned
     * immediately; everything else falls back to the system glyph until the
     * asynchronous pipeline reports availability.
     */
    [[nodiscard]] AssetDecision decide(
        std::string_view sequence,
        std::uint32_t revision) const;

    /**
     * @brief Normalized provider key for a sequence, if it is an emoji cluster.
     */
    [[nodiscard]] std::optional<std::string> normalizedKey(std::string_view sequence) const;

    /**
     * @brief Returns true when a validated animated asset is renderable now.
     */
    [[nodiscard]] bool isAvailable(std::string_view sequence, std::uint32_t revision) const;

    /**
     * @brief Registers a validated asset (bundled, cached, or downloaded) into
     * the bounded in-memory registry.
     */
    void registerValidatedAsset(domain::emoji::AnimatedEmojiAsset asset) noexcept;

    /**
     * @brief Marks a sequence as unavailable so repeated failures are not
     * retried immediately (cooldown window).
     */
    void markUnavailable(std::string_view sequence) noexcept;

    /** @brief Returns true while the sequence is inside the failure cooldown. */
    [[nodiscard]] bool recentlyFailed(std::string_view sequence) const noexcept;

    /** @brief Number of validated assets currently held in memory. */
    [[nodiscard]] std::size_t registeredCount() const noexcept;

    /** @brief Removes the in-memory registry without touching disk. */
    void clearMemoryRegistry() noexcept;

private:
    [[nodiscard]] std::string memoryKey(
        std::string_view provider,
        std::string_view key,
        std::uint32_t revision) const;
    void pruneMemoryRegistry() noexcept;

    capability::animated_emoji::IAnimatedEmojiProvider& m_provider;
    capability::animated_emoji::IEmojiAssetCache& m_cache;
    catalog::EmojiCatalog& m_catalog;
    std::size_t m_memoryBudget{512U};
    std::unordered_map<std::string, domain::emoji::AnimatedEmojiAsset> m_registry{};
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_failedKeys{};
};

} // namespace farcaster::application::emoji::repository