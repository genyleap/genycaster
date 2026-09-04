module;

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

module farcaster.transport.curl;

import farcaster.foundation.error;
import farcaster.foundation.result;
import farcaster.transport.http;

namespace farcaster::transport::curl {
namespace {

std::once_flag curlInitializationFlag{};
CURLcode curlInitializationCode{CURLE_OK};

void initializeCurl()
{
    curlInitializationCode = curl_global_init(CURL_GLOBAL_DEFAULT);
}

struct BodyAccumulator final {
    std::string* body{};
    std::size_t maxBytes{};
    bool exceeded{};
};

struct HeaderAccumulator final {
    std::vector<std::string>* headers{};
};

[[nodiscard]] std::size_t appendBody(
    char* data,
    const std::size_t size,
    const std::size_t count,
    void* userData)
{
    if (data == nullptr || userData == nullptr || size == 0) {
        return 0;
    }

    auto* accumulator = static_cast<BodyAccumulator*>(userData);
    if (accumulator->body == nullptr || count > (std::numeric_limits<std::size_t>::max() / size)) {
        return 0;
    }

    const std::size_t byteCount = size * count;
    if (byteCount > accumulator->maxBytes ||
        accumulator->body->size() > accumulator->maxBytes - byteCount) {
        accumulator->exceeded = true;
        return 0;
    }

    accumulator->body->append(data, byteCount);
    return byteCount;
}

[[nodiscard]] std::size_t appendHeader(
    char* data,
    const std::size_t size,
    const std::size_t count,
    void* userData)
{
    if (data == nullptr || userData == nullptr || size == 0 ||
        count > (std::numeric_limits<std::size_t>::max() / size)) {
        return 0;
    }

    const std::size_t byteCount = size * count;
    auto* accumulator = static_cast<HeaderAccumulator*>(userData);
    if (accumulator->headers == nullptr) {
        return 0;
    }

    std::string line{data, byteCount};
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    if (line.starts_with("HTTP/")) {
        accumulator->headers->clear();
    } else if (!line.empty() && line.find(':') != std::string::npos) {
        accumulator->headers->push_back(std::move(line));
    }
    return byteCount;
}

[[nodiscard]] foundation::error::Error transportError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

class CurlHttpTransport final : public http::IHttpTransport {
public:
    [[nodiscard]] foundation::result::Result<http::HttpResponse>
    get(const http::HttpRequest& request) override
    {
        return perform(
            request.url,
            request.headers,
            request.timeout,
            request.maxBodyBytes,
            nullptr);
    }

    [[nodiscard]] foundation::result::Result<http::HttpResponse>
    post(const http::HttpPostRequest& request) override
    {
        return perform(
            request.url,
            request.headers,
            request.timeout,
            request.maxBodyBytes,
            &request.body);
    }

private:
    [[nodiscard]] foundation::result::Result<http::HttpResponse> perform(
        const std::string& url,
        const std::vector<std::string>& requestHeaders,
        const std::chrono::milliseconds timeout,
        const std::size_t maxBodyBytes,
        const std::string* const requestBody)
    {
        if (url.empty()) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP request URL must not be empty."));
        }

        using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
        CurlHandle handle{curl_easy_init(), &curl_easy_cleanup};
        if (!handle) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::TransportFailure,
                "libcurl could not create an easy handle."));
        }

        if (maxBodyBytes == 0) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::InvalidArgument,
                "HTTP maximum response-body size must be greater than zero."));
        }

        std::string responseBody{};
        std::vector<std::string> responseHeaders{};
        BodyAccumulator accumulator{
            .body = &responseBody,
            .maxBytes = maxBodyBytes,
        };
        HeaderAccumulator headerAccumulator{.headers = &responseHeaders};
        curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(handle.get(), CURLOPT_USERAGENT, "Genycaster/0.5");
        curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, &appendBody);
        curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &accumulator);
        curl_easy_setopt(handle.get(), CURLOPT_HEADERFUNCTION, &appendHeader);
        curl_easy_setopt(handle.get(), CURLOPT_HEADERDATA, &headerAccumulator);
        curl_easy_setopt(handle.get(), CURLOPT_MAXREDIRS, 5L);
        if (requestBody != nullptr) {
            curl_easy_setopt(handle.get(), CURLOPT_POST, 1L);
            curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, requestBody->data());
            curl_easy_setopt(
                handle.get(),
                CURLOPT_POSTFIELDSIZE_LARGE,
                static_cast<curl_off_t>(requestBody->size()));
        }

        const auto timeoutCount = timeout.count();
        const auto clampedTimeout = std::clamp<std::int64_t>(
            timeoutCount,
            1,
            static_cast<std::int64_t>(std::numeric_limits<long>::max()));
        curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(clampedTimeout));
        curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(clampedTimeout));

        curl_slist* rawHeaders = nullptr;
        for (const std::string& header : requestHeaders) {
            curl_slist* appended = curl_slist_append(rawHeaders, header.c_str());
            if (appended == nullptr) {
                curl_slist_free_all(rawHeaders);
                return std::unexpected(transportError(
                    foundation::error::ErrorCode::TransportFailure,
                    "libcurl could not allocate the HTTP header list."));
            }
            rawHeaders = appended;
        }
        using HeaderList = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
        HeaderList headers{rawHeaders, &curl_slist_free_all};
        if (headers) {
            curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get());
        }

        const CURLcode performCode = curl_easy_perform(handle.get());
        if (performCode != CURLE_OK) {
            if (accumulator.exceeded) {
                return std::unexpected(transportError(
                    foundation::error::ErrorCode::TransportFailure,
                    "HTTP response body exceeded the configured size limit."));
            }
            const auto errorCode = performCode == CURLE_OPERATION_TIMEDOUT
                ? foundation::error::ErrorCode::Timeout
                : foundation::error::ErrorCode::TransportFailure;
            return std::unexpected(transportError(errorCode, curl_easy_strerror(performCode)));
        }

        long statusCode = 0;
        if (curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &statusCode) != CURLE_OK ||
            statusCode < 0 || statusCode > 65'535) {
            return std::unexpected(transportError(
                foundation::error::ErrorCode::TransportFailure,
                "libcurl returned an invalid HTTP status code."));
        }

        return http::HttpResponse{
            .statusCode = static_cast<std::uint16_t>(statusCode),
            .body = std::move(responseBody),
            .headers = std::move(responseHeaders),
        };
    }
};
}

foundation::result::Result<std::unique_ptr<http::IHttpTransport>> makeTransport()
{
    std::call_once(curlInitializationFlag, &initializeCurl);
    if (curlInitializationCode != CURLE_OK) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::TransportFailure,
            curl_easy_strerror(curlInitializationCode)));
    }

    std::unique_ptr<http::IHttpTransport> transport = std::make_unique<CurlHttpTransport>();
    return transport;
}

}
