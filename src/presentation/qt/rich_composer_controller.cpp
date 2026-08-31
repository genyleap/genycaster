#include "rich_composer_controller.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QColor>
#include <QCryptographicHash>
#include <QDateTime>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QUrl>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

import farcaster.application.rich_format;
import farcaster.application.syntax;
import farcaster.domain.identity;
import farcaster.domain.rich_document;

namespace {
using farcaster::domain::rich::BlockKind;
using farcaster::domain::rich::CodeBlock;
using farcaster::domain::rich::Direction;
using farcaster::domain::rich::HeadingBlock;
using farcaster::domain::rich::InlineKind;
using farcaster::domain::rich::ListBlock;
using farcaster::domain::rich::ListItem;
using farcaster::domain::rich::RichBlock;
using farcaster::domain::rich::RichCastDocument;
using farcaster::domain::rich::RichInline;
using farcaster::domain::rich::TextBlock;

constexpr auto DraftKey = "composer/richDraftV1";
constexpr auto SidecarPrefix = "richCastSidecar/v1/";
constexpr std::size_t MaxHistory = 64U;

class NativeSyntaxDocumentHighlighter final : public QSyntaxHighlighter {
public:
    NativeSyntaxDocumentHighlighter(
        QTextDocument* document,
        farcaster::application::syntax::SyntaxHighlighter& engine)
        : QSyntaxHighlighter(static_cast<QObject*>(nullptr)), engine(engine)
    {
        setDocument(document);
    }

