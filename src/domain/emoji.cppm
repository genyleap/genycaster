/*!
 * @file        src/domain/emoji.cppm
 * @brief       Emoji domain identifiers and presentation states.
 *
 * @details
 * Models emoji references, animation availability and safe fallback
 * metadata without coupling to a particular renderer.
 *
 * @author      Kambiz Asadzadeh
 * @since       05 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

export module farcaster.domain.emoji;

export namespace farcaster::domain::emoji {

/** @brief Stable identity of a future Geny custom emoji reference. */
struct CustomEmojiRef final {
    std::string assetId{};
    std::string packId{};
    std::string fallback{};
    std::uint32_t revision{};
};

/** @brief Presentation status of one emoji glyph inside the product. */
enum class EmojiPresentation : std::uint8_t {
    /** @brief Render the system Unicode emoji glyph. */
    SystemFallback,
    /** @brief Render a validated static provider asset. */
    StaticAsset,
    /** @brief Render the validated animated provider asset. */
    AnimatedAsset,
};

/**
 * @brief Renderer-facing descriptor for one resolved emoji asset.
 *
 * The unicode sequence is always canonical content. assetKey and provider are
 * presentation metadata: they never replace the Unicode text and never appear
 * in published protocol text.
 */
struct AnimatedEmojiAsset final {
    std::string unicodeSequence{};
    std::string provider{};
    std::string assetKey{};
    std::filesystem::path localPath{};
    std::uint32_t revision{};
    std::uint32_t width{};
    std::uint32_t height{};
    bool animated{true};
    bool availableOffline{false};
};

/**
 * @brief Static fallback description for one emoji sequence.
 *
 * Static assets are optional; the system font glyph is the terminal fallback.
 */
struct StaticEmojiFallback final {
    std::string unicodeSequence{};
    std::optional<std::string> provider{};
    std::optional<std::string> assetKey{};
};

} // namespace farcaster::domain::emoji