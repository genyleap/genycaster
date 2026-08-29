module;

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

export module farcaster.application.syntax;

export namespace farcaster::application::syntax {

enum class TokenKind : std::uint8_t {
    Plain,
    Keyword,
    Type,
    Function,
    Variable,
    String,
    Number,
    Comment,
    Operator,
    Punctuation,
    Attribute,
    Preprocessor,
    Namespace,
    Property,
    Literal,
    Tag,
    Command,
    Url,
    Header,
    Parameter,
};

struct Token final {
    TokenKind kind{TokenKind::Plain};
    std::size_t start{};
    std::size_t length{};
};

struct Language final {
    std::string id{};
    std::string displayName{};
    std::vector<std::string> aliases{};
};

/** @brief Immutable registry of syntax languages supported by the composer/viewer. */
class LanguageRegistry final {
public:
    [[nodiscard]] static const std::vector<Language>& languages();
    [[nodiscard]] static std::string canonicalId(std::string_view value);
    [[nodiscard]] static std::string displayName(std::string_view value);
};

class ISyntaxHighlighter {
public:
    virtual ~ISyntaxHighlighter() = default;
    [[nodiscard]] virtual std::vector<Token> highlight(
        std::string_view source, std::string_view language) const = 0;
    [[nodiscard]] virtual std::string detectLanguage(std::string_view source) const = 0;
};

/** @brief Bounded native tokenizer designed for incremental block-level reuse. */
class SyntaxHighlighter final : public ISyntaxHighlighter {
public:
    [[nodiscard]] std::vector<Token> highlight(
        std::string_view source, std::string_view language) const override;
    [[nodiscard]] std::string detectLanguage(std::string_view source) const override;
};

[[nodiscard]] std::string_view tokenKindName(TokenKind kind) noexcept;

} // namespace farcaster::application::syntax
