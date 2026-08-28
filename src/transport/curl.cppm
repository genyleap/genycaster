module;

#include <memory>

export module farcaster.transport.curl;

import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::transport::curl {

/**
 * @brief Creates a libcurl-backed blocking HTTP transport.
 * @return Owned transport or a structured initialization failure.
 */
[[nodiscard]] foundation::result::Result<std::unique_ptr<http::IHttpTransport>>
makeTransport();

}
