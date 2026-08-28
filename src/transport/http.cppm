module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

export module farcaster.transport.http;

import farcaster.foundation.result;

export namespace farcaster::transport::http {

/** @brief Immutable HTTP request for provider adapter operations. */
struct HttpRequest final {
    std::string url{};
    std::vector<std::string> headers{};
    std::chrono::milliseconds timeout{15'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
};

/** @brief Owned HTTP response returned by a transport adapter. */
struct HttpResponse final {
    std::uint16_t statusCode{};
    std::string body{};
};

/** @brief Immutable HTTP POST request with an owned UTF-8/binary body. */
struct HttpPostRequest final {
    std::string url{};
    std::vector<std::string> headers{};
    std::string body{};
    std::chrono::milliseconds timeout{15'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
};

/**
 * @brief Blocking provider-neutral HTTP transport.
 *
 * Implementations own transport-library details. Calls may block and therefore
 * must execute on an I/O executor rather than the Qt GUI thread.
 */
class IHttpTransport {
public:
    /** @brief Destroys the transport. */
    virtual ~IHttpTransport() = default;

    /** @brief Executes one HTTP GET request and returns the complete response body. */
    [[nodiscard]] virtual foundation::result::Result<HttpResponse>
    get(const HttpRequest& request) = 0;

    /** @brief Executes one HTTP POST request and returns the complete response body. */
    [[nodiscard]] virtual foundation::result::Result<HttpResponse>
    post(const HttpPostRequest& request) = 0;
};

}
