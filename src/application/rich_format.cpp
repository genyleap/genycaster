module;

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <expected>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

module farcaster.application.rich_format;

import farcaster.application.emoji.unicode_normalizer;
import farcaster.domain.identity;
import farcaster.domain.rich_document;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::application::rich {
namespace {

using domain::rich::BlockKind;
using domain::rich::CodeBlock;
using domain::rich::Direction;
using domain::rich::HeadingBlock;
using domain::rich::InlineKind;
using domain::rich::ListBlock;
using domain::rich::ListItem;
using domain::rich::RichBlock;
using domain::rich::RichCastDocument;
using domain::rich::RichInline;
using domain::rich::TextBlock;

constexpr std::size_t MaxInputBytes = 256U * 1024U;
constexpr std::size_t MaxLineBytes = 64U * 1024U;
constexpr std::size_t MaxInlineSpansPerBlock = 512U;

[[nodiscard]] foundation::error::Error invalid(std::string message)
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::InvalidArgument,
        std::move(message));
}

[[nodiscard]] std::string trim(std::string_view input)
{
    auto begin = input.begin();
    auto end = input.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0) { ++begin; }
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) { --end; }
    return std::string{begin, end};
}

[[nodiscard]] std::string lowerAscii(std::string_view input)
{
    std::string result{};
    result.reserve(input.size());
    for (const char raw : input) {
        const auto ch = static_cast<unsigned char>(raw);
        result.push_back(static_cast<char>(std::tolower(ch)));
    }
    return result;
}

[[nodiscard]] bool startsWith(std::string_view value, std::string_view prefix) noexcept
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool isHorizontalSeparator(std::string_view line)
{
    const auto canonical = trim(line);
    if (canonical.size() < 3U) { return false; }
    const char marker = canonical.front();
    if (marker != '-' && marker != '*' && marker != '_') { return false; }
    std::size_t count{};
    for (const char ch : canonical) {
        if (ch == marker) { ++count; continue; }
        if (std::isspace(static_cast<unsigned char>(ch)) == 0) { return false; }
    }
    return count >= 3U;
}

[[nodiscard]] bool looksLikeMentionUsernameChar(const unsigned char ch) noexcept
{
    return std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.';
}

