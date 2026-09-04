#include "rich_inline_text_item.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QMetaType>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QTextObjectInterface>

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

import farcaster.application.emoji.unicode_normalizer;

namespace {

constexpr int EmojiObjectType = QTextFormat::UserObject + 1;
constexpr int EmojiSequenceProperty = QTextFormat::UserProperty + 1;
constexpr int EmojiWidthProperty = QTextFormat::UserProperty + 2;

/**
 * @brief QTextObjectInterface that paints emoji inline objects.
 *
 * The object replacement character participates in normal text layout; the
 * handler draws the actual Unicode glyph (platform color-emoji fallback) so
 * the box is never empty, even before any animation overlay appears.
 */
class EmojiObjectHandler final : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    [[nodiscard]] QSizeF intrinsicSize(
        QTextDocument* document,
        const int /*position*/,
        const QTextFormat& format) override
    {
        const qreal em = QFontMetricsF(document->defaultFont()).height();
        const qreal storedWidth = format.property(EmojiWidthProperty).toDouble();
        const qreal side = storedWidth > 0.0 ? storedWidth : em;
        return QSizeF{side, em};
    }

    void drawObject(
        QPainter* painter,
        const QRectF& rect,
        QTextDocument* document,
        const int /*position*/,
        const QTextFormat& format) override
    {
        const QString sequence = format.property(EmojiSequenceProperty).toString();
        if (sequence.isEmpty()) {
            return;
        }
        painter->save();
        QFont font = document->defaultFont();
        font.setPixelSize(qMax(1, qRound(rect.height())));
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignCenter, sequence);
        painter->restore();
    }
};

} // namespace

class RichInlineTextItem::Impl final {
public:
    explicit Impl(RichInlineTextItem& owner)
        : owner(owner)
    {
        document.setDefaultFont(QFont{});
        document.documentLayout()->registerHandler(EmojiObjectType, &handler);
        if (const auto* screen = QGuiApplication::primaryScreen()) {
            document.documentLayout()->setPaintDevice(screen);
        }
        QTextOption option{};
        option.setWrapMode(QTextOption::WordWrap);
        document.setDefaultTextOption(option);
        document.setDocumentMargin(0.0);
        recomputeLayout(0.0);
    }

    void rebuild()
    {
        buildDocument();
        recomputeLayout(owner.width());
        owner.update();
    }

    void recomputeLayout(const qreal wrapWidth)
    {
        if (wrapWidth > 0.0) {
            document.setTextWidth(wrapWidth);
        }
        const qreal height = document.size().height();
        if (!qFuzzyCompare(height, contentHeight)) {
            contentHeight = height;
            emit owner.contentHeightChanged();
        }
        collectEmojiRects();
    }

