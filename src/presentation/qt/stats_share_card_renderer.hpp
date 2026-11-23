#pragma once

#include <QString>
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
    int periodDays,
    const QString& style,
    double capturedAtMs);

} // namespace genycaster::presentation
