/*!
 * @file        src/providers/compatibility/signer_attestation/attestor.cppm
 * @brief       Signer registration attestation adapter.
 *
 * @details
 * Submits and verifies scoped App-FID SignedKeyRequest evidence through
 * an authenticated gateway.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

export module farcaster.provider.compatibility.signer_attestation;

import farcaster.capability.signer_attestation;
import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::provider::compatibility::signer_attestation {

/** @brief First-party endpoint configuration for Genycaster App-FID signer attestation. */
struct Config final {
    std::string endpointUrl{};
    std::chrono::milliseconds timeout{10'000};
    std::size_t maxBodyBytes{64U * 1024U};
};

/**
 * @brief Creates an App-FID SignedKeyRequest attestor over the shared HTTP transport.
 *
 * HTTPS is required except for explicit localhost/127.0.0.1 development URLs.
 * The endpoint receives no user custody signature or private key material.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<capability::signer_attestation::ISignedKeyRequestAttestor>>
makeAttestor(transport::http::IHttpTransport& transport, Config config);

} // namespace farcaster::provider::compatibility::signer_attestation
