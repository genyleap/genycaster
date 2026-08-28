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