    void configure(const QString& language, const QVariantMap& semanticPalette)
    {
        requestedLanguage = language.trimmed().toUtf8().toStdString();
        if (requestedLanguage.empty()) requestedLanguage = "auto";
        palette = semanticPalette;
        if (requestedLanguage == "auto" && document()) {
            effectiveLanguage = engine.detectLanguage(document()->toPlainText().toUtf8().toStdString());
        } else {
            effectiveLanguage = farcaster::application::syntax::LanguageRegistry::canonicalId(requestedLanguage);
        }
        rehighlight();
    }

protected:
    void highlightBlock(const QString& text) override
    {
        const QByteArray utf8 = text.toUtf8();
        const auto language = effectiveLanguage.empty() ? requestedLanguage : effectiveLanguage;
        for (const auto& token : engine.highlight(utf8.toStdString(), language)) {
            if (token.start + token.length > static_cast<std::size_t>(utf8.size())) continue;
            const QString prefix = QString::fromUtf8(utf8.constData(), static_cast<qsizetype>(token.start));
            const QString body = QString::fromUtf8(
                utf8.constData() + token.start,
                static_cast<qsizetype>(token.length));
            if (body.isEmpty()) continue;
            const QString key = QString::fromUtf8(
                farcaster::application::syntax::tokenKindName(token.kind));
            const QColor color = palette.value(key).value<QColor>();
            if (!color.isValid()) continue;
            if (prefix.size() > static_cast<qsizetype>(std::numeric_limits<int>::max()) ||
                body.size() > static_cast<qsizetype>(std::numeric_limits<int>::max())) {
                continue;
            }
            QTextCharFormat format{};
            format.setForeground(color);
            setFormat(static_cast<int>(prefix.size()), static_cast<int>(body.size()), format);
        }
    }

private:
    farcaster::application::syntax::SyntaxHighlighter& engine;
    std::string requestedLanguage{"auto"};
    std::string effectiveLanguage{};
    QVariantMap palette{};
};

QString blockKindName(const BlockKind kind)
{
    switch (kind) {
    case BlockKind::Paragraph: return QStringLiteral("paragraph");
    case BlockKind::Heading: return QStringLiteral("heading");
    case BlockKind::BlockQuote: return QStringLiteral("quote");
    case BlockKind::BulletList: return QStringLiteral("bullet-list");
    case BlockKind::OrderedList: return QStringLiteral("ordered-list");
    case BlockKind::CodeBlock: return QStringLiteral("code");
    case BlockKind::HorizontalSeparator: return QStringLiteral("separator");
    case BlockKind::Embed: return QStringLiteral("embed");
    case BlockKind::CastReference: return QStringLiteral("cast-reference");
    case BlockKind::Media: return QStringLiteral("media");
    }
    return QStringLiteral("paragraph");
}

std::optional<BlockKind> blockKindFromName(QString value)
{
    value = value.trimmed().toLower();
    if (value == QStringLiteral("paragraph")) return BlockKind::Paragraph;
    if (value == QStringLiteral("heading")) return BlockKind::Heading;
    if (value == QStringLiteral("quote")) return BlockKind::BlockQuote;
    if (value == QStringLiteral("bullet-list") || value == QStringLiteral("list")) return BlockKind::BulletList;
    if (value == QStringLiteral("ordered-list")) return BlockKind::OrderedList;
    if (value == QStringLiteral("code")) return BlockKind::CodeBlock;
    if (value == QStringLiteral("separator")) return BlockKind::HorizontalSeparator;
    if (value == QStringLiteral("embed")) return BlockKind::Embed;
    if (value == QStringLiteral("cast-reference")) return BlockKind::CastReference;
    if (value == QStringLiteral("media")) return BlockKind::Media;
    return std::nullopt;
}

QString inlineKindName(const InlineKind kind)
{
    switch (kind) {
    case InlineKind::Text: return QStringLiteral("text");
    case InlineKind::Strong: return QStringLiteral("strong");
    case InlineKind::Emphasis: return QStringLiteral("emphasis");
    case InlineKind::InlineCode: return QStringLiteral("inline-code");
    case InlineKind::Link: return QStringLiteral("link");
    case InlineKind::Mention: return QStringLiteral("mention");
    case InlineKind::Emoji: return QStringLiteral("emoji");
    }
    return QStringLiteral("text");
}

InlineKind inlineKindFromName(const QString& name)
{
    if (name == QStringLiteral("strong")) return InlineKind::Strong;
    if (name == QStringLiteral("emphasis")) return InlineKind::Emphasis;
    if (name == QStringLiteral("inline-code")) return InlineKind::InlineCode;
    if (name == QStringLiteral("link")) return InlineKind::Link;
    if (name == QStringLiteral("mention")) return InlineKind::Mention;
    if (name == QStringLiteral("emoji")) return InlineKind::Emoji;
    return InlineKind::Text;
}

QJsonObject inlineToJson(const RichInline& span)
{
    QJsonObject object{
        {QStringLiteral("kind"), inlineKindName(span.kind)},
        {QStringLiteral("text"), QString::fromUtf8(span.text)},
    };
    if (!span.target.empty()) object.insert(QStringLiteral("target"), QString::fromUtf8(span.target));
    if (span.fid) object.insert(QStringLiteral("fid"), QString::number(span.fid->value));
    if (!span.username.empty()) object.insert(QStringLiteral("username"), QString::fromUtf8(span.username));
    if (!span.displayName.empty()) object.insert(QStringLiteral("displayName"), QString::fromUtf8(span.displayName));
    return object;
}

RichInline inlineFromJson(const QJsonObject& object)
{
    RichInline span{};
    span.kind = inlineKindFromName(object.value(QStringLiteral("kind")).toString());
    span.text = object.value(QStringLiteral("text")).toString().toUtf8().toStdString();
    span.target = object.value(QStringLiteral("target")).toString().toUtf8().toStdString();
    bool ok = false;
    const auto fid = object.value(QStringLiteral("fid")).toString().toULongLong(&ok);
    if (ok && fid > 0U) span.fid = farcaster::domain::identity::Fid{fid};
    span.username = object.value(QStringLiteral("username")).toString().toUtf8().toStdString();
    span.displayName = object.value(QStringLiteral("displayName")).toString().toUtf8().toStdString();
    return span;
}

QJsonArray inlinesToJson(const std::vector<RichInline>& inlines)
{
    QJsonArray array{};
    for (const auto& span : inlines) array.append(inlineToJson(span));
    return array;
}

std::vector<RichInline> inlinesFromJson(const QJsonArray& array)
{
    std::vector<RichInline> result{};
    result.reserve(static_cast<std::size_t>(array.size()));
    for (const auto& value : array) {
        if (value.isObject()) result.push_back(inlineFromJson(value.toObject()));
    }
    return result;
}

QString inlinesPlainText(const std::vector<RichInline>& inlines)
{
    return QString::fromUtf8(farcaster::domain::rich::inlinePlainText(inlines));
}

QJsonObject blockToJson(const RichBlock& block)
{
    QJsonObject object{
        {QStringLiteral("id"), QString::fromUtf8(block.id)},
        {QStringLiteral("kind"), blockKindName(block.kind)},
    };
    if (const auto* text = std::get_if<TextBlock>(&block.payload)) {
        object.insert(QStringLiteral("inlines"), inlinesToJson(text->inlines));
        object.insert(QStringLiteral("direction"), static_cast<int>(text->direction));
    } else if (const auto* heading = std::get_if<HeadingBlock>(&block.payload)) {
        object.insert(QStringLiteral("inlines"), inlinesToJson(heading->inlines));
        object.insert(QStringLiteral("level"), heading->level);
        object.insert(QStringLiteral("direction"), static_cast<int>(heading->direction));
    } else if (const auto* list = std::get_if<ListBlock>(&block.payload)) {
        QJsonArray items{};
        for (const auto& item : list->items) items.append(inlinesToJson(item.inlines));
        object.insert(QStringLiteral("items"), items);
        object.insert(QStringLiteral("direction"), static_cast<int>(list->direction));
    } else if (const auto* code = std::get_if<CodeBlock>(&block.payload)) {
        object.insert(QStringLiteral("language"), QString::fromUtf8(code->language));
        object.insert(QStringLiteral("source"), QString::fromUtf8(code->source));
        object.insert(QStringLiteral("detectedLanguage"), QString::fromUtf8(code->detectedLanguage));
        object.insert(QStringLiteral("displayLanguage"), QString::fromUtf8(code->displayLanguage));
        object.insert(QStringLiteral("lineNumbers"), code->lineNumbers);
        object.insert(QStringLiteral("wrap"), code->wrap);
    } else if (const auto* embed = std::get_if<farcaster::domain::rich::EmbedBlock>(&block.payload)) {
        object.insert(QStringLiteral("url"), QString::fromUtf8(embed->url));
        object.insert(QStringLiteral("title"), QString::fromUtf8(embed->title));
    } else if (const auto* cast = std::get_if<farcaster::domain::rich::CastReferenceBlock>(&block.payload)) {
        object.insert(QStringLiteral("castId"), QString::fromUtf8(cast->castId));
        if (cast->author) object.insert(QStringLiteral("authorFid"), QString::number(cast->author->value));
    } else if (const auto* media = std::get_if<farcaster::domain::rich::MediaBlock>(&block.payload)) {
        object.insert(QStringLiteral("url"), QString::fromUtf8(media->url));
        object.insert(QStringLiteral("mediaType"), QString::fromUtf8(media->mediaType));
        object.insert(QStringLiteral("altText"), QString::fromUtf8(media->altText));
    }
    return object;
}

std::optional<RichBlock> blockFromJson(const QJsonObject& object)
{
    const auto kind = blockKindFromName(object.value(QStringLiteral("kind")).toString());
    if (!kind) return std::nullopt;
    RichBlock block{};
    block.id = object.value(QStringLiteral("id")).toString().toUtf8().toStdString();
    block.kind = *kind;
    const auto direction = static_cast<Direction>(std::clamp(object.value(QStringLiteral("direction")).toInt(), 0, 2));
    switch (*kind) {
    case BlockKind::Paragraph:
    case BlockKind::BlockQuote:
        block.payload = TextBlock{.inlines = inlinesFromJson(object.value(QStringLiteral("inlines")).toArray()), .direction = direction};
        break;
    case BlockKind::Heading:
        block.payload = HeadingBlock{
            .level = static_cast<std::uint8_t>(std::clamp(object.value(QStringLiteral("level")).toInt(1), 1, 6)),
            .inlines = inlinesFromJson(object.value(QStringLiteral("inlines")).toArray()),
            .direction = direction,
        };
        break;
    case BlockKind::BulletList:
    case BlockKind::OrderedList: {
        ListBlock list{.direction = direction};
        for (const auto& item : object.value(QStringLiteral("items")).toArray()) {
            list.items.push_back(ListItem{.inlines = inlinesFromJson(item.toArray())});
        }
        block.payload = std::move(list);
        break;
    }
    case BlockKind::CodeBlock:
        block.payload = CodeBlock{
            .language = object.value(QStringLiteral("language")).toString(QStringLiteral("auto")).toUtf8().toStdString(),
            .source = object.value(QStringLiteral("source")).toString().toUtf8().toStdString(),
            .detectedLanguage = object.value(QStringLiteral("detectedLanguage")).toString().toUtf8().toStdString(),
            .displayLanguage = object.value(QStringLiteral("displayLanguage")).toString().toUtf8().toStdString(),
            .lineNumbers = object.value(QStringLiteral("lineNumbers")).toBool(),
            .wrap = object.value(QStringLiteral("wrap")).toBool(),
        };
        break;
    case BlockKind::HorizontalSeparator: block.payload = std::monostate{}; break;
    case BlockKind::Embed:
        block.payload = farcaster::domain::rich::EmbedBlock{
            .url = object.value(QStringLiteral("url")).toString().toUtf8().toStdString(),
            .title = object.value(QStringLiteral("title")).toString().toUtf8().toStdString(),
        };
        break;
    case BlockKind::CastReference: {
        bool ok = false;
        const auto fid = object.value(QStringLiteral("authorFid")).toString().toULongLong(&ok);
        block.payload = farcaster::domain::rich::CastReferenceBlock{
            .castId = object.value(QStringLiteral("castId")).toString().toUtf8().toStdString(),
            .author = ok && fid > 0U ? std::optional{farcaster::domain::identity::Fid{fid}} : std::nullopt,
        };
        break;
    }
    case BlockKind::Media:
        block.payload = farcaster::domain::rich::MediaBlock{
            .url = object.value(QStringLiteral("url")).toString().toUtf8().toStdString(),
            .mediaType = object.value(QStringLiteral("mediaType")).toString().toUtf8().toStdString(),
            .altText = object.value(QStringLiteral("altText")).toString().toUtf8().toStdString(),
        };
        break;
    }
    if (block.id.empty()) block.id = farcaster::domain::rich::makeBlockId(1U, object.value(QStringLiteral("kind")).toString().toStdString());
    return block;
}

QByteArray blocksChecksum(const QJsonArray& blocks)
{
    return QCryptographicHash::hash(
        QJsonDocument{blocks}.toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

QByteArray documentToJson(
    const RichCastDocument& document, const QString& markdown, const QString& castHash = {})
{
    QJsonArray blocks{};
    for (const auto& block : document.blocks) blocks.append(blockToJson(block));
    QJsonObject object{
        {QStringLiteral("schemaVersion"), static_cast<int>(document.schemaVersion)},
        {QStringLiteral("markdown"), markdown},
        {QStringLiteral("blocks"), blocks},
        {QStringLiteral("checksum"), QStringLiteral("sha256:") + QString::fromLatin1(blocksChecksum(blocks))},
    };
    if (!castHash.trimmed().isEmpty()) {
        object.insert(QStringLiteral("castHash"), castHash.trimmed());
        object.insert(QStringLiteral("createdBy"), QStringLiteral("genycaster"));
        object.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    }
    return QJsonDocument{object}.toJson(QJsonDocument::Compact);
}

std::optional<std::pair<RichCastDocument, QString>> documentFromJson(const QByteArray& bytes)
{
    QJsonParseError error{};
    const auto json = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !json.isObject()) return std::nullopt;
    const auto object = json.object();
    const auto blocks = object.value(QStringLiteral("blocks")).toArray();
    const auto checksum = object.value(QStringLiteral("checksum")).toString();
    if (!checksum.isEmpty()) {
        const auto expected = QStringLiteral("sha256:") + QString::fromLatin1(blocksChecksum(blocks));
        if (checksum != expected) return std::nullopt;
    }
    RichCastDocument document{};
    document.schemaVersion = static_cast<std::uint32_t>(object.value(QStringLiteral("schemaVersion")).toInt());
    if (document.schemaVersion == 0U || document.schemaVersion > farcaster::domain::rich::CurrentSchemaVersion) return std::nullopt;
    for (const auto& value : blocks) {
        if (!value.isObject()) continue;
        if (auto block = blockFromJson(value.toObject())) document.blocks.push_back(std::move(*block));
    }
    if (!farcaster::domain::rich::structurallyValid(document)) return std::nullopt;
    return std::pair{std::move(document), object.value(QStringLiteral("markdown")).toString()};
}

QVariantMap inlineProjection(const RichInline& span)
{
    QString safeTarget{};
    if (span.kind == InlineKind::Link) {
        if (const auto target = farcaster::application::rich::safeLinkTarget(span.target)) {
            safeTarget = QString::fromUtf8(*target);
        }
    }
    QVariantMap map{
        {QStringLiteral("kind"), inlineKindName(span.kind)},
        {QStringLiteral("text"), QString::fromUtf8(span.text)},
        {QStringLiteral("target"), safeTarget},
        {QStringLiteral("username"), QString::fromUtf8(span.username)},
        {QStringLiteral("displayName"), QString::fromUtf8(span.displayName)},
    };
    if (span.fid) map.insert(QStringLiteral("fid"), QVariant::fromValue<qulonglong>(span.fid->value));
    return map;
}

QVariantList inlineProjection(const std::vector<RichInline>& inlines)
{
    QVariantList result{};
    for (const auto& span : inlines) result.push_back(inlineProjection(span));
    return result;
}

QVariantMap blockProjection(const RichBlock& block)
{
    QVariantMap map{
        {QStringLiteral("id"), QString::fromUtf8(block.id)},
        {QStringLiteral("kind"), blockKindName(block.kind)},
    };
    if (const auto* text = std::get_if<TextBlock>(&block.payload)) {
        map.insert(QStringLiteral("text"), inlinesPlainText(text->inlines));
        map.insert(QStringLiteral("inlines"), inlineProjection(text->inlines));
        map.insert(QStringLiteral("direction"), static_cast<int>(text->direction));
    } else if (const auto* heading = std::get_if<HeadingBlock>(&block.payload)) {
        map.insert(QStringLiteral("text"), inlinesPlainText(heading->inlines));
        map.insert(QStringLiteral("inlines"), inlineProjection(heading->inlines));
        map.insert(QStringLiteral("level"), heading->level);
        map.insert(QStringLiteral("direction"), static_cast<int>(heading->direction));
    } else if (const auto* list = std::get_if<ListBlock>(&block.payload)) {
        QVariantList items{};
        QStringList lines{};
        for (const auto& item : list->items) {
            const auto itemText = inlinesPlainText(item.inlines);
            lines.push_back(itemText);
            items.push_back(QVariantMap{
                {QStringLiteral("text"), itemText},
                {QStringLiteral("inlines"), inlineProjection(item.inlines)},
            });
        }
        map.insert(QStringLiteral("text"), lines.join(QLatin1Char('\n')));
        map.insert(QStringLiteral("items"), items);
        map.insert(QStringLiteral("direction"), static_cast<int>(list->direction));
    } else if (const auto* code = std::get_if<CodeBlock>(&block.payload)) {
        map.insert(QStringLiteral("text"), QString::fromUtf8(code->source));
        map.insert(QStringLiteral("language"), QString::fromUtf8(code->language));
        map.insert(QStringLiteral("detectedLanguage"), QString::fromUtf8(code->detectedLanguage));
        map.insert(QStringLiteral("displayLanguage"), QString::fromUtf8(code->displayLanguage));
        map.insert(QStringLiteral("lineNumbers"), code->lineNumbers);
        map.insert(QStringLiteral("wrap"), code->wrap);
    } else if (const auto* embed = std::get_if<farcaster::domain::rich::EmbedBlock>(&block.payload)) {
        map.insert(QStringLiteral("url"), QString::fromUtf8(embed->url));
        map.insert(QStringLiteral("title"), QString::fromUtf8(embed->title));
    } else if (const auto* cast = std::get_if<farcaster::domain::rich::CastReferenceBlock>(&block.payload)) {
        map.insert(QStringLiteral("castId"), QString::fromUtf8(cast->castId));
        if (cast->author) map.insert(QStringLiteral("authorFid"), QVariant::fromValue<qulonglong>(cast->author->value));
    } else if (const auto* media = std::get_if<farcaster::domain::rich::MediaBlock>(&block.payload)) {
        map.insert(QStringLiteral("url"), QString::fromUtf8(media->url));
        map.insert(QStringLiteral("mediaType"), QString::fromUtf8(media->mediaType));
        map.insert(QStringLiteral("altText"), QString::fromUtf8(media->altText));
    }
    return map;
}

std::vector<RichInline>* editableInlines(RichBlock& block)
{
    if (auto* text = std::get_if<TextBlock>(&block.payload)) return &text->inlines;
    if (auto* heading = std::get_if<HeadingBlock>(&block.payload)) return &heading->inlines;
    return nullptr;
}

std::string editorReadableText(const RichCastDocument& document)
{
    std::string output{};
    const auto appendBreak = [&] {
        if (!output.empty() && output.back() != '\n') output.push_back('\n');
    };
    for (const auto& block : document.blocks) {
        if (!output.empty()) appendBreak();
        if (const auto* text = std::get_if<TextBlock>(&block.payload)) {
            output.append(farcaster::domain::rich::inlinePlainText(text->inlines));
        } else if (const auto* heading = std::get_if<HeadingBlock>(&block.payload)) {
            output.append(farcaster::domain::rich::inlinePlainText(heading->inlines));
        } else if (const auto* list = std::get_if<ListBlock>(&block.payload)) {
            for (std::size_t index{}; index < list->items.size(); ++index) {
                if (index > 0U) output.push_back('\n');
                output.append(farcaster::domain::rich::inlinePlainText(list->items[index].inlines));
            }
        } else if (const auto* code = std::get_if<CodeBlock>(&block.payload)) {
            output.append(code->source);
        } else if (const auto* embed = std::get_if<farcaster::domain::rich::EmbedBlock>(&block.payload)) {
            output.append(embed->title.empty() ? embed->url : embed->title);
        } else if (const auto* media = std::get_if<farcaster::domain::rich::MediaBlock>(&block.payload)) {
            output.append(media->altText.empty() ? media->url : media->altText);
        }
    }
    return output;
}

bool sameOptionalFid(
    const std::optional<farcaster::domain::identity::Fid>& lhs,
    const std::optional<farcaster::domain::identity::Fid>& rhs) noexcept
{
    if (lhs.has_value() != rhs.has_value()) return false;
    return !lhs.has_value() || lhs->value == rhs->value;
}

bool sameInline(const RichInline& lhs, const RichInline& rhs) noexcept
{
    return lhs.kind == rhs.kind && lhs.text == rhs.text && lhs.target == rhs.target &&
        sameOptionalFid(lhs.fid, rhs.fid) && lhs.username == rhs.username &&
        lhs.displayName == rhs.displayName;
}

bool sameInlines(const std::vector<RichInline>& lhs, const std::vector<RichInline>& rhs) noexcept
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin(), sameInline);
}

} // namespace