[[nodiscard]] std::vector<RichInline> parseInlines(std::string_view text)
{
    std::vector<RichInline> spans{};
    spans.reserve(8U);

    auto pushText = [&spans](const InlineKind kind, std::string value, std::string target = {}) {
        if (value.empty()) { return; }
        if (spans.size() >= MaxInlineSpansPerBlock) { return; }
        spans.push_back(RichInline{
            .kind = kind,
            .text = std::move(value),
            .target = std::move(target),
        });
    };

    std::size_t index{};
    std::string pending{};
    pending.reserve(text.size());
    auto flushPending = [&] {
        pushText(InlineKind::Text, std::move(pending));
        pending.clear();
    };

    while (index < text.size() && spans.size() < MaxInlineSpansPerBlock) {
        if (startsWith(text.substr(index), "**")) {
            const auto close = text.find("**", index + 2U);
            if (close != std::string_view::npos && close > index + 2U) {
                flushPending();
                pushText(InlineKind::Strong, std::string{text.substr(index + 2U, close - index - 2U)});
                index = close + 2U;
                continue;
            }
        }
        if (text[index] == '`') {
            std::size_t width{1U};
            while (index + width < text.size() && text[index + width] == '`') ++width;
            const auto marker = std::string(width, '`');
            auto close = text.find(marker, index + width);
            while (close != std::string_view::npos &&
                   ((close > 0U && text[close - 1U] == '`') ||
                    (close + width < text.size() && text[close + width] == '`'))) {
                close = text.find(marker, close + width);
            }
            if (close != std::string_view::npos && close > index + width) {
                flushPending();
                pushText(InlineKind::InlineCode, std::string{text.substr(index + width, close - index - width)});
                index = close + width;
                continue;
            }
        }
        if ((text[index] == '_' || text[index] == '*') && index + 1U < text.size()) {
            const char marker = text[index];
            const auto close = text.find(marker, index + 1U);
            if (close != std::string_view::npos && close > index + 1U) {
                flushPending();
                pushText(InlineKind::Emphasis, std::string{text.substr(index + 1U, close - index - 1U)});
                index = close + 1U;
                continue;
            }
        }
        if (text[index] == '[') {
            const auto closeLabel = text.find(']', index + 1U);
            if (closeLabel != std::string_view::npos && closeLabel + 1U < text.size() && text[closeLabel + 1U] == '(') {
                const auto closeTarget = text.find(')', closeLabel + 2U);
                if (closeTarget != std::string_view::npos) {
                    const auto target = safeLinkTarget(text.substr(closeLabel + 2U, closeTarget - closeLabel - 2U));
                    if (target) {
                        flushPending();
                        pushText(
                            InlineKind::Link,
                            std::string{text.substr(index + 1U, closeLabel - index - 1U)},
                            *target);
                        index = closeTarget + 1U;
                        continue;
                    }
                }
            }
        }
        if (const std::size_t emojiLength =
                emoji::unicode::emojiByteLengthAt(text, index);
            emojiLength > 0U) {
            // Emoji clusters are semantic inline nodes with the Unicode text as
            // identity. Animation is a presentation concern layered on top by
            // the renderer; serialization always emits the plain Unicode text.
            flushPending();
            pushText(InlineKind::Emoji, std::string{text.substr(index, emojiLength)});
            index += emojiLength;
            continue;
        }
        if (text[index] == '@' && (index == 0U || std::isspace(static_cast<unsigned char>(text[index - 1U])) != 0 ||
                                    std::ispunct(static_cast<unsigned char>(text[index - 1U])) != 0)) {
            std::size_t end = index + 1U;
            while (end < text.size() && looksLikeMentionUsernameChar(static_cast<unsigned char>(text[end]))) { ++end; }
            if (end > index + 1U) {
                flushPending();
                const std::string username{text.substr(index + 1U, end - index - 1U)};
                spans.push_back(RichInline{
                    .kind = InlineKind::Mention,
                    .text = "@" + username,
                    .username = username,
                });
                index = end;
                continue;
            }
        }
        pending.push_back(text[index]);
        ++index;
    }
    if (index < text.size()) {
        pending.append(text.substr(index));
    }
    flushPending();
    return spans;
}

[[nodiscard]] RichBlock textBlock(
    const BlockKind kind,
    const std::uint64_t ordinal,
    std::string_view text,
    const Direction direction = Direction::Auto)
{
    const auto id = domain::rich::makeBlockId(ordinal, text);
    if (kind == BlockKind::Heading) {
        return RichBlock{
            .id = id,
            .kind = kind,
            .payload = HeadingBlock{.level = 1U, .inlines = parseInlines(text), .direction = direction},
        };
    }
    return RichBlock{
        .id = id,
        .kind = kind,
        .payload = TextBlock{.inlines = parseInlines(text), .direction = direction},
    };
}

[[nodiscard]] std::vector<std::string_view> linesOf(std::string_view input)
{
    std::vector<std::string_view> lines{};
    std::size_t start{};
    while (start <= input.size()) {
        const auto end = input.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(input.substr(start));
            break;
        }
        lines.push_back(input.substr(start, end - start));
        start = end + 1U;
        if (start == input.size()) {
            lines.push_back({});
            break;
        }
    }
    return lines;
}

[[nodiscard]] std::optional<std::pair<std::uint32_t, std::string_view>> orderedListPrefix(std::string_view line)
{
    std::size_t digitEnd{};
    while (digitEnd < line.size() && std::isdigit(static_cast<unsigned char>(line[digitEnd])) != 0) { ++digitEnd; }
    if (digitEnd == 0U || digitEnd + 1U >= line.size() || line[digitEnd] != '.' || line[digitEnd + 1U] != ' ') {
        return std::nullopt;
    }
    std::uint32_t number{};
    const auto parse = std::from_chars(line.data(), line.data() + digitEnd, number);
    if (parse.ec != std::errc{}) { return std::nullopt; }
    return std::pair{number, line.substr(digitEnd + 2U)};
}

