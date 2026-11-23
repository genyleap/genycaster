/*!
 * @file        src/presentation/qt/stats_share_card_renderer.hpp
 * @brief       QtGui raster share-card rendering compatibility boundary.
 *
 * @details
 * Textual QtGui boundary between the media-upload QObject adapter and
 * its CPU-raster implementation; Qt QImage/QString/QVariantMap are Qt SDK
 * types and never enter the provider-neutral C++ application modules.
 *
 * @author      Kambiz Asadzadeh
 * @since       23 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include <QString>
#include <QImage>
#include <QUrl>
#include <QVariantMap>

namespace genycaster::presentation {

// Produces an actual PNG using Qt's CPU raster engine. No Qt Quick scene
// texture, WebGL readback, browser canvas, network or storage dependency.
struct StatsShareImage final {
    QUrl fileUrl{};
    QString error{};
};

[[nodiscard]] StatsShareImage renderStatsShareCard(
    const QVariantMap& snapshot,
    const QString& displayName,
    const QString& username,
    const QImage& avatar,
    int periodDays,
    const QString& style,
    double capturedAtMs);

} // namespace genycaster::presentation
