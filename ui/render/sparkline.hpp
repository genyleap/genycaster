/*!
 * @file        ui/render/sparkline.hpp
 * @brief       Native market sparkline geometry and sampling.
 *
 * @details
 * Exposes bounded, deterministic market-series decimation and point geometry
 * to Qt Quick while keeping colors, fills and scene composition in QML.
 *
 * @author      Kambiz Asadzadeh
 * @since       06 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * @brief Stateless C++ sparkline geometry helper for Qt Quick.
 *
 * Computes finite, decimated and gently smoothed market points. The returned
 * QVariantMap contains `linePoints` and `areaPoints` as lists of QPointF;
 * Qt Quick owns the final Shape geometry and fill rendering.
 */
class TokenSparklineGeometry final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(TokenSparklineGeometry)
    QML_SINGLETON

public:
    explicit TokenSparklineGeometry(QObject* parent = nullptr);

    /**
     * @brief Creates one bounded series of line and filled-area coordinates.
     * @param samples Source samples (invalid or non-finite values are skipped).
     * @param width Current Qt Quick item width.
     * @param height Current Qt Quick item height.
     * @param contentPadding Item padding, in logical pixels.
     * @param strokeWidth Stroke thickness, in logical pixels.
     * @return A map with `linePoints` and `areaPoints` for PathPolyline.
     */
    Q_INVOKABLE QVariantMap compute(
        const QVariantList& samples,
        qreal width,
        qreal height,
        qreal contentPadding,
        qreal strokeWidth) const;

    /** @brief Shape-preserving cubic interpolation for large market charts.
     * Interpolates already observed data, without creating false extrema or
     * labeling interpolation coordinates as independently observed prices.
     * Output remains bounded and is computed only for opt-in charts.
     */
    Q_INVOKABLE QVariantMap computeSmooth(
        const QVariantList& samples,
        qreal width,
        qreal height,
        qreal contentPadding,
        qreal strokeWidth) const;
};
