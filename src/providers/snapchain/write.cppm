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

/** @brief Snapchain canonical write/reconciliation HTTP configuration. */
struct Config final {
    std::string baseUrl{};
    std::chrono::milliseconds timeout{15'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
};

/**
 * @brief Creates a canonical Snapchain CAST_ADD submit/reconcile adapter.
 *
 * submit() POSTs the exact protobuf Message to `/v1/submitMessage` and validates
 * the returned canonical hash. reconcile() queries `/v1/castById` by FID/hash so
 * crash/timeout ambiguity is resolved before retry. The borrowed transport must
 * outlive the returned adapter.
 */
[[nodiscard]] std::unique_ptr<capability::write::IWriteSubmitter> makeSubmitter(
    transport::http::IHttpTransport& transport,
    Config config);

} // namespace farcaster::provider::snapchain::write
