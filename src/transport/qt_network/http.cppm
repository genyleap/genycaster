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
 * @brief Creates a Qt Network-backed blocking HTTP transport for graphical runtimes.
 *
 * Each request owns its Qt network objects on the calling worker thread. The
 * application must already have a QCoreApplication instance. TLS certificate
 * verification remains enabled and Qt uses the platform trust facilities. Proxy
 * participation is an explicit composition policy.
 *
 * @return Owned transport, or a structured construction failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<http::IHttpTransport>>
makeTransport(ProxyPolicy proxyPolicy = ProxyPolicy::System);

}
