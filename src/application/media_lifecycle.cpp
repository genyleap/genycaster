module;

#include <expected>
#include <optional>
#include <string>
#include <utility>

module farcaster.application.media.lifecycle;

import farcaster.application.executor;
import farcaster.capability.media;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;

namespace farcaster::application::media::lifecycle {
namespace {

[[nodiscard]] foundation::error::Error invalidSessionError()
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::Unauthorized,
        "Managed media cleanup requires an active authenticated Farcaster session.");
}

} // namespace

MediaLifecycleService::MediaLifecycleService(
    executor::IExecutor& executor,
    capability::media::IMediaAssetLifecycleGateway& gateway) noexcept
    : m_executor{executor},
      m_gateway{gateway}
{
}

foundation::result::Status MediaLifecycleService::remove(
    std::optional<domain::session::ClientSession> session,
    std::string managementToken,
    Observer observer)
{
    if (!session || session->fid.value == 0U || domain::session::isExpired(*session, foundation::time::now())) {
        return std::unexpected(invalidSessionError());
    }
    if (managementToken.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Managed media cleanup requires an asset management token."));
    }
    if (!observer) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Managed media cleanup requires a lifecycle observer."));
    }

    const auto fid = session->fid;
    return m_executor.post(
        [this, fid, managementToken = std::move(managementToken), observer = std::move(observer)]() mutable {
            observer(CleanupState{
                .phase = CleanupPhase::Queued,
                .managementToken = managementToken,
            });
            observer(CleanupState{
                .phase = CleanupPhase::Deleting,
                .managementToken = managementToken,
            });
            auto result = m_gateway.remove(fid, managementToken);
            if (!result) {
                observer(CleanupState{
                    .phase = CleanupPhase::Failed,
                    .managementToken = std::move(managementToken),
                    .error = std::move(result.error()),
                });
                return;
            }
            observer(CleanupState{
                .phase = CleanupPhase::Completed,
                .managementToken = std::move(managementToken),
            });
        });
}

} // namespace farcaster::application::media::lifecycle