class RichComposerController::Impl final {
public:
    explicit Impl(QSettings& settings, RichComposerController& owner)
        : settings(settings), owner(owner)
    {
        autosave.setSingleShot(true);
        autosave.setInterval(700);
        QObject::connect(&autosave, &QTimer::timeout, &owner, [&] { saveDraft(false); });
        newEmpty(false);
    }

    QSettings& settings;
    RichComposerController& owner;
    farcaster::application::rich::MarkdownImporter importer{};
    farcaster::application::rich::MarkdownExporter exporter{};
    farcaster::application::rich::FarcasterSafeSerializer serializer{};
    farcaster::application::syntax::SyntaxHighlighter highlighter{};
    std::vector<std::unique_ptr<NativeSyntaxDocumentHighlighter>> documentHighlighters{};
    RichCastDocument document{};
    farcaster::application::rich::FarcasterSafeLayout layout{};
    QString markdown{};
    std::deque<RichCastDocument> undoStack{};
    std::deque<RichCastDocument> redoStack{};
    QTimer autosave{};
    bool dirty{};
    QString saveStatusText{};
    qulonglong revision{};

    void rebuild()
    {
        if (auto serialized = serializer.serialize(document)) layout = std::move(*serialized);
        else layout = {};
        ++revision;
        emit owner.documentChanged();
    }

