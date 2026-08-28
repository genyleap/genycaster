module;

#include <chrono>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module farcaster.provider.hypersnap.feed;

import farcaster.domain.channel;
import farcaster.domain.identity;
import farcaster.capability.catalog;
import farcaster.capability.model;
import farcaster.domain.feed;
import farcaster.foundation.error;
import farcaster.provider.hypersnap.codec;
import farcaster.provider.hypersnap.continuation;
import farcaster.provider.hypersnap.request;

namespace farcaster::provider::hypersnap::feed {
namespace {

constexpr std::chrono::milliseconds followingRequestTimeout{5'000};
constexpr std::chrono::milliseconds discoveryRequestTimeout{8'000};

[[nodiscard]] foundation::result::Result<std::size_t> validatedLimit(
    const capability::pagination::PageRequest& pageRequest)
{
    if (pageRequest.limit == 0 || pageRequest.limit > 100) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Hypersnap v2 page limit must be between 1 and 100."));
    }
    return pageRequest.limit;
}

[[nodiscard]] foundation::result::Result<capability::feed::FeedPage> readFeed(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuationStore,
    const config::Config& config,
    const capability::model::CapabilityId& capabilityId,
    const std::string_view path,
    std::vector<std::pair<std::string, std::string>> query,
    const capability::pagination::PageRequest& pageRequest,
    const std::chrono::milliseconds requestTimeout)
{
    if (auto status = request::validateConfig(config); !status) {
        return std::unexpected(std::move(status.error()));
    }
    auto limit = validatedLimit(pageRequest);
    if (!limit) {
        return std::unexpected(std::move(limit.error()));
    }

    continuation::Store continuations{continuationStore, config.provider.id};
    auto cursor = continuations.resolve(capabilityId, pageRequest);
    if (!cursor) {
        return std::unexpected(std::move(cursor.error()));
    }
    query.emplace_back("limit", std::to_string(*limit));
    if (*cursor) {
        query.emplace_back("cursor", **cursor);
    }

    request::Client requests{transport, config};
    auto body = requests.getJson(
        path,
        query,
        request::GetOptions{.timeout = requestTimeout});
    if (!body) {
        return std::unexpected(std::move(body.error()));
    }
    auto payload = codec::decodeFeed(*body);
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }

    capability::feed::FeedPage page{};
    page.casts = std::move(payload->casts);
    page.profiles = std::move(payload->profiles);
    page.entries.reserve(page.casts.size());
    for (const auto& cast : page.casts) {
        page.entries.push_back(domain::feed::FeedEntry{
            .castId = cast.id,
            .sortTime = cast.createdAt,
        });
    }
    auto next = continuations.bind(capabilityId, std::move(payload->providerCursor));
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    page.next = std::move(*next);
    return page;
}

}

FeedSource::FeedSource(
    transport::http::IHttpTransport& transport,
    storage::continuation::ContinuationStore& continuations,
    config::Config config)
    : m_transport(transport), m_continuations(continuations), m_config(std::move(config))
{
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::following(
    const domain::identity::Fid fid,
    const capability::pagination::PageRequest& pageRequest)
{
    return readFeed(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedFollowingRead(),
        "/v2/farcaster/feed/following",
        {{"fid", std::to_string(fid.value)}},
        pageRequest,
        followingRequestTimeout);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::trending(
    const capability::pagination::PageRequest& pageRequest)
{
    return readFeed(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedTrendingRead(),
        "/v2/farcaster/feed/trending",
        {},
        pageRequest,
        discoveryRequestTimeout);
}

foundation::result::Result<capability::feed::FeedPage> FeedSource::channel(
    const domain::channel::ChannelId& channelId,
    const capability::pagination::PageRequest& pageRequest)
{
    if (channelId.value.empty()) {
        return std::unexpected(foundation::error::Error::make(
            foundation::error::ErrorCode::InvalidArgument,
            "Channel id must not be empty."));
    }
    return readFeed(
        m_transport,
        m_continuations,
        m_config,
        capability::catalog::feedChannelRead(),
        "/v2/farcaster/feed/channels",
        {{"channel_ids", channelId.value}},
        pageRequest,
        discoveryRequestTimeout);
}

}