void appendBlockSeparator(std::string& output)
{
    while (!output.empty() && output.back() == '\n') {
        if (output.size() >= 2U && output[output.size() - 2U] == '\n') { return; }
        break;
    }
    if (!output.empty() && output.back() != '\n') { output.push_back('\n'); }
    output.push_back('\n');
}

[[nodiscard]] std::string portableCodeFence(const std::string_view source,
                                             const std::size_t minimum = 3U)
{
    std::size_t maxRun{};
    std::size_t run{};
    for (const char ch : source) {
        run = ch == '`' ? run + 1U : 0U;
        maxRun = std::max(maxRun, run);
    }
    return std::string(std::max(minimum, maxRun + 1U), '`');
}

void appendInlineSafe(
    std::string& output,
    const std::vector<RichInline>& inlines,
    std::vector<MentionPlacement>& mentions,
    std::size_t& simplified)
{
    for (const auto& span : inlines) {
        switch (span.kind) {
        case InlineKind::Mention:
            if (span.fid && span.fid->value > 0U && mentions.size() < 10U) {
                mentions.push_back(MentionPlacement{
                    .fid = *span.fid,
                    .username = span.username,
                    .bytePosition = static_cast<std::uint32_t>(output.size()),
                });
            } else if (!span.username.empty()) {
                output.push_back('@');
                output.append(span.username);
            } else {
                output.append(span.text);
            }
            break;
        case InlineKind::Link: {
            output.append(span.text);
            const auto target = safeLinkTarget(span.target);
            if (target && !target->empty() && *target != span.text) {
                output.append(" (");
                output.append(*target);
                output.push_back(')');
            }
            ++simplified;
            break;
        }
        case InlineKind::InlineCode: {
            const auto marker = portableCodeFence(span.text, 1U);
            output.append(marker).append(span.text).append(marker);
            break;
        }
        case InlineKind::Strong:
        case InlineKind::Emphasis:
            output.append(span.text);
            ++simplified;
            break;
        case InlineKind::Text:
        case InlineKind::Emoji:
            output.append(span.text);
            break;
        }
    }
}

[[nodiscard]] std::string markdownInline(const std::vector<RichInline>& inlines)
{
    std::string output{};
    for (const auto& span : inlines) {
        switch (span.kind) {
        case InlineKind::Strong: output.append("**").append(span.text).append("**"); break;
        case InlineKind::Emphasis: output.push_back('_'); output.append(span.text); output.push_back('_'); break;
        case InlineKind::InlineCode: {
            const auto marker = portableCodeFence(span.text, 1U);
            output.append(marker).append(span.text).append(marker);
            break;
        }
        case InlineKind::Link: output.push_back('['); output.append(span.text).append("](").append(span.target).push_back(')'); break;
        case InlineKind::Mention:
            if (!span.username.empty()) { output.push_back('@'); output.append(span.username); }
            else { output.append(span.text); }
            break;
        case InlineKind::Text:
        case InlineKind::Emoji: output.append(span.text); break;
        }
    }
    return output;
}

} // namespace

std::optional<std::string> safeLinkTarget(std::string_view target)
{
    if (!foundation::utf8::isValid(target)) return std::nullopt;
    const auto canonical = trim(target);
    const auto lowered = lowerAscii(canonical);
    if (startsWith(lowered, "https://") || startsWith(lowered, "http://") ||
        startsWith(lowered, "farcaster://")) {
        if (canonical.size() <= 2048U && canonical.find_first_of("\r\n\t") == std::string::npos) {
            return canonical;
        }
    }
    return std::nullopt;
}

