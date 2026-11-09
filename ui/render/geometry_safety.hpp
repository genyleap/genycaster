/*!
 * @file        ui/render/geometry_safety.hpp
 * @brief       Provides bounded scene-graph tessellation segment conversion.
 *
 * @details
 * Rejects non-finite estimates and clamps values before floating-point to
 * integer conversion while retaining the existing tessellation quality caps.
 *
 * @author      Kambiz Asadzadeh
 * @since       09 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#include <QtGlobal>

#include <cmath>

namespace genycaster::render::safety {

[[nodiscard]] inline int boundedSegmentCount(
    const qreal estimate, const int minimum, const int maximum) noexcept
{
    // !(x > minimum) deliberately also rejects NaN; -infinity is minimum
    // and +infinity saturates at maximum before any integer conversion.
    if (!(estimate > static_cast<qreal>(minimum)))
        return minimum;
    if (estimate >= static_cast<qreal>(maximum))
        return maximum;
    return static_cast<int>(std::ceil(estimate));
}

} // namespace genycaster::render::safety
