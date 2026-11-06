/*!
 * @file        ui/render/scene_graph_superellipse_item.hpp
 * @brief       GPU scene-graph continuous-corner surface.
 *
 * @details
 * Defines the native Qt Quick superellipse component for reusable,
 * responsive and consistently shaped application surfaces.
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
#include <QtQml/qqmlregistration.h>

/**
 * @brief Scene-graph-native continuous-corner surface used by the Genycaster visual system.
 *
 * Fill, border and edge antialiasing are emitted as retained QSG triangle geometry. This
 * avoids PathSvg string construction and keeps the project's core surface primitive on the
 * same GPU-backed rendering path as the rest of Qt Quick.
 */
class SceneGraphSuperellipseItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(SceneGraphSuperellipse)

    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY borderWidthChanged)
    Q_PROPERTY(qreal curveDepth READ curveDepth WRITE setCurveDepth NOTIFY curveDepthChanged)
    Q_PROPERTY(qreal exponent READ exponent WRITE setExponent NOTIFY exponentChanged)
    Q_PROPERTY(qreal qualityScale READ qualityScale WRITE setQualityScale NOTIFY qualityScaleChanged)
    Q_PROPERTY(bool curveTopLeft READ curveTopLeft WRITE setCurveTopLeft NOTIFY curveTopLeftChanged)
    Q_PROPERTY(bool curveTopRight READ curveTopRight WRITE setCurveTopRight NOTIFY curveTopRightChanged)
    Q_PROPERTY(bool curveBottomRight READ curveBottomRight WRITE setCurveBottomRight NOTIFY curveBottomRightChanged)
    Q_PROPERTY(bool curveBottomLeft READ curveBottomLeft WRITE setCurveBottomLeft NOTIFY curveBottomLeftChanged)

public:
    /** @brief Constructs a transparent continuous-corner scene-graph surface. */
    explicit SceneGraphSuperellipseItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QColor fillColor() const;
    [[nodiscard]] QColor borderColor() const;
    [[nodiscard]] qreal borderWidth() const noexcept;
    [[nodiscard]] qreal curveDepth() const noexcept;
    [[nodiscard]] qreal exponent() const noexcept;
    [[nodiscard]] qreal qualityScale() const noexcept;
    [[nodiscard]] bool curveTopLeft() const noexcept;
    [[nodiscard]] bool curveTopRight() const noexcept;
    [[nodiscard]] bool curveBottomRight() const noexcept;
    [[nodiscard]] bool curveBottomLeft() const noexcept;

    void setFillColor(const QColor& value);
    void setBorderColor(const QColor& value);
    void setBorderWidth(qreal value);
    void setCurveDepth(qreal value);
    void setExponent(qreal value);
    void setQualityScale(qreal value);
    void setCurveTopLeft(bool value);
    void setCurveTopRight(bool value);
    void setCurveBottomRight(bool value);
    void setCurveBottomLeft(bool value);

    /** @brief Performs hit testing against the same mathematical continuous-corner boundary. */
    [[nodiscard]] bool contains(const QPointF& point) const override;

signals:
    void fillColorChanged();
    void borderColorChanged();
    void borderWidthChanged();
    void curveDepthChanged();
    void exponentChanged();
    void qualityScaleChanged();
    void curveTopLeftChanged();
    void curveTopRightChanged();
    void curveBottomRightChanged();
    void curveBottomLeftChanged();

protected:
    /** @brief Synchronizes surface state into render-thread-owned QSG geometry. */
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void scheduleUpdate();

    QColor m_fillColor{Qt::transparent};
    QColor m_borderColor{Qt::transparent};
    qreal m_borderWidth{};
    qreal m_curveDepth{12.0};
    qreal m_exponent{4.0};
    qreal m_qualityScale{1.0};
    bool m_curveTopLeft{true};
    bool m_curveTopRight{true};
    bool m_curveBottomRight{true};
    bool m_curveBottomLeft{true};
};
