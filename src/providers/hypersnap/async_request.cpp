module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.async_request;

import farcaster.execution.weighted_thread_pool;
import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.provider.hypersnap.request;
import farcaster.transport.async_http;
import farcaster.transport.http;

namespace farcaster::provider::hypersnap::async_request {
namespace {

using Error = foundation::error::Error;
using ErrorCode = foundation::error::ErrorCode;

[[nodiscard]] Error makeError(const ErrorCode code, std::string message)
{
    return Error::make(code, std::move(message));
}

[[nodiscard]] bool isUnreserved(const unsigned char character) noexcept
{
    return std::isalnum(character) != 0 || character == '-' || character == '_' ||
        character == '.' || character == '~';
}

[[nodiscard]] std::string percentEncode(const std::string_view value)
{
    constexpr std::array<char, 16> hexadecimal{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    std::string encoded{};
    encoded.reserve(value.size());
    for (const char rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        if (isUnreserved(character)) {
            encoded.push_back(rawCharacter);
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(hexadecimal[(character >> 4U) & 0x0FU]);
        encoded.push_back(hexadecimal[character & 0x0FU]);
    }
    return encoded;
}

[[nodiscard]] std::string makeUrl(
    const config::Config& config,
    const std::string_view path,
    const std::vector<std::pair<std::string, std::string>>& query)
{
    std::string url = config.baseUrl;
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url.append(path);
    if (query.empty()) {
        return url;
    }
    url.push_back('?');
    bool first = true;
    for (const auto& [name, value] : query) {
        if (!first) {
            url.push_back('&');
        }
        first = false;
        url.append(percentEncode(name));
        url.push_back('=');
        url.append(percentEncode(value));
    }
    return url;
}

[[nodiscard]] Error httpStatusError(
    const std::uint16_t statusCode,
    const std::string_view body)
{
    ErrorCode code = ErrorCode::TransportFailure;
    if (statusCode == 400U) {
        code = ErrorCode::InvalidArgument;
    } else if (statusCode == 401U || statusCode == 403U) {
        code = ErrorCode::Unauthorized;
    } else if (statusCode == 404U) {
        code = ErrorCode::NotFound;
    } else if (statusCode == 429U) {
        code = ErrorCode::RateLimited;
    } else if (statusCode >= 500U) {
        code = ErrorCode::Unavailable;
    }
    std::string message = "Hypersnap HTTP status ";
    message.append(std::to_string(statusCode));
    if (!body.empty()) {
        constexpr std::size_t maximumExcerptBytes{320U};
        message.append(": ");
        message.append(body.substr(0U, std::min(body.size(), maximumExcerptBytes)));
    }
    return makeError(code, std::move(message));
}


[[nodiscard]] bool isBackgroundPriority(
    const transport::async_http::Priority priority) noexcept
{
    return priority == transport::async_http::Priority::Background
        || priority == transport::async_http::Priority::Prefetch;
}

[[nodiscard]] transport::async_http::RetryPolicy retryPolicyFor(
    const transport::async_http::Priority priority) noexcept
{
    return isBackgroundPriority(priority)
        ? transport::async_http::RetryPolicy::None
        : transport::async_http::RetryPolicy::TransientRead;
}

[[nodiscard]] std::size_t maximumAttemptsFor(
    const transport::async_http::Priority priority) noexcept
{
    return isBackgroundPriority(priority) ? 1U : 3U;
}

}

struct Operation::State final {
    std::atomic_bool cancelled{false};
    transport::async_http::Operation network{};
};

Operation::Operation(std::shared_ptr<State> state) noexcept
    : m_state(std::move(state))
{
}

void Operation::cancel() const noexcept
{
    if (!m_state) {
        return;
    }
    m_state->cancelled.store(true, std::memory_order_release);
    m_state->network.cancel();
}

bool Operation::isActive() const noexcept
{
    return static_cast<bool>(m_state);
}

Client::Client(
    transport::async_http::IAsyncHttpTransport& transport,
    execution::weighted_thread_pool::WeightedThreadPoolExecutor& decodeExecutor,
    config::Config config)
    : m_transport(transport),
      m_decodeExecutor(decodeExecutor),
      m_config(std::move(config))
{
}

const config::Config& Client::config() const noexcept
{
    return m_config;
}

foundation::result::Result<Operation> Client::getJson(
    const std::string_view path,
    const std::vector<std::pair<std::string, std::string>>& query,
    JsonCompletion completion,
    const request::GetOptions options,
    const transport::async_http::Priority priority)
{
    if (!completion || options.timeout <= std::chrono::milliseconds::zero() ||
        options.maxBodyBytes == 0U) {
        return std::unexpected(makeError(
            ErrorCode::InvalidArgument,
            "Hypersnap async GET requires a completion, deadline and body limit."));
    }
    auto state = std::make_shared<Operation::State>();
    auto start = m_transport.start(
        transport::async_http::Request{
            .method = transport::async_http::Method::Get,
            .url = makeUrl(m_config, path, query),
            .headers = {"Accept: application/json"},
            .body = {},
            .timeout = options.timeout,
            .maxBodyBytes = options.maxBodyBytes,
            .priority = priority,
            .retryPolicy = retryPolicyFor(priority),
            .maximumAttempts = maximumAttemptsFor(priority),
        },
        [this, state, completion = std::move(completion)](
            foundation::result::Result<transport::http::HttpResponse> response) mutable {
            if (state->cancelled.load(std::memory_order_acquire)) {
                return;
            }
            if (!response) {
                completion(std::unexpected(std::move(response.error())));
                return;
            }
            if (response->statusCode < 200U || response->statusCode >= 300U) {
                completion(std::unexpected(httpStatusError(response->statusCode, response->body)));
                return;
            }
            const std::size_t retainedBytes = response->body.size();
            auto body = std::move(response->body);
            auto status = m_decodeExecutor.post(
                retainedBytes,
                [state, completion = std::move(completion), body = std::move(body)]() mutable {
                    if (!state->cancelled.load(std::memory_order_acquire)) {
                        completion(std::move(body));
                    }
                });
            if (!status && !state->cancelled.load(std::memory_order_acquire)) {
                completion(std::unexpected(std::move(status.error())));
            }
        });
    if (!start) {
        return std::unexpected(std::move(start.error()));
    }
    state->network = std::move(*start);
    return Operation{std::move(state)};
}

foundation::result::Result<Operation> Client::postReadJson(
    const std::string_view path,
    std::string body,
    JsonCompletion completion,
    const request::GetOptions options,
    const transport::async_http::Priority priority)
{
    if (!completion || options.timeout <= std::chrono::milliseconds::zero() ||
        options.maxBodyBytes == 0U) {
        return std::unexpected(makeError(
            ErrorCode::InvalidArgument,
            "Hypersnap async read POST requires a completion, deadline and body limit."));
    }
    auto state = std::make_shared<Operation::State>();
    auto start = m_transport.start(
        transport::async_http::Request{
            .method = transport::async_http::Method::Post,
            .url = makeUrl(m_config, path, {}),
            .headers = {"Accept: application/json", "Content-Type: application/json"},
            .body = std::move(body),
            .timeout = options.timeout,
            .maxBodyBytes = options.maxBodyBytes,
            .priority = priority,
            .retryPolicy = retryPolicyFor(priority),
            .maximumAttempts = maximumAttemptsFor(priority),
        },
        [this, state, completion = std::move(completion)](
            foundation::result::Result<transport::http::HttpResponse> response) mutable {
            if (state->cancelled.load(std::memory_order_acquire)) {
                return;
            }
            if (!response) {
                completion(std::unexpected(std::move(response.error())));
                return;
            }
            if (response->statusCode < 200U || response->statusCode >= 300U) {
                completion(std::unexpected(httpStatusError(response->statusCode, response->body)));
                return;
            }
            const std::size_t retainedBytes = response->body.size();
            auto responseBody = std::move(response->body);
            auto status = m_decodeExecutor.post(
                retainedBytes,
                [state, completion = std::move(completion), body = std::move(responseBody)]() mutable {
                    if (!state->cancelled.load(std::memory_order_acquire)) {
                        completion(std::move(body));
                    }
                });
            if (!status && !state->cancelled.load(std::memory_order_acquire)) {
                completion(std::unexpected(std::move(status.error())));
            }
        });
    if (!start) {
        return std::unexpected(std::move(start.error()));
    }
    state->network = std::move(*start);
    return Operation{std::move(state)};
}

}