    void markDirty()
    {
        dirty = true;
        saveStatusText = qtTrId("genycaster.composer.unsaved");
        emit owner.saveStateChanged();
        autosave.start();
    }

    void pushHistory()
    {
        undoStack.push_back(document);
        if (undoStack.size() > MaxHistory) undoStack.pop_front();
        redoStack.clear();
        emit owner.historyChanged();
    }

    void setDocument(RichCastDocument next, QString source, bool recordHistory)
    {
        if (recordHistory) pushHistory();
        document = std::move(next);
        markdown = std::move(source);
        rebuild();
        markDirty();
    }

    void syncMarkdownFromDocument()
    {
        markdown = QString::fromUtf8(exporter.serialize(document));
    }

    void newEmpty(bool mark)
    {
        document = RichCastDocument{};
        document.blocks.push_back(RichBlock{
            .id = farcaster::domain::rich::makeBlockId(1U, "paragraph"),
            .kind = BlockKind::Paragraph,
            .payload = TextBlock{},
        });
        markdown.clear();
        layout = {};
        undoStack.clear();
        redoStack.clear();
        dirty = false;
        saveStatusText.clear();
        rebuild();
        emit owner.historyChanged();
        if (mark) markDirty(); else emit owner.saveStateChanged();
    }

    void saveDraft(bool explicitSave)
    {
        settings.setValue(QString::fromLatin1(DraftKey), documentToJson(document, markdown));
        settings.sync();
        dirty = false;
        saveStatusText = explicitSave ? qtTrId("genycaster.composer.saved") : qtTrId("genycaster.composer.autosaved");
        emit owner.saveStateChanged();
    }
};

RichComposerController::RichComposerController(QSettings& settings, QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(settings, *this))
{
}

RichComposerController::~RichComposerController() = default;

