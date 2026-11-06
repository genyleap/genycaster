/*!
 * @file        src/foundation/result.cppm
 * @brief       Typed result and status vocabulary.
 *
 * @details
 * Represents success or structured recoverable failure consistently
 * across C++ module interfaces.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <expected>

export module farcaster.foundation.result;

import farcaster.foundation.error;

export namespace farcaster::foundation::result {

/**
 * @brief Expected value or structured recoverable error.
 * @tparam T Owned success value returned by the operation.
 */
template <typename T>
using Result = std::expected<T, error::Error>;

/**
 * @brief Successful completion without a value or a structured recoverable error.
 */
using Status = std::expected<void, error::Error>;

}
