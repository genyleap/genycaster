module;

#include <string>

export module farcaster.foundation.error;

export namespace farcaster::foundation::error {

/**
 * @brief Stable error categories shared by provider-neutral core subsystems.
 */
enum class ErrorCode {
    InvalidArgument,
    NotFound,
    Unavailable,
    Unauthorized,
    RateLimited,
    Timeout,
    Conflict,
    Unsupported,
    StorageFailure,
    TransportFailure,
    ProtocolFailure,
    InternalFailure,
};

/**
 * @brief Structured recoverable failure returned through Result and Status.
 *
 * The message is human-readable diagnostic context. Callers should branch on
 * code rather than parse message text.
 */
struct Error final {
    ErrorCode code{ErrorCode::InternalFailure};
    std::string message{};

    /**
     * @brief Creates an error with an owned diagnostic message.
     */
    [[nodiscard]] static Error make(ErrorCode code, std::string message);
};

}