    void collectEmojiRects()
    {
        QVariantList rects{};
        const auto* layout = document.documentLayout();
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            const QTextLayout* blockLayout = block.layout();
            const qreal blockTop = layout->blockBoundingRect(block).top();
            for (int lineIndex = 0; lineIndex < blockLayout->lineCount(); ++lineIndex) {
                const QTextLine line = blockLayout->lineAt(lineIndex);
                const int start = line.textStart();
                const int length = line.textLength();
                for (int index = start; index < start + length; ++index) {
                    const QTextCharFormat format = blockLayout->formatAt(index);
                    if (format.objectType() != EmojiObjectType) {
                        continue;
                    }
                    const qreal x0 = line.cursorToX(index);
                    const qreal x1 = line.cursorToX(index + 1);
                    const qreal em = QFontMetricsF(document.defaultFont()).height();
                    const qreal width = qMax(x1 - x0, 1.0);
                    const qreal y = blockTop + line.y() + (line.height() - em) / 2.0;
                    rects.push_back(QVariantMap{
                        {QStringLiteral("sequence"), format.property(EmojiSequenceProperty).toString()},
                        {QStringLiteral("x"), x0},
                        {QStringLiteral("y"), y},
                        {QStringLiteral("width"), width},
                        {QStringLiteral("height"), em},
                    });
                }
            }
        }
        if (rects != emojiRects) {
            emojiRects = std::move(rects);
            emit owner.emojiRectsChanged();
        }
    }

    void buildDocument()
    {
        document.clear();
        document.documentLayout()->registerHandler(EmojiObjectType, &handler);
        QTextCursor cursor{&document};
        cursor.beginEditBlock();
        QTextBlockFormat blockFormat{};
        if (lineHeight > 0.0) {
            blockFormat.setLineHeight(lineHeight, QTextBlockFormat::ProportionalHeight);
        }
        if (alignRight) {
            blockFormat.setAlignment(Qt::AlignRight);
        }
        cursor.setBlockFormat(blockFormat);
        for (const auto& entry : inlines) {
            insertSpan(cursor, entry);
        }
        cursor.endEditBlock();
        document.setDefaultFont(effectiveFont());
        if (const auto* screen = QGuiApplication::primaryScreen()) {
            document.documentLayout()->setPaintDevice(screen);
        }
    }

    void insertSpan(QTextCursor& cursor, const QVariant& entry)
    {
        QVariantMap map{};
        if (entry.typeId() == QMetaType::QString) {
            map.insert(QStringLiteral("kind"), QStringLiteral("text"));
            map.insert(QStringLiteral("text"), entry.toString());
        } else if (entry.typeId() == QMetaType::QVariantMap) {
            map = entry.toMap();
        } else {
            return;
        }
        const QString kind = map.value(QStringLiteral("kind"), QStringLiteral("text")).toString();
        const QString text = map.value(QStringLiteral("text")).toString();
        if (text.isEmpty()) {
            return;
        }

        QTextCharFormat base = baseFormat();
        if (kind == QLatin1String("strong")) {
            base.setFontWeight(QFont::Bold);
        } else if (kind == QLatin1String("emphasis")) {
            base.setFontItalic(true);
        } else if (kind == QLatin1String("inline-code")) {
            base.setFontFamily(codeFontFamily);
            base.setForeground(codeColor);
            base.setBackground(codeBackground);
        } else if (kind == QLatin1String("link")) {
            base.setForeground(linkColor);
            base.setAnchor(true);
            base.setAnchorHref(map.value(QStringLiteral("target")).toString());
            base.setUnderline(false);
        } else if (kind == QLatin1String("mention")) {
            base.setForeground(linkColor);
            const QString username = map.value(QStringLiteral("username")).toString();
            if (!username.isEmpty()) {
                insertTextWithEmojiObjects(cursor, QStringLiteral("@") + username, base);
                return;
            }
        }

        insertTextWithEmojiObjects(cursor, text, base);
    }

    void insertTextWithEmojiObjects(
        QTextCursor& cursor,
        const QString& text,
        const QTextCharFormat& base)
    {
        const QByteArray utf8 = text.toUtf8();
        const std::string_view bytes{utf8.constData(), static_cast<std::size_t>(utf8.size())};
        const auto clusters = farcaster::application::emoji::unicode::scanEmojiClusters(bytes);
        if (clusters.empty()) {
            cursor.insertText(text, base);
            return;
        }

        const QFontMetricsF metrics{effectiveFont()};
        const qreal em = metrics.height() * emojiBoxScale;
        const qreal side = qMax(em, 1.0);

        std::size_t consumedBytes = 0U;
        for (const auto& cluster : clusters) {
            if (cluster.byteOffset > consumedBytes) {
                cursor.insertText(
                    QString::fromUtf8(
                        utf8.constData() + static_cast<qsizetype>(consumedBytes),
                        static_cast<qsizetype>(cluster.byteOffset - consumedBytes)),
                    base);
            }
            QTextCharFormat emojiFormat = base;
            emojiFormat.setObjectType(EmojiObjectType);
            emojiFormat.setProperty(EmojiSequenceProperty, QString::fromUtf8(cluster.sequence));
            emojiFormat.setProperty(EmojiWidthProperty, side);
            cursor.insertText(QString{QChar(0xFFFC)}, emojiFormat);
            consumedBytes = cluster.byteOffset + cluster.byteLength;
        }
        if (consumedBytes < bytes.size()) {
            cursor.insertText(
                QString::fromUtf8(
                    utf8.constData() + static_cast<qsizetype>(consumedBytes),
                    static_cast<qsizetype>(bytes.size() - consumedBytes)),
                base);
        }
    }

    [[nodiscard]] QFont effectiveFont() const
    {
        QFont font{fontFamily.isEmpty() ? QFont{}.family() : fontFamily};
        font.setPixelSize(qMax(1, pixelSize));
        return font;
    }

    [[nodiscard]] QTextCharFormat baseFormat() const
    {
        QTextCharFormat format{};
        format.setFont(effectiveFont());
        if (bold) {
            format.setFontWeight(QFont::Bold);
        }
        format.setForeground(textColor);
        return format;
    }

    [[nodiscard]] QString anchorAt(const QPointF& position) const
    {
        return document.documentLayout()->anchorAt(position);
    }

    RichInlineTextItem& owner;
    QTextDocument document{};
    EmojiObjectHandler handler{};

    QVariantList inlines{};
    int pixelSize{15};
    bool bold{false};
    qreal lineHeight{1.0};
    QString fontFamily{};
    QColor textColor{Qt::black};
    QColor linkColor{Qt::blue};
    QColor codeColor{Qt::darkGreen};
    QColor codeBackground{QColor{0, 0, 0, 16}};
    QString codeFontFamily{};
    bool alignRight{false};
    qreal emojiBoxScale{1.0};

    qreal contentHeight{0.0};
    QVariantList emojiRects{};
};

RichInlineTextItem::RichInlineTextItem(QQuickItem* parent)
    : QQuickPaintedItem(parent),
      m_impl(std::make_unique<Impl>(*this))
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(ItemHasContents, true);
}

RichInlineTextItem::~RichInlineTextItem() = default;

QVariantList RichInlineTextItem::inlines() const
{
    return m_impl->inlines;
}

void RichInlineTextItem::setInlines(const QVariantList& inlines)
{
    if (inlines == m_impl->inlines) {
        return;
    }
    m_impl->inlines = inlines;
    emit inlinesChanged();
    m_impl->rebuild();
}

