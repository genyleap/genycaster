module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

module farcaster.domain.rich_document;

namespace farcaster::domain::rich {
namespace {

[[nodiscard]] bool hasMeaningfulInline(const std::vector<RichInline>& inlines) noexcept
{
    return std::ranges::any_of(inlines, [](const RichInline& span) {
        return !span.text.empty() || span.kind == InlineKind::Mention;
    });
}

[[nodiscard]] bool blockMeaningful(const RichBlock& block) noexcept
{
    return std::visit([&](const auto& payload) -> bool {
        using Payload = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<Payload, TextBlock>) {
            return hasMeaningfulInline(payload.inlines);
        } else if constexpr (std::is_same_v<Payload, HeadingBlock>) {
            return hasMeaningfulInline(payload.inlines);
        } else if constexpr (std::is_same_v<Payload, ListBlock>) {
            return std::ranges::any_of(payload.items, [](const ListItem& item) {
                return hasMeaningfulInline(item.inlines);
            });
        } else if constexpr (std::is_same_v<Payload, CodeBlock>) {
            return !payload.source.empty();
        } else if constexpr (std::is_same_v<Payload, EmbedBlock>) {
            return !payload.url.empty();
        } else if constexpr (std::is_same_v<Payload, CastReferenceBlock>) {
            return !payload.castId.empty();
        } else if constexpr (std::is_same_v<Payload, MediaBlock>) {
            return !payload.url.empty();
        } else {
            return block.kind == BlockKind::HorizontalSeparator;
        }
    }, block.payload);
}

} // namespace

bool empty(const RichCastDocument& document) noexcept
{
    return std::ranges::none_of(document.blocks, blockMeaningful);
}

std::string inlinePlainText(const std::vector<RichInline>& inlines)
{
    std::string result{};
    for (const auto& span : inlines) {
        if (span.kind == InlineKind::Mention) {
            if (!span.username.empty()) {
                result.push_back('@');
                result.append(span.username);
            } else {
                result.append(span.text);
            }
            continue;
        }
        result.append(span.text);
    }
    return result;
}

bool structurallyValid(const RichCastDocument& document) noexcept
{
    if (document.schemaVersion == 0U || document.schemaVersion > CurrentSchemaVersion ||
        document.blocks.size() > MaxDocumentBlocks) {
        return false;
    }
    for (const auto& block : document.blocks) {
        if (block.id.empty()) {
            return false;
        }
        if (block.kind == BlockKind::CodeBlock) {
            const auto* code = std::get_if<CodeBlock>(&block.payload);
            if (code == nullptr || code->source.size() > MaxCodeBlockBytes) {
                return false;
            }
        }
        if (block.kind == BlockKind::Heading) {
            const auto* heading = std::get_if<HeadingBlock>(&block.payload);
            if (heading == nullptr || heading->level == 0U || heading->level > 6U) {
                return false;
            }
        }
    }
    return true;
}

std::string makeBlockId(const std::uint64_t ordinal, const std::string_view seed)
{
    // FNV-1a is sufficient for a local presentation/persistence identifier; this
    // is deliberately not a protocol hash and carries no security meaning.
    std::uint64_t hash = 1469598103934665603ULL ^ ordinal;
    for (const char raw : seed) {
        const auto ch = static_cast<unsigned char>(raw);
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream{};
    stream << 'b' << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

} // namespace farcaster::domain::rich
