#include "rich_inline_text_item.hpp"
#include <QSet>

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QLineF>
#include <QStyleHints>
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
#include <QTouchEvent>
#include <QTextObjectInterface>
#include <QAbstractTextDocumentLayout>

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
    QSet<int> animatedPositions{};
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
        const int position,
        const QTextFormat& format) override
    {
        const QString sequence = format.property(EmojiSequenceProperty).toString();
        if (sequence.isEmpty() || animatedPositions.contains(position)) {
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
        QTextOption option{};
        // Narrow feed cards can contain long prices, hashes, joined metrics
        // and mentions without whitespace. WordWrap lets those words paint
        // outside the allotted QTextDocument width; always allow an
        // emergency character-boundary break without altering the text.
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        document.setDefaultTextOption(option);
        document.setDocumentMargin(0.0);
        recomputeLayout(0.0);
    }

    void rebuild()
    {
        // QTextCursor selections from a recycled cast are never retained.
        selection.clearSelection();
        buildDocument();
        recomputeLayout(owner.width());
        emit owner.selectionChanged();
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
        const qreal em = QFontMetricsF(document.defaultFont()).height();
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            const QTextLayout* blockLayout = block.layout();
            const qreal blockTop = layout->blockBoundingRect(block).top();
            // Every emoji cluster is one object-replacement character with its
            // own format range; locate its line through the range position.
            // Use QTextBlock::textFormats() (per-character document formats);
            // QTextLayout::formats() only returns manually-set additional
            // formats and is empty for QTextDocument content.
            const auto ranges = block.textFormats();
            for (const auto& range : ranges) {
                if (range.format.objectType() != EmojiObjectType) {
                    continue;
                }
                for (int position = range.start; position < range.start + range.length; ++position) {
                    for (int lineIndex = 0; lineIndex < blockLayout->lineCount(); ++lineIndex) {
                        const QTextLine line = blockLayout->lineAt(lineIndex);
                        const int start = line.textStart();
                        const int length = line.textLength();
                        if (position < start || position >= start + length) {
                            continue;
                        }
                        const qreal x0 = line.cursorToX(position);
                        const qreal x1 = line.cursorToX(position + 1);
                        const qreal width = qMax(qAbs(x1 - x0), 1.0);
                        const qreal storedHeight = range.format.property(EmojiWidthProperty).toDouble();
                        const qreal height = storedHeight > 0.0 ? storedHeight : em;
                        const qreal lineTop = blockTop + line.y();
                        const qreal lineBottom = lineTop + line.height();
                        const qreal baseline = lineTop + line.ascent();
                        const qreal preferredY = baseline - height;
                        const qreal y = qBound(lineTop, preferredY, qMax(lineTop, lineBottom - height));
                        rects.push_back(QVariantMap{
                            {QStringLiteral("sequence"), range.format.property(EmojiSequenceProperty).toString()},
                            {QStringLiteral("position"), block.position() + position},
                            {QStringLiteral("x"), layout->blockBoundingRect(block).left() + qMin(x0, x1)},
                            {QStringLiteral("y"), y},
                            {QStringLiteral("width"), width},
                            {QStringLiteral("height"), height},
                        });
                        break;
                    }
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
            // QML exposes line height as a multiplier (1.55 == 155%), while
            // QTextBlockFormat::ProportionalHeight expects an integer-like
            // percentage. Accept an already-percent value as a boundary aid.
            const qreal proportionalHeight = lineHeight <= 10.0 ? lineHeight * 100.0 : lineHeight;
            blockFormat.setLineHeight(proportionalHeight, QTextBlockFormat::ProportionalHeight);
        }
        if (alignRight) {
            blockFormat.setAlignment(Qt::AlignRight);
        }
        cursor.setBlockFormat(blockFormat);
        for (const auto& entry : inlines) {
            insertSpan(cursor, entry);
        }
        cursor.endEditBlock();
        // Newline characters in plain spans split the document into further
        // blocks; carry the paragraph format (line height, alignment) onto all
        // of them so RTL multi-line casts stay right-aligned throughout.
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            QTextCursor blockCursor{block};
            blockCursor.setBlockFormat(blockFormat);
        }
        document.setDefaultFont(effectiveFont());
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
            base.setFontFamilies({codeFontFamily});
            base.setForeground(codeColor);
            base.setBackground(codeBackground);
        } else if (kind == QLatin1String("link")) {
            base.setForeground(linkColor);
            base.setAnchor(true);
            base.setAnchorHref(map.value(QStringLiteral("target")).toString());
            base.setFontUnderline(false);
        } else if (kind == QLatin1String("mention")) {
            base.setForeground(linkColor);
            const QString username = map.value(QStringLiteral("username")).toString();
            if (!username.isEmpty()) {
                insertTextWithEmojiObjects(cursor, QStringLiteral("@") + username, base);
                return;
            }
        }

        // Optional per-span color override; the feed fallback path uses it to
        // keep channel/hashtag tokens accent-colored like the HTML renderer.
        const QString colorText = map.value(QStringLiteral("color")).toString();
        if (!colorText.isEmpty()) {
            const QColor color{colorText};
            if (color.isValid()) {
                base.setForeground(color);
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
            emojiFormat.setFontUnderline(false);
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
    QTextCursor selection{&document};
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
    setAcceptTouchEvents(true);
    setFlag(ItemHasContents, true);
    // Handle actual touch points as well as synthesized mouse events here.
    // A separate QML TapHandler used to compete for the same touch grab.
    m_pressHoldTimer.setSingleShot(true);
    m_pressHoldTimer.setInterval(460);
    connect(&m_pressHoldTimer, &QTimer::timeout, this, [this] {
        if (m_leftPressed && !m_dragSelecting)
            selectWordAt(m_pressPosition.x(), m_pressPosition.y());
    });
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

void RichInlineTextItem::setEmojiAnimated(const int position, const bool animated)
{
    if (animated) {
        m_impl->handler.animatedPositions.insert(position);
    } else {
        m_impl->handler.animatedPositions.remove(position);
    }
    update();
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

QPointF RichInlineTextItem::selectionMenuPosition() const noexcept
{
    return m_selectionMenuPosition;
}

QRectF RichInlineTextItem::boundaryRect(const int position) const
{
    const int clamped = qBound(0, position,
        qMax(0, m_impl->document.characterCount() - 1));
    const QTextBlock block = m_impl->document.findBlock(clamped);
    if (!block.isValid() || !block.layout()) return {};
    const QTextLayout* layout = block.layout();
    if (layout->lineCount() < 1) return {};
    const int local = clamped - block.position();
    QTextLine line = layout->lineForTextPosition(local);
    if (!line.isValid()) line = layout->lineAt(layout->lineCount() - 1);
    const QRectF blockRect = m_impl->document.documentLayout()->blockBoundingRect(block);
    return QRectF(blockRect.left() + line.cursorToX(local),
                  blockRect.top() + line.y(), 2, line.height());
}

QRectF RichInlineTextItem::selectionStartRect() const
{
    return m_impl->selection.hasSelection()
        ? boundaryRect(m_impl->selection.selectionStart()) : QRectF{};
}

QRectF RichInlineTextItem::selectionEndRect() const
{
    return m_impl->selection.hasSelection()
        ? boundaryRect(m_impl->selection.selectionEnd()) : QRectF{};
}

void RichInlineTextItem::dragSelectionBoundary(
    const bool leading, const qreal x, const qreal y)
{
    if (!m_impl->selection.hasSelection()) return;
    const int hit = m_impl->document.documentLayout()->hitTest(
        QPointF{x, y}, Qt::FuzzyHit);
    if (hit < 0) return;
    const int first = m_impl->selection.selectionStart();
    const int last = m_impl->selection.selectionEnd();
    QTextCursor cursor{&m_impl->document};
    if (leading) {
        cursor.setPosition(last);
        cursor.setPosition(qBound(0, hit, last - 1), QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(first);
        cursor.setPosition(qBound(first + 1, hit,
            qMax(first + 1, m_impl->document.characterCount() - 1)),
            QTextCursor::KeepAnchor);
    }
    if (!cursor.hasSelection()) return;
    m_impl->selection = cursor;
    m_selectionMenuPosition = QPointF{x, y};
    update();
    emit selectionChanged();
}

QString RichInlineTextItem::selectedText() const
{
    if (!m_impl->selection.hasSelection()) return {};
    QString text;
    const int start = m_impl->selection.selectionStart();
    const int end = m_impl->selection.selectionEnd();
    // QTextDocument uses U+FFFC for emoji object glyphs. Preserve their
    // original Unicode in the clipboard instead of copying replacement boxes.
    for (int i = start; i < end; ++i) {
        QTextCursor letter{&m_impl->document};
        letter.setPosition(i);
        letter.setPosition(i + 1, QTextCursor::KeepAnchor);
        const QString character = letter.selectedText();
        if (character == QString{QChar{0xFFFC}}) {
            const QString emoji = letter.charFormat().property(EmojiSequenceProperty).toString();
            text += emoji.isEmpty() ? character : emoji;
        } else if (character == QString{QChar{0x2029}}) {
            text += QLatin1Char('\n');
        } else {
            text += character;
        }
    }
    return text;
}

void RichInlineTextItem::clearSelection()
{
    if (!m_impl->selection.hasSelection()) return;
    m_impl->selection.clearSelection();
    update();
    emit selectionChanged();
}

void RichInlineTextItem::selectWordAt(const qreal x, const qreal y)
{
    QTextCursor cursor{&m_impl->document};
    const int pos = m_impl->document.documentLayout()->hitTest(QPointF{x,y}, Qt::FuzzyHit);
    if (pos < 0) return;
    cursor.setPosition(pos);
    cursor.select(QTextCursor::WordUnderCursor);
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::LineUnderCursor);
    m_impl->selection = cursor;
    m_selectionMenuPosition = QPointF{x, y};
    m_selectedWordStart = cursor.selectionStart();
    m_selectedWordEnd = cursor.selectionEnd();
    m_longPressSelected = true;
    // Once the user has held the text, subsequent movement belongs to text
    // selection, not the parent Flickable. Keep the grab only for this gesture.
    if (m_leftPressed) {
        setKeepMouseGrab(true);
        setKeepTouchGrab(true);
    }
    update();
    emit selectionChanged();
    // Show the contextual action after finger release, not underneath
    // the finger while it is still extending the selection.
}

void RichInlineTextItem::selectAllText()
{
    m_impl->selection.select(QTextCursor::Document);
    m_selectionMenuPosition = QPointF{width() * 0.5, qMin(22.0, height() * 0.5)};
    update();
    emit selectionChanged();
    emit selectionCompleted();
}

void RichInlineTextItem::paint(QPainter* painter)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    QAbstractTextDocumentLayout::PaintContext context{};
    if (m_impl->selection.hasSelection()) {
        QAbstractTextDocumentLayout::Selection selection{};
        selection.cursor = m_impl->selection;
        selection.format.setBackground(QColor{93, 119, 208, 125});
        context.selections.push_back(selection);
    }
    m_impl->document.documentLayout()->draw(painter, context);
    painter->restore();
}

void RichInlineTextItem::beginSelectionGesture(const QPointF& position)
{
    m_pressPosition = position;
    m_leftPressed = true;
    m_dragSelecting = false;
    m_longPressSelected = false;
    m_dragAnchor = m_impl->document.documentLayout()->hitTest(
        position, Qt::FuzzyHit);
    m_selectedWordStart = -1;
    m_selectedWordEnd = -1;
    if (m_dragAnchor < 0) return;

    // A second gesture on an existing selection can extend either boundary,
    // without another long press. Fix the opposite end as the drag anchor.
    if (m_impl->selection.hasSelection()
        && m_dragAnchor >= m_impl->selection.selectionStart()
        && m_dragAnchor <= m_impl->selection.selectionEnd()) {
        const int start = m_impl->selection.selectionStart();
        const int end = m_impl->selection.selectionEnd();
        m_dragAnchor = (m_dragAnchor - start <= end - m_dragAnchor) ? end : start;
        m_longPressSelected = true;
        setKeepMouseGrab(true);
        setKeepTouchGrab(true);
    } else {
        m_pressHoldTimer.start();
    }
}

void RichInlineTextItem::extendSelectionGesture(
    const QPointF& position, const bool touch)
{
    if (!m_leftPressed || m_dragAnchor < 0) return;
    if (!m_dragSelecting
        && QLineF{m_pressPosition, position}.length()
             <= QGuiApplication::styleHints()->startDragDistance())
        return;
    m_pressHoldTimer.stop();
    // Before a hold, a swipe of the phone belongs to Flickable. Desktop mouse
    // dragging selects as usual. After a hold, both devices extend selection.
    if (!m_longPressSelected && touch) return;
    m_dragSelecting = true;
    setKeepMouseGrab(true);
    setKeepTouchGrab(true);
    const int end = m_impl->document.documentLayout()->hitTest(
        position, Qt::FuzzyHit);
    if (end < 0) return;
    // Keep the initially selected whole word, regardless of drag direction.
    int anchor = m_dragAnchor;
    if (m_selectedWordStart >= 0 && m_selectedWordEnd >= 0)
        anchor = end < m_selectedWordStart ? m_selectedWordEnd
                                           : m_selectedWordStart;
    m_impl->selection.setPosition(anchor);
    m_impl->selection.setPosition(end, QTextCursor::KeepAnchor);
    m_selectionMenuPosition = position;
    update();
    emit selectionChanged();
}

void RichInlineTextItem::finishSelectionGesture(const QPointF& position)
{
    m_pressHoldTimer.stop();
    const bool wasSelecting = m_dragSelecting || m_longPressSelected;
    const bool shortTap = m_leftPressed
        && QLineF{m_pressPosition, position}.length()
                <= QGuiApplication::styleHints()->startDragDistance();
    cancelSelectionGesture();
    if (wasSelecting) {
        if (m_impl->selection.hasSelection()) {
            m_selectionMenuPosition = position;
            emit selectionChanged();
            emit selectionCompleted();
        }
        return;
    }
    if (!shortTap) return;
    if (m_impl->selection.hasSelection()) {
        clearSelection();
        return;
    }
    const QString anchor = m_impl->anchorAt(position);
    if (!anchor.isEmpty()) emit linkActivated(anchor);
    else emit plainTextActivated();
}

void RichInlineTextItem::cancelSelectionGesture()
{
    m_pressHoldTimer.stop();
    m_leftPressed = false;
    m_dragSelecting = false;
    m_longPressSelected = false;
    m_dragAnchor = -1;
    m_selectedWordStart = -1;
    m_selectedWordEnd = -1;
    setKeepMouseGrab(false);
    setKeepTouchGrab(false);
}

void RichInlineTextItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    beginSelectionGesture(event->position());
    event->accept();
}

void RichInlineTextItem::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_leftPressed) {
        event->ignore();
        return;
    }
    const bool touch = event->source() != Qt::MouseEventNotSynthesized;
    extendSelectionGesture(event->position(), touch);
    if (touch && !m_longPressSelected && !m_dragSelecting
        && QLineF{m_pressPosition, event->position()}.length()
            > QGuiApplication::styleHints()->startDragDistance()) {
        // Allow the surrounding feed to steal ordinary swipe-to-scroll.
        event->ignore();
        return;
    }
    event->accept();
}

void RichInlineTextItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    finishSelectionGesture(event->position());
    event->accept();
}

void RichInlineTextItem::mouseUngrabEvent()
{
    cancelSelectionGesture();
}

void RichInlineTextItem::touchEvent(QTouchEvent* event)
{
    if (event->points().isEmpty()) {
        event->ignore();
        return;
    }
    const QPointF position = event->points().front().position();
    switch (event->type()) {
    case QEvent::TouchBegin:
        beginSelectionGesture(position);
        event->accept();
        break;
    case QEvent::TouchUpdate:
        if (!m_leftPressed) {
            event->ignore();
            return;
        }
        extendSelectionGesture(position, true);
        // Before a hold, touch-drag remains a normal feed scroll.
        if (!m_longPressSelected && !m_dragSelecting
            && QLineF{m_pressPosition, position}.length()
                > QGuiApplication::styleHints()->startDragDistance()) {
            event->ignore();
            return;
        }
        event->accept();
        break;
    case QEvent::TouchEnd:
        finishSelectionGesture(position);
        event->accept();
        break;
    case QEvent::TouchCancel:
        cancelSelectionGesture();
        event->accept();
        break;
    default:
        event->ignore();
        break;
    }
}

#include "rich_inline_text_item.moc"