std::string canonicalLanguageId(std::string_view language)
{
    const auto value = lowerAscii(trim(language));
    if (value.empty() || value == "auto" || value == "automatic") return "auto";
    if (value == "c++" || value == "cpp" || value == "cxx" || value == "cc") return "cpp";
    if (value == "c") return "c";
    if (value == "php") return "php";
    if (value == "javascript" || value == "js" || value == "jsx") return "javascript";
    if (value == "typescript" || value == "ts" || value == "tsx") return "typescript";
    if (value == "rust" || value == "rs") return "rust";
    if (value == "qml") return "qml";
    if (value == "json") return "json";
    if (value == "html" || value == "htm") return "html";
    if (value == "css") return "css";
    if (value == "scss" || value == "sass") return "scss";
    if (value == "bash" || value == "sh" || value == "shell" || value == "zsh") return "bash";
    if (value == "curl") return "curl";
    if (value == "sql") return "sql";
    if (value == "python" || value == "py") return "python";
    if (value == "java") return "java";
    if (value == "c#" || value == "csharp" || value == "cs") return "csharp";
    if (value == "swift") return "swift";
    if (value == "objective-c" || value == "objc" || value == "objectivec") return "objective-c";
    if (value == "cmake") return "cmake";
    if (value == "yaml" || value == "yml") return "yaml";
    if (value == "xml") return "xml";
    if (value == "markdown" || value == "md") return "markdown";
    if (value == "docker" || value == "dockerfile") return "dockerfile";
    if (value == "nginx" || value == "nginxconf") return "nginx";
    if (value == "http") return "http";
    if (value == "graphql" || value == "gql") return "graphql";
    return value;
}

foundation::result::Result<RichCastDocument> MarkdownImporter::parse(const std::string_view input) const
{
    if (!foundation::utf8::isValid(input)) {
        return std::unexpected(invalid("Rich composer input must be valid UTF-8."));
    }
    if (input.size() > MaxInputBytes) {
        return std::unexpected(invalid("Rich composer input exceeds the bounded 256 KiB parser limit."));
    }

    RichCastDocument document{};
    const auto lines = linesOf(input);
    std::uint64_t ordinal{};
    std::size_t index{};
    while (index < lines.size()) {
        if (document.blocks.size() >= domain::rich::MaxDocumentBlocks) {
            return std::unexpected(invalid("Rich composer input exceeds the maximum block count."));
        }
        const auto raw = lines[index];
        if (raw.size() > MaxLineBytes) {
            return std::unexpected(invalid("Rich composer input contains an oversized line."));
        }
        const auto canonical = trim(raw);
        if (canonical.empty()) { ++index; continue; }

        if (startsWith(canonical, "```")) {
            std::size_t fenceWidth{};
            while (fenceWidth < canonical.size() && canonical[fenceWidth] == '`') ++fenceWidth;
            const auto language = canonicalLanguageId(canonical.substr(fenceWidth));
            std::string source{};
            ++index;
            while (index < lines.size()) {
                const auto closing = trim(lines[index]);
                const bool closeFence = closing.size() >= fenceWidth
                    && closing.find_first_not_of('`') == std::string::npos;
                if (closeFence) break;
                if (!source.empty()) source.push_back('\n');
                source.append(lines[index]);
                if (source.size() > domain::rich::MaxCodeBlockBytes) {
                    return std::unexpected(invalid("Code block exceeds the bounded 64 KiB source limit."));
                }
                ++index;
            }
            if (index < lines.size()) ++index;
            document.blocks.push_back(RichBlock{
                .id = domain::rich::makeBlockId(++ordinal, source),
                .kind = BlockKind::CodeBlock,
                .payload = CodeBlock{
                    .language = language.empty() ? "auto" : language,
                    .source = std::move(source),
                    .displayLanguage = language,
                },
            });
            continue;
        }

        if (canonical.front() == '#' ) {
            std::size_t level{};
            while (level < canonical.size() && canonical[level] == '#' && level < 6U) ++level;
            if (level > 0U && level < canonical.size() && canonical[level] == ' ') {
                const auto text = std::string_view{canonical}.substr(level + 1U);
                auto block = textBlock(BlockKind::Heading, ++ordinal, text);
                auto& heading = std::get<HeadingBlock>(block.payload);
                heading.level = static_cast<std::uint8_t>(level);
                document.blocks.push_back(std::move(block));
                ++index;
                continue;
            }
        }

        if (canonical.front() == '>') {
            std::string_view body = std::string_view{canonical}.substr(1U);
            if (!body.empty() && body.front() == ' ') body.remove_prefix(1U);
            document.blocks.push_back(textBlock(BlockKind::BlockQuote, ++ordinal, body));
            ++index;
            continue;
        }

        if ((startsWith(canonical, "- ") || startsWith(canonical, "* ") || startsWith(canonical, "+ "))) {
            ListBlock list{};
            while (index < lines.size()) {
                const auto itemLine = trim(lines[index]);
                if (!(startsWith(itemLine, "- ") || startsWith(itemLine, "* ") || startsWith(itemLine, "+ "))) break;
                list.items.push_back(ListItem{.inlines = parseInlines(std::string_view{itemLine}.substr(2U))});
                ++index;
            }
            document.blocks.push_back(RichBlock{
                .id = domain::rich::makeBlockId(++ordinal, canonical),
                .kind = BlockKind::BulletList,
                .payload = std::move(list),
            });
            continue;
        }

        if (orderedListPrefix(canonical)) {
            ListBlock list{};
            while (index < lines.size()) {
                const auto itemLine = trim(lines[index]);
                const auto prefix = orderedListPrefix(itemLine);
                if (!prefix) break;
                list.items.push_back(ListItem{.inlines = parseInlines(prefix->second)});
                ++index;
            }
            document.blocks.push_back(RichBlock{
                .id = domain::rich::makeBlockId(++ordinal, canonical),
                .kind = BlockKind::OrderedList,
                .payload = std::move(list),
            });
            continue;
        }

        if (isHorizontalSeparator(canonical)) {
            document.blocks.push_back(RichBlock{
                .id = domain::rich::makeBlockId(++ordinal, canonical),
                .kind = BlockKind::HorizontalSeparator,
                .payload = std::monostate{},
            });
            ++index;
            continue;
        }

        std::string paragraph{};
        while (index < lines.size()) {
            const auto paragraphLine = trim(lines[index]);
            if (paragraphLine.empty()) { break; }
            if (!paragraph.empty()) paragraph.push_back('\n');
            paragraph.append(lines[index]);
            ++index;
        }
        document.blocks.push_back(textBlock(BlockKind::Paragraph, ++ordinal, paragraph));
    }

    if (!domain::rich::structurallyValid(document)) {
        return std::unexpected(invalid("Markdown import produced an invalid rich document."));
    }
    return document;
}

