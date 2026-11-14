/*!
 * @file        src/providers/snapchain/write.cppm
 * @brief       Snapchain signed message submitter.
 *
 * @details
 * Submits exact canonical envelopes and confirms outcomes without blindly
 * repeating ambiguous writes.
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

export module farcaster.provider.snapchain.write;

import farcaster.foundation.result;
import farcaster.transport.http;
import farcaster.capability.write_submit;

export namespace farcaster::provider::snapchain::write {

/**
 * @brief Snapchain canonical write/reconciliation HTTP configuration.
 *
 * Composition supplies the deployment origin. The adapter is intentionally
 * unaware of gateway/vendor identity; optional HTTP credentials are modeled
 * only as an explicit header name/value pair.
 */
struct Config final {
    std::string baseUrl{};
    // Optional independent authoritative read origin for reconciliation.
    // The base URL above is the signed submission destination.
    std::string readBaseUrl{};
    std::string readAuthorizationHeaderName{};
    std::string readAuthorizationHeaderValue{};
    std::string authorizationHeaderName{};
    std::string authorizationHeaderValue{};
    std::chrono::milliseconds timeout{5'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
    // Only the HyperSnap-native writer may probe its own shard-0 registry
    // after a rejected unknown-FID message. Never use this with Farcaster.
    bool probeNativeRegistryOnUnknownFid{false};
};

/**
 * @brief Creates a canonical Snapchain protocol-write submit/reconcile adapter.
 *
 * submit() POSTs the exact protobuf Message to `/v1/submitMessage` at the
 * selected write-capable origin and validates canonical hash/type/FID.
 * reconcile() resolves Cast, Reaction and Follow state through the independent
 * authoritative read origin before retrying an ambiguous operation. The borrowed transport must outlive the adapter.
 */
[[nodiscard]] std::unique_ptr<capability::write::IWriteSubmitter> makeSubmitter(
    transport::http::IHttpTransport& transport,
    Config config);

} // namespace farcaster::provider::snapchain::write
