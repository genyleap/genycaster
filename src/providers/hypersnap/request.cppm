module;

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module farcaster.provider.hypersnap.request;

import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.transport.http;

export namespace farcaster::provider::hypersnap::request {

/**
 * @brief Per-request transport budget for one Hypersnap JSON read.
 *
 * Provider capability implementations may override the default budget when an
 * endpoint performs materially heavier server-side aggregation. The finite
 * budget remains provider-local and never becomes an Application concern.
 */
struct GetOptions final {
    std::chrono::milliseconds timeout{15'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
};

/**
 * @brief Blocking Hypersnap HTTP request client over a borrowed generic transport.
 *
 * The transport must outlive this client. Provider URL/query semantics stay
 * inside this module rather than leaking into capability implementations.
 */
class Client final {
public:
    /** @brief Creates a request client over borrowed transport and owned configuration. */
    Client(transport::http::IHttpTransport& transport, config::Config config);

    /** @brief Returns the configured provider-instance metadata. */
    [[nodiscard]] const config::Config& config() const noexcept;

    /**
     * @brief Executes a percent-encoded Hypersnap v2 JSON GET.
     * @param path Provider-local endpoint path.
     * @param query Provider-local query parameters.
     * @param options Finite transport budget and response-size limit.
     */
    [[nodiscard]] foundation::result::Result<std::string> getJson(
        std::string_view path,
        const std::vector<std::pair<std::string, std::string>>& query,
        GetOptions options = {});

    /** @brief Executes a Hypersnap JSON POST with a caller-owned serialized body. */
    [[nodiscard]] foundation::result::Result<std::string> postJson(
        std::string_view path,
        std::string body,
        GetOptions options = {});

private:
    transport::http::IHttpTransport& m_transport;
    config::Config m_config{};
};

/** @brief Validates adapter configuration before provider services are composed. */
[[nodiscard]] foundation::result::Status validateConfig(const config::Config& config);

}
