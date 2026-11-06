/*!
 * @file        tests/providers/hypersnap/fake_transport.cppm
 * @brief       Deterministic HTTP fake for provider tests.
 *
 * @details
 * Queues controlled responses and records transport operations without
 * accessing external network services.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

export module farcaster.test.hypersnap.transport;

import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::test::hypersnap::transport {

/** @brief Deterministic queued HTTP transport for provider contract tests. */
class FakeTransport final : public farcaster::transport::http::IHttpTransport {
public:
    /** @brief Appends one response to the deterministic response queue. */
    void enqueue(
        std::uint16_t statusCode,
        std::string body,
        std::vector<std::string> headers = {});

    /** @brief Returns the next queued response and records the requested URL. */
    [[nodiscard]] farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse>
    get(const farcaster::transport::http::HttpRequest& request) override;

    /** @brief Returns the next queued response for an HTTP POST. */
    [[nodiscard]] farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse>
    post(const farcaster::transport::http::HttpPostRequest& request) override;

    /** @brief Returns requested URLs in call order. */
    [[nodiscard]] const std::vector<std::string>& urls() const noexcept;

    /** @brief Returns complete provider-neutral requests in call order. */
    [[nodiscard]] const std::vector<farcaster::transport::http::HttpRequest>& requests() const noexcept;

    /** @brief Returns captured POST requests for header-isolation assertions. */
    [[nodiscard]] const std::vector<farcaster::transport::http::HttpPostRequest>& postRequests() const noexcept;

private:
    std::deque<farcaster::transport::http::HttpResponse> m_responses{};
    std::vector<std::string> m_urls{};
    std::vector<farcaster::transport::http::HttpRequest> m_requests{};
    std::vector<farcaster::transport::http::HttpPostRequest> m_postRequests{};
};

}
