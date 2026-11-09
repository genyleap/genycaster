/*!
 * @file src/transport/qt_network/media_json_numbers.hpp
 * @brief Bounded numeric decoding at the untrusted media-gateway boundary.
 */
#pragma once

#include <QJsonValue>

#include <cstdint>
#include <optional>

namespace farcaster::transport::qtnetwork::media::detail {

// QJsonValue::toInteger rejects fractional, negative, nonnumeric and
// out-of-qint64-range values without performing undefined floating-to-unsigned
// conversions. A media file is bounded by QFileInfo::size() (qint64), and all
// other callers provide even smaller protocol maxima.
[[nodiscard]] inline std::optional<std::uint64_t> boundedJsonUnsigned(
    const QJsonValue& value, const std::uint64_t maximum) noexcept
{
    if (!value.isDouble()) return std::nullopt;
    const qint64 number = value.toInteger(-1);
    if (number < 0) return std::nullopt;
    const auto positive = static_cast<std::uint64_t>(number);
    if (positive > maximum) return std::nullopt;
    return positive;
}

} // namespace farcaster::transport::qtnetwork::media::detail
