module;

#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>

module farcaster.test.hypersnap.transport;

import farcaster.foundation.error;

namespace farcaster::test::hypersnap::transport {

void FakeTransport::enqueue(
    const std::uint16_t statusCode,
    std::string body,
    std::vector<std::string> headers)
{
    m_responses.push_back(farcaster::transport::http::HttpResponse{
        .statusCode = statusCode,
        .body = std::move(body),
        .headers = std::move(headers),
    });
}

farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse>
FakeTransport::get(const farcaster::transport::http::HttpRequest& request)
{
    m_urls.push_back(request.url);
    m_requests.push_back(request);
    if (m_responses.empty()) {
        return std::unexpected(farcaster::foundation::error::Error::make(
            farcaster::foundation::error::ErrorCode::InternalFailure,
            "Fake transport has no queued response."));
    }
    auto response = std::move(m_responses.front());
    m_responses.pop_front();
    return response;
}

farcaster::foundation::result::Result<farcaster::transport::http::HttpResponse>
FakeTransport::post(const farcaster::transport::http::HttpPostRequest& request)
{
    m_urls.push_back(request.url);
    m_postRequests.push_back(request);
    if (m_responses.empty()) {
        return std::unexpected(farcaster::foundation::error::Error::make(
            farcaster::foundation::error::ErrorCode::InternalFailure,
            "Fake transport has no queued response."));
    }
    auto response = std::move(m_responses.front());
    m_responses.pop_front();
    return response;
}

const std::vector<std::string>& FakeTransport::urls() const noexcept
{
    return m_urls;
}

const std::vector<farcaster::transport::http::HttpRequest>& FakeTransport::requests() const noexcept
{
    return m_requests;
}

const std::vector<farcaster::transport::http::HttpPostRequest>&
FakeTransport::postRequests() const noexcept
{
    return m_postRequests;
}

}
