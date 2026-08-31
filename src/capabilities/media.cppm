module;

#include <cstdint>
#include <filesystem>
#include <functional>

export module farcaster.capability.media;

import farcaster.domain.identity;
import farcaster.domain.media;
import farcaster.foundation.result;

export namespace farcaster::capability::media {

/** @brief Stable phases emitted while a local file is uploaded and verified. */
enum class UploadStage : std::uint8_t {
    Preparing,
    Authorizing,
    Uploading,
    Verifying,
};

/** @brief Progress callback invoked on the media I/O executor thread. */
using ProgressObserver = std::function<void(UploadStage stage, std::uint64_t transferred, std::uint64_t total)>;

/** @brief Cancellation probe checked by the concrete upload adapter. */
using CancellationProbe = std::function<bool()>;

/**
 * @brief Provider-neutral media-upload gateway port.
 *
 * Concrete implementations may use direct pre-signed object-store uploads or
 * a relay. Storage-provider endpoints, credentials and response DTOs terminate
 * behind this interface.
 */
class IMediaUploadGateway {
public:
    /** @brief Destroys the upload gateway. */
    virtual ~IMediaUploadGateway() = default;

    /**
     * @brief Uploads one local file for an authenticated Farcaster identity.
     * @param fid Verified client-session identity that owns the upload request.
     * @param localPath Local file selected by the user; read only by the outer adapter.
     * @param progress Optional progress observer invoked from the I/O executor.
     * @param cancelled Probe that returns true when the caller requested cancellation.
     * @return Stable public media asset or a structured recoverable failure.
     */
    [[nodiscard]] virtual foundation::result::Result<domain::media::Asset> upload(
        domain::identity::Fid fid,
        const std::filesystem::path& localPath,
        ProgressObserver progress,
        CancellationProbe cancelled) = 0;
};

} // namespace farcaster::capability::media
