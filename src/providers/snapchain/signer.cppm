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
 * The default targets Farcaster's public mainnet node. Composition may replace
 * it with a self-hosted or otherwise policy-approved Snapchain origin.
 * Credentials are intentionally absent from this value.
 */
struct Config final {
    std::string baseUrl{"https://crackle.farcaster.xyz:3381"};
    std::chrono::milliseconds timeout{15'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
};

/**
 * @brief Creates a production Snapchain unified signer-registry adapter.
 *
 * Inspection uses `/v1/signersByFid`. Gasless KEY_ADD and self-revoking
 * KEY_REMOVE are encoded as canonical protobuf messages and submitted through
 * `/v1/submitMessage`, then re-read from Snapchain before success is returned.
 * The borrowed HTTP transport must outlive the returned adapter.
 */
[[nodiscard]] std::unique_ptr<capability::signer::ISignerRegistry> makeRegistry(
    transport::http::IHttpTransport& transport,
    Config config);

}
