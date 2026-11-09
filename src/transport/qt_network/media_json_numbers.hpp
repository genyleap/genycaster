/*!
 * @file        src/transport/qt_network/media_json_numbers.hpp
 * @brief       Validates untrusted Qt media-gateway integer JSON values.
 *
 * @details
 * This Qt SDK textual compatibility boundary must remain a header: GCC 16
 * cannot compile the included Qt 6.12 QJsonValue template internals as an
 * exported named-module interface (TU-local entity diagnostics). Restrict
 * this helper to the Qt transport adapter and its focused boundary test.
 * Rejects fractional, negative and out-of-range media sizes and part indices.
 *
 * @author      Kambiz Asadzadeh
 * @since       09 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QJsonValue>

#include <cstdint>
#include <optional>

namespace farcaster::transport::qtnetwork::media::detail {

// QJsonValue::toInteger rejects fractional, negative, nonnumeric and
// out-of-qint64-range values without undefined floating-to-unsigned casts.
// Caller-supplied protocol maxima also bound otherwise valid integers.
[[nodiscard]] inline std::optional<std::uint64_t> boundedJsonUnsigned(
    const QJsonValue& value, const std::uint64_t maximum) noexcept
{
    if (!value.isDouble())
        return std::nullopt;
    const qint64 number = value.toInteger(-1);
    if (number < 0)
        return std::nullopt;
    const auto positive = static_cast<std::uint64_t>(number);
    if (positive > maximum)
        return std::nullopt;
    return positive;
}

} // namespace farcaster::transport::qtnetwork::media::detail
