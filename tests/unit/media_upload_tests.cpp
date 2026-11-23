#include <cassert>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import farcaster.application.executor;
import farcaster.application.media.upload;
import farcaster.application.media.lifecycle;
import farcaster.capability.media;
import farcaster.domain.identity;
import farcaster.domain.media;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.foundation.time;

namespace {

using namespace farcaster;

class DeferredExecutor final : public application::executor::IExecutor {
public:
    [[nodiscard]] foundation::result::Status post(application::executor::Job job) override
    {
        if (!acceptJobs) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Executor is unavailable."));
        }
        m_jobs.push_back(std::move(job));
        return {};
    }

    void drain()
    {
        auto jobs = std::exchange(m_jobs, {});
        for (auto& job : jobs) {
            job();
        }
    }

    bool acceptJobs{true};

private:
    std::vector<application::executor::Job> m_jobs{};
};

class FakeMediaGateway final : public capability::media::IMediaUploadGateway {
public:
    [[nodiscard]] foundation::result::Result<capability::media::ManagedAsset> upload(
        const domain::identity::Fid fid,
        const std::filesystem::path& localPath,
        std::uint64_t maxImageBytes,
        capability::media::ProgressObserver progress,
        capability::media::CancellationProbe cancelled) override
    {
        ++calls;
        observedFid = fid.value;
        observedPath = localPath;
        observedImageLimit = maxImageBytes;

        if (cancelled()) {
            return std::unexpected(foundation::error::Error::make(
                foundation::error::ErrorCode::Unavailable,
                "Cancelled before gateway work."));
        }

        progress(capability::media::UploadStage::Preparing, 0U, 4096U);
        progress(capability::media::UploadStage::Authorizing, 0U, 4096U);
        progress(capability::media::UploadStage::Uploading, 2048U, 4096U);
        progress(capability::media::UploadStage::Optimizing, 4096U, 4096U);
        progress(capability::media::UploadStage::Finalizing, 4096U, 4096U);

        if (failure) {
            return std::unexpected(*failure);
        }

        return capability::media::ManagedAsset{
            .asset = domain::media::Asset{
                .url = "https://media-farcaster.genyleap.com/m/v1/test.jpg",
                .contentType = "image/jpeg",
                .fileName = "test.jpg",
                .sizeBytes = 4096U,
                .kind = domain::media::Kind::Image,
            },
            .managementToken = "opaque-management-token",
        };
    }

    std::size_t calls{};
    std::uint64_t observedFid{};
    std::filesystem::path observedPath{};
    std::uint64_t observedImageLimit{};
    std::optional<foundation::error::Error> failure{};
};

class FakeMediaLifecycleGateway final : public capability::media::IMediaAssetLifecycleGateway {
public:
    [[nodiscard]] foundation::result::Status remove(
        const domain::identity::Fid fid,
        const std::string_view managementToken) override
    {
        ++calls;
        observedFid = fid.value;
        observedToken = std::string{managementToken};
        if (failure) {
            return std::unexpected(*failure);
        }
        return {};
    }

    std::size_t calls{};
    std::uint64_t observedFid{};
    std::string observedToken{};
    std::optional<foundation::error::Error> failure{};
};

[[nodiscard]] domain::session::ClientSession activeSession()
{
    const auto now = foundation::time::now();
    return domain::session::ClientSession{
        .fid = domain::identity::Fid{4242U},
        .username = std::string{"alice"},
        .method = domain::session::AuthenticationMethod::Custody,
        .authenticatedAt = now - std::chrono::minutes{1},
        .expiresAt = now + std::chrono::hours{1},
    };
}

