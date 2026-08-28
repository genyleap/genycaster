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
