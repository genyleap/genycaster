// Textual compatibility boundary: Qt MOC requires this QObject declaration in
// a .hpp file. The renderer owns a QTextDocument whose layout engine positions
// emoji as real inline objects (wrapping, BiDi, alignment); animation frames
// are overlaid by QML at the exact rectangles this item reports. No emoji or
// provider logic lives here — cluster detection uses the native module.
#pragma once

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QTouchEvent>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class QTextDocument;

/**
 * @brief QML-facing rich-text renderer with inline emoji objects.
 *
 * The item converts markup spans into a QTextDocument, replacing emoji
 * clusters with U+FFFC inline objects so they participate in normal text
 * layout (line wrapping, BiDi, mixed direction, alignment). Every emoji box
 * is reported through emojiRects in item coordinates; the QML wrapper places
 * an AnimatedEmoji overlay on top, while the item itself paints the system
 * Unicode glyph as an always-correct fallback. Layout is done entirely on the
 * GUI thread; the document is immutable after layout so the scene-graph paint
 * pass can read it safely.
 */
class RichInlineTextItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList inlines READ inlines WRITE setInlines NOTIFY inlinesChanged)
    Q_PROPERTY(int pixelSize READ pixelSize WRITE setPixelSize NOTIFY pixelSizeChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY boldChanged)
    Q_PROPERTY(qreal lineHeight READ lineHeight WRITE setLineHeight NOTIFY lineHeightChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor NOTIFY linkColorChanged)
    Q_PROPERTY(QColor codeColor READ codeColor WRITE setCodeColor NOTIFY codeColorChanged)
    Q_PROPERTY(QColor codeBackground READ codeBackground WRITE setCodeBackground NOTIFY codeBackgroundChanged)
    Q_PROPERTY(QString codeFontFamily READ codeFontFamily WRITE setCodeFontFamily NOTIFY codeFontFamilyChanged)
    Q_PROPERTY(bool alignRight READ alignRight WRITE setAlignRight NOTIFY alignRightChanged)
    Q_PROPERTY(qreal emojiBoxScale READ emojiBoxScale WRITE setEmojiBoxScale NOTIFY emojiBoxScaleChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged)
    Q_PROPERTY(QVariantList emojiRects READ emojiRects NOTIFY emojiRectsChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(QPointF selectionMenuPosition READ selectionMenuPosition NOTIFY selectionChanged)

public:
    /** @brief Creates the renderer. */
    explicit RichInlineTextItem(QQuickItem* parent = nullptr);
    ~RichInlineTextItem() override;

    RichInlineTextItem(const RichInlineTextItem&) = delete;
    auto operator=(const RichInlineTextItem&) -> RichInlineTextItem& = delete;

    [[nodiscard]] QVariantList inlines() const;
    void setInlines(const QVariantList& inlines);

    [[nodiscard]] int pixelSize() const noexcept;
    void setPixelSize(int pixelSize);

    /** @brief Applies a bold weight to the whole paragraph (headings). */
    [[nodiscard]] bool bold() const noexcept;
    void setBold(bool bold);

    [[nodiscard]] qreal lineHeight() const noexcept;
    void setLineHeight(qreal lineHeight);

    [[nodiscard]] QString fontFamily() const;
    void setFontFamily(const QString& fontFamily);

    [[nodiscard]] QColor textColor() const;
    void setTextColor(const QColor& color);

    [[nodiscard]] QColor linkColor() const;
    void setLinkColor(const QColor& color);

    [[nodiscard]] QColor codeColor() const;
    void setCodeColor(const QColor& color);

    [[nodiscard]] QColor codeBackground() const;
    void setCodeBackground(const QColor& color);

    [[nodiscard]] QString codeFontFamily() const;
    void setCodeFontFamily(const QString& fontFamily);

    [[nodiscard]] bool alignRight() const noexcept;
    void setAlignRight(bool alignRight);

    [[nodiscard]] qreal emojiBoxScale() const noexcept;
    void setEmojiBoxScale(qreal scale);

    /** @brief Laid-out content height in item coordinates. */
    [[nodiscard]] qreal contentHeight() const noexcept;

    /** @brief Emoji boxes in item coordinates: [{sequence,x,y,width,height}]. */
    [[nodiscard]] QVariantList emojiRects() const;
    /** @brief Suppresses one inline glyph only while its animation replaces it; layout is unchanged. */
    Q_INVOKABLE void setEmojiAnimated(int position, bool animated);
    [[nodiscard]] QString selectedText() const;
    [[nodiscard]] QPointF selectionMenuPosition() const noexcept;
    /** Native QTextDocument hit-testing; works for bidi runs, emoji and links. */
    Q_INVOKABLE void selectWordAt(qreal x, qreal y);
    Q_INVOKABLE void selectAllText();
    Q_INVOKABLE void clearSelection();

signals:
    void inlinesChanged();
    void pixelSizeChanged();
    void boldChanged();
    void lineHeightChanged();
    void fontFamilyChanged();
    void textColorChanged();
    void linkColorChanged();
    void codeColorChanged();
    void codeBackgroundChanged();
    void codeFontFamilyChanged();
    void alignRightChanged();
    void emojiBoxScaleChanged();
    void contentHeightChanged();
    void emojiRectsChanged();
    /** @brief Emitted when a link target inside the text is activated. */
    void linkActivated(const QString& url);
    /** @brief A short left-click on ordinary text (never a hyperlink). */
    void plainTextActivated();
    void selectionChanged();
    void selectionCompleted();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void paint(QPainter* painter) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;
    void touchEvent(QTouchEvent* event) override;

private:
    class Impl;
    void beginSelectionGesture(const QPointF& position);
    void extendSelectionGesture(const QPointF& position, bool touch);
    void finishSelectionGesture(const QPointF& position);
    void cancelSelectionGesture();
    std::unique_ptr<Impl> m_impl;
    QPointF m_pressPosition{};
    QPointF m_selectionMenuPosition{};
    int m_selectedWordStart{-1};
    int m_selectedWordEnd{-1};
    bool m_leftPressed{};
    bool m_dragSelecting{};
    bool m_longPressSelected{};
    QTimer m_pressHoldTimer{};
    int m_dragAnchor{-1};
};
