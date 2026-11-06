/*!
 * @file        src/transport/curl.cppm
 * @brief       libcurl HTTP transport factory.
 *
 * @details
 * Provides a blocking C++ HTTP adapter for provider code running on
 * controlled I/O executors.
 *
 * @author      Kambiz Asadzadeh
 * @since       28 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

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