std::string MarkdownExporter::serialize(const RichCastDocument& document) const
{
    std::string output{};
    for (const auto& block : document.blocks) {
        if (!output.empty()) output.append("\n\n");
        switch (block.kind) {
        case BlockKind::Paragraph:
            if (const auto* text = std::get_if<TextBlock>(&block.payload)) output.append(markdownInline(text->inlines));
            break;
        case BlockKind::Heading:
            if (const auto* heading = std::get_if<HeadingBlock>(&block.payload)) {
                output.append(std::clamp<std::size_t>(heading->level, 1U, 6U), '#');
                output.push_back(' ');
                output.append(markdownInline(heading->inlines));
            }
            break;
        case BlockKind::BlockQuote:
            if (const auto* quote = std::get_if<TextBlock>(&block.payload)) output.append("> ").append(markdownInline(quote->inlines));
            break;
        case BlockKind::BulletList:
        case BlockKind::OrderedList:
            if (const auto* list = std::get_if<ListBlock>(&block.payload)) {
                for (std::size_t i{}; i < list->items.size(); ++i) {
                    if (i > 0U) output.push_back('\n');
                    if (block.kind == BlockKind::OrderedList) output.append(std::to_string(i + 1U)).append(". ");
                    else output.append("- ");
                    output.append(markdownInline(list->items[i].inlines));
                }
            }
            break;
        case BlockKind::CodeBlock:
            if (const auto* code = std::get_if<CodeBlock>(&block.payload)) {
                const auto fence = portableCodeFence(code->source);
                output.append(fence).append(code->language == "auto" ? "" : code->language).push_back('\n');
                output.append(code->source).push_back('\n');
                output.append(fence);
            }
            break;
        case BlockKind::HorizontalSeparator: output.append("---"); break;
        case BlockKind::Embed:
            if (const auto* embed = std::get_if<domain::rich::EmbedBlock>(&block.payload)) output.append(embed->url);
            break;
        case BlockKind::CastReference:
            if (const auto* cast = std::get_if<domain::rich::CastReferenceBlock>(&block.payload)) output.append("cast:").append(cast->castId);
            break;
        case BlockKind::Media:
            if (const auto* media = std::get_if<domain::rich::MediaBlock>(&block.payload)) output.append(media->url);
            break;
        }
    }
    return output;
}

