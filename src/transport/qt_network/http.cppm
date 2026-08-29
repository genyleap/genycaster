module;

#include <cstdint>
#include <memory>

export module farcaster.transport.qtnetwork;

import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::transport::qtnetwork {

/**
 * @brief Controls whether Qt Network connects directly or follows OS proxy settings.
 */
enum class ProxyPolicy : std::uint8_t {
    Direct,
    System,
};

/**
 * @brief Controls HTTP protocol negotiation for Qt Network requests.
 */
enum class ProtocolPolicy : std::uint8_t {
    Automatic,
    Http1Only,
};

/**
 * @brief Creates a Qt Network-backed blocking HTTP transport for graphical runtimes.
 *
 * Each worker thread reuses one QNetworkAccessManager per proxy policy while
 * each request owns its reply/event-loop state on that same worker thread. The
 * application must already have a QCoreApplication instance. TLS certificate
 * verification remains enabled and Qt uses the platform trust facilities. Proxy
 * participation is an explicit composition policy.
 *
 * @param proxyPolicy Whether requests honor the system proxy or connect directly.
 * @param protocolPolicy Whether Qt may negotiate HTTP/2 or must use HTTP/1.x.
 * @return Owned transport, or a structured construction failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<http::IHttpTransport>>
makeTransport(
    ProxyPolicy proxyPolicy = ProxyPolicy::System,
    ProtocolPolicy protocolPolicy = ProtocolPolicy::Automatic);

}
