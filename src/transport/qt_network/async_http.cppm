/*!
 * @file        src/transport/qt_network/async_http.cppm
 * @brief       Qt Network asynchronous HTTP reactor.
 *
 * @details
 * Creates a bounded nonblocking Qt transport that admits requests and
 * completes callbacks without blocking GUI work.
 *
 * @author      Kambiz Asadzadeh
 * @since       04 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include <cstddef>
#include <memory>

export module farcaster.transport.qtnetwork.async;

import farcaster.foundation.result;
import farcaster.transport.async_http;
import farcaster.transport.qtnetwork;

export namespace farcaster::transport::qtnetwork::async {

/** @brief Hard resource limits enforced by one Qt asynchronous network reactor. */
struct NetworkLimits final {
    std::size_t maximumActiveRequests{96U};
    std::size_t maximumActiveRequestsPerOrigin{32U};
    std::size_t maximumBackgroundActiveRequests{64U};
    std::size_t maximumBackgroundActiveRequestsPerOrigin{24U};
    std::size_t maximumAcceptedRequests{320U};
    std::size_t maximumAcceptedRequestBodyBytes{64U * 1024U * 1024U};
    std::size_t maximumBufferedResponseBytes{96U * 1024U * 1024U};
    std::size_t replyReadBufferBytes{256U * 1024U};
};

/**
 * @brief Creates a dedicated event-driven Qt Network reactor.
 *
 * The returned transport owns one QNetworkAccessManager on one dedicated Qt
 * event-loop thread. Callers may start requests from any thread. No request
 * occupies an application worker while waiting for network I/O.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<async_http::IAsyncHttpTransport>>
makeAsyncTransport(
    ProxyPolicy proxyPolicy = ProxyPolicy::System,
    ProtocolPolicy protocolPolicy = ProtocolPolicy::Automatic,
    NetworkLimits limits = {});

}
