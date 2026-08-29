module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

export module farcaster.domain.rich_document;

import farcaster.domain.identity;

export namespace farcaster::domain::rich {

inline constexpr std::uint32_t CurrentSchemaVersion = 1U;
inline constexpr std::size_t MaxDocumentBlocks = 256U;
inline constexpr std::size_t MaxCodeBlockBytes = 64U * 1024U;

enum class Direction : std::uint8_t {
    Auto,
    LeftToRight,
    RightToLeft,
};

enum class InlineKind : std::uint8_t {
    Text,
    Strong,
    Emphasis,
    InlineCode,
    Link,
    Mention,
    Emoji,
};

enum class BlockKind : std::uint8_t {
    Paragraph,
    Heading,
    BlockQuote,
    BulletList,
    OrderedList,
    CodeBlock,
    HorizontalSeparator,
    Embed,
    CastReference,
    Media,
};

/** @brief Semantic inline content independent from QML or any presentation markup. */
struct RichInline final {
    InlineKind kind{InlineKind::Text};
    std::string text{};
    std::string target{};
    std::optional<domain::identity::Fid> fid{};
    std::string username{};
    std::string displayName{};
};

/** @brief Plain paragraph-like block with semantic inline spans. */
struct TextBlock final {
    std::vector<RichInline> inlines{};
    Direction direction{Direction::Auto};
};

/** @brief Heading with level 1..6 and semantic inline spans. */
struct HeadingBlock final {
    std::uint8_t level{1U};
    std::vector<RichInline> inlines{};
    Direction direction{Direction::Auto};
};

/** @brief A list item is intentionally flat in schema v1 to bound parser complexity. */
struct ListItem final {
    std::vector<RichInline> inlines{};
};

/** @brief Semantic ordered or unordered list. */
struct ListBlock final {
    std::vector<ListItem> items{};
    Direction direction{Direction::Auto};
};

/** @brief First-class technical source block. */
struct CodeBlock final {
    std::string language{"auto"};
    std::string source{};
    std::string detectedLanguage{};
    std::string displayLanguage{};
    bool lineNumbers{false};
    bool wrap{false};
};

/** @brief Generic URL enhancement that remains optional for understanding the cast. */
struct EmbedBlock final {
    std::string url{};
    std::string title{};
};

/** @brief Semantic embedded Farcaster cast reference. */
struct CastReferenceBlock final {
    std::string castId{};
    std::optional<domain::identity::Fid> author{};
};

/** @brief Media enhancement; the protocol-safe text remains independent from it. */
struct MediaBlock final {
    std::string url{};
    std::string mediaType{};
    std::string altText{};
};

using BlockPayload = std::variant<
    TextBlock,
    HeadingBlock,
    ListBlock,
    CodeBlock,
    EmbedBlock,
    CastReferenceBlock,
    MediaBlock,
    std::monostate>;

/** @brief One immutable-friendly AST node. */
struct RichBlock final {
    std::string id{};
    BlockKind kind{BlockKind::Paragraph};
    BlockPayload payload{TextBlock{}};
};

/** @brief Versioned rich-social document that is canonical inside Genycaster. */
struct RichCastDocument final {
    std::uint32_t schemaVersion{CurrentSchemaVersion};
    std::vector<RichBlock> blocks{};
};

/** @brief Returns true when the document has no meaningful user content. */
[[nodiscard]] bool empty(const RichCastDocument& document) noexcept;

/** @brief Returns the concatenated human-readable inline text without presentation markers. */
[[nodiscard]] std::string inlinePlainText(const std::vector<RichInline>& inlines);

/** @brief Validates bounded schema invariants before persistence/rendering/serialization. */
[[nodiscard]] bool structurallyValid(const RichCastDocument& document) noexcept;

/** @brief Generates a stable-enough local block identifier without global mutable state. */
[[nodiscard]] std::string makeBlockId(std::uint64_t ordinal, std::string_view seed);

} // namespace farcaster::domain::rich