QString RichComposerController::markdownSource() const { return m_impl->markdown; }
QString RichComposerController::safeText() const { return QString::fromUtf8(m_impl->layout.text); }
int RichComposerController::editorCharacters() const
{
    return static_cast<int>(farcaster::application::rich::utf8CodePointCount(
        editorReadableText(m_impl->document)));
}
int RichComposerController::publishedBytes() const noexcept { return static_cast<int>(m_impl->layout.text.size()); }
bool RichComposerController::withinProtocolLimit() const noexcept { return m_impl->layout.text.size() <= 1024U; }
bool RichComposerController::longCast() const noexcept { return m_impl->layout.text.size() > 320U; }
int RichComposerController::simplifiedElementCount() const noexcept { return static_cast<int>(m_impl->layout.simplifiedElementCount); }
QStringList RichComposerController::warnings() const
{
    QStringList result{};
    for (const auto& warning : m_impl->layout.warnings) {
        switch (warning.kind) {
        case farcaster::application::rich::CompatibilityWarningKind::FormattingSimplified:
            result.push_back(qtTrId("genycaster.composer.formatting_simplified_count")
                .arg(static_cast<qulonglong>(warning.count)));
            break;
        }
    }
    if (longCast()) result.push_back(qtTrId("genycaster.composer.long_cast_notice"));
    return result;
}
QVariantList RichComposerController::blocks() const
{
    QVariantList result{};
    for (const auto& block : m_impl->document.blocks) result.push_back(blockProjection(block));
    return result;
}
QVariantList RichComposerController::languages() const
{
    QVariantList result{};
    result.push_back(QVariantMap{
        {QStringLiteral("id"), QStringLiteral("auto")},
        {QStringLiteral("label"), qtTrId("genycaster.composer.auto_detect")},
    });
    for (const auto& language : farcaster::application::syntax::LanguageRegistry::languages()) {
        result.push_back(QVariantMap{
            {QStringLiteral("id"), QString::fromUtf8(language.id)},
            {QStringLiteral("label"), QString::fromUtf8(language.displayName)},
        });
    }
    return result;
}
QVariantList RichComposerController::mentionFids() const
{
    QVariantList result{};
    for (const auto& mention : m_impl->layout.mentions) result.push_back(QVariant::fromValue<qulonglong>(mention.fid.value));
    return result;
}
QVariantList RichComposerController::mentionPositions() const
{
    QVariantList result{};
    for (const auto& mention : m_impl->layout.mentions) result.push_back(mention.bytePosition);
    return result;
}
QVariantList RichComposerController::protocolEmbeds() const
{
    QVariantList result{};
    result.reserve(2);
    for (const auto& block : m_impl->document.blocks) {
        if (result.size() >= 2) break;
        if (const auto* embed = std::get_if<farcaster::domain::rich::EmbedBlock>(&block.payload)) {
            const auto target = farcaster::application::rich::safeLinkTarget(embed->url);
            if (target) {
                result.push_back(QVariantMap{
                    {QStringLiteral("kind"), QStringLiteral("url")},
                    {QStringLiteral("value"), QString::fromUtf8(*target)},
                });
            }
            continue;
        }
        if (const auto* media = std::get_if<farcaster::domain::rich::MediaBlock>(&block.payload)) {
            const auto target = farcaster::application::rich::safeLinkTarget(media->url);
            if (target) {
                result.push_back(QVariantMap{
                    {QStringLiteral("kind"), QStringLiteral("url")},
                    {QStringLiteral("value"), QString::fromUtf8(*target)},
                });
            }
            continue;
        }
        if (const auto* cast = std::get_if<farcaster::domain::rich::CastReferenceBlock>(&block.payload);
            cast && cast->author && cast->author->value > 0U && !cast->castId.empty()) {
            result.push_back(QVariantMap{
                {QStringLiteral("kind"), QStringLiteral("cast-reference")},
                {QStringLiteral("value"), QString::fromUtf8(cast->castId)},
                {QStringLiteral("authorFid"), QVariant::fromValue<qulonglong>(cast->author->value)},
            });
        }
    }
    return result;
}
bool RichComposerController::canUndo() const noexcept { return !m_impl->undoStack.empty(); }
bool RichComposerController::canRedo() const noexcept { return !m_impl->redoStack.empty(); }
bool RichComposerController::dirty() const noexcept { return m_impl->dirty; }
QString RichComposerController::saveStatus() const { return m_impl->saveStatusText; }
qulonglong RichComposerController::revision() const noexcept { return m_impl->revision; }

void RichComposerController::setMarkdownSource(const QString& source)
{
    if (source == m_impl->markdown) return;
    auto parsed = m_impl->importer.parse(source.toUtf8().toStdString());
    if (!parsed) return;
    m_impl->setDocument(std::move(*parsed), source, true);
}

void RichComposerController::newDocument() { m_impl->newEmpty(false); }
bool RichComposerController::restoreDraft()
{
    const auto bytes = m_impl->settings.value(QString::fromLatin1(DraftKey)).toByteArray();
    if (bytes.isEmpty()) return false;
    auto restored = documentFromJson(bytes);
    if (!restored) return false;
    m_impl->document = std::move(restored->first);
    m_impl->markdown = std::move(restored->second);
    if (m_impl->markdown.isEmpty()) m_impl->syncMarkdownFromDocument();
    m_impl->dirty = false;
    m_impl->saveStatusText = qtTrId("genycaster.composer.draft_restored");
    m_impl->undoStack.clear();
    m_impl->redoStack.clear();
    m_impl->rebuild();
    emit historyChanged();
    emit saveStateChanged();
    return true;
}
void RichComposerController::saveDraft() { m_impl->saveDraft(true); }
void RichComposerController::clearDraft()
{
    m_impl->settings.remove(QString::fromLatin1(DraftKey));
    m_impl->settings.sync();
    m_impl->dirty = false;
    m_impl->saveStatusText.clear();
    emit saveStateChanged();
}
void RichComposerController::undo()
{
    if (m_impl->undoStack.empty()) return;
    m_impl->redoStack.push_back(m_impl->document);
    m_impl->document = std::move(m_impl->undoStack.back());
    m_impl->undoStack.pop_back();
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    emit historyChanged();
}
void RichComposerController::redo()
{
    if (m_impl->redoStack.empty()) return;
    m_impl->undoStack.push_back(m_impl->document);
    m_impl->document = std::move(m_impl->redoStack.back());
    m_impl->redoStack.pop_back();
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    emit historyChanged();
}

