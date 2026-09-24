#include <cassert>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

import farcaster.application.rich_format;
import farcaster.application.syntax;
import farcaster.domain.identity;
import farcaster.domain.rich_document;

namespace {

namespace app = farcaster::application;
namespace domain = farcaster::domain;

void fidStrongTypeSupportsOptionalEquality()
{
    const std::optional<domain::identity::Fid> first{domain::identity::Fid{42U}};
    const std::optional<domain::identity::Fid> same{domain::identity::Fid{42U}};
    const std::optional<domain::identity::Fid> different{domain::identity::Fid{43U}};
    const std::optional<domain::identity::Fid> empty{};

    assert(first == same);
    assert(first != different);
    assert(first != empty);
}

void richMarkdownDegradesIntentionally()
{
    constexpr std::string_view markdown = R"md(## Important Update

This is **very important**.

> Native applications still matter.

- Fast
- Native
- Open

```cpp
import <vector>;

auto main() -> int
{
    return 0;
}
```
)md";

    const app::rich::MarkdownImporter importer{};
    const auto document = importer.parse(markdown);
    assert(document);

    const app::rich::FarcasterSafeSerializer serializer{};
    const auto safe = serializer.serialize(*document);
    assert(safe);
    assert(safe->text.find("##") == std::string::npos);
    assert(safe->text.find("**") == std::string::npos);
    assert(safe->text.find("```") == std::string::npos);
    assert(safe->text.find("Important Update") != std::string::npos);
    assert(safe->text.find("very important") != std::string::npos);
    assert(safe->text.find("“Native applications still matter.”") != std::string::npos);
    assert(safe->text.find("• Fast\n• Native\n• Open") != std::string::npos);
    assert(safe->text.find("C++:\n\nimport <vector>;") != std::string::npos);
    assert(safe->simplifiedElementCount > 0U);
}

void semanticMentionUsesFinalUtf8BytePosition()
{
    domain::rich::RichCastDocument document{};
    document.blocks.push_back(domain::rich::RichBlock{
        .id = "mention-test",
        .kind = domain::rich::BlockKind::Paragraph,
        .payload = domain::rich::TextBlock{
            .inlines = {
                domain::rich::RichInline{.kind = domain::rich::InlineKind::Text, .text = "سلام "},
                domain::rich::RichInline{
                    .kind = domain::rich::InlineKind::Mention,
                    .text = "@alice",
                    .fid = domain::identity::Fid{42U},
                    .username = "alice",
                    .displayName = "Alice",
                },
                domain::rich::RichInline{.kind = domain::rich::InlineKind::Text, .text = " 👋"},
            },
        },
    });

    const app::rich::FarcasterSafeSerializer serializer{};
    const auto safe = serializer.serialize(document);
    assert(safe);
    assert(safe->text == "سلام  👋");
    assert(safe->mentions.size() == 1U);
    assert(safe->mentions.front().fid.value == 42U);
    // Four Persian letters are 8 UTF-8 bytes, followed by one ASCII space.
    assert(safe->mentions.front().bytePosition == 9U);
}

void unsafeLinksNeverReachPublication()
{
    assert(!app::rich::safeLinkTarget("javascript:alert(1)"));
    assert(!app::rich::safeLinkTarget("data:text/html,boom"));
    assert(app::rich::safeLinkTarget("https://example.com/path"));
    assert(app::rich::safeLinkTarget("farcaster://casts/0x1234"));
}

void schemaAndInputBoundsFailClosed()
{
    app::rich::FarcasterSafeSerializer serializer{};
    domain::rich::RichCastDocument future{};
    future.schemaVersion = domain::rich::CurrentSchemaVersion + 1U;
    assert(!serializer.serialize(future));

    app::rich::MarkdownImporter importer{};
    const std::string oversized(257U * 1024U, 'x');
    assert(!importer.parse(oversized));
}

void languageRegistryRecognizesTechnicalContent()
{
    const app::syntax::SyntaxHighlighter highlighter{};
    assert(highlighter.detectLanguage("export module genycaster.composer;\nimport <vector>;") == "cpp");
    assert(highlighter.detectLanguage("import QtQuick\nDrawer { property bool open: true }") == "qml");
    assert(highlighter.detectLanguage("<?php final readonly class Cast {}") == "php");
    assert(highlighter.detectLanguage("interface CastDocument { readonly id: string; }") == "typescript");
    assert(highlighter.detectLanguage("pub struct CastDocument { blocks: Vec<CastBlock> }") == "rust");
    assert(highlighter.detectLanguage("curl --request POST --url https://api.example.com") == "curl");
    assert(highlighter.detectLanguage("POST /v1/casts HTTP/1.1\nContent-Type: application/json") == "http");

    const auto tokens = highlighter.highlight("export module sample;\nimport <vector>;", "cpp");
    assert(!tokens.empty());
}


