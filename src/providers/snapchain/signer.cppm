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
