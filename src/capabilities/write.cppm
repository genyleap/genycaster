module;

#include <optional>
#include <string>

export module farcaster.capability.write;

import farcaster.domain.write;
import farcaster.foundation.result;

export namespace farcaster::capability::write {

/** @brief Canonical outcome of a provider submit attempt. */
enum class SubmitOutcome : std::uint8_t {
    Submitted,
    AlreadyConfirmed,
    Conflict,
    RateLimited,
    Unavailable,
    Unsupported,
    ProtocolRejected,
};

/**
 * @brief Provider-neutral result of one submit attempt.
 *
 * Application and Presentation only see this canonical outcome. Raw provider
 * errors, cursors, and SDK types never cross this boundary.
 */
struct SubmitResult final {
    SubmitOutcome outcome{SubmitOutcome::Unsupported};
    /** @brief Canonical identifier the provider assigned on success, when known. */
    std::optional<domain::write::IdempotencyKey> confirmedKey{};
};

/**
 * @brief Blocking provider-neutral protocol-write submission port.
 *
 * Implementations may perform network I/O. Callers must execute these methods
 * on an I/O executor and never on the Qt GUI thread.
 */
class IWriteSubmitter {
public:
    /** @brief Destroys the submitter. */
    virtual ~IWriteSubmitter() = default;

    /**
     * @brief Submits one already-signed canonical protocol write.
     * @param operation Signed canonical message produced by the local signer.
     * @return Canonical submit outcome.
     */
    [[nodiscard]] virtual foundation::result::Result<SubmitResult> submit(
        const domain::write::SignedOperation& operation) = 0;
};

}