void testSuccessfulUploadEmitsProviderNeutralLifecycle()
{
    DeferredExecutor executor{};
    FakeMediaGateway gateway{};
    application::media::upload::MediaUploadService service{executor, gateway};
    std::vector<application::media::upload::UploadState> states{};

    auto result = service.start(
        activeSession(),
        std::filesystem::path{"/tmp/test.jpg"},
        [&states](application::media::upload::UploadState state) {
            states.push_back(std::move(state));
        });

    assert(result);
    assert(result->valid());
    assert(states.empty());

    executor.drain();

    assert(gateway.calls == 1U);
    assert(gateway.observedFid == 4242U);
    assert(gateway.observedImageLimit == 2U * 1024U * 1024U);
    assert(gateway.observedPath == std::filesystem::path{"/tmp/test.jpg"});
    assert(states.size() == 7U);
    assert(states[0].phase == application::media::upload::UploadPhase::Queued);
    assert(states[1].phase == application::media::upload::UploadPhase::Preparing);
    assert(states[2].phase == application::media::upload::UploadPhase::Authorizing);
    assert(states[3].phase == application::media::upload::UploadPhase::Uploading);
    assert(states[3].transferred == 2048U);
    assert(states[3].total == 4096U);
    assert(states[4].phase == application::media::upload::UploadPhase::Optimizing);
    assert(states[5].phase == application::media::upload::UploadPhase::Finalizing);
    assert(states[6].phase == application::media::upload::UploadPhase::Completed);
    assert(states[6].asset.has_value());
    assert(states[6].asset->url == "https://media-farcaster.genyleap.com/m/v1/test.jpg");
    assert(states[6].asset->sizeBytes == 4096U);
    assert(states[6].managementToken == "opaque-management-token");
}

void testVerifiedProLimitPropagatesToNativeGateway()
{
    DeferredExecutor executor{};
    FakeMediaGateway gateway{};
    application::media::upload::MediaUploadService service{executor, gateway};
    const auto observe = [](application::media::upload::UploadState) {};

    const auto invalid = service.start(
        activeSession(), std::filesystem::path{"/tmp/image.jpg"},
        observe, 6U * 1024U * 1024U);
    assert(!invalid);
    assert(invalid.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(gateway.calls == 0U);

    const auto pro = service.start(
        activeSession(), std::filesystem::path{"/tmp/image.jpg"},
        observe, 5U * 1024U * 1024U);
    assert(pro);
    executor.drain();
    assert(gateway.calls == 1U);
    assert(gateway.observedImageLimit == 5U * 1024U * 1024U);
}

void testCancellationBeforeExecutionDoesNotTouchGateway()
{
    DeferredExecutor executor{};
    FakeMediaGateway gateway{};
    application::media::upload::MediaUploadService service{executor, gateway};
    std::vector<application::media::upload::UploadState> states{};

    auto result = service.start(
        activeSession(),
        std::filesystem::path{"/tmp/cancel.jpg"},
        [&states](application::media::upload::UploadState state) {
            states.push_back(std::move(state));
        });
    assert(result);

    result->cancel();
    executor.drain();

    assert(gateway.calls == 0U);
    assert(states.size() == 2U);
    assert(states[0].phase == application::media::upload::UploadPhase::Queued);
    assert(states[1].phase == application::media::upload::UploadPhase::Cancelled);
}

void testExpiredSessionFailsBeforeScheduling()
{
    DeferredExecutor executor{};
    FakeMediaGateway gateway{};
    application::media::upload::MediaUploadService service{executor, gateway};
    auto session = activeSession();
    session.expiresAt = foundation::time::now() - std::chrono::seconds{1};

    auto result = service.start(
        session,
        std::filesystem::path{"/tmp/test.jpg"},
        [](application::media::upload::UploadState) {});

    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Unauthorized);
    assert(gateway.calls == 0U);
}

void testGatewayFailureBecomesFailedState()
{
    DeferredExecutor executor{};
    FakeMediaGateway gateway{};
    gateway.failure = foundation::error::Error::make(
        foundation::error::ErrorCode::RateLimited,
        "Gateway rate limited the upload.");
    application::media::upload::MediaUploadService service{executor, gateway};
    std::vector<application::media::upload::UploadState> states{};

    auto result = service.start(
        activeSession(),
        std::filesystem::path{"/tmp/test.jpg"},
        [&states](application::media::upload::UploadState state) {
            states.push_back(std::move(state));
        });
    assert(result);

    executor.drain();

    assert(!states.empty());
    assert(states.back().phase == application::media::upload::UploadPhase::Failed);
    assert(states.back().error.has_value());
    assert(states.back().error->code == foundation::error::ErrorCode::RateLimited);
}

