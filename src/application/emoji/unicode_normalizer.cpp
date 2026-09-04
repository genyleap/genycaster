module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.application.emoji.unicode_normalizer;

namespace farcaster::application::emoji::unicode {
namespace {

using CodePoint = char32_t;

constexpr CodePoint Replacement = 0xFFFDU;
constexpr CodePoint VariationSelector16 = 0xFE0FU;
constexpr CodePoint VariationSelector15 = 0xFE0EU;
constexpr CodePoint ZeroWidthJoiner = 0x200DU;
constexpr CodePoint CombiningEnclosingKeycap = 0x20E3U;
constexpr CodePoint SkinToneStart = 0x1F3FBU;
constexpr CodePoint SkinToneEnd = 0x1F3FFU;
constexpr CodePoint RegionalIndicatorStart = 0x1F1E6U;
constexpr CodePoint RegionalIndicatorEnd = 0x1F1FFU;
constexpr CodePoint TagStart = 0xE0020U;
constexpr CodePoint TagEnd = 0xE007FU;
constexpr CodePoint CancelTag = 0xE007FU;

// Bounded scan: a social cast line is at most 1024 bytes; generous bounds keep
// adversarial input from turning one scan into a long-running computation.
constexpr std::size_t MaximumClusterCodePoints = 16U;
constexpr std::size_t MaximumClusterBytes = 96U;

/**
 * @brief Unicode ranges with Emoji=Yes that default to emoji presentation.
 *
 * These codepoints render as emoji without VS16. The list follows the stable
 * Emoji_Presentation property ranges (Unicode 15/16).
 */
constexpr std::array<std::pair<CodePoint, CodePoint>, 17> EmojiDefaultRanges{{
    {0x231AU, 0x231BU},
    {0x23E9U, 0x23F3U},
    {0x23F8U, 0x23FAU},
    {0x25FDU, 0x25FEU},
    {0x2614U, 0x2615U},
    {0x2648U, 0x2653U},
    {0x267FU, 0x267FU},
    {0x2693U, 0x2693U},
    {0x26A1U, 0x26A1U},
    {0x26AAU, 0x26ABU},
    {0x26BDU, 0x26BEU},
    {0x26C4U, 0x26C5U},
    {0x26CEU, 0x26CEU},
    {0x26D4U, 0x26D4U},
    {0x26EAU, 0x26EAU},
    {0x26F2U, 0x26F3U},
    {0x1F000U, 0x1FAFFU},
}};

constexpr bool isDefaultSingleton(const CodePoint value) noexcept
{
    switch (value) {
    case 0x26F5U:
    case 0x26F9U:
    case 0x26FAU:
    case 0x26FDU:
    case 0x2705U:
    case 0x270AU:
    case 0x270BU:
    case 0x2728U:
    case 0x274CU:
    case 0x274EU:
    case 0x2753U:
    case 0x2754U:
    case 0x2755U:
    case 0x2757U:
    case 0x2795U:
    case 0x2796U:
    case 0x2797U:
    case 0x27B0U:
    case 0x27BFU:
    case 0x2B1BU:
    case 0x2B1CU:
    case 0x2B50U:
    case 0x2B55U:
    case 0x1F004U:
    case 0x1F0CFU:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Emoji=Yes codepoints that need VS16 for emoji presentation.
 *
 * Digits and the hash/asterisk symbols are included for keycap sequences; the
 * scanner only starts a cluster from these when VS16 actually follows.
 */
constexpr bool isTextDefaultEmoji(const CodePoint value) noexcept
{
    if ((value >= 0x0023U && value <= 0x0026U) || (value >= 0x0030U && value <= 0x0039U) ||
        value == 0x002AU) {
        return true;
    }
    switch (value) {
    case 0x00A9U:
    case 0x00AEU:
    case 0x203CU:
    case 0x2049U:
    case 0x2122U:
    case 0x2139U:
    case 0x2194U:
    case 0x2195U:
    case 0x2196U:
    case 0x2197U:
    case 0x2198U:
    case 0x2199U:
    case 0x21A9U:
    case 0x21AAU:
    case 0x2328U:
    case 0x23CFU:
    case 0x24C2U:
    case 0x25AAU:
    case 0x25ABU:
    case 0x25B6U:
    case 0x25C0U:
    case 0x2600U:
    case 0x2601U:
    case 0x2602U:
    case 0x2603U:
    case 0x2604U:
    case 0x260EU:
    case 0x2611U:
    case 0x2618U:
    case 0x261DU:
    case 0x2620U:
    case 0x2622U:
    case 0x2623U:
    case 0x2626U:
    case 0x262AU:
    case 0x262EU:
    case 0x262FU:
    case 0x2638U:
    case 0x2639U:
    case 0x263AU:
    case 0x2640U:
    case 0x2642U:
    case 0x265FU:
    case 0x2660U:
    case 0x2663U:
    case 0x2665U:
    case 0x2666U:
    case 0x2668U:
    case 0x267BU:
    case 0x267EU:
    case 0x2692U:
    case 0x2694U:
    case 0x2695U:
    case 0x2696U:
    case 0x2697U:
    case 0x2699U:
    case 0x269BU:
    case 0x269CU:
    case 0x26A0U:
    case 0x26A7U:
    case 0x26B0U:
    case 0x26B1U:
    case 0x26C8U:
    case 0x26CFU:
    case 0x26D1U:
    case 0x26D3U:
    case 0x26E9U:
    case 0x26F0U:
    case 0x26F1U:
    case 0x26F4U:
    case 0x26F7U:
    case 0x26F8U:
    case 0x2702U:
    case 0x2708U:
    case 0x2709U:
    case 0x270CU:
    case 0x270DU:
    case 0x270FU:
    case 0x2712U:
    case 0x2714U:
    case 0x2716U:
    case 0x271DU:
    case 0x2721U:
    case 0x2733U:
    case 0x2734U:
    case 0x2744U:
    case 0x2747U:
    case 0x2763U:
    case 0x2764U:
    case 0x27A1U:
    case 0x2934U:
    case 0x2935U:
    case 0x2B05U:
    case 0x2B06U:
    case 0x2B07U:
    case 0x3030U:
    case 0x303DU:
    case 0x3297U:
    case 0x3299U:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool inRanges(const CodePoint value, const auto& ranges) noexcept
{
    for (const auto& [lo, hi] : ranges) {
        if (value >= lo && value <= hi) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isEmojiDefaultPresentation(const CodePoint value) noexcept
{
    return inRanges(value, EmojiDefaultRanges) || isDefaultSingleton(value);
}

[[nodiscard]] bool isEmojiCapable(const CodePoint value) noexcept
{
    return isEmojiDefaultPresentation(value) || isTextDefaultEmoji(value);
}

[[nodiscard]] bool isSkinTone(const CodePoint value) noexcept
{
    return value >= SkinToneStart && value <= SkinToneEnd;
}

[[nodiscard]] bool isRegionalIndicator(const CodePoint value) noexcept
{
    return value >= RegionalIndicatorStart && value <= RegionalIndicatorEnd;
}

[[nodiscard]] bool isTag(const CodePoint value) noexcept
{
    return value >= TagStart && value <= TagEnd;
}

/**
 * @brief Decodes one UTF-8 codepoint at byteOffset, returning its size in bytes.
 */
[[nodiscard]] std::size_t decodeCodePoint(
    const std::string_view text,
    const std::size_t byteOffset,
    CodePoint& out) noexcept
{
    if (byteOffset >= text.size()) {
        return 0;
    }
    const auto first = static_cast<unsigned char>(text[byteOffset]);
    std::size_t length{1};
    CodePoint value{};
    if (first < 0x80U) {
        value = first;
    } else if ((first & 0xE0U) == 0xC0U) {
        length = 2;
        value = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
        value = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
        value = first & 0x07U;
    } else {
        out = Replacement;
        return 1;
    }
    if (byteOffset + length > text.size()) {
        out = Replacement;
        return 1;
    }
    for (std::size_t index = 1; index < length; ++index) {
        const auto continuation = static_cast<unsigned char>(text[byteOffset + index]);
        if ((continuation & 0xC0U) != 0x80U) {
            out = Replacement;
            return 1;
        }
        value = static_cast<CodePoint>((value << 6U) | (continuation & 0x3FU));
    }
    // Reject overlong encodings and surrogates.
    if ((length == 2U && value < 0x80U) || (length == 3U && value < 0x800U) ||
        (length == 4U && value < 0x10000U) || (value >= 0xD800U && value <= 0xDFFFU) ||
        value > 0x10FFFFU) {
        out = Replacement;
        return 1;
    }
    out = value;
    return length;
}

/**
 * @brief Returns the byte length of the cluster beginning at byteOffset.
 *
 * The caller has already verified that byteOffset starts an emoji-capable
 * sequence. Bounds are enforced so the scan always terminates.
 */
[[nodiscard]] std::size_t clusterLengthAt(
    const std::string_view text,
    const std::size_t byteOffset) noexcept
{
    std::size_t cursor = byteOffset;
    std::size_t codePointCount{};
    std::size_t regionalIndicatorCount{};
    bool previousWasZwj{false};

    auto decodeAt = [&](CodePoint& out) -> std::size_t {
        return decodeCodePoint(text, cursor, out);
    };

    while (cursor < text.size() && codePointCount < MaximumClusterCodePoints &&
           cursor - byteOffset < MaximumClusterBytes) {
        CodePoint cp{};
        const std::size_t advance = decodeAt(cp);
        if (advance == 0U) {
            break;
        }
        const bool first = cursor == byteOffset;
        if (first) {
            // The cluster must start with an emoji-capable codepoint. For
            // text-presentation-default bases, VS16 must follow immediately.
            if (isEmojiDefaultPresentation(cp)) {
                // fine
            } else if (isTextDefaultEmoji(cp)) {
                CodePoint next{};
                const std::size_t nextAdvance = decodeCodePoint(text, cursor + advance, next);
                if (nextAdvance == 0U || next != VariationSelector16) {
                    return 0;
                }
            } else if (isSkinTone(cp) || isTag(cp)) {
                // A skin tone or tag outside a base cannot form a standalone
                // cluster; the enclosing scanner will not start here.
                return 0;
            } else {
                return 0;
            }
            cursor += advance;
            ++codePointCount;
            if (isRegionalIndicator(cp)) {
                regionalIndicatorCount = 1U;
            }
            previousWasZwj = false;
            continue;
        }

        if (cp == ZeroWidthJoiner) {
            // ZWJ must be followed by another emoji-capable codepoint.
            CodePoint next{};
            const std::size_t nextAdvance = decodeCodePoint(text, cursor + advance, next);
            if (nextAdvance == 0U || !isEmojiCapable(next) || isTag(next)) {
                break;
            }
            cursor += advance;
            ++codePointCount;
            previousWasZwj = true;
            continue;
        }
        if (cp == VariationSelector16 || cp == VariationSelector15) {
            cursor += advance;
            ++codePointCount;
            previousWasZwj = false;
            continue;
        }
        if (cp == CombiningEnclosingKeycap) {
            cursor += advance;
            ++codePointCount;
            previousWasZwj = false;
            continue;
        }
        if (isSkinTone(cp)) {
            cursor += advance;
            ++codePointCount;
            previousWasZwj = false;
            continue;
        }
        if (isRegionalIndicator(cp)) {
            // A flag is exactly two regional indicators; a third breaks the run.
            if (regionalIndicatorCount >= 2U) {
                break;
            }
            cursor += advance;
            ++codePointCount;
            ++regionalIndicatorCount;
            previousWasZwj = false;
            continue;
        }
        if (isTag(cp)) {
            // Tag sequences continue until the cancel tag. They only follow an
            // emoji base, which the first-codepoint rule already guaranteed.
            cursor += advance;
            ++codePointCount;
            previousWasZwj = false;
            if (cp == CancelTag) {
                break;
            }
            continue;
        }
        if (isEmojiDefaultPresentation(cp)) {
            // An emoji-default codepoint extends the cluster only when it is a
            // ZWJ component. A fresh base (even after VS16 or a skin tone)
            // starts a new cluster.
            if (!previousWasZwj) {
                break;
            }
            cursor += advance;
            ++codePointCount;
            previousWasZwj = false;
            continue;
        }
        // Text codepoint ends the cluster.
        break;
    }
    return cursor - byteOffset;
}

[[nodiscard]] std::string keyForCodePoints(const std::vector<CodePoint>& codePoints) noexcept
{
    std::string key{};
    key.reserve(codePoints.size() * 6U);
    bool firstComponent = true;
    for (std::size_t index = 0; index < codePoints.size(); ++index) {
        const CodePoint cp = codePoints[index];
        if (cp == VariationSelector16) {
            // Retain VS16 only when the base is text-presentation-default.
            if (index == 0U) {
                continue;
            }
            const CodePoint base = codePoints[index - 1U];
            if (isEmojiDefaultPresentation(base) || isRegionalIndicator(base)) {
                continue;
            }
        } else if (cp == VariationSelector15) {
            continue;
        }
        char buffer[9]{};
        char* cursor = buffer;
        CodePoint value = cp;
        do {
            const unsigned int digit = static_cast<unsigned int>(value & 0xFU);
            *cursor++ = static_cast<char>(digit < 10U ? '0' + digit : 'a' + (digit - 10U));
            value >>= 4U;
        } while (value != 0U);
        if (!firstComponent) {
            key.push_back('_');
        }
        firstComponent = false;
        while (cursor != buffer) {
            key.push_back(*--cursor);
        }
    }
    return key;
}

} // namespace

std::optional<std::string> normalizedKey(const std::string_view sequence) noexcept
{
    if (sequence.empty()) {
        return std::nullopt;
    }
    std::vector<CodePoint> codePoints{};
    codePoints.reserve(8U);
    std::size_t cursor{};
    while (cursor < sequence.size() && codePoints.size() < MaximumClusterCodePoints) {
        CodePoint cp{};
        const std::size_t advance = decodeCodePoint(sequence, cursor, cp);
        if (advance == 0U || cp == Replacement) {
            return std::nullopt;
        }
        cursor += advance;
        codePoints.push_back(cp);
    }
    if (codePoints.empty()) {
        return std::nullopt;
    }
    // The sequence must actually form one valid cluster.
    const std::size_t length = clusterLengthAt(sequence, 0U);
    if (length == 0U || length != sequence.size()) {
        return std::nullopt;
    }
    const std::string key = keyForCodePoints(codePoints);
    if (key.empty()) {
        return std::nullopt;
    }
    return key;
}

std::size_t emojiByteLengthAt(const std::string_view text, const std::size_t byteOffset) noexcept
{
    if (byteOffset >= text.size()) {
        return 0;
    }
    CodePoint first{};
    const std::size_t firstAdvance = decodeCodePoint(text, byteOffset, first);
    if (firstAdvance == 0U || first == Replacement) {
        return 0;
    }
    if (!isEmojiCapable(first)) {
        return 0;
    }
    return clusterLengthAt(text, byteOffset);
}

std::vector<EmojiCluster> scanEmojiClusters(const std::string_view text) noexcept
{
    std::vector<EmojiCluster> clusters{};
    std::size_t cursor{};
    while (cursor < text.size()) {
        const std::size_t length = emojiByteLengthAt(text, cursor);
        if (length == 0U) {
            CodePoint ignored{};
            const std::size_t advance = decodeCodePoint(text, cursor, ignored);
            cursor += advance > 0U ? advance : 1U;
            continue;
        }
        const std::string_view slice = text.substr(cursor, length);
        EmojiCluster cluster{};
        cluster.sequence = std::string{slice};
        cluster.key = normalizedKey(slice).value_or(std::string{});
        cluster.byteOffset = cursor;
        cluster.byteLength = length;
        if (!cluster.key.empty()) {
            clusters.push_back(std::move(cluster));
        }
        cursor += length;
    }
    return clusters;
}

bool isEmojiSequence(const std::string_view sequence) noexcept
{
    return emojiByteLengthAt(sequence, 0U) == sequence.size() && !sequence.empty();
}

} // namespace farcaster::application::emoji::unicode