module;

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

export module farcaster.application.media.lifecycle;

import farcaster.application.executor;
import farcaster.capability.media;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;

export namespace farcaster::application::media::lifecycle {

/** @brief Product-visible lifecycle for one managed-media deletion request. */
enum class CleanupPhase : std::uint8_t {
    Queued,
    Deleting,
    Completed,
    Failed,
};

/** @brief Immutable cleanup state emitted from the media I/O executor. */
struct CleanupState final {
    CleanupPhase phase{CleanupPhase::Queued};
    std::string managementToken{};
    std::optional<foundation::error::Error> error{};
};

/** @brief Observer invoked from the configured media I/O executor. */
using Observer = std::function<void(CleanupState)>;

/**
 * @brief Orchestrates deletion of managed media without exposing provider storage semantics.
 *
 * The service validates the authenticated session, then delegates the opaque
 * management token to the lifecycle gateway on the configured executor.
 */
class MediaLifecycleService final {
public:
    MediaLifecycleService(
        executor::IExecutor& executor,
        capability::media::IMediaAssetLifecycleGateway& gateway) noexcept;

    /**
     * @brief Schedules deletion of one managed asset owned by the current account.
     * @return synchronous validation/scheduling status; asynchronous outcome is emitted to observer.
     */
    [[nodiscard]] foundation::result::Status remove(
        std::optional<domain::session::ClientSession> session,
        std::string managementToken,
        Observer observer);

private:
    executor::IExecutor& m_executor;
    capability::media::IMediaAssetLifecycleGateway& m_gateway;
};

} // namespace farcaster::application::media::lifecycle