bool RichComposerController::setBlockText(const int index, const QString& text)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_impl->document.blocks.size()) return false;
    const auto utf8 = text.toUtf8().toStdString();
    const auto& currentBlock = m_impl->document.blocks[static_cast<std::size_t>(index)];
    if (std::holds_alternative<CodeBlock>(currentBlock.payload)
        && utf8.size() > farcaster::domain::rich::MaxCodeBlockBytes) {
        return false;
    }
    m_impl->pushHistory();
    auto& block = m_impl->document.blocks[static_cast<std::size_t>(index)];
    if (auto* inlines = editableInlines(block)) {
        *inlines = {RichInline{.kind = InlineKind::Text, .text = utf8}};
    } else if (auto* list = std::get_if<ListBlock>(&block.payload)) {
        list->items.clear();
        for (const auto& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            list->items.push_back(ListItem{.inlines = {RichInline{.kind = InlineKind::Text, .text = line.toUtf8().toStdString()}}});
        }
    } else if (auto* code = std::get_if<CodeBlock>(&block.payload)) {
        code->source = utf8;
        if (code->language == "auto") code->detectedLanguage = m_impl->highlighter.detectLanguage(code->source);
    } else {
        return false;
    }
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    return true;
}

bool RichComposerController::setBlockFormattedText(const int index, const QString& formattedText)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_impl->document.blocks.size()) return false;
    auto& block = m_impl->document.blocks[static_cast<std::size_t>(index)];
    auto* inlines = editableInlines(block);
    if (!inlines || formattedText.toUtf8().size() > 256 * 1024) return false;

    QTextDocument formatted{};
    formatted.setHtml(formattedText);
    std::vector<RichInline> next{};
    auto appendPlain = [&](std::string text) {
        if (text.empty()) return;
        if (!next.empty() && next.back().kind == InlineKind::Text) next.back().text.append(text);
        else next.push_back(RichInline{.kind = InlineKind::Text, .text = std::move(text)});
    };

    for (QTextBlock textBlock = formatted.begin(); textBlock.isValid(); textBlock = textBlock.next()) {
        if (!next.empty()) appendPlain("\n");
        for (auto fragmentIt = textBlock.begin(); !fragmentIt.atEnd(); ++fragmentIt) {
            const QTextFragment fragment = fragmentIt.fragment();
            if (!fragment.isValid() || fragment.text().isEmpty()) continue;
            const QTextCharFormat format = fragment.charFormat();
            const QString visible = fragment.text();
            const std::string utf8 = visible.toUtf8().toStdString();

            if (format.isAnchor()) {
                const QUrl href{format.anchorHref()};
                if (href.scheme() == QStringLiteral("genycaster-mention")) {
                    bool ok = false;
                    const auto fidValue = href.host().toULongLong(&ok);
                    const QString username = QUrl::fromPercentEncoding(
                        href.path().mid(1).toUtf8()).remove(QChar{'@'});
                    if (ok && fidValue > 0U && !username.isEmpty()) {
                        next.push_back(RichInline{
                            .kind = InlineKind::Mention,
                            .text = utf8,
                            .fid = farcaster::domain::identity::Fid{static_cast<std::uint64_t>(fidValue)},
                            .username = username.toUtf8().toStdString(),
                        });
                        continue;
                    }
                }
                const auto target = farcaster::application::rich::safeLinkTarget(
                    format.anchorHref().toUtf8().toStdString());
                if (target) {
                    next.push_back(RichInline{
                        .kind = InlineKind::Link,
                        .text = utf8,
                        .target = *target,
                    });
                    continue;
                }
            }

            if (format.fontFixedPitch() || format.background().style() != Qt::NoBrush) {
                next.push_back(RichInline{.kind = InlineKind::InlineCode, .text = utf8});
            } else if (format.fontWeight() >= QFont::DemiBold) {
                next.push_back(RichInline{.kind = InlineKind::Strong, .text = utf8});
            } else if (format.fontItalic()) {
                next.push_back(RichInline{.kind = InlineKind::Emphasis, .text = utf8});
            } else {
                appendPlain(utf8);
            }
        }
    }

    if (next.empty()) next.push_back(RichInline{.kind = InlineKind::Text, .text = {}});
    if (sameInlines(*inlines, next)) return true;
    m_impl->pushHistory();
    *inlines = std::move(next);
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    return true;
}

bool RichComposerController::setCodeLanguage(const int index, const QString& language)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_impl->document.blocks.size()) return false;
    auto* code = std::get_if<CodeBlock>(&m_impl->document.blocks[static_cast<std::size_t>(index)].payload);
    if (!code) return false;
    m_impl->pushHistory();
    code->language = farcaster::application::syntax::LanguageRegistry::canonicalId(language.toUtf8().toStdString());
    if (code->language.empty()) code->language = "auto";
    code->detectedLanguage = code->language == "auto" ? m_impl->highlighter.detectLanguage(code->source) : code->language;
    code->displayLanguage = farcaster::application::syntax::LanguageRegistry::displayName(code->detectedLanguage);
    m_impl->syncMarkdownFromDocument(); m_impl->rebuild(); m_impl->markDirty();
    return true;
}
bool RichComposerController::setCodeWrap(const int index, const bool wrap)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_impl->document.blocks.size()) return false;
    auto* code = std::get_if<CodeBlock>(&m_impl->document.blocks[static_cast<std::size_t>(index)].payload);
    if (!code) return false;
    m_impl->pushHistory(); code->wrap = wrap; m_impl->rebuild(); m_impl->markDirty(); return true;
}
bool RichComposerController::setCodeLineNumbers(const int index, const bool enabled)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_impl->document.blocks.size()) return false;
    auto* code = std::get_if<CodeBlock>(&m_impl->document.blocks[static_cast<std::size_t>(index)].payload);
    if (!code) return false;
    m_impl->pushHistory(); code->lineNumbers = enabled; m_impl->rebuild(); m_impl->markDirty(); return true;
}