void unicodeMentionPositionsStayByteExact()
{
    const app::rich::FarcasterSafeSerializer serializer{};

    auto positionForPrefix = [&](const std::string& prefix) {
        domain::rich::RichCastDocument document{};
        document.blocks.push_back(domain::rich::RichBlock{
            .id = "unicode-mention",
            .kind = domain::rich::BlockKind::Paragraph,
            .payload = domain::rich::TextBlock{
                .inlines = {
                    domain::rich::RichInline{.kind = domain::rich::InlineKind::Text, .text = prefix},
                    domain::rich::RichInline{
                        .kind = domain::rich::InlineKind::Mention,
                        .text = "@alice",
                        .fid = domain::identity::Fid{42U},
                        .username = "alice",
                    },
                },
            },
        });
        const auto safe = serializer.serialize(document);
        assert(safe && safe->mentions.size() == 1U);
        return safe->mentions.front().bytePosition;
    };

    assert(positionForPrefix("👋 ") == std::string{"👋 "}.size());
    assert(positionForPrefix("مرحبا ") == std::string{"مرحبا "}.size());
    assert(positionForPrefix("你好 ") == std::string{"你好 "}.size());
    assert(positionForPrefix("café ") == std::string{"café "}.size());
}

void malformedMarkdownAndLargeCodeFailGracefully()
{
    const app::rich::MarkdownImporter importer{};
    const app::rich::FarcasterSafeSerializer serializer{};

    const auto malformed = importer.parse("**unterminated\n\n[broken](javascript:alert(1))");
    assert(malformed);
    const auto safe = serializer.serialize(*malformed);
    assert(safe);
    assert(safe->text.find("javascript:") != std::string::npos); // literal user text remains readable, never activated as a link

    std::string largeCode{"```cpp\n"};
    largeCode.append(domain::rich::MaxCodeBlockBytes + 1U, 'x');
    largeCode.append("\n```");
    assert(!importer.parse(largeCode));
}

void utf8CountersDistinguishBytesAndScalars()
{
    assert(app::rich::utf8CodePointCount("ASCII") == 5U);
    assert(app::rich::utf8CodePointCount("سلام") == 4U);
    assert(app::rich::utf8CodePointCount("👋") == 1U);
    assert(std::string{"سلام"}.size() == 8U);
    assert(std::string{"👋"}.size() == 4U);
}


void malformedUtf8FailsClosed()
{
    const std::string invalid{"\xF0\x28\x8C\x28", 4};
    const app::rich::MarkdownImporter importer{};
    assert(!importer.parse(invalid));
    assert(app::rich::utf8CodePointCount(invalid) == 0U);
}


void protocolEmbedsNeverLeakIntoCastText()
{
    domain::rich::RichCastDocument document{};
    document.blocks.push_back(domain::rich::RichBlock{
        .id = "text",
        .kind = domain::rich::BlockKind::Paragraph,
        .payload = domain::rich::TextBlock{
            .inlines = {domain::rich::RichInline{
                .kind = domain::rich::InlineKind::Text,
                .text = "Rendered by Genycaster",
            }},
        },
    });
    document.blocks.push_back(domain::rich::RichBlock{
        .id = "image",
        .kind = domain::rich::BlockKind::Media,
        .payload = domain::rich::MediaBlock{
            .url = "https://media-farcaster.genyleap.com/m/example.png",
            .altText = "Screenshot",
        },
    });
    document.blocks.push_back(domain::rich::RichBlock{
        .id = "link",
        .kind = domain::rich::BlockKind::Embed,
        .payload = domain::rich::EmbedBlock{.url = "https://example.com/article"},
    });

    const app::rich::FarcasterSafeSerializer serializer{};
    const auto safe = serializer.serialize(document);
    assert(safe);
    assert(safe->text == "Rendered by Genycaster");
    assert(safe->text.find("media-farcaster.genyleap.com") == std::string::npos);
    assert(safe->text.find("example.com/article") == std::string::npos);

    domain::rich::RichCastDocument mediaOnly{};
    mediaOnly.blocks.push_back(domain::rich::RichBlock{
        .id = "image-only",
        .kind = domain::rich::BlockKind::Media,
        .payload = domain::rich::MediaBlock{
            .url = "https://media-farcaster.genyleap.com/m/image-only.png",
            .altText = "Image only",
        },
    });
    const auto mediaOnlySafe = serializer.serialize(mediaOnly);
    assert(mediaOnlySafe);
    assert(mediaOnlySafe->text.empty());
}

void markdownRoundTripKeepsReadableSemantics()
{
    constexpr std::string_view source = "# Release\n\n**Native** and _open_.\n\n1. First\n2. Second";
    const app::rich::MarkdownImporter importer{};
    const app::rich::MarkdownExporter exporter{};
    const app::rich::FarcasterSafeSerializer safeSerializer{};

    const auto first = importer.parse(source);
    assert(first);
    const auto exported = exporter.serialize(*first);
    const auto second = importer.parse(exported);
    assert(second);
    const auto firstSafe = safeSerializer.serialize(*first);
    const auto secondSafe = safeSerializer.serialize(*second);
    assert(firstSafe && secondSafe);
    assert(firstSafe->text == secondSafe->text);
}

} // namespace

int main()
{
    fidStrongTypeSupportsOptionalEquality();
    richMarkdownDegradesIntentionally();
    semanticMentionUsesFinalUtf8BytePosition();
    unicodeMentionPositionsStayByteExact();
    unsafeLinksNeverReachPublication();
    malformedMarkdownAndLargeCodeFailGracefully();
    utf8CountersDistinguishBytesAndScalars();
    malformedUtf8FailsClosed();
    schemaAndInputBoundsFailClosed();
    languageRegistryRecognizesTechnicalContent();
    markdownRoundTripKeepsReadableSemantics();
    protocolEmbedsNeverLeakIntoCastText();
    return 0;
}
