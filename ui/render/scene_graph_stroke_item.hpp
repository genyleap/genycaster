/*!
 * @file        ui/render/scene_graph_stroke_item.hpp
 * @brief       GPU scene-graph stroke component.
 *
 * @details
 * Defines a Qt Quick geometry node for smooth, theme-aware strokes
 * without relying on CPU-painted overlays.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC/QML registration requires this declaration in a .hpp file.
#pragma once

#include <QColor>
#include <QQuickItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

/**
 * @brief Hardware-accelerated anti-aliased stroke primitive for compact dynamic UI graphics.
 *
 * The item emits triangle geometry directly into the Qt Quick Scene Graph from
 * updatePaintNode(). It is intended for small high-frequency vector visuals such as
 * progress arcs, sparklines and dashed rounded drop targets. No QPainter surface,
 * Canvas texture or off-screen rasterization is used.
 *
 * The QML-facing object remains on the GUI thread. Scene-graph nodes are created and
 * mutated only from updatePaintNode(), which is the synchronization boundary documented
 * by Qt for threaded rendering.
 */
class SceneGraphStrokeItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(SceneGraphStroke)

    Q_PROPERTY(PathKind pathKind READ pathKind WRITE setPathKind NOTIFY pathKindChanged)
    Q_PROPERTY(QColor strokeColor READ strokeColor WRITE setStrokeColor NOTIFY strokeColorChanged)
    Q_PROPERTY(qreal strokeWidth READ strokeWidth WRITE setStrokeWidth NOTIFY strokeWidthChanged)
    Q_PROPERTY(QVariantList samples READ samples WRITE setSamples NOTIFY samplesChanged)
    Q_PROPERTY(qreal startAngle READ startAngle WRITE setStartAngle NOTIFY startAngleChanged)
    Q_PROPERTY(qreal sweepAngle READ sweepAngle WRITE setSweepAngle NOTIFY sweepAngleChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(qreal inset READ inset WRITE setInset NOTIFY insetChanged)
    Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(qreal dashLength READ dashLength WRITE setDashLength NOTIFY dashLengthChanged)
    Q_PROPERTY(qreal gapLength READ gapLength WRITE setGapLength NOTIFY gapLengthChanged)
    Q_PROPERTY(qreal contentPadding READ contentPadding WRITE setContentPadding NOTIFY contentPaddingChanged)
    Q_PROPERTY(qreal qualityScale READ qualityScale WRITE setQualityScale NOTIFY qualityScaleChanged)

public:
    enum class PathKind {
        Sparkline,
        Arc,
        RoundedRectangle,
    };
    Q_ENUM(PathKind)

    /** @brief Constructs an empty scene-graph stroke item. */
    explicit SceneGraphStrokeItem(QQuickItem* parent = nullptr);

    [[nodiscard]] PathKind pathKind() const noexcept;
    [[nodiscard]] QColor strokeColor() const;
    [[nodiscard]] qreal strokeWidth() const noexcept;
    [[nodiscard]] QVariantList samples() const;
    [[nodiscard]] qreal startAngle() const noexcept;
    [[nodiscard]] qreal sweepAngle() const noexcept;
    [[nodiscard]] qreal radius() const noexcept;
    [[nodiscard]] qreal inset() const noexcept;
    [[nodiscard]] qreal cornerRadius() const noexcept;
    [[nodiscard]] qreal dashLength() const noexcept;
    [[nodiscard]] qreal gapLength() const noexcept;
    [[nodiscard]] qreal contentPadding() const noexcept;
    [[nodiscard]] qreal qualityScale() const noexcept;

    void setPathKind(PathKind value);
    void setStrokeColor(const QColor& value);
    void setStrokeWidth(qreal value);
    void setSamples(const QVariantList& value);
    void setStartAngle(qreal value);
    void setSweepAngle(qreal value);
    void setRadius(qreal value);
    void setInset(qreal value);
    void setCornerRadius(qreal value);
    void setDashLength(qreal value);
    void setGapLength(qreal value);
    void setContentPadding(qreal value);
    void setQualityScale(qreal value);

signals:
    void pathKindChanged();
    void strokeColorChanged();
    void strokeWidthChanged();
    void samplesChanged();
    void startAngleChanged();
    void sweepAngleChanged();
    void radiusChanged();
    void insetChanged();
    void cornerRadiusChanged();
    void dashLengthChanged();
    void gapLengthChanged();
    void contentPaddingChanged();
    void qualityScaleChanged();

protected:
    /** @brief Synchronizes QML state into render-thread-owned scene-graph geometry. */
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void scheduleUpdate();

    PathKind m_pathKind{PathKind::Sparkline};
    QColor m_strokeColor{Qt::black};
    qreal m_strokeWidth{1.5};
    QVariantList m_samples{};
    qreal m_startAngle{-90.0};
    qreal m_sweepAngle{270.0};
    qreal m_radius{-1.0};
    qreal m_inset{};
    qreal m_cornerRadius{12.0};
    qreal m_dashLength{};
    qreal m_gapLength{};
    qreal m_contentPadding{1.0};
    qreal m_qualityScale{1.0};
};
