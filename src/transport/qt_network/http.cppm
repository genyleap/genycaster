module;

#include <memory>

export module farcaster.transport.qtnetwork;

import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::transport::qtnetwork {

/**
 * @brief Creates a Qt Network-backed blocking HTTP transport for graphical runtimes.
 *
 * Each request owns its Qt network objects on the calling worker thread. The
 * application must already have a QCoreApplication instance. TLS certificate
 * verification remains enabled and Qt uses the platform's configured trust and
 * proxy facilities.
 *
 * @return Owned transport, or a structured construction failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<http::IHttpTransport>>
makeTransport();

}
