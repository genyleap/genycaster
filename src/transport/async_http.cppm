module;

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

export module farcaster.transport.async_http;

import farcaster.foundation.result;
import farcaster.transport.http;

export namespace farcaster::transport::async_http {

/** @brief HTTP verb supported by the asynchronous transport reactor. */
enum class Method : std::uint8_t {
    Get,
    Post,
    Put,
    Delete,
};

/** @brief Scheduling priority used for bounded network admission. */
enum class Priority : std::uint8_t {
    Interactive,
    Visible,
    Normal,
    Background,
    Prefetch,
};

/** @brief Retry behavior owned by the asynchronous transport operation. */
enum class RetryPolicy : std::uint8_t {
    None,
    TransientRead,
};

/** @brief Upload progress callback receiving sent and total bytes for the active attempt. */
using UploadProgress = std::function<void(std::uint64_t sentBytes, std::uint64_t totalBytes)>;

/**
 * @brief Owned asynchronous HTTP request.
 *
 * timeout is one overall operation deadline shared by all retry attempts.
 */
struct Request final {
    Method method{Method::Get};
    std::string url{};
    std::vector<std::string> headers{};
    std::string body{};
    std::chrono::milliseconds timeout{15'000};
    std::size_t maxBodyBytes{8U * 1024U * 1024U};
    Priority priority{Priority::Normal};
    RetryPolicy retryPolicy{RetryPolicy::None};
    std::size_t maximumAttempts{1U};
    UploadProgress uploadProgress{};
};

/** @brief Completion called exactly once unless the process is terminating. */
using Completion = std::function<void(foundation::result::Result<http::HttpResponse>)>;

/**
 * @brief Copyable cancellation handle for one asynchronous HTTP operation.
 *
 * Cancellation is idempotent. The transport owns request/reply resources; the
 * handle only requests cancellation and never owns the transport object.
 */
class Operation final {
public:
    /** @brief Creates an inactive operation. */
    Operation() = default;

    /** @brief Creates an operation backed by an independent cancellation closure. */
    explicit Operation(std::function<void()> cancelFunction)
        : m_cancelFunction(std::make_shared<std::function<void()>>(std::move(cancelFunction)))
    {
    }

    /** @brief Requests cancellation. Safe to call repeatedly and from any thread. */
    void cancel() const noexcept
    {
        if (!m_cancelFunction) {
            return;
        }
        try {
            const auto function = *m_cancelFunction;
            if (function) {
                function();
            }
        } catch (...) {
            // Cancellation must never throw across UI/application teardown.
        }
    }

    /** @brief Returns whether this handle can request cancellation. */
    [[nodiscard]] bool isActive() const noexcept
    {
        return static_cast<bool>(m_cancelFunction);
    }

private:
    std::shared_ptr<std::function<void()>> m_cancelFunction{};
};

/**
 * @brief Event-driven provider-neutral HTTP transport.
 *
 * Implementations must not block the caller while waiting for network I/O.
 * Completion may run on the transport reactor thread; CPU-heavy parsing must be
 * dispatched to a bounded CPU executor by the caller.
 */
class IAsyncHttpTransport {
public:
    virtual ~IAsyncHttpTransport() = default;

    /** @brief Starts one operation and returns its cancellation handle. */
    [[nodiscard]] virtual foundation::result::Result<Operation>
    start(Request request, Completion completion) = 0;
};

}
