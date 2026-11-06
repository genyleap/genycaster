/*!
 * @file        src/foundation/time.cppm
 * @brief       Application time and timestamp primitives.
 *
 * @details
 * Declares wall-clock values and clock access used by persisted state and
 * protocol operations.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <chrono>

export module farcaster.foundation.time;

export namespace farcaster::foundation::time {

/** @brief Wall-clock source used for persisted protocol and application time. */
using Clock = std::chrono::system_clock;

/** @brief Millisecond-resolution wall-clock timestamp. */
using Timestamp = std::chrono::time_point<Clock, std::chrono::milliseconds>;

/**
 * @brief Returns the current wall-clock time truncated to milliseconds.
 */
[[nodiscard]] Timestamp now();

}
