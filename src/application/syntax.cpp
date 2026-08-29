module;

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

module farcaster.application.syntax;

namespace farcaster::application::syntax {
namespace {

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

[[nodiscard]] bool identifierStart(const unsigned char ch) noexcept
{
    return std::isalpha(ch) != 0 || ch == '_' || ch == '$';
}

[[nodiscard]] bool identifierPart(const unsigned char ch) noexcept
{
    return identifierStart(ch) || std::isdigit(ch) != 0;
}

[[nodiscard]] const std::unordered_set<std::string_view>& keywordsFor(const std::string_view language)
{
    static const std::unordered_set<std::string_view> cpp{
        "alignas","alignof","and","asm","auto","break","case","catch","class","concept","const","consteval","constexpr","constinit","const_cast","continue","co_await","co_return","co_yield","decltype","default","delete","do","else","enum","explicit","export","extern","false","for","friend","goto","if","import","inline","module","mutable","namespace","new","noexcept","not","nullptr","operator","private","protected","public","requires","return","sizeof","static","static_assert","static_cast","struct","switch","template","this","thread_local","throw","true","try","typedef","typeid","typename","union","using","virtual","volatile","while"};
    static const std::unordered_set<std::string_view> php{
        "abstract","and","array","as","break","callable","case","catch","class","clone","const","continue","declare","default","do","echo","else","elseif","empty","enddeclare","endfor","endforeach","endif","endswitch","endwhile","enum","eval","exit","extends","final","finally","fn","for","foreach","function","global","goto","if","implements","include","instanceof","interface","isset","match","namespace","new","or","print","private","protected","public","readonly","require","return","static","switch","throw","trait","try","unset","use","var","while","xor","yield"};
    static const std::unordered_set<std::string_view> js{
        "async","await","break","case","catch","class","const","continue","debugger","default","delete","do","else","export","extends","false","finally","for","from","function","get","if","import","in","instanceof","let","new","null","of","return","set","static","super","switch","this","throw","true","try","typeof","undefined","var","void","while","with","yield"};
    static const std::unordered_set<std::string_view> ts{
        "abstract","any","as","asserts","async","await","boolean","break","case","catch","class","const","constructor","continue","declare","default","delete","do","else","enum","export","extends","false","finally","for","from","function","get","if","implements","import","in","infer","instanceof","interface","is","keyof","let","module","namespace","never","new","null","number","object","of","override","private","protected","public","readonly","require","return","satisfies","set","static","string","super","switch","symbol","this","throw","true","try","type","typeof","undefined","unique","unknown","var","void","while","with","yield"};
    static const std::unordered_set<std::string_view> rust{
        "as","async","await","break","const","continue","crate","dyn","else","enum","extern","false","fn","for","if","impl","in","let","loop","match","mod","move","mut","pub","ref","return","self","Self","static","struct","super","trait","true","type","unsafe","use","where","while"};
    static const std::unordered_set<std::string_view> qml{
        "import","as","pragma","property","readonly","required","default","signal","function","on","id","alias","enum","component"};
    static const std::unordered_set<std::string_view> python{
        "and","as","assert","async","await","break","class","continue","def","del","elif","else","except","False","finally","for","from","global","if","import","in","is","lambda","None","nonlocal","not","or","pass","raise","return","True","try","while","with","yield"};
    static const std::unordered_set<std::string_view> java{
        "abstract","assert","boolean","break","byte","case","catch","char","class","const","continue","default","do","double","else","enum","extends","final","finally","float","for","if","implements","import","instanceof","int","interface","long","native","new","null","package","private","protected","public","return","short","static","strictfp","super","switch","synchronized","this","throw","throws","transient","true","try","void","volatile","while"};
    static const std::unordered_set<std::string_view> csharp{
        "abstract","as","async","await","base","bool","break","byte","case","catch","char","checked","class","const","continue","decimal","default","delegate","do","double","else","enum","event","explicit","extern","false","finally","fixed","float","for","foreach","get","goto","if","implicit","in","int","interface","internal","is","lock","long","namespace","new","null","object","operator","out","override","params","private","protected","public","readonly","ref","return","sbyte","sealed","set","short","sizeof","stackalloc","static","string","struct","switch","this","throw","true","try","typeof","uint","ulong","unchecked","unsafe","ushort","using","var","virtual","void","volatile","while"};
    static const std::unordered_set<std::string_view> empty{};
    if (language == "cpp" || language == "c") return cpp;
    if (language == "php") return php;
    if (language == "javascript") return js;
    if (language == "typescript") return ts;
    if (language == "rust") return rust;
    if (language == "qml") return qml;
    if (language == "python") return python;
    if (language == "java") return java;
    if (language == "csharp") return csharp;
    return empty;
}

[[nodiscard]] bool isTypeWord(std::string_view word, std::string_view language)
{
    static const std::unordered_set<std::string_view> generic{
        "bool","byte","char","char8_t","char16_t","char32_t","double","float","int","int8_t","int16_t","int32_t","int64_t","long","short","signed","size_t","string","string_view","uint8_t","uint16_t","uint32_t","uint64_t","unsigned","void","wchar_t","auto","number","boolean","unknown","never","object","Vec","Option","Result","QString","QUrl","QVariant","Item","Rectangle","Text","Drawer"};
    return generic.contains(word) || (language == "typescript" && (word == "Record" || word == "Readonly"));
}

[[nodiscard]] bool isUrlAt(std::string_view source, std::size_t i) noexcept
{
    return source.substr(i).starts_with("https://") || source.substr(i).starts_with("http://");
}

[[nodiscard]] std::size_t scanQuoted(std::string_view source, std::size_t i, char quote)
{
    ++i;
    bool escaped = false;
    while (i < source.size()) {
        const char ch = source[i++];
        if (escaped) { escaped = false; continue; }
        if (ch == '\\') { escaped = true; continue; }
        if (ch == quote) break;
    }
    return i;
}

} // namespace

const std::vector<Language>& LanguageRegistry::languages()
{
    static const std::vector<Language> values{
        {"auto", "Auto Detect", {"automatic"}},
        {"cpp", "C++", {"c++","cxx","cc"}}, {"c", "C", {}},
        {"php", "PHP", {}}, {"javascript", "JavaScript", {"js","jsx"}},
        {"typescript", "TypeScript", {"ts","tsx"}}, {"rust", "Rust", {"rs"}},
        {"qml", "QML", {}}, {"json", "JSON", {}}, {"html", "HTML", {"htm"}},
        {"css", "CSS", {}}, {"scss", "SCSS", {"sass"}}, {"bash", "Bash / Shell", {"sh","shell","zsh"}},
        {"curl", "cURL", {}}, {"sql", "SQL", {}}, {"python", "Python", {"py"}},
        {"java", "Java", {}}, {"csharp", "C#", {"c#","cs"}}, {"swift", "Swift", {}},
        {"objective-c", "Objective-C", {"objc","objectivec"}}, {"cmake", "CMake", {}},
        {"yaml", "YAML", {"yml"}}, {"xml", "XML", {}}, {"markdown", "Markdown", {"md"}},
        {"dockerfile", "Dockerfile", {"docker"}}, {"nginx", "Nginx", {"nginxconf"}},
        {"http", "HTTP", {}}, {"graphql", "GraphQL", {"gql"}},
    };
    return values;
}

std::string LanguageRegistry::canonicalId(std::string_view value)
{
    const auto lowered = lowerAscii(value);
    for (const auto& language : languages()) {
        if (lowerAscii(language.id) == lowered || lowerAscii(language.displayName) == lowered) return language.id;
        if (std::ranges::any_of(language.aliases, [&](const std::string& alias) { return lowerAscii(alias) == lowered; })) return language.id;
    }
    return lowered.empty() ? "auto" : lowered;
}

std::string LanguageRegistry::displayName(std::string_view value)
{
    const auto id = canonicalId(value);
    for (const auto& language : languages()) if (language.id == id) return language.displayName;
    return std::string{value};
}

std::string SyntaxHighlighter::detectLanguage(const std::string_view source) const
{
    const auto lower = lowerAscii(source.substr(0U, std::min<std::size_t>(source.size(), 8192U)));
    if (lower.find("curl ") != std::string::npos || lower.starts_with("curl")) return "curl";
    if (lower.find("export module ") != std::string::npos || lower.find("#include <") != std::string::npos || lower.find("std::") != std::string::npos) return "cpp";
    if (lower.find("<?php") != std::string::npos || lower.find("function __construct") != std::string::npos) return "php";
    if (lower.find("interface ") != std::string::npos && lower.find(':') != std::string::npos) return "typescript";
    if (lower.find("pub struct ") != std::string::npos || lower.find("impl ") != std::string::npos || lower.find("fn ") != std::string::npos) return "rust";
    if (lower.find("import qtquick") != std::string::npos || lower.find("anchors.") != std::string::npos || lower.find("property ") != std::string::npos) return "qml";
    if (lower.find("def ") != std::string::npos || (lower.find("from ") != std::string::npos && lower.find(" import ") != std::string::npos)) return "python";
    if (lower.find("select ") != std::string::npos && lower.find(" from ") != std::string::npos) return "sql";
    if (lower.starts_with("get /") || lower.starts_with("post /") || lower.find("http/1.1") != std::string::npos) return "http";
    if (!lower.empty() && lower.front() == '{' && lower.find(':') != std::string::npos) return "json";
    if (lower.find("<html") != std::string::npos || lower.find("</") != std::string::npos) return "html";
    return "text";
}

std::vector<Token> SyntaxHighlighter::highlight(
    const std::string_view source, const std::string_view requestedLanguage) const
{
    constexpr std::size_t MaxHighlightBytes = 128U * 1024U;
    const auto bounded = source.substr(0U, std::min(source.size(), MaxHighlightBytes));
    std::string language = LanguageRegistry::canonicalId(requestedLanguage);
    if (language == "auto") language = detectLanguage(bounded);
    const auto& keywords = keywordsFor(language);

    std::vector<Token> tokens{};
    tokens.reserve(std::min<std::size_t>(bounded.size() / 4U, 8192U));
    std::size_t i{};
    while (i < bounded.size()) {
        const unsigned char ch = static_cast<unsigned char>(bounded[i]);
        if (isUrlAt(bounded, i)) {
            std::size_t end = i;
            while (end < bounded.size() && std::isspace(static_cast<unsigned char>(bounded[end])) == 0 && bounded[end] != ')' && bounded[end] != ']') ++end;
            tokens.push_back({TokenKind::Url, i, end - i}); i = end; continue;
        }
        if ((language == "cpp" || language == "c") && bounded[i] == '#' && (i == 0U || bounded[i - 1U] == '\n')) {
            std::size_t end = bounded.find('\n', i);
            if (end == std::string_view::npos) end = bounded.size();
            tokens.push_back({TokenKind::Preprocessor, i, end - i}); i = end; continue;
        }
        if (i + 1U < bounded.size() && bounded[i] == '/' && bounded[i + 1U] == '/') {
            std::size_t end = bounded.find('\n', i);
            if (end == std::string_view::npos) end = bounded.size();
            tokens.push_back({TokenKind::Comment, i, end - i}); i = end; continue;
        }
        if ((language == "python" || language == "bash" || language == "curl" || language == "yaml") && bounded[i] == '#') {
            std::size_t end = bounded.find('\n', i);
            if (end == std::string_view::npos) end = bounded.size();
            tokens.push_back({TokenKind::Comment, i, end - i}); i = end; continue;
        }
        if (i + 1U < bounded.size() && bounded[i] == '/' && bounded[i + 1U] == '*') {
            const auto close = bounded.find("*/", i + 2U);
            const std::size_t end = close == std::string_view::npos ? bounded.size() : close + 2U;
            tokens.push_back({TokenKind::Comment, i, end - i}); i = end; continue;
        }
        if (bounded[i] == '"' || bounded[i] == '\'') {
            const auto end = scanQuoted(bounded, i, bounded[i]);
            tokens.push_back({TokenKind::String, i, end - i}); i = end; continue;
        }
        if (std::isdigit(ch) != 0) {
            std::size_t end = i + 1U;
            while (end < bounded.size() && (std::isalnum(static_cast<unsigned char>(bounded[end])) != 0 || bounded[end] == '.' || bounded[end] == '_' || bounded[end] == 'x')) ++end;
            tokens.push_back({TokenKind::Number, i, end - i}); i = end; continue;
        }
        if (identifierStart(ch)) {
            std::size_t end = i + 1U;
            while (end < bounded.size() && identifierPart(static_cast<unsigned char>(bounded[end]))) ++end;
            const auto word = bounded.substr(i, end - i);
            TokenKind kind = TokenKind::Plain;
            if (keywords.contains(word)) kind = TokenKind::Keyword;
            else if (isTypeWord(word, language)) kind = TokenKind::Type;
            else if (end < bounded.size() && bounded[end] == '(') kind = TokenKind::Function;
            else if (language == "qml" && end < bounded.size() && bounded[end] == ':') kind = TokenKind::Property;
            else if (language == "curl" && word == "curl") kind = TokenKind::Command;
            if (kind != TokenKind::Plain) tokens.push_back({kind, i, end - i});
            i = end; continue;
        }
        if (language == "html" || language == "xml") {
            if (bounded[i] == '<') {
                std::size_t end = bounded.find('>', i);
                if (end == std::string_view::npos) end = bounded.size() - 1U;
                tokens.push_back({TokenKind::Tag, i, end - i + 1U}); i = end + 1U; continue;
            }
        }
        ++i;
    }
    return tokens;
}

std::string_view tokenKindName(const TokenKind kind) noexcept
{
    switch (kind) {
    case TokenKind::Keyword: return "keyword";
    case TokenKind::Type: return "type";
    case TokenKind::Function: return "function";
    case TokenKind::Variable: return "variable";
    case TokenKind::String: return "string";
    case TokenKind::Number: return "number";
    case TokenKind::Comment: return "comment";
    case TokenKind::Operator: return "operator";
    case TokenKind::Punctuation: return "punctuation";
    case TokenKind::Attribute: return "attribute";
    case TokenKind::Preprocessor: return "preprocessor";
    case TokenKind::Namespace: return "namespace";
    case TokenKind::Property: return "property";
    case TokenKind::Literal: return "literal";
    case TokenKind::Tag: return "tag";
    case TokenKind::Command: return "command";
    case TokenKind::Url: return "url";
    case TokenKind::Header: return "header";
    case TokenKind::Parameter: return "parameter";
    case TokenKind::Plain: return "plain";
    }
    return "plain";
}

} // namespace farcaster::application::syntax
