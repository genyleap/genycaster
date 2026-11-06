/*!
 * @file        src/application/media_upload.cppm
 * @brief       Media upload application service.
 *
 * @details
 * Models bounded upload progress and delegates authenticated media
 * transport through a provider-neutral capability port.
 *
 * @author      Kambiz Asadzadeh
 * @since       31 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

export module farcaster.application.media.upload;

import farcaster.application.executor;
import farcaster.capability.media;
import farcaster.domain.media;
import farcaster.domain.session;
import farcaster.foundation.error;
import farcaster.foundation.result;

export namespace farcaster::application::media::upload {

/** @brief Product-visible lifecycle for one media upload. */
enum class UploadPhase : std::uint8_t {
    Queued,
    Preparing,
    Authorizing,
    Uploading,
    Verifying,
    Completed,
    Cancelled,
    Failed,
};

/** @brief Immutable state snapshot emitted by MediaUploadService. */
struct UploadState final {
    UploadPhase phase{UploadPhase::Queued};
    std::uint64_t transferred{};
    std::uint64_t total{};
    std::optional<domain::media::Asset> asset{};
    std::string managementToken{};
    std::optional<foundation::error::Error> error{};
};

/** @brief Observer invoked from the configured application executor. */
using Observer = std::function<void(UploadState)>;

/** @brief Lightweight cancellation handle for one queued/in-flight upload. */
class UploadHandle final {
public:
    UploadHandle() = default;

    /** @brief Requests cooperative cancellation; safe to call repeatedly. */
    void cancel() const noexcept;

    /** @brief Returns true when this handle still refers to an upload. */
    [[nodiscard]] bool valid() const noexcept;

private:
    explicit UploadHandle(std::shared_ptr<std::atomic_bool> cancelled) noexcept;
    std::shared_ptr<std::atomic_bool> m_cancelled{};
    friend class MediaUploadService;
};

/**
 * @brief Runs media upload work outside the UI thread over a replaceable gateway.
 *
 * Session validity is checked before scheduling. The service owns no network,
 * filesystem, Qt or object-storage behavior.
 */
class MediaUploadService final {
public:
    /** @brief Creates the service over an executor and provider-neutral upload gateway. */
    MediaUploadService(executor::IExecutor& executor, capability::media::IMediaUploadGateway& gateway) noexcept;

    /**
     * @brief Starts one upload for the current verified account session.
     * @return Cancellation handle or a synchronous validation/scheduling failure.
     */
    [[nodiscard]] foundation::result::Result<UploadHandle> start(
        std::optional<domain::session::ClientSession> session,
        std::filesystem::path localPath,
        Observer observer);

private:
    executor::IExecutor& m_executor;
    capability::media::IMediaUploadGateway& m_gateway;
};

} // namespace farcaster::application::media::upload
