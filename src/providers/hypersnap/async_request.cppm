module;

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module farcaster.provider.hypersnap.async_request;

import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.provider.hypersnap.request;
import farcaster.transport.async_http;

export namespace farcaster::provider::hypersnap::async_request {

/** @brief Completion for one decoded-as-bytes JSON operation. */
using JsonCompletion = std::function<void(foundation::result::Result<std::string>)>;

/** @brief Cancellation handle spanning network I/O and queued decode delivery. */
class Operation final {
public:
    Operation() = default;
    /** @brief Requests cancellation and suppresses future completion delivery. */
    void cancel() const noexcept;
    /** @brief Returns whether this handle represents an operation. */
    [[nodiscard]] bool isActive() const noexcept;

private:
    struct State;
    explicit Operation(std::shared_ptr<State> state) noexcept;
    std::shared_ptr<State> m_state{};
    friend class Client;
};

/**
 * @brief Event-driven Hypersnap JSON client.
 *
 * Network waiting occurs on the shared asynchronous transport reactor. Complete
 * response ownership is transferred to a byte-weighted CPU pool before provider
 * parsing, preventing JSON work from stalling the network reactor.
 */
class Client final {
public:
    Client(
        transport::async_http::IAsyncHttpTransport& transport,
        execution::weighted_thread_pool::WeightedThreadPoolExecutor& decodeExecutor,
        config::Config config);

    [[nodiscard]] const config::Config& config() const noexcept;

    [[nodiscard]] foundation::result::Result<Operation> getJson(
        std::string_view path,
        const std::vector<std::pair<std::string, std::string>>& query,
        JsonCompletion completion,
        request::GetOptions options = {},
        transport::async_http::Priority priority = transport::async_http::Priority::Normal);

    /**
     * @brief Starts an idempotent read-side JSON POST such as Hypersnap batch hydration.
     */
    [[nodiscard]] foundation::result::Result<Operation> postReadJson(
        std::string_view path,
        std::string body,
        JsonCompletion completion,
        request::GetOptions options = {},
        transport::async_http::Priority priority = transport::async_http::Priority::Normal);

private:
    transport::async_http::IAsyncHttpTransport& m_transport;
    execution::weighted_thread_pool::WeightedThreadPoolExecutor& m_decodeExecutor;
    config::Config m_config{};
};

}