void testExecutorRejectionIsSynchronous()
{
    DeferredExecutor executor{};
    executor.acceptJobs = false;
    FakeMediaGateway gateway{};
    application::media::upload::MediaUploadService service{executor, gateway};

    auto result = service.start(
        activeSession(),
        std::filesystem::path{"/tmp/test.jpg"},
        [](application::media::upload::UploadState) {});

    assert(!result);
    assert(result.error().code == foundation::error::ErrorCode::Unavailable);
    assert(gateway.calls == 0U);
}

void testSuccessfulManagedAssetCleanup()
{
    DeferredExecutor executor{};
    FakeMediaLifecycleGateway gateway{};
    application::media::lifecycle::MediaLifecycleService service{executor, gateway};
    std::vector<application::media::lifecycle::CleanupState> states{};

    auto status = service.remove(
        activeSession(),
        "opaque-management-token",
        [&states](application::media::lifecycle::CleanupState state) {
            states.push_back(std::move(state));
        });
    assert(status);
    assert(states.empty());

    executor.drain();

    assert(gateway.calls == 1U);
    assert(gateway.observedFid == 4242U);
    assert(gateway.observedToken == "opaque-management-token");
    assert(states.size() == 3U);
    assert(states[0].phase == application::media::lifecycle::CleanupPhase::Queued);
    assert(states[1].phase == application::media::lifecycle::CleanupPhase::Deleting);
    assert(states[2].phase == application::media::lifecycle::CleanupPhase::Completed);
}

void testManagedAssetCleanupFailureStaysObservable()
{
    DeferredExecutor executor{};
    FakeMediaLifecycleGateway gateway{};
    gateway.failure = foundation::error::Error::make(
        foundation::error::ErrorCode::Unavailable,
        "Gateway unavailable.");
    application::media::lifecycle::MediaLifecycleService service{executor, gateway};
    std::vector<application::media::lifecycle::CleanupState> states{};

    auto status = service.remove(
        activeSession(),
        "opaque-management-token",
        [&states](application::media::lifecycle::CleanupState state) {
            states.push_back(std::move(state));
        });
    assert(status);
    executor.drain();

    assert(states.back().phase == application::media::lifecycle::CleanupPhase::Failed);
    assert(states.back().error.has_value());
    assert(states.back().error->code == foundation::error::ErrorCode::Unavailable);
}

void testManagedAssetCleanupRejectsExpiredSessionAndEmptyToken()
{
    DeferredExecutor executor{};
    FakeMediaLifecycleGateway gateway{};
    application::media::lifecycle::MediaLifecycleService service{executor, gateway};
    auto expired = activeSession();
    expired.expiresAt = foundation::time::now() - std::chrono::seconds{1};

    auto expiredStatus = service.remove(
        expired,
        "opaque-management-token",
        [](application::media::lifecycle::CleanupState) {});
    assert(!expiredStatus);
    assert(expiredStatus.error().code == foundation::error::ErrorCode::Unauthorized);

    auto emptyStatus = service.remove(
        activeSession(),
        {},
        [](application::media::lifecycle::CleanupState) {});
    assert(!emptyStatus);
    assert(emptyStatus.error().code == foundation::error::ErrorCode::InvalidArgument);
    assert(gateway.calls == 0U);
}

} // namespace

int main()
{
    testSuccessfulUploadEmitsProviderNeutralLifecycle();
    testVerifiedProLimitPropagatesToNativeGateway();
    testCancellationBeforeExecutionDoesNotTouchGateway();
    testExpiredSessionFailsBeforeScheduling();
    testGatewayFailureBecomesFailedState();
    testExecutorRejectionIsSynchronous();
    testSuccessfulManagedAssetCleanup();
    testManagedAssetCleanupFailureStaysObservable();
    testManagedAssetCleanupRejectsExpiredSessionAndEmptyToken();
    return 0;
}
