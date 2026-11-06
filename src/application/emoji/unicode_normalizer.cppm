/*!
 * @file        src/application/emoji/unicode_normalizer.cppm
 * @brief       Emoji sequence detection and normalization.
 *
 * @details
 * Parses UTF-8 emoji clusters and converts sequences to stable lookup
 * keys for asset and presentation decisions.
 *
 * @author      Kambiz Asadzadeh
 * @since       05 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.application.emoji.unicode_normalizer;

export namespace farcaster::application::emoji::unicode {

/**
 * @brief One detected emoji cluster inside UTF-8 text.
 *
 * A cluster is a maximal sequence of codepoints that must stay together for
 * emoji presentation: base + variation selectors, skin tones, ZWJ chains,
 * flags, keycaps, and tag sequences. The cluster is the unit of animation.
 */
struct EmojiCluster final {
    std::string sequence{};
    std::string key{};
    std::size_t byteOffset{};
    std::size_t byteLength{};
};

/**
 * @brief Normalizes an emoji sequence to a canonical lowercase-hex key.
 *
 * Key rules follow the upstream Noto asset naming: codepoints joined with
 * underscores, VS16 retained only when the base needs it for emoji
 * presentation (e.g. "2764_fe0f"), VS15 stripped, ZWJ/skin tones retained.
 *
 * @return std::nullopt for invalid UTF-8, empty, or non-emoji input.
 */
[[nodiscard]] std::optional<std::string> normalizedKey(std::string_view sequence) noexcept;

/**
 * @brief Scans UTF-8 text and returns every emoji cluster with byte offsets.
 *
 * Text-only codepoints never start a cluster. A text-presentation-default
 * codepoint starts a cluster only when followed by VS16. Malformed UTF-8 is
 * skipped without terminating the scan.
 */
[[nodiscard]] std::vector<EmojiCluster> scanEmojiClusters(std::string_view text) noexcept;

/**
 * @brief Returns the byte length of the emoji cluster starting at byteOffset.
 *
 * Returns zero when the byte at byteOffset does not begin an emoji cluster.
 * The length is bounded (see implementation) so streaming parsers can make
 * forward progress safely.
 */
[[nodiscard]] std::size_t emojiByteLengthAt(std::string_view text, std::size_t byteOffset) noexcept;

/**
 * @brief Reports whether a standalone sequence is a well-formed emoji cluster.
 */
[[nodiscard]] bool isEmojiSequence(std::string_view sequence) noexcept;

} // namespace farcaster::application::emoji::unicode