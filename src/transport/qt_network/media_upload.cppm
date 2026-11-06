/*!
 * @file        src/transport/qt_network/media_upload.cppm
 * @brief       Qt Network managed media upload adapter.
 *
 * @details
 * Implements authenticated multipart media-gateway operations behind the
 * application capability contract.
 *
 * @author      Kambiz Asadzadeh
 * @since       31 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

export module farcaster.transport.qtnetwork.media;

import farcaster.capability.media;
import farcaster.capability.write_authority;
import farcaster.foundation.result;
import farcaster.transport.async_http;
import farcaster.transport.qtnetwork;

export namespace farcaster::transport::qtnetwork::media {

/** @brief Configuration for the provider-neutral Genyleap media gateway adapter. */
struct Config final {
    std::string baseUrl{"https://storage-farcaster.genyleap.com"};
    std::chrono::milliseconds requestTimeout{30'000};
    std::chrono::milliseconds uploadTimeout{15 * 60 * 1000};
    std::size_t maxResponseBytes{1024U * 1024U};
    std::uint64_t maxImageBytes{25U * 1024U * 1024U};
    std::uint64_t maxVideoBytes{512U * 1024U * 1024U};
};

/**
 * @brief Creates the Qt Network implementation of the media-upload gateway port.
 *
 * The adapter talks only to the Genyleap media gateway. It never contains
 * qStorage/S3 credentials. Per-request authorization is proven with the current
 * network-authorized Farcaster signer supplied through IWriteAuthority.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::media::IMediaUploadGateway>>
makeGateway(
    capability::write::IWriteAuthority& authority,
    async_http::IAsyncHttpTransport& transport,
    Config config = {});

/**
 * @brief Creates the Qt Network implementation of managed-media deletion.
 *
 * The adapter sends only an opaque lifecycle token to the Genyleap gateway;
 * QStorage keys and credentials remain server-side.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::media::IMediaAssetLifecycleGateway>>
makeLifecycleGateway(
    capability::write::IWriteAuthority& authority,
    async_http::IAsyncHttpTransport& transport,
    Config config = {});

} // namespace farcaster::transport::qtnetwork::media
