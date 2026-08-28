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
    std::string authorizationHeaderName{};
    std::string authorizationHeaderValue{};
    std::chrono::milliseconds timeout{5'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
};

/**
 * @brief Creates a canonical Snapchain protocol-write submit/reconcile adapter.
 *
 * submit() POSTs the exact protobuf Message to `/v1/submitMessage` and validates
 * the returned canonical hash/type/FID. reconcile() resolves Cast, Reaction and
 * Follow desired state through Snapchain's authoritative read endpoints before
 * retrying an ambiguous operation. The borrowed transport must outlive the adapter.
 */
[[nodiscard]] std::unique_ptr<capability::write::IWriteSubmitter> makeSubmitter(
    transport::http::IHttpTransport& transport,
    Config config);

} // namespace farcaster::provider::snapchain::write
