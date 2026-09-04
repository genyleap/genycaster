module;

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.request;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.transport.http;

namespace farcaster::provider::hypersnap::request {
namespace {

constexpr std::size_t maxGetAttempts{3U};
constexpr std::chrono::milliseconds retryBaseDelay{150};
constexpr std::chrono::milliseconds retryMaximumDelay{1'200};

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] std::string trimTrailingSlashes(std::string value)
{
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] bool isUnreserved(const unsigned char character)
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

[[nodiscard]] std::string responseExcerpt(const std::string_view body)
{
    constexpr std::size_t maxLength{320};
    std::string excerpt{};
    excerpt.reserve(std::min(body.size(), maxLength));
    bool previousWhitespace{false};
    for (const char rawCharacter : body) {
        if (excerpt.size() >= maxLength) {
            break;
        }
        const auto character = static_cast<unsigned char>(rawCharacter);
        const bool whitespace = std::isspace(character) != 0;
        if (whitespace) {
            if (!excerpt.empty() && !previousWhitespace) {
                excerpt.push_back(' ');
            }
            previousWhitespace = true;
            continue;
        }
        if (std::iscntrl(character) != 0) {
            continue;
        }
        excerpt.push_back(rawCharacter);
        previousWhitespace = false;
    }
    while (!excerpt.empty() && excerpt.back() == ' ') {
        excerpt.pop_back();
    }
    if (body.size() > maxLength) {
        excerpt.append("...");
    }
    return excerpt;
}

[[nodiscard]] foundation::error::Error httpStatusError(
    const std::uint16_t statusCode,
    const std::string_view responseBody)
{
    foundation::error::ErrorCode code = foundation::error::ErrorCode::ProtocolFailure;
    switch (statusCode) {
    case 400:
        code = foundation::error::ErrorCode::InvalidArgument;
        break;
    case 401:
    case 403:
        code = foundation::error::ErrorCode::Unauthorized;
        break;
    case 404:
        code = foundation::error::ErrorCode::NotFound;
        break;
    case 405:
    case 501:
        code = foundation::error::ErrorCode::Unsupported;
        break;
    case 408:
        code = foundation::error::ErrorCode::Timeout;
        break;
    case 409:
        code = foundation::error::ErrorCode::Conflict;
        break;
    case 429:
        code = foundation::error::ErrorCode::RateLimited;
        break;
    default:
        if (statusCode >= 500) {
            code = foundation::error::ErrorCode::Unavailable;
        }
        break;
    }

    std::string message = "Hypersnap HTTP request failed with status " + std::to_string(statusCode) + ".";
    const std::string excerpt = responseExcerpt(responseBody);
    if (!excerpt.empty()) {
        message.append(" Provider response: ");
        message.append(excerpt);
    }
    return makeError(code, std::move(message));
}

[[nodiscard]] bool isRetryableStatus(const std::uint16_t statusCode) noexcept
{
    return statusCode == 408 || statusCode == 429 || statusCode == 500 ||
        statusCode == 502 || statusCode == 503 || statusCode == 504;
}

[[nodiscard]] bool isRetryableTransportFailure(
    const foundation::error::ErrorCode code) noexcept
{
    using foundation::error::ErrorCode;
    return code == ErrorCode::Timeout || code == ErrorCode::TransportFailure ||
        code == ErrorCode::Unavailable || code == ErrorCode::RateLimited;
}

[[nodiscard]] char asciiLower(const char character) noexcept
{
    const auto raw = static_cast<unsigned char>(character);
    return static_cast<char>(std::tolower(raw));
}

[[nodiscard]] bool equalsAsciiCaseInsensitive(
    const std::string_view left,
    const std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (asciiLower(left[index]) != asciiLower(right[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string_view trimAsciiWhitespace(std::string_view value) noexcept
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] std::optional<std::string_view> headerValue(
    const std::vector<std::string>& headers,
    const std::string_view name) noexcept
{
    for (const std::string& header : headers) {
        const std::size_t separator = header.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string_view headerName{header.data(), separator};
        if (!equalsAsciiCaseInsensitive(trimAsciiWhitespace(headerName), name)) {
            continue;
        }
        return trimAsciiWhitespace(std::string_view{header}.substr(separator + 1U));
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::chrono::milliseconds> retryAfterDelay(
    const transport::http::HttpResponse& response) noexcept
{
    const auto value = headerValue(response.headers, "Retry-After");
    if (!value || value->empty()) {
        return std::nullopt;
    }

    std::uint64_t seconds{};
    for (const char character : *value) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (seconds > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) {
            return std::nullopt;
        }
        seconds = seconds * 10U + digit;
    }

    constexpr std::uint64_t maxMilliseconds =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (seconds > maxMilliseconds / 1'000U) {
        return std::nullopt;
    }
    return std::chrono::milliseconds{static_cast<std::int64_t>(seconds * 1'000U)};
}

[[nodiscard]] std::chrono::milliseconds jitteredBackoff(const std::size_t completedAttempts)
{
    const std::size_t exponent = std::min<std::size_t>(completedAttempts - 1U, 3U);
    const auto scaled = retryBaseDelay * static_cast<std::int64_t>(1ULL << exponent);
    const auto cap = std::min(scaled, retryMaximumDelay);
    thread_local std::mt19937_64 generator{std::random_device{}()};
    std::uniform_int_distribution<std::int64_t> distribution{0, cap.count()};
    return std::chrono::milliseconds{distribution(generator)};
}

[[nodiscard]] std::chrono::milliseconds remainingBudget(
    const std::chrono::steady_clock::time_point deadline) noexcept
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return std::chrono::milliseconds::zero();
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    return std::max(remaining, std::chrono::milliseconds{1});
}

[[nodiscard]] bool waitForRetry(
    std::chrono::milliseconds delay,
    const std::chrono::steady_clock::time_point deadline)
{
    const auto remaining = remainingBudget(deadline);
    if (remaining <= std::chrono::milliseconds::zero() || delay >= remaining) {
        return false;
    }
    if (delay > std::chrono::milliseconds::zero()) {
        std::this_thread::sleep_for(delay);
    }
    return remainingBudget(deadline) > std::chrono::milliseconds::zero();
}

[[nodiscard]] std::string makeUrl(
    const config::Config& config,
    const std::string_view path,
    const std::vector<std::pair<std::string, std::string>>& query)
{
    std::string url = trimTrailingSlashes(config.baseUrl);
    url.append(path);
    if (query.empty()) {
        return url;
    }
    url.push_back('?');
    bool isFirst = true;
    for (const auto& [name, value] : query) {
        if (!isFirst) {
            url.push_back('&');
        }
        isFirst = false;
        url.append(percentEncode(name));
        url.push_back('=');
        url.append(percentEncode(value));
    }
    return url;
}

[[nodiscard]] foundation::result::Result<std::string> performPost(
    transport::http::IHttpTransport& transport,
    const std::string& url,
    std::string body,
    const GetOptions& options)
{
    auto response = transport.post(transport::http::HttpPostRequest{
        .url = url,
        .headers = {"Accept: application/json", "Content-Type: application/json"},
        .body = std::move(body),
        .timeout = options.timeout,
        .maxBodyBytes = options.maxBodyBytes,
    });
    if (!response) {
        return std::unexpected(std::move(response.error()));
    }
    if (response->statusCode < 200 || response->statusCode >= 300) {
        return std::unexpected(httpStatusError(response->statusCode, response->body));
    }
    return std::move(response->body);
}

[[nodiscard]] std::string queryValue(
    const std::vector<std::pair<std::string, std::string>>& query,
    const std::string_view name)
{
    for (const auto& [queryName, value] : query) {
        if (queryName == name) {
            return value;
        }
    }
    return "n/a";
}

[[nodiscard]] foundation::error::Error withGetDiagnostics(
    foundation::error::Error error,
    const std::string_view path,
    const std::vector<std::pair<std::string, std::string>>& query,
    const GetOptions& options,
    const std::size_t attempts)
{
    std::string diagnostic = "Endpoint: ";
    diagnostic.append(path);
    diagnostic.append("\nLimit: ");
    diagnostic.append(queryValue(query, "limit"));
    diagnostic.append("\nDeadline: ");
    diagnostic.append(std::to_string(options.timeout.count()));
    diagnostic.append("ms\nAttempts: ");
    diagnostic.append(std::to_string(attempts));
    diagnostic.push_back('\n');
    diagnostic.append(error.message);
    error.message = std::move(diagnostic);
    return error;
}

}

foundation::result::Status validateConfig(const config::Config& config)
{
    if (config.baseUrl.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap base URL must not be empty."));
    }
    if (config.provider.id.value.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap provider id must not be empty."));
    }
    if (!config.baseUrl.starts_with("https://") && !config.baseUrl.starts_with("http://")) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap base URL must use http or https."));
    }
    if (config.baseUrl.find_first_of("?#\r\n") != std::string::npos) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap base URL must be an origin/root URL without query, fragment, or control characters."));
    }
    return {};
}

Client::Client(transport::http::IHttpTransport& transport, config::Config config)
    : m_transport(transport), m_config(std::move(config))
{
}

const config::Config& Client::config() const noexcept
{
    return m_config;
}

foundation::result::Result<std::string> Client::getJson(
    const std::string_view path,
    const std::vector<std::pair<std::string, std::string>>& query,
    const GetOptions options)
{
    if (options.timeout <= std::chrono::milliseconds::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap GET deadline must be greater than zero."));
    }
    if (options.maxBodyBytes == 0U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap GET maximum response-body size must be greater than zero."));
    }

    const std::string url = makeUrl(m_config, path, query);
    const auto deadline = std::chrono::steady_clock::now() + options.timeout;
    foundation::error::Error lastError = makeError(
        foundation::error::ErrorCode::InternalFailure,
        "Hypersnap GET did not execute.");
    std::size_t attempts{};

    while (attempts < maxGetAttempts) {
        const auto remaining = remainingBudget(deadline);
        if (remaining <= std::chrono::milliseconds::zero()) {
            if (attempts == 0U) {
                lastError = makeError(
                    foundation::error::ErrorCode::Timeout,
                    "Hypersnap GET deadline expired before transport execution.");
            }
            break;
        }

        const auto attemptTimeout = attempts == 0U ? options.timeout : remaining;
        ++attempts;
        auto response = m_transport.get(transport::http::HttpRequest{
            .url = url,
            .headers = {"Accept: application/json"},
            .timeout = attemptTimeout,
            .maxBodyBytes = options.maxBodyBytes,
        });

        if (!response) {
            lastError = std::move(response.error());
            if (attempts >= maxGetAttempts || !isRetryableTransportFailure(lastError.code)) {
                break;
            }
            if (!waitForRetry(jitteredBackoff(attempts), deadline)) {
                break;
            }
            continue;
        }

        if (response->statusCode >= 200 && response->statusCode < 300) {
            return std::move(response->body);
        }

        lastError = httpStatusError(response->statusCode, response->body);
        if (attempts >= maxGetAttempts || !isRetryableStatus(response->statusCode)) {
            break;
        }

        const auto delay = retryAfterDelay(*response).value_or(jitteredBackoff(attempts));
        if (!waitForRetry(delay, deadline)) {
            break;
        }
    }

    return std::unexpected(withGetDiagnostics(
        std::move(lastError), path, query, options, attempts));
}

foundation::result::Result<std::string> Client::postJson(
    const std::string_view path,
    std::string body,
    const GetOptions options)
{
    auto result = performPost(m_transport, makeUrl(m_config, path, {}), std::move(body), options);
    if (result) {
        return result;
    }
    foundation::error::Error error = std::move(result.error());
    error.message = "Endpoint: " + std::string{path} + "\nDeadline: " +
        std::to_string(options.timeout.count()) + "ms\n" + error.message;
    return std::unexpected(std::move(error));
}

}
