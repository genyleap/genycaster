/*!
 * @file        ui/render/token_sparkline_geometry.cpp
 * @brief       Bounded native generation of market sparkline coordinates.
 *
 * @details
 * Moves finite-value filtering, market sample decimation, smoothing and
 * chart coordinate conversion out of QML JavaScript into reusable C++ code.
 *
 * @author      Kambiz Asadzadeh
 * @since       06 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#include "token_sparkline_geometry.hpp"

#include <QPointF>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::vector<qreal> finiteSamples(const QVariantList& samples)
{
    std::vector<qreal> values{};
    values.reserve(static_cast<std::size_t>(samples.size()));
    for (const QVariant& sample : samples) {
        bool valid = false;
        const qreal value = sample.toDouble(&valid);
        if (valid && std::isfinite(value)) {
            values.push_back(value);
        }
    }
    return values;
}

[[nodiscard]] std::vector<qreal> decimate(std::vector<qreal> values, const qreal width)
{
    const auto count = values.size();
    const int target = static_cast<int>(std::clamp(std::floor(std::max<qreal>(1.0, width) / 5.0), 28.0, 96.0));
    if (count <= static_cast<std::size_t>(target)) {
        return values;
    }
    std::vector<qreal> selected{};
    selected.reserve(static_cast<std::size_t>(target));
    std::size_t previous = count;
    for (int index = 0; index < target; ++index) {
        const qreal position = static_cast<qreal>(index) * static_cast<qreal>(count - 1)
            / static_cast<qreal>(target - 1);
        const std::size_t sourceIndex = static_cast<std::size_t>(std::floor(position + 0.5));
        if (sourceIndex != previous) {
            selected.push_back(values[sourceIndex]);
            previous = sourceIndex;
        }
    }
    return selected;
}

void smooth(std::vector<qreal>& values)
{
    if (values.size() < 5U) return;
    std::vector<qreal> result{};
    result.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        const qreal previous = values[index > 0 ? index - 1 : index];
        const qreal next = values[index + 1 < values.size() ? index + 1 : index];
        result.push_back((previous + values[index] * 2.0 + next) / 4.0);
    }
    values = std::move(result);
}

} // namespace

TokenSparklineGeometry::TokenSparklineGeometry(QObject* parent)
    : QObject(parent)
{
}

QVariantMap TokenSparklineGeometry::compute(
    const QVariantList& samples,
    const qreal width,
    const qreal height,
    const qreal contentPadding,
    const qreal strokeWidth) const
{
    QVariantList linePoints{};
    QVariantList areaPoints{};
    const auto empty = [&]() {
        return QVariantMap{{"linePoints", linePoints}, {"areaPoints", areaPoints}};
    };

    if (!std::isfinite(width) || !std::isfinite(height) || width <= 1.0 || height <= 1.0) {
        return empty();
    }

    auto values = decimate(finiteSamples(samples), width);
    if (values.size() < 2U) {
        return empty();
    }
    smooth(values);

    const auto [minimumIt, maximumIt] = std::minmax_element(values.begin(), values.end());
    const qreal minimum = *minimumIt;
    const qreal maximum = *maximumIt;

    const qreal padding = std::max<qreal>(
        0.0, contentPadding + strokeWidth / 2.0);
    const qreal usableWidth = std::max<qreal>(0.0, width - padding * 2.0);
    const qreal usableHeight = std::max<qreal>(0.0, height - padding * 2.0);
    const qreal baseline = std::max({std::abs(minimum), std::abs(maximum), 1e-12});
    const qreal effectiveSpan = std::max(maximum - minimum, baseline * 0.0015);
    const qreal visualMinimum = (minimum + maximum - effectiveSpan) / 2.0;

    linePoints.reserve(static_cast<qsizetype>(values.size()));
    areaPoints.reserve(static_cast<qsizetype>(values.size() + 2U));

    for (std::size_t index = 0; index < values.size(); ++index) {
        const qreal normalized = std::clamp((values[index] - visualMinimum) / effectiveSpan, 0.0, 1.0);
        const qreal fraction = static_cast<qreal>(index) / static_cast<qreal>(values.size() - 1U);
        linePoints.push_back(QPointF(
            padding + usableWidth * fraction,
            padding + usableHeight * (1.0 - normalized)));
    }

    const qreal bottom = std::max<qreal>(0.0, height - contentPadding);
    areaPoints.push_back(QPointF(linePoints.first().toPointF().x(), bottom));
    for (const QVariant& point : linePoints) {
        areaPoints.push_back(point);
    }
    areaPoints.push_back(QPointF(linePoints.last().toPointF().x(), bottom));

    return {{"linePoints", linePoints}, {"areaPoints", areaPoints}};
}
