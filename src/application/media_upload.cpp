module;

#include <atomic>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

module farcaster.application.media.upload;

import farcaster.application.executor;
import farcaster.capability.media;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;

namespace farcaster::application::media::upload {
namespace {

[[nodiscard]] UploadPhase phaseFor(const capability::media::UploadStage stage) noexcept
{
    switch (stage) {
    case capability::media::UploadStage::Preparing:
        return UploadPhase::Preparing;
    case capability::media::UploadStage::Authorizing:
        return UploadPhase::Authorizing;
    case capability::media::UploadStage::Uploading:
        return UploadPhase::Uploading;
    case capability::media::UploadStage::Verifying:
        return UploadPhase::Verifying;
    }
    return UploadPhase::Preparing;
}

[[nodiscard]] foundation::error::Error invalidSessionError()
{
    return foundation::error::Error::make(
        foundation::error::ErrorCode::Unauthorized,
        "Media upload requires an active authenticated Farcaster session.");
}

} // namespace

UploadHandle::UploadHandle(std::shared_ptr<std::atomic_bool> cancelled) noexcept
    : m_cancelled{std::move(cancelled)}
{
}

void UploadHandle::cancel() const noexcept
{
    if (m_cancelled) {
        m_cancelled->store(true, std::memory_order_relaxed);
    }
}

bool UploadHandle::valid() const noexcept
{
    return static_cast<bool>(m_cancelled);
}

MediaUploadService::MediaUploadService(
    executor::IExecutor& executor,
    capability::media::IMediaUploadGateway& gateway) noexcept
    : m_executor{executor},
      m_gateway{gateway}
{
}

foundation::result::Result<UploadHandle> MediaUploadService::start(
    std::optional<domain::session::ClientSession> session,
    std::filesystem::path localPath,
    Observer observer)
{
    if (!session || session->fid.value == 0U || domain::session::isExpired(*session, foundation::time::now())) {
        return std::unexpected(invalidSessionError());
    }
    if (localPath.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Media upload requires a local file path."));
    }
    if (!observer) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Media upload requires a lifecycle observer."));
    }

    auto cancelled = std::make_shared<std::atomic_bool>(false);
    const auto fid = session->fid;
    auto status = m_executor.post(
        [this, fid, localPath = std::move(localPath), observer = std::move(observer), cancelled]() mutable {
            observer(UploadState{.phase = UploadPhase::Queued});
            if (cancelled->load(std::memory_order_relaxed)) {
                observer(UploadState{.phase = UploadPhase::Cancelled});
                return;
            }

            auto result = m_gateway.upload(
                fid,
                localPath,
                [observer](const capability::media::UploadStage stage,
                           const std::uint64_t transferred,
                           const std::uint64_t total) mutable {
                    observer(UploadState{
                        .phase = phaseFor(stage),
                        .transferred = transferred,
                        .total = total,
                    });
                },
                [cancelled] { return cancelled->load(std::memory_order_relaxed); });

            if (cancelled->load(std::memory_order_relaxed)) {
                observer(UploadState{.phase = UploadPhase::Cancelled});
                return;
            }
            if (!result) {
                observer(UploadState{
                    .phase = UploadPhase::Failed,
                    .error = std::move(result.error()),
                });
                return;
            }
            observer(UploadState{
                .phase = UploadPhase::Completed,
                .transferred = result->asset.sizeBytes,
                .total = result->asset.sizeBytes,
                .asset = std::move(result->asset),
                .managementToken = std::move(result->managementToken),
            });
        });
    if (!status) {
        return std::unexpected(std::move(status.error()));
    }
    return UploadHandle{std::move(cancelled)};
}

} // namespace farcaster::application::media::upload
