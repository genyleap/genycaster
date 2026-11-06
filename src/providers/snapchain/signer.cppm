/*!
 * @file        src/providers/snapchain/signer.cppm
 * @brief       Snapchain signer registry adapter.
 *
 * @details
 * Reads and reconciles signer registration and revocation state through a
 * chosen protocol endpoint.
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

export module farcaster.provider.snapchain.signer;

import farcaster.capability.signer;
import farcaster.transport.http;

export namespace farcaster::provider::snapchain::signer {

/**
 * @brief Snapchain signer-registry HTTP configuration owned by composition code.
 *
 * Composition supplies the deployment origin. The adapter is intentionally
 * unaware of gateway/vendor identity; optional HTTP credentials are modeled
 * only as an explicit header name/value pair.
 */
struct Config final {
    std::string baseUrl{};
    // Optional validator/submit endpoint. When absent the node serves both
    // queries and writes; read-only nodes require an explicit validator URL.
    std::string submitBaseUrl{};
    // Credentials are route-scoped: read-node credentials never follow a
    // submission to a different validator origin.
    std::string submitAuthorizationHeaderName{};
    std::string submitAuthorizationHeaderValue{};
    std::string authorizationHeaderName{};
    std::string authorizationHeaderValue{};
    std::chrono::milliseconds timeout{15'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};

    /** @brief Maximum time spent waiting for submitted signer state to become authoritative. */
    std::chrono::milliseconds confirmationTimeout{60'000};

    /** @brief Delay between signer-registry reads during bounded confirmation. */
    std::chrono::milliseconds confirmationPollInterval{500};
};

/**
 * @brief Creates a production Snapchain unified signer-registry adapter.
 *
 * Inspection uses `/v1/signersByFid`. Gasless KEY_ADD and self-revoking
 * KEY_REMOVE are encoded as canonical protobuf messages and submitted through
 * `/v1/submitMessage`, then polled from Snapchain for a bounded confirmation
 * window before success is returned.
 * The borrowed HTTP transport must outlive the returned adapter.
 */
[[nodiscard]] std::unique_ptr<capability::signer::ISignerRegistry> makeRegistry(
    transport::http::IHttpTransport& transport,
    Config config);

}
