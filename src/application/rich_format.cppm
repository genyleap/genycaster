/*!
 * @file        src/application/rich_format.cppm
 * @brief       Safe rich-text and protocol mention formatting.
 *
 * @details
 * Converts rich document intent to bounded Farcaster-safe layouts while
 * preserving mention offsets and reporting compatibility limitations.
 *
 * @author      Kambiz Asadzadeh
 * @since       30 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.application.rich_format;

import farcaster.application.emoji.unicode_normalizer;
import farcaster.domain.identity;
import farcaster.domain.rich_document;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.utf8;

export namespace farcaster::application::rich {

/** @brief One protocol mention after final safe-text layout has been established. */
struct MentionPlacement final {
    domain::identity::Fid fid{};
    std::string username{};
    std::uint32_t bytePosition{};
};

enum class CompatibilityWarningKind : std::uint8_t {
    FormattingSimplified,
};

/** @brief Presentation-neutral compatibility warning emitted by safe serialization. */
struct CompatibilityWarning final {
    CompatibilityWarningKind kind{CompatibilityWarningKind::FormattingSimplified};
    std::size_t count{};
};

/** @brief Exact cross-client-safe publication projection of a rich document. */
struct FarcasterSafeLayout final {
    std::string text{};
    std::vector<MentionPlacement> mentions{};
    std::vector<CompatibilityWarning> warnings{};
    std::size_t simplifiedElementCount{};
};

/** @brief Parser contract independent from Markdown or any specific authoring syntax. */
class IRichDocumentParser {
public:
    virtual ~IRichDocumentParser() = default;
    [[nodiscard]] virtual foundation::result::Result<domain::rich::RichCastDocument>
    parse(std::string_view input) const = 0;
};

/** @brief Serializer contract independent from renderer/UI state. */
class IRichDocumentSerializer {
public:
    virtual ~IRichDocumentSerializer() = default;
    [[nodiscard]] virtual foundation::result::Result<FarcasterSafeLayout>
    serialize(const domain::rich::RichCastDocument& document) const = 0;
};

/** @brief Safe bounded Markdown importer; arbitrary HTML is never interpreted. */
class MarkdownImporter final : public IRichDocumentParser {
public:
    [[nodiscard]] foundation::result::Result<domain::rich::RichCastDocument>
    parse(std::string_view input) const override;
};

/** @brief Markdown exporter intended for clipboard/editing, never direct publication. */
class MarkdownExporter final {
public:
    [[nodiscard]] std::string serialize(const domain::rich::RichCastDocument& document) const;
};

/**
 * @brief Converts RichCastDocument to intentional plain UTF-8 Farcaster text.
 *
 * Formatting markers are not leaked. Resolved mentions are removed from text and
 * emitted as FID + final UTF-8 byte positions exactly as CastAddBody expects.
 */
class FarcasterSafeSerializer final : public IRichDocumentSerializer {
public:
    [[nodiscard]] foundation::result::Result<FarcasterSafeLayout>
    serialize(const domain::rich::RichCastDocument& document) const override;
};

/** @brief Resolves a case-insensitive language alias to Genycaster's canonical id. */
[[nodiscard]] std::string canonicalLanguageId(std::string_view language);

/** @brief Sanitizes a user-supplied link target against Genycaster's allowlist. */
[[nodiscard]] std::optional<std::string> safeLinkTarget(std::string_view target);

/** @brief Counts Unicode scalar values in validated UTF-8 without conflating byte count. */
[[nodiscard]] std::size_t utf8CodePointCount(std::string_view value) noexcept;

} // namespace farcaster::application::rich