foundation::result::Result<FarcasterSafeLayout> FarcasterSafeSerializer::serialize(
    const RichCastDocument& document) const
{
    if (!domain::rich::structurallyValid(document)) {
        return std::unexpected(invalid("Cannot serialize an invalid RichCastDocument."));
    }

    FarcasterSafeLayout result{};
    auto& output = result.text;
    for (const auto& block : document.blocks) {
        const std::size_t beforeBlock = output.size();
        if (!output.empty()) appendBlockSeparator(output);
        const std::size_t afterSeparator = output.size();
        switch (block.kind) {
        case BlockKind::Paragraph:
            if (const auto* text = std::get_if<TextBlock>(&block.payload)) {
                appendInlineSafe(output, text->inlines, result.mentions, result.simplifiedElementCount);
            }
            break;
        case BlockKind::Heading:
            if (const auto* heading = std::get_if<HeadingBlock>(&block.payload)) {
                appendInlineSafe(output, heading->inlines, result.mentions, result.simplifiedElementCount);
                ++result.simplifiedElementCount;
            }
            break;
        case BlockKind::BlockQuote:
            if (const auto* quote = std::get_if<TextBlock>(&block.payload)) {
                output.append("“");
                appendInlineSafe(output, quote->inlines, result.mentions, result.simplifiedElementCount);
                output.append("”");
                ++result.simplifiedElementCount;
            }
            break;
        case BlockKind::BulletList:
        case BlockKind::OrderedList:
            if (const auto* list = std::get_if<ListBlock>(&block.payload)) {
                for (std::size_t i{}; i < list->items.size(); ++i) {
                    if (i > 0U) output.push_back('\n');
                    if (block.kind == BlockKind::BulletList) output.append("• ");
                    else output.append(std::to_string(i + 1U)).append(". ");
                    appendInlineSafe(output, list->items[i].inlines, result.mentions, result.simplifiedElementCount);
                }
            }
            break;
        case BlockKind::CodeBlock:
            if (const auto* code = std::get_if<CodeBlock>(&block.payload)) {
                // Portable, parseable Markdown rather than irreversible
                // "javascript:\n\nsource" flattening. Other clients see
                // literal Markdown; GenyCaster reconstructs native syntax UI.
                const auto fence = portableCodeFence(code->source);
                const auto language = canonicalLanguageId(code->language);
                output.append(fence);
                if (!language.empty() && language != "auto") output.append(language);
                output.push_back('\n');
                output.append(code->source).push_back('\n');
                output.append(fence);
            }
            break;
        case BlockKind::HorizontalSeparator:
            output.append("────────");
            ++result.simplifiedElementCount;
            break;
        case BlockKind::Embed:
            // URL embeds are protocol fields, not cast text. Duplicating the URL
            // here makes clients render both a raw link and the rich embed.
            break;
        case BlockKind::CastReference:
            // A cast reference is represented through a protocol embed when available;
            // leaking hashes into fallback text would make the cross-client result ugly.
            break;
        case BlockKind::Media:
            // Media URLs are emitted through protocolEmbeds(). Keep cast text
            // exactly user-authored instead of leaking CDN URLs into the post.
            break;
        }
        if (output.size() == afterSeparator && afterSeparator != beforeBlock) {
            output.resize(beforeBlock);
        }
    }

    while (!output.empty() && (output.back() == '\n' || output.back() == ' ' || output.back() == '\t')) output.pop_back();
    if (result.simplifiedElementCount > 0U) {
        result.warnings.push_back(CompatibilityWarning{
            .kind = CompatibilityWarningKind::FormattingSimplified,
            .count = result.simplifiedElementCount,
        });
    }
    return result;
}

std::size_t utf8CodePointCount(const std::string_view value) noexcept
{
    return foundation::utf8::scalarCount(value);
}

} // namespace farcaster::application::rich
