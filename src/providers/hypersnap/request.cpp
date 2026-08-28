module;

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.request;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.provider.hypersnap.config;
import farcaster.transport.http;

namespace farcaster::provider::hypersnap::request {
namespace {

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
    case 401:
    case 403:
        code = foundation::error::ErrorCode::Unauthorized;
        break;
    case 404:
        code = foundation::error::ErrorCode::NotFound;
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

static std::string makeUrl(
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

static foundation::result::Result<std::string> performGet(
    transport::http::IHttpTransport& transport,
    const std::string& url,
    const farcaster::provider::hypersnap::request::GetOptions& options)
{
    auto response = transport.get(transport::http::HttpRequest{
        .url = url,
        .headers = {"Accept: application/json"},
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
    const farcaster::provider::hypersnap::request::GetOptions options)
{
    auto result = performGet(m_transport, makeUrl(m_config, path, query), options);
    if (result) {
        return result;
    }

    foundation::error::Error error = std::move(result.error());
    std::string diagnostic = "Endpoint: ";
    diagnostic.append(path);
    diagnostic.append("\nLimit: ");
    diagnostic.append(queryValue(query, "limit"));
    diagnostic.append("\nDeadline: ");
    diagnostic.append(std::to_string(options.timeout.count()));
    diagnostic.append("ms\n");
    diagnostic.append(error.message);
    error.message = std::move(diagnostic);
    return std::unexpected(std::move(error));
}

}