bool RichComposerController::insertBlock(const QString& kindName, int afterIndex)
{
    const auto kind = blockKindFromName(kindName);
    if (!kind || m_impl->document.blocks.size() >= farcaster::domain::rich::MaxDocumentBlocks) return false;
    RichBlock block{.id = farcaster::domain::rich::makeBlockId(m_impl->revision + 1U, kindName.toStdString()), .kind = *kind};
    switch (*kind) {
    case BlockKind::Heading: block.payload = HeadingBlock{}; break;
    case BlockKind::BulletList:
    case BlockKind::OrderedList: block.payload = ListBlock{.items = {ListItem{}}}; break;
    case BlockKind::CodeBlock: block.payload = CodeBlock{}; break;
    case BlockKind::HorizontalSeparator: block.payload = std::monostate{}; break;
    case BlockKind::Embed: block.payload = farcaster::domain::rich::EmbedBlock{}; break;
    case BlockKind::CastReference: block.payload = farcaster::domain::rich::CastReferenceBlock{}; break;
    case BlockKind::Media: block.payload = farcaster::domain::rich::MediaBlock{}; break;
    case BlockKind::Paragraph:
    case BlockKind::BlockQuote: block.payload = TextBlock{}; break;
    }
    m_impl->pushHistory();
    const auto position = afterIndex < 0
        ? static_cast<int>(m_impl->document.blocks.size())
        : std::clamp(afterIndex + 1, 0, static_cast<int>(m_impl->document.blocks.size()));
    m_impl->document.blocks.insert(m_impl->document.blocks.begin() + position, std::move(block));
    m_impl->syncMarkdownFromDocument(); m_impl->rebuild(); m_impl->markDirty();
    return true;
}
bool RichComposerController::addCodeBlock(const QString& language)
{
    if (m_impl->document.blocks.size() >= farcaster::domain::rich::MaxDocumentBlocks) return false;
    auto canonical = farcaster::application::syntax::LanguageRegistry::canonicalId(
        language.toUtf8().toStdString());
    if (canonical.empty()) canonical = "auto";
    CodeBlock code{.language = canonical};
    code.detectedLanguage = canonical == "auto" ? std::string{} : canonical;
    code.displayLanguage = code.detectedLanguage.empty()
        ? std::string{}
        : farcaster::application::syntax::LanguageRegistry::displayName(code.detectedLanguage);
    RichBlock block{
        .id = farcaster::domain::rich::makeBlockId(m_impl->revision + 1U, "code"),
        .kind = BlockKind::CodeBlock,
        .payload = std::move(code),
    };
    m_impl->pushHistory();
    m_impl->document.blocks.push_back(std::move(block));
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    return true;
}
bool RichComposerController::removeBlock(const int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_impl->document.blocks.size()) return false;
    m_impl->pushHistory();
    m_impl->document.blocks.erase(m_impl->document.blocks.begin() + index);
    if (m_impl->document.blocks.empty()) {
        m_impl->document.blocks.push_back(RichBlock{.id = farcaster::domain::rich::makeBlockId(m_impl->revision + 1U, "paragraph"), .kind = BlockKind::Paragraph, .payload = TextBlock{}});
    }
    m_impl->syncMarkdownFromDocument(); m_impl->rebuild(); m_impl->markDirty();
    return true;
}
bool RichComposerController::duplicateBlock(const int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_impl->document.blocks.size() || m_impl->document.blocks.size() >= farcaster::domain::rich::MaxDocumentBlocks) return false;
    m_impl->pushHistory();
    auto block = m_impl->document.blocks[static_cast<std::size_t>(index)];
    block.id = farcaster::domain::rich::makeBlockId(m_impl->revision + 1U, block.id);
    m_impl->document.blocks.insert(m_impl->document.blocks.begin() + index + 1, std::move(block));
    m_impl->syncMarkdownFromDocument(); m_impl->rebuild(); m_impl->markDirty();
    return true;
}
bool RichComposerController::moveBlock(const int from, const int to)
{
    const auto count = static_cast<int>(m_impl->document.blocks.size());
    if (from < 0 || to < 0 || from >= count || to >= count || from == to) return false;
    m_impl->pushHistory();
    auto block = std::move(m_impl->document.blocks[static_cast<std::size_t>(from)]);
    m_impl->document.blocks.erase(m_impl->document.blocks.begin() + from);
    m_impl->document.blocks.insert(m_impl->document.blocks.begin() + to, std::move(block));
    m_impl->syncMarkdownFromDocument(); m_impl->rebuild(); m_impl->markDirty();
    return true;
}

bool RichComposerController::applyInlineStyle(
    const int blockIndex, const int selectionStart, const int selectionLength, const QString& style)
{
    if (blockIndex < 0 || static_cast<std::size_t>(blockIndex) >= m_impl->document.blocks.size() || selectionLength <= 0) return false;
    auto& block = m_impl->document.blocks[static_cast<std::size_t>(blockIndex)];
    auto* inlines = editableInlines(block);
    if (!inlines) return false;
    const QString plain = inlinesPlainText(*inlines);
    if (selectionStart < 0 || selectionStart + selectionLength > plain.size()) return false;
    InlineKind kind = InlineKind::Text;
    if (style == QStringLiteral("strong")) kind = InlineKind::Strong;
    else if (style == QStringLiteral("emphasis")) kind = InlineKind::Emphasis;
    else if (style == QStringLiteral("inline-code")) kind = InlineKind::InlineCode;
    else return false;
    m_impl->pushHistory();
    std::vector<RichInline> next{};
    const auto before = plain.left(selectionStart);
    const auto selected = plain.mid(selectionStart, selectionLength);
    const auto after = plain.mid(selectionStart + selectionLength);
    if (!before.isEmpty()) next.push_back(RichInline{.kind = InlineKind::Text, .text = before.toUtf8().toStdString()});
    next.push_back(RichInline{.kind = kind, .text = selected.toUtf8().toStdString()});
    if (!after.isEmpty()) next.push_back(RichInline{.kind = InlineKind::Text, .text = after.toUtf8().toStdString()});
    *inlines = std::move(next);
    m_impl->syncMarkdownFromDocument(); m_impl->rebuild(); m_impl->markDirty();
    return true;
}

bool RichComposerController::applyLink(
    const int blockIndex, const int selectionStart, const int selectionLength, const QString& target)
{
    if (blockIndex < 0 || static_cast<std::size_t>(blockIndex) >= m_impl->document.blocks.size()
        || selectionLength <= 0) {
        return false;
    }
    const auto safeTarget = farcaster::application::rich::safeLinkTarget(target.toUtf8().toStdString());
    if (!safeTarget) return false;
    auto& block = m_impl->document.blocks[static_cast<std::size_t>(blockIndex)];
    auto* inlines = editableInlines(block);
    if (!inlines) return false;
    const QString plain = inlinesPlainText(*inlines);
    if (selectionStart < 0 || selectionStart + selectionLength > plain.size()) return false;

    m_impl->pushHistory();
    const auto before = plain.left(selectionStart);
    const auto selected = plain.mid(selectionStart, selectionLength);
    const auto after = plain.mid(selectionStart + selectionLength);
    std::vector<RichInline> next{};
    if (!before.isEmpty()) next.push_back(RichInline{.kind = InlineKind::Text, .text = before.toUtf8().toStdString()});
    next.push_back(RichInline{
        .kind = InlineKind::Link,
        .text = selected.toUtf8().toStdString(),
        .target = *safeTarget,
    });
    if (!after.isEmpty()) next.push_back(RichInline{.kind = InlineKind::Text, .text = after.toUtf8().toStdString()});
    *inlines = std::move(next);
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    return true;
}

