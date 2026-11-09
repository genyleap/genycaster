// Guard C++ floating-point-to-integer conversions in scene-graph tessellation.
// A value outside int's representable range has undefined conversion behavior.
// Keep the existing visual quality thresholds and their fixed tessellation caps.
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