int RichInlineTextItem::pixelSize() const noexcept
{
    return m_impl->pixelSize;
}

void RichInlineTextItem::setPixelSize(const int pixelSize)
{
    if (pixelSize == m_impl->pixelSize) {
        return;
    }
    m_impl->pixelSize = pixelSize;
    emit pixelSizeChanged();
    m_impl->rebuild();
}

bool RichInlineTextItem::bold() const noexcept
{
    return m_impl->bold;
}

void RichInlineTextItem::setBold(const bool bold)
{
    if (bold == m_impl->bold) {
        return;
    }
    m_impl->bold = bold;
    emit boldChanged();
    m_impl->rebuild();
}

qreal RichInlineTextItem::lineHeight() const noexcept
{
    return m_impl->lineHeight;
}

void RichInlineTextItem::setLineHeight(const qreal lineHeight)
{
    if (qFuzzyCompare(lineHeight, m_impl->lineHeight)) {
        return;
    }
    m_impl->lineHeight = lineHeight;
    emit lineHeightChanged();
    m_impl->rebuild();
}

QString RichInlineTextItem::fontFamily() const
{
    return m_impl->fontFamily;
}

void RichInlineTextItem::setFontFamily(const QString& fontFamily)
{
    if (fontFamily == m_impl->fontFamily) {
        return;
    }
    m_impl->fontFamily = fontFamily;
    emit fontFamilyChanged();
    m_impl->rebuild();
}

QColor RichInlineTextItem::textColor() const
{
    return m_impl->textColor;
}

void RichInlineTextItem::setTextColor(const QColor& color)
{
    if (color == m_impl->textColor) {
        return;
    }
    m_impl->textColor = color;
    emit textColorChanged();
    m_impl->rebuild();
}

QColor RichInlineTextItem::linkColor() const
{
    return m_impl->linkColor;
}

void RichInlineTextItem::setLinkColor(const QColor& color)
{
    if (color == m_impl->linkColor) {
        return;
    }
    m_impl->linkColor = color;
    emit linkColorChanged();
    m_impl->rebuild();
}

QColor RichInlineTextItem::codeColor() const
{
    return m_impl->codeColor;
}

void RichInlineTextItem::setCodeColor(const QColor& color)
{
    if (color == m_impl->codeColor) {
        return;
    }
    m_impl->codeColor = color;
    emit codeColorChanged();
    m_impl->rebuild();
}

QColor RichInlineTextItem::codeBackground() const
{
    return m_impl->codeBackground;
}

void RichInlineTextItem::setCodeBackground(const QColor& color)
{
    if (color == m_impl->codeBackground) {
        return;
    }
    m_impl->codeBackground = color;
    emit codeBackgroundChanged();
    m_impl->rebuild();
}

QString RichInlineTextItem::codeFontFamily() const
{
    return m_impl->codeFontFamily;
}

void RichInlineTextItem::setCodeFontFamily(const QString& fontFamily)
{
    if (fontFamily == m_impl->codeFontFamily) {
        return;
    }
    m_impl->codeFontFamily = fontFamily;
    emit codeFontFamilyChanged();
    m_impl->rebuild();
}

bool RichInlineTextItem::alignRight() const noexcept
{
    return m_impl->alignRight;
}

void RichInlineTextItem::setAlignRight(const bool alignRight)
{
    if (alignRight == m_impl->alignRight) {
        return;
    }
    m_impl->alignRight = alignRight;
    emit alignRightChanged();
    m_impl->rebuild();
}

qreal RichInlineTextItem::emojiBoxScale() const noexcept
{
    return m_impl->emojiBoxScale;
}

void RichInlineTextItem::setEmojiBoxScale(const qreal scale)
{
    if (qFuzzyCompare(scale, m_impl->emojiBoxScale)) {
        return;
    }
    m_impl->emojiBoxScale = scale;
    emit emojiBoxScaleChanged();
    m_impl->rebuild();
}

qreal RichInlineTextItem::contentHeight() const noexcept
{
    return m_impl->contentHeight;
}

QVariantList RichInlineTextItem::emojiRects() const
{
    return m_impl->emojiRects;
}

void RichInlineTextItem::componentComplete()
{
    QQuickPaintedItem::componentComplete();
    m_impl->rebuild();
}

void RichInlineTextItem::geometryChange(
    const QRectF& newGeometry,
    const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())) {
        m_impl->recomputeLayout(newGeometry.width());
        update();
    }
}

void RichInlineTextItem::paint(QPainter* painter)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    m_impl->document.drawContents(painter);
    painter->restore();
}

void RichInlineTextItem::mousePressEvent(QMouseEvent* event)
{
    event->accept();
}

void RichInlineTextItem::mouseReleaseEvent(QMouseEvent* event)
{
    const QString anchor = m_impl->anchorAt(event->position());
    event->accept();
    if (!anchor.isEmpty()) {
        emit linkActivated(anchor);
    }
}

#include "rich_inline_text_item.moc"