bool RichComposerController::insertEmbed(const QString& url, const QString& title, const int afterIndex)
{
    const auto target = farcaster::application::rich::safeLinkTarget(url.toUtf8().toStdString());
    if (!target || m_impl->document.blocks.size() >= farcaster::domain::rich::MaxDocumentBlocks) return false;
    RichBlock block{
        .id = farcaster::domain::rich::makeBlockId(m_impl->revision + 1U, "embed"),
        .kind = BlockKind::Embed,
        .payload = farcaster::domain::rich::EmbedBlock{
            .url = *target,
            .title = title.toUtf8().toStdString(),
        },
    };
    m_impl->pushHistory();
    const auto position = afterIndex < 0
        ? static_cast<int>(m_impl->document.blocks.size())
        : std::clamp(afterIndex + 1, 0, static_cast<int>(m_impl->document.blocks.size()));
    m_impl->document.blocks.insert(m_impl->document.blocks.begin() + position, std::move(block));
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    return true;
}

bool RichComposerController::insertMedia(
    const QString& url, const QString& mediaType, const QString& altText, const int afterIndex)
{
    const auto target = farcaster::application::rich::safeLinkTarget(url.toUtf8().toStdString());
    if (!target || m_impl->document.blocks.size() >= farcaster::domain::rich::MaxDocumentBlocks) return false;
    RichBlock block{
        .id = farcaster::domain::rich::makeBlockId(m_impl->revision + 1U, "media"),
        .kind = BlockKind::Media,
        .payload = farcaster::domain::rich::MediaBlock{
            .url = *target,
            .mediaType = mediaType.toUtf8().toStdString(),
            .altText = altText.toUtf8().toStdString(),
        },
    };
    m_impl->pushHistory();
    const auto position = afterIndex < 0
        ? static_cast<int>(m_impl->document.blocks.size())
        : std::clamp(afterIndex + 1, 0, static_cast<int>(m_impl->document.blocks.size()));
    m_impl->document.blocks.insert(m_impl->document.blocks.begin() + position, std::move(block));
    m_impl->syncMarkdownFromDocument();
    m_impl->rebuild();
    m_impl->markDirty();
    return true;
}

bool RichComposerController::resolveMention(
    const int blockIndex, const int selectionStart, const int selectionLength, const qulonglong fid,
    const QString& username, const QString& displayName)
{
    if (fid == 0U || blockIndex < 0 || static_cast<std::size_t>(blockIndex) >= m_impl->document.blocks.size() || selectionLength <= 0) return false;
    auto& block = m_impl->document.blocks[static_cast<std::size_t>(blockIndex)];
    auto* inlines = editableInlines(block);
    if (!inlines) return false;
    const QString plain = inlinesPlainText(*inlines);
    if (selectionStart < 0 || selectionStart + selectionLength > plain.size()) return false;
    m_impl->pushHistory();
    const auto before = plain.left(selectionStart);
    const auto after = plain.mid(selectionStart + selectionLength);
    std::vector<RichInline> next{};
    if (!before.isEmpty()) next.push_back(RichInline{.kind = InlineKind::Text, .text = before.toUtf8().toStdString()});
    next.push_back(RichInline{
        .kind = InlineKind::Mention,
        .text = QStringLiteral("@%1").arg(username).toUtf8().toStdString(),
        .fid = farcaster::domain::identity::Fid{static_cast<std::uint64_t>(fid)},
        .username = username.toUtf8().toStdString(),
        .displayName = displayName.toUtf8().toStdString(),
    });
    if (!after.isEmpty()) next.push_back(RichInline{.kind = InlineKind::Text, .text = after.toUtf8().toStdString()});
    *inlines = std::move(next);
    m_impl->syncMarkdownFromDocument(); m_impl->rebuild(); m_impl->markDirty();
    return true;
}

QVariantList RichComposerController::highlightTokens(const QString& source, const QString& language) const
{
    QVariantList result{};
    const auto utf8 = source.toUtf8();
    for (const auto& token : m_impl->highlighter.highlight(utf8.toStdString(), language.toUtf8().toStdString())) {
        const auto prefix = QString::fromUtf8(utf8.constData(), static_cast<qsizetype>(token.start));
        const auto body = QString::fromUtf8(utf8.constData() + token.start, static_cast<qsizetype>(token.length));
        result.push_back(QVariantMap{
            {QStringLiteral("start"), prefix.size()},
            {QStringLiteral("length"), body.size()},
            {QStringLiteral("kind"), QString::fromUtf8(farcaster::application::syntax::tokenKindName(token.kind))},
        });
    }
    return result;
}
QString RichComposerController::detectLanguage(const QString& source) const
{
    return QString::fromUtf8(m_impl->highlighter.detectLanguage(source.toUtf8().toStdString()));
}

void RichComposerController::attachSyntaxDocument(
    QObject* quickTextDocument, const QString& language, const QVariantMap& semanticPalette)
{
    auto* quickDocument = qobject_cast<QQuickTextDocument*>(quickTextDocument);
    if (!quickDocument || !quickDocument->textDocument()) return;
    auto* document = quickDocument->textDocument();

    std::erase_if(m_impl->documentHighlighters, [](const auto& highlighter) {
        return highlighter->document() == nullptr;
    });
    for (auto& highlighter : m_impl->documentHighlighters) {
        if (highlighter->document() == document) {
            highlighter->configure(language, semanticPalette);
            return;
        }
    }

    auto highlighter = std::make_unique<NativeSyntaxDocumentHighlighter>(document, m_impl->highlighter);
    highlighter->configure(language, semanticPalette);
    m_impl->documentHighlighters.push_back(std::move(highlighter));
}

QVariantList RichComposerController::richBlocksForCast(const QString& castId) const
{
    const auto key = QString::fromLatin1(SidecarPrefix) + castId.trimmed();
    const auto bytes = m_impl->settings.value(key).toByteArray();
    QJsonParseError metadataError{};
    const auto metadata = QJsonDocument::fromJson(bytes, &metadataError);
    if (metadataError.error != QJsonParseError::NoError || !metadata.isObject()) return {};
    const auto storedHash = metadata.object().value(QStringLiteral("castHash")).toString();
    if (!storedHash.isEmpty() && storedHash != castId.trimmed()) return {};
    const auto stored = documentFromJson(bytes);
    if (!stored) return {};
    QVariantList result{};
    for (const auto& block : stored->first.blocks) result.push_back(blockProjection(block));
    return result;
}
void RichComposerController::commitPublishedCast(const QString& castId)
{
    const auto canonical = castId.trimmed();
    if (canonical.isEmpty() || farcaster::domain::rich::empty(m_impl->document)) return;
    m_impl->settings.setValue(QString::fromLatin1(SidecarPrefix) + canonical, documentToJson(m_impl->document, m_impl->markdown, canonical));
    m_impl->settings.remove(QString::fromLatin1(DraftKey));
    m_impl->settings.sync();
    m_impl->dirty = false;
    m_impl->saveStatusText = qtTrId("genycaster.composer.published_metadata_saved");
    emit saveStateChanged();
